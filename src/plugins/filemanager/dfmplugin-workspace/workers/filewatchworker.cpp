// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filewatchworker.h"

#include <dfm-base/dfm_log_defines.h>

#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QThread>

using namespace dfmplugin_workspace;

FileWatchWorker::FileWatchWorker(QObject *parent)
    : QObject(parent)
{
    fmDebug() << "FileWatchWorker created in thread:" << QThread::currentThreadId();
    
    // Initialize batch timer
    m_batchTimer = new QTimer(this);
    m_batchTimer->setSingleShot(false);
    m_batchTimer->setInterval(kBatchIntervalMs);
    connect(m_batchTimer, &QTimer::timeout, this, &FileWatchWorker::processBatchedChanges);
}

FileWatchWorker::~FileWatchWorker()
{
    fmDebug() << "FileWatchWorker destroyed in thread:" << QThread::currentThreadId();
    stopAllWatching();
}

void FileWatchWorker::startWatching(const QUrl &directoryUrl, const QString &watcherId)
{
    fmDebug() << "Starting file watch for:" << directoryUrl.toString() << "ID:" << watcherId;
    
    // Stop existing watcher with same ID if any
    if (m_watchContexts.contains(watcherId)) {
        fmDebug() << "Stopping existing watcher with ID:" << watcherId;
        auto &context = m_watchContexts[watcherId];
        context.isActive = false;
        context.pendingChanges.clear();
    }
    
    // Create new watch context
    WatchContext context;
    context.directoryUrl = directoryUrl;
    context.watcherId = watcherId;
    context.isActive = true;
    context.lastBatchTime.start();
    
    m_watchContexts[watcherId] = context;
    
    // Start batch timer if not already running
    if (!m_batchTimer->isActive()) {
        m_batchTimer->start();
    }
    
    fmDebug() << "Watch context created for:" << directoryUrl.toString();
}

void FileWatchWorker::stopWatching(const QString &watcherId)
{
    fmDebug() << "Stopping file watch for ID:" << watcherId;
    
    auto it = m_watchContexts.find(watcherId);
    if (it != m_watchContexts.end()) {
        it->isActive = false;
        
        // Send any pending changes before stopping
        if (!it->pendingChanges.isEmpty()) {
            QList<FileChange> changes = deduplicateChanges(it->pendingChanges);
            emit fileChangesBatched(watcherId, changes);
        }
        
        m_watchContexts.erase(it);
    }
    
    // Stop batch timer if no active watchers
    if (m_watchContexts.isEmpty() && m_batchTimer->isActive()) {
        m_batchTimer->stop();
    }
}

void FileWatchWorker::stopAllWatching()
{
    fmDebug() << "Stopping all file watchers";
    
    // Send pending changes for all active watchers
    for (auto it = m_watchContexts.begin(); it != m_watchContexts.end(); ++it) {
        if (it->isActive && !it->pendingChanges.isEmpty()) {
            QList<FileChange> changes = deduplicateChanges(it->pendingChanges);
            emit fileChangesBatched(it->watcherId, changes);
        }
    }
    
    m_watchContexts.clear();
    
    if (m_batchTimer->isActive()) {
        m_batchTimer->stop();
    }
}

void FileWatchWorker::onFileCreated(const QUrl &url)
{
    QString watcherId = findWatcherForUrl(url);
    if (!watcherId.isEmpty()) {
        FileChange change(url, FileChange::ChangeType::Created, QDateTime::currentDateTime());
        addPendingChange(watcherId, change);
        fmDebug() << "File created:" << url.toString() << "Watcher:" << watcherId;
    }
}

void FileWatchWorker::onFileDeleted(const QUrl &url)
{
    QString watcherId = findWatcherForUrl(url);
    if (!watcherId.isEmpty()) {
        FileChange change(url, FileChange::ChangeType::Deleted, QDateTime::currentDateTime());
        addPendingChange(watcherId, change);
        fmDebug() << "File deleted:" << url.toString() << "Watcher:" << watcherId;
    }
}

void FileWatchWorker::onFileModified(const QUrl &url)
{
    QString watcherId = findWatcherForUrl(url);
    if (!watcherId.isEmpty()) {
        FileChange change(url, FileChange::ChangeType::Modified, QDateTime::currentDateTime());
        addPendingChange(watcherId, change);
        fmDebug() << "File modified:" << url.toString() << "Watcher:" << watcherId;
    }
}

