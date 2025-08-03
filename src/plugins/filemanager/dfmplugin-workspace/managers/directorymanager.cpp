// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directorymanager.h"
#include "directorydatamanager.h"
#include "filewatchmanager.h"
#include "workers/traversalworker.h"

#include <dfm-base/dfm_log_defines.h>

#include <QThread>
#include <QMutexLocker>
#include <QUuid>
#include <QUrlQuery>

using namespace dfmplugin_workspace;

int DirectoryManager::s_requestCounter = 0;

DirectoryManager::DirectoryManager(QObject *parent)
    : QObject(parent)
    , m_rootUrl()
    , m_cacheEnabled(true)
    , m_dataManager(nullptr)
    , m_watchManager(nullptr)
    , m_traversalThread(nullptr)
    , m_traversalWorker(nullptr)
{
    fmInfo() << "DirectoryManager created (default) in thread:" << QThread::currentThreadId();
    
    // Initialize component managers
    m_dataManager = new DirectoryDataManager(this);
    m_watchManager = new FileWatchManager(this);
    
    // Initialize traversal worker
    initializeTraversalWorker();
    
    setupConnections();
    
    fmDebug() << "DirectoryManager initialization completed";
}

DirectoryManager::DirectoryManager(const QUrl& rootUrl, bool enableCache, QObject *parent)
    : QObject(parent)
    , m_rootUrl(rootUrl)
    , m_cacheEnabled(enableCache)
    , m_dataManager(nullptr)
    , m_watchManager(nullptr)
    , m_traversalThread(nullptr)
    , m_traversalWorker(nullptr)
{
    fmInfo() << "DirectoryManager created for URL:" << rootUrl.toString() << "cache:" << enableCache;
    
    // Initialize component managers
    m_dataManager = new DirectoryDataManager(this);
    m_watchManager = new FileWatchManager(this);
    
    // Initialize traversal worker
    initializeTraversalWorker();
    
    setupConnections();
    
    fmDebug() << "DirectoryManager initialization completed";
}

DirectoryManager::~DirectoryManager()
{
    fmDebug() << "DirectoryManager destroyed";
    cleanupTraversalWorker();
}

QString DirectoryManager::requestDirectoryData(const QUrl& directoryUrl,
                                              const SortConfig& sortConfig,
                                              const FilterConfig& filterConfig,
                                              bool useCache)
{
    QString requestId = generateRequestId();
    
    fmDebug() << "Requesting directory data for:" << directoryUrl.toString() 
              << "requestId:" << requestId << "useCache:" << useCache;
    
    // Check cache first if requested
    if (useCache) {
        DirectoryRequest cacheRequest(directoryUrl, requestId, sortConfig, filterConfig, useCache);
        DirectoryData cachedData = m_dataManager->getCachedData(cacheRequest);
        
        if (cachedData.directoryUrl().isValid()) {
            fmDebug() << "Cache hit for:" << directoryUrl.toString();
            
            // Emit cached data immediately
            emit directoryDataReady(requestId, cachedData);
            return requestId;
        }
    }
    
    // Create directory request
    DirectoryRequest request(directoryUrl, requestId, sortConfig, filterConfig, useCache);
    
    // Track the request
    m_activeRequests[requestId] = directoryUrl;
    
    // Forward to traversal worker
    emit requestTraversal(request);
    
    // Start file watching if not already watching
    enableWatching(directoryUrl, true);
    
    return requestId;
}

void DirectoryManager::cancelRequest(const QString& requestId)
{
    if (!m_activeRequests.contains(requestId)) {
        fmWarning() << "Cannot cancel unknown request:" << requestId;
        return;
    }
    
    fmDebug() << "Cancelling request:" << requestId;
    
    // Forward to traversal worker
    emit requestTraversalCancellation(requestId);
    
    // Remove from active requests
    m_activeRequests.remove(requestId);
}

void DirectoryManager::refreshDirectory(const QUrl& directoryUrl)
{
    fmDebug() << "Refreshing directory:" << directoryUrl.toString();
    
    // Clear cached data
    m_dataManager->invalidateCache(directoryUrl);
    m_cachedData.remove(directoryUrl);
    
    // Request fresh data
    SortConfig defaultSort;
    FilterConfig defaultFilter;
    requestDirectoryData(directoryUrl, defaultSort, defaultFilter, false);
}

