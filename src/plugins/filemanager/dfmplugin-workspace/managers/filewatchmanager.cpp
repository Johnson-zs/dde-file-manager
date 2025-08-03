// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filewatchmanager.h"
#include "workers/filewatchworker.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/dfm_log_defines.h>

#include <QMutexLocker>

using namespace dfmplugin_workspace;

FileWatchManager::FileWatchManager(QObject *parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
{
    fmDebug() << "FileWatchManager created in thread:" << QThread::currentThreadId();
    initializeWorkerThread();
}

FileWatchManager::~FileWatchManager()
{
    fmDebug() << "FileWatchManager destroyed";
    
    // Stop all watchers
    QMutexLocker locker(&m_mutex);
    for (auto it = m_watchers.begin(); it != m_watchers.end(); ++it) {
        if (it->watcher) {
            it->watcher->stopWatcher();
        }
    }
    m_watchers.clear();
    locker.unlock();
    
    cleanupWorkerThread();
}

bool FileWatchManager::startWatching(const QUrl &directoryUrl, const QString &watcherId)
{
    fmDebug() << "Starting file watch for:" << directoryUrl.toString() << "ID:" << watcherId;
    
    QMutexLocker locker(&m_mutex);
    
    // Check if already watching with this ID
    if (m_watchers.contains(watcherId)) {
        fmWarning() << "Watcher ID already exists:" << watcherId;
        return false;
    }
    
    // Create file watcher in main thread
    AbstractFileWatcherPointer watcher = createWatcher(directoryUrl);
    if (!watcher) {
        fmWarning() << "Failed to create file watcher for:" << directoryUrl.toString();
        return false;
    }
    
    // Connect watcher signals
    connectWatcherSignals(watcher);
    
    // Store watcher info
    WatcherInfo info(directoryUrl, watcherId, watcher);
    m_watchers[watcherId] = info;
    
    fmDebug() << "File watcher created successfully for:" << directoryUrl.toString();
    
    // Start watcher
    watcher->startWatcher();
    
    locker.unlock();
    
    // Notify worker thread about the new watcher
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "startWatching", Qt::QueuedConnection,
                                  Q_ARG(QUrl, directoryUrl), Q_ARG(QString, watcherId));
    }
    
    return true;
}

void FileWatchManager::stopWatching(const QString &watcherId)
{
    fmDebug() << "Stopping file watch for ID:" << watcherId;
    
    QMutexLocker locker(&m_mutex);
    
    auto it = m_watchers.find(watcherId);
    if (it != m_watchers.end()) {
        // Stop the watcher
        if (it->watcher) {
            it->watcher->stopWatcher();
        }
        
        QUrl directoryUrl = it->directoryUrl;
        m_watchers.erase(it);
        
        fmDebug() << "Stopped file watcher for:" << directoryUrl.toString();
        
        locker.unlock();
        
        // Notify worker thread about stopping the watcher
        if (m_worker) {
            QMetaObject::invokeMethod(m_worker, "stopWatching", Qt::QueuedConnection,
                                      Q_ARG(QString, watcherId));
        }
    }
}

void FileWatchManager::onFileCreated(const QUrl &url)
{
    // Find the watcher ID for this URL and forward to worker thread
    QString watcherId = findWatcherIdForUrl(url);
    if (!watcherId.isEmpty() && m_worker) {
        QMetaObject::invokeMethod(m_worker, "onFileCreated", Qt::QueuedConnection,
                                  Q_ARG(QUrl, url));
    }
}

void FileWatchManager::onFileDeleted(const QUrl &url)
{
    // Find the watcher ID for this URL and forward to worker thread
    QString watcherId = findWatcherIdForUrl(url);
    if (!watcherId.isEmpty() && m_worker) {
        QMetaObject::invokeMethod(m_worker, "onFileDeleted", Qt::QueuedConnection,
                                  Q_ARG(QUrl, url));
    }
}

void FileWatchManager::onFileModified(const QUrl &url)
{
    // Find the watcher ID for this URL and forward to worker thread
    QString watcherId = findWatcherIdForUrl(url);
    if (!watcherId.isEmpty() && m_worker) {
        QMetaObject::invokeMethod(m_worker, "onFileModified", Qt::QueuedConnection,
                                  Q_ARG(QUrl, url));
    }
}

void FileWatchManager::onFileMoved(const QUrl &fromUrl, const QUrl &toUrl)
{
    // Find the watcher ID for either URL and forward to worker thread
    QString watcherId = findWatcherIdForUrl(fromUrl);
    if (watcherId.isEmpty()) {
        watcherId = findWatcherIdForUrl(toUrl);
    }
    
    if (!watcherId.isEmpty() && m_worker) {
        QMetaObject::invokeMethod(m_worker, "onFileMoved", Qt::QueuedConnection,
                                  Q_ARG(QUrl, fromUrl), Q_ARG(QUrl, toUrl));
    }
}