void FileWatchWorker::onFileMoved(const QUrl &fromUrl, const QUrl &toUrl)
{
    QString watcherId = findWatcherForUrl(fromUrl);
    if (watcherId.isEmpty()) {
        watcherId = findWatcherForUrl(toUrl);
    }
    
    if (!watcherId.isEmpty()) {
        FileChange change(toUrl, FileChange::ChangeType::Moved, QDateTime::currentDateTime(), fromUrl);
        addPendingChange(watcherId, change);
        fmDebug() << "File moved:" << fromUrl.toString() << "to" << toUrl.toString() << "Watcher:" << watcherId;
    }
}

void FileWatchWorker::onFileChanged(const QUrl &url)
{
    QString watcherId = findWatcherForUrl(url);
    if (!watcherId.isEmpty()) {
        FileChange change(url, FileChange::ChangeType::Modified, QDateTime::currentDateTime());
        addPendingChange(watcherId, change);
        fmDebug() << "File changed:" << url.toString() << "Watcher:" << watcherId;
    }
}

void FileWatchWorker::processBatchedChanges()
{
    for (auto it = m_watchContexts.begin(); it != m_watchContexts.end(); ++it) {
        if (!it->isActive) {
            continue;
        }
        
        // Check if it's time to send batch or if we have too many pending changes
        bool shouldSendBatch = false;
        
        if (!it->pendingChanges.isEmpty()) {
            qint64 elapsedMs = it->lastBatchTime.elapsed();
            bool timeThresholdReached = elapsedMs >= kBatchIntervalMs;
            bool countThresholdReached = it->pendingChanges.size() >= kMaxPendingChanges;
            
            shouldSendBatch = timeThresholdReached || countThresholdReached;
        }
        
        if (shouldSendBatch) {
            QList<FileChange> changes = deduplicateChanges(it->pendingChanges);
            QString watcherId = it->watcherId;
            
            it->pendingChanges.clear();
            it->lastBatchTime.restart();
            
            fmDebug() << "Sending batched changes for watcher:" << watcherId
                     << "Count:" << changes.size();
            
            emit fileChangesBatched(watcherId, changes);
        }
    }
}

void FileWatchWorker::addPendingChange(const QString &watcherId, const FileChange &change)
{
    auto it = m_watchContexts.find(watcherId);
    if (it != m_watchContexts.end() && it->isActive) {
        it->pendingChanges.append(change);
        
        // Prevent memory overflow
        if (it->pendingChanges.size() > kMaxPendingChanges * 2) {
            fmWarning() << "Too many pending changes for watcher:" << watcherId
                       << "Dropping oldest changes";
            it->pendingChanges = it->pendingChanges.mid(it->pendingChanges.size() - kMaxPendingChanges);
        }
    }
}

QList<FileChange> FileWatchWorker::deduplicateChanges(const QList<FileChange> &changes) const
{
    if (changes.isEmpty()) {
        return changes;
    }
    
    // Use a map to keep track of the latest change for each file
    QMap<QUrl, FileChange> latestChanges;
    
    for (const auto &change : changes) {
        QUrl fileUrl = change.fileUrl();
        
        // Handle moved files specially
        if (change.changeType() == FileChange::ChangeType::Moved && !change.oldUrl().isEmpty()) {
            // Remove any previous changes for the old URL
            latestChanges.remove(change.oldUrl());
        }
        
        // Check if we already have a change for this file
        auto it = latestChanges.find(fileUrl);
        if (it != latestChanges.end()) {
            // Merge changes intelligently
            FileChange::ChangeType existingType = it->changeType();
            FileChange::ChangeType newType = change.changeType();
            
            // Created -> Deleted = no change (file created and deleted in same batch)
            if (existingType == FileChange::ChangeType::Created && 
                newType == FileChange::ChangeType::Deleted) {
                latestChanges.erase(it);
                continue;
            }
            
            // Created -> Modified = Created (still a new file)
            if (existingType == FileChange::ChangeType::Created && 
                newType == FileChange::ChangeType::Modified) {
                // Keep the created change, update timestamp
                continue;
            }
            
            // Modified -> Deleted = Deleted
            // Any -> Moved = Moved
            // Default: keep the latest change
        }
        
        latestChanges[fileUrl] = change;
    }
    
    // Convert map back to list, preserving chronological order as much as possible
    QList<FileChange> result;
    result.reserve(latestChanges.size());
    
    for (auto it = latestChanges.begin(); it != latestChanges.end(); ++it) {
        result.append(it.value());
    }
    
    return result;
}

QString FileWatchWorker::findWatcherForUrl(const QUrl &url) const
{
    for (auto it = m_watchContexts.begin(); it != m_watchContexts.end(); ++it) {
        if (it->isActive && isUrlWithinDirectory(it->directoryUrl, url)) {
            return it->watcherId;
        }
    }
    
    return QString();
}

bool FileWatchWorker::isUrlWithinDirectory(const QUrl &watchedDir, const QUrl &fileUrl) const
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