void DirectoryManager::enableWatching(const QUrl& directoryUrl, bool enabled)
{
    fmDebug() << "Setting file watching for" << directoryUrl.toString() << "to" << enabled;
    
    QString watcherId = QString("watch_%1").arg(directoryUrl.toString());
    
    if (enabled) {
        m_watchManager->startWatching(directoryUrl, watcherId);
    } else {
        m_watchManager->stopWatching(watcherId);
    }
}

QList<FileItem> DirectoryManager::getFileItems(const QUrl& directoryUrl) const
{
    if (m_cachedData.contains(directoryUrl)) {
        return m_cachedData[directoryUrl].files();
    }
    
    return QList<FileItem>();
}

QStringList DirectoryManager::getSearchKeywords(const QUrl& directoryUrl) const
{
    // Check if this is a search URL
    if (directoryUrl.scheme() != "search") {
        return QStringList();
    }
    
    // Extract keywords from search URL
    QStringList keywords;
    
    // Parse query parameters for keywords
    QUrlQuery query(directoryUrl);
    if (query.hasQueryItem("keyword")) {
        QString keywordStr = query.queryItemValue("keyword");
        keywords = keywordStr.split(' ', Qt::SkipEmptyParts);
    }
    
    // Also check the path for keywords
    QString path = directoryUrl.path();
    if (!path.isEmpty() && path != "/") {
        keywords.append(path.split('/', Qt::SkipEmptyParts));
    }
    
    return keywords;
}

void DirectoryManager::onDataManagerDataReady(const QString& requestId, const DirectoryData& data)
{
    fmDebug() << "Data manager reported data ready for request:" << requestId;
    
    // Cache the data
    m_cachedData[data.directoryUrl()] = data;
    m_dataManager->setCachedData(data);
    
    // Forward the signal
    emit directoryDataReady(requestId, data);
    
    // Remove from active requests
    m_activeRequests.remove(requestId);
}

void DirectoryManager::onWatchManagerChangesDetected(const QUrl& directoryUrl, const QList<FileChange>& changes)
{
    fmDebug() << "Watch manager reported" << changes.size() << "changes for:" << directoryUrl.toString();
    
    // Update cached data if available
    if (m_cachedData.contains(directoryUrl)) {
        // Apply changes to cached data
        auto& cachedData = m_cachedData[directoryUrl];
        auto fileItems = cachedData.files();
        
        for (const auto& change : changes) {
            switch (change.changeType()) {
                case FileChange::ChangeType::Created:
                    // Add new file item (would need FileInfo to create FileItem)
                    // This is a simplified version - in practice we'd need to create FileItem from the URL
                    break;
                case FileChange::ChangeType::Deleted:
                    // Remove file item
                    fileItems.removeIf([&change](const FileItem& item) {
                        return item.url() == change.fileUrl();
                    });
                    break;
                case FileChange::ChangeType::Modified:
                    // Update existing file item
                    // This would require refreshing the FileInfo for the changed file
                    break;
                case FileChange::ChangeType::Moved:
                    // Handle move: remove old, add new
                    fileItems.removeIf([&change](const FileItem& item) {
                        return item.url() == change.oldUrl();
                    });
                    // Add new location (would need FileInfo)
                    break;
            }
        }
        
        // Update cached data
        DirectoryData updatedData(cachedData.directoryUrl(), fileItems, 
                                 cachedData.sortConfig(), cachedData.filterConfig(),
                                 cachedData.requestId(), cachedData.isComplete());
        m_cachedData[directoryUrl] = updatedData;
    }
    
    // Forward the signal
    emit directoryDataUpdated(directoryUrl, changes);
}

void DirectoryManager::onTraversalProgress(const QString& requestId, const QList<FileItem>& items, bool isFirstBatch)
{
    fmDebug() << "Traversal progress for request:" << requestId << "items:" << items.size();
    
    // For now, we don't emit progress signals as they're not in the simplified interface
    // This could be added later if needed
}