void FileWatchManager::onWorkerFileChangesBatched(const QString &watcherId, const QList<FileChange> &changes)
{
    fmDebug() << "Received batched changes from worker:" << watcherId << "Count:" << changes.size();
    
    // Convert watcherId back to directoryUrl for proper signal emission
    QUrl directoryUrl = getDirectoryUrlForWatcher(watcherId);
    if (directoryUrl.isValid()) {
        emit fileChangesBatched(directoryUrl, changes);
    } else {
        fmWarning() << "Cannot find directory URL for watcher ID:" << watcherId;
    }
}

void FileWatchManager::onWorkerWatchError(const QString &watcherId, const QString &errorMessage)
{
    fmWarning() << "Watch error from worker:" << watcherId << errorMessage;
    emit watchError(watcherId, errorMessage);
}

AbstractFileWatcherPointer FileWatchManager::createWatcher(const QUrl &directoryUrl)
{
    try {
        // Use factory to create appropriate watcher for the URL scheme
        auto watcher = WatcherFactory::create<AbstractFileWatcher>(directoryUrl);
        if (!watcher) {
            fmWarning() << "WatcherFactory failed to create watcher for:" << directoryUrl.toString();
            return nullptr;
        }
        
        fmDebug() << "Created file watcher for scheme:" << directoryUrl.scheme() 
                 << "URL:" << directoryUrl.toString();
        
        return watcher;
        
    } catch (const std::exception &e) {
        fmWarning() << "Exception creating file watcher:" << e.what();
        return nullptr;
    } catch (...) {
        fmWarning() << "Unknown exception creating file watcher";
        return nullptr;
    }
}

void FileWatchManager::connectWatcherSignals(AbstractFileWatcherPointer watcher)
{
    if (!watcher) {
        return;
    }
    
    // Connect file system events to our handlers
    connect(watcher.get(), &AbstractFileWatcher::fileDeleted,
            this, &FileWatchManager::onFileDeleted, Qt::DirectConnection);
    
    connect(watcher.get(), &AbstractFileWatcher::fileAttributeChanged,
            this, &FileWatchManager::onFileModified, Qt::DirectConnection);
    
    connect(watcher.get(), &AbstractFileWatcher::subfileCreated,
            this, &FileWatchManager::onFileCreated, Qt::DirectConnection);
    
    connect(watcher.get(), &AbstractFileWatcher::fileRename,
            this, &FileWatchManager::onFileMoved, Qt::DirectConnection);
    
    fmDebug() << "Connected file watcher signals";
}

void FileWatchManager::initializeWorkerThread()
{
    fmDebug() << "Initializing file watch worker thread";
    
    // Create worker thread
    m_workerThread = new QThread(this);
    m_workerThread->setObjectName("FileWatchWorkerThread");
    
    // Create worker
    m_worker = new FileWatchWorker();
    m_worker->moveToThread(m_workerThread);
    
    // Connect worker signals
    connect(m_worker, &FileWatchWorker::fileChangesBatched,
            this, &FileWatchManager::onWorkerFileChangesBatched, Qt::QueuedConnection);
    
    connect(m_worker, &FileWatchWorker::watchError,
            this, &FileWatchManager::onWorkerWatchError, Qt::QueuedConnection);
    
    // Handle thread cleanup
    connect(m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    
    // Start thread
    m_workerThread->start();
    
    fmDebug() << "File watch worker thread started";
}

void FileWatchManager::cleanupWorkerThread()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        fmDebug() << "Cleaning up file watch worker thread";
        
        // Stop all watching in worker
        if (m_worker) {
            QMetaObject::invokeMethod(m_worker, "stopAllWatching", Qt::QueuedConnection);
        }
        
        // Quit and wait for thread
        m_workerThread->quit();
        if (!m_workerThread->wait(5000)) {
            fmWarning() << "File watch worker thread did not finish in time, terminating";
            m_workerThread->terminate();
            m_workerThread->wait(1000);
        }
        
        fmDebug() << "File watch worker thread cleaned up";
    }
    
    m_worker = nullptr; // Will be deleted by thread finished signal
} 

QString FileWatchManager::findWatcherIdForUrl(const QUrl &url) const
{
    QMutexLocker locker(&m_mutex);
    
    for (auto it = m_watchers.begin(); it != m_watchers.end(); ++it) {
        if (isUrlWithinDirectory(it->directoryUrl, url)) {
            return it->watcherId;
        }
    }
    
    return QString();
}

bool FileWatchManager::isUrlWithinDirectory(const QUrl &watchedDir, const QUrl &fileUrl) const
{
    if (watchedDir.scheme() != fileUrl.scheme()) {
        return false;
    }
    
    QString watchedPath = watchedDir.path();
    QString filePath = fileUrl.path();
    
    // Ensure watched path ends with /
    if (!watchedPath.endsWith('/')) {
        watchedPath += '/';
    }
    
    // Check if file path starts with watched path
    return filePath.startsWith(watchedPath) || filePath == watchedDir.path();
} 

QUrl FileWatchManager::getDirectoryUrlForWatcher(const QString &watcherId) const
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_watchers.find(watcherId);
    if (it != m_watchers.end()) {
        return it->directoryUrl;
    }
    
    return QUrl(); // Return empty URL if watcher not found
} 