void DirectoryManager::onTraversalCompleted(const QString& requestId, const DirectoryData& data, bool noDataProduced)
{
    fmDebug() << "Traversal completed for request:" << requestId << "files:" << data.fileCount();
    
    // Cache the data
    m_cachedData[data.directoryUrl()] = data;
    m_dataManager->setCachedData(data);
    
    // Forward the signal
    emit directoryDataReady(requestId, data);
    
    // Remove from active requests
    m_activeRequests.remove(requestId);
}

void DirectoryManager::onTraversalError(const QString& requestId, const QString& errorMessage)
{
    fmWarning() << "Traversal error for request:" << requestId << "error:" << errorMessage;
    
    // Forward error signal
    emit requestError(requestId, errorMessage);
    
    // Remove from active requests
    m_activeRequests.remove(requestId);
}

void DirectoryManager::setupConnections()
{
    // Connect data manager signals
    connect(m_dataManager, &DirectoryDataManager::dataReady,
            this, &DirectoryManager::onDataManagerDataReady, Qt::QueuedConnection);
    
    connect(m_dataManager, &DirectoryDataManager::dataError,
            this, &DirectoryManager::requestError, Qt::QueuedConnection);
    
    // Connect watch manager signals
    connect(m_watchManager, &FileWatchManager::fileChangesBatched,
            this, &DirectoryManager::onWatchManagerChangesDetected, Qt::QueuedConnection);
    
    // Connect traversal worker signals
    connect(m_traversalWorker, &TraversalWorker::traversalProgress,
            this, &DirectoryManager::onTraversalProgress, Qt::QueuedConnection);
    
    connect(m_traversalWorker, &TraversalWorker::traversalCompleted,
            this, &DirectoryManager::onTraversalCompleted, Qt::QueuedConnection);
    
    connect(m_traversalWorker, &TraversalWorker::traversalError,
            this, &DirectoryManager::onTraversalError, Qt::QueuedConnection);
    
    // Connect manager requests to worker
    connect(this, &DirectoryManager::requestTraversal,
            m_traversalWorker, &TraversalWorker::startTraversal, Qt::QueuedConnection);
    
    connect(this, &DirectoryManager::requestTraversalCancellation,
            m_traversalWorker, &TraversalWorker::cancelTraversal, Qt::QueuedConnection);
    
    fmDebug() << "DirectoryManager connections established";
}

void DirectoryManager::initializeTraversalWorker()
{
    fmDebug() << "Initializing traversal worker thread";
    
    // Create worker thread
    m_traversalThread = new QThread(this);
    m_traversalThread->setObjectName("DirectoryTraversalThread");
    
    // Create worker
    m_traversalWorker = new TraversalWorker();
    m_traversalWorker->moveToThread(m_traversalThread);
    
    // Handle thread cleanup
    connect(m_traversalThread, &QThread::finished,
            m_traversalWorker, &QObject::deleteLater);
    
    // Start thread
    m_traversalThread->start();
    
    fmDebug() << "Traversal worker thread started successfully";
}

void DirectoryManager::cleanupTraversalWorker()
{
    if (m_traversalThread && m_traversalThread->isRunning()) {
        fmDebug() << "Cleaning up traversal worker thread";
        
        // Cancel all traversals
        if (m_traversalWorker) {
            QMetaObject::invokeMethod(m_traversalWorker, "cancelAllTraversals", Qt::QueuedConnection);
        }
        
        // Quit and wait for thread
        m_traversalThread->quit();
        if (!m_traversalThread->wait(5000)) {
            fmWarning() << "Traversal worker thread did not finish in time, terminating";
            m_traversalThread->terminate();
            m_traversalThread->wait(1000);
        }
        
        fmDebug() << "Traversal worker thread cleaned up successfully";
    }
    
    m_traversalWorker = nullptr; // Will be deleted by thread finished signal
}

QString DirectoryManager::generateRequestId() const
{
    return QString("req_%1_%2").arg(QDateTime::currentMSecsSinceEpoch())
                              .arg(++s_requestCounter);
} 