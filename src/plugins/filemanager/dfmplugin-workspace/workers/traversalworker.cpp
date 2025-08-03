// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "traversalworker.h"

#include <dfm-base/base/schemefactory.h>
#include <dfm-base/utils/universalutils.h>

#include <QDebug>
#include <QMutexLocker>

using namespace dfmplugin_workspace;

TraversalWorker::TraversalWorker(QObject *parent)
    : QObject(parent)
{
    fmDebug() << "TraversalWorker created in thread:" << QThread::currentThreadId();
    
    // Initialize batch timer for streaming mode
    m_batchTimer = new QTimer(this);
    m_batchTimer->setSingleShot(true);
    m_batchTimer->setInterval(kBatchIntervalMs);
    connect(m_batchTimer, &QTimer::timeout, this, &TraversalWorker::processNextBatch);
}

TraversalWorker::~TraversalWorker()
{
    fmDebug() << "TraversalWorker destroyed in thread:" << QThread::currentThreadId();
    cancelAllTraversals();
}

void TraversalWorker::startTraversal(const DirectoryRequest &request)
{
    fmDebug() << "Starting traversal for request:" << request.requestId()
             << "URL:" << request.directoryUrl().toString();

    QMutexLocker locker(&m_mutex);
    
    // Cancel any existing traversal with the same request ID
    if (m_activeTraversals.contains(request.requestId())) {
        fmDebug() << "Cancelling existing traversal for request:" << request.requestId();
        m_activeTraversals[request.requestId()].isCancelled = true;
    }

    // Create new traversal context with request
    TraversalContext context(request);
    context.timer.start();

    // Create directory iterator using the factory
    context.iterator = DirIteratorFactory::create<AbstractDirIterator>(
        request.directoryUrl(),
        request.filterConfig().nameFilters,
        request.filterConfig().dirFilters,
        QDirIterator::FollowSymlinks
    );

    if (!context.iterator) {
        locker.unlock();
        emit traversalError(request.requestId(),
                           QString("Failed to create directory iterator for: %1")
                           .arg(request.directoryUrl().toString()));
        return;
    }

    // Store context and start traversal
    m_activeTraversals[request.requestId()] = context;
    
    // Determine traversal mode and start processing
    auto &activeContext = m_activeTraversals[request.requestId()];
    
    // Critical: Use the same decision logic as original TraversalDirThreadManager
    if (activeContext.iterator->oneByOne()) {
        fmDebug() << "Using streaming mode (iteratorOneByOne) for request:" << request.requestId();
        locker.unlock();
        processStreamMode(activeContext);
    } else {
        fmDebug() << "Using batch mode (iteratorAll) for request:" << request.requestId();
        locker.unlock();
        processBatchMode(activeContext);
    }
}

void TraversalWorker::cancelTraversal(const QString &requestId)
{
    fmDebug() << "Cancelling traversal for request:" << requestId;
    
    QMutexLocker locker(&m_mutex);
    auto it = m_activeTraversals.find(requestId);
    if (it != m_activeTraversals.end()) {
        it->isCancelled = true;
        fmDebug() << "Marked traversal as cancelled:" << requestId;
    }
}

void TraversalWorker::cancelAllTraversals()
{
    fmDebug() << "Cancelling all active traversals";
    
    QMutexLocker locker(&m_mutex);
    for (auto &context : m_activeTraversals) {
        context.isCancelled = true;
    }
    
    if (m_batchTimer->isActive()) {
        m_batchTimer->stop();
    }
}

void TraversalWorker::processNextBatch()
{
    QMutexLocker locker(&m_mutex);
    
    // Find the first active streaming traversal
    for (auto it = m_activeTraversals.begin(); it != m_activeTraversals.end(); ++it) {
        if (!it->isCancelled && it->iterator && it->iterator->oneByOne()) {
            locker.unlock();
            processStreamMode(*it);
            return;
        }
    }
}

void TraversalWorker::processBatchMode(TraversalContext &context)
{
    fmDebug() << "Processing batch mode (iteratorAll equivalent) for request:" << context.request.requestId();
    
    if (shouldCancel(context.request.requestId())) {
        cleanupTraversal(context.request.requestId());
        return;
    }

    try {
        // Equivalent to original iteratorAll() - get all files at once
        auto sortInfoList = context.iterator->sortFileInfoList();
        
        fmDebug() << "Batch mode retrieved" << sortInfoList.size() << "items for request:"
                 << context.request.requestId();

        // Convert SortInfoPointer list to FileItem list
        QList<FileItem> fileItems;
        fileItems.reserve(sortInfoList.size());
        
        for (const auto &sortInfo : sortInfoList) {
            if (shouldCancel(context.request.requestId())) {
                cleanupTraversal(context.request.requestId());
                return;
            }
            
            // Create FileItem from SortInfo
            FileItem item(
                sortInfo->fileUrl(),
                sortInfo->fileSize(),
                QDateTime::fromSecsSinceEpoch(sortInfo->lastModifiedTime()),
                QDateTime::fromSecsSinceEpoch(sortInfo->lastReadTime()),
                QDateTime::fromSecsSinceEpoch(sortInfo->createTime()),
                sortInfo->isDir(),
                sortInfo->isHide(),
                sortInfo->isSymLink(),
                sortInfo->isReadable(),
                sortInfo->isWriteable(),
                sortInfo->isExecutable()
            );
            
            fileItems.append(item);
        }
        
        context.collectedItems = fileItems;
        
        // Create final directory data
        DirectoryData data = createDirectoryData(context);
        bool noDataProduced = fileItems.isEmpty();
        
        fmDebug() << "Batch mode completed for request:" << context.request.requestId()
                 << "Items:" << fileItems.size() << "No data:" << noDataProduced;

        cleanupTraversal(context.request.requestId());
        emit traversalCompleted(context.request.requestId(), data, noDataProduced);
        
    } catch (const std::exception &e) {
        fmWarning() << "Exception in batch mode processing:" << e.what();
        cleanupTraversal(context.request.requestId());
        emit traversalError(context.request.requestId(), QString("Batch processing error: %1").arg(e.what()));
    } catch (...) {
        fmWarning() << "Unknown exception in batch mode processing";
        cleanupTraversal(context.request.requestId());
        emit traversalError(context.request.requestId(), "Unknown batch processing error");
    }
}

void TraversalWorker::processStreamMode(TraversalContext &context)
{
    fmDebug() << "Processing stream mode (iteratorOneByOne equivalent) for request:" << context.request.requestId();
    
    if (shouldCancel(context.request.requestId())) {
        cleanupTraversal(context.request.requestId());
        return;
    }

    try {
        QList<FileItem> batchItems;
        int processedInBatch = 0;
        
        // Process files one by one until batch limit or timeout (preserving original logic)
        while (context.iterator->hasNext() && processedInBatch < kBatchSizeLimit) {
            if (shouldCancel(context.request.requestId())) {
                cleanupTraversal(context.request.requestId());
                return;
            }
            
            auto fileUrl = context.iterator->next();
            if (fileUrl.isValid()) {
                auto fileInfo = context.iterator->fileInfo();
                if (fileInfo) {
                    FileItem item = convertToFileItem(fileInfo);
                    batchItems.append(item);
                    context.collectedItems.append(item);
                    processedInBatch++;
                    context.itemCount++;
                }
            }
            
            // Check if we should emit progress (time-based, preserving 200ms logic)
            if (context.timer.elapsed() >= kBatchIntervalMs) {
                break;
            }
        }
        
        // Emit progress if we have items
        if (!batchItems.isEmpty()) {
            fmDebug() << "Stream mode progress for request:" << context.request.requestId()
                     << "Batch items:" << batchItems.size() << "Total items:" << context.itemCount;
            
            emit traversalProgress(context.request.requestId(), batchItems, context.isFirstBatch);
            context.isFirstBatch = false;
        }
        
        // Check if traversal is complete
        if (!context.iterator->hasNext()) {
            fmDebug() << "Stream mode completed for request:" << context.request.requestId()
                     << "Total items:" << context.itemCount;
            
            DirectoryData data = createDirectoryData(context);
            bool noDataProduced = context.collectedItems.isEmpty();
            
            cleanupTraversal(context.request.requestId());
            emit traversalCompleted(context.request.requestId(), data, noDataProduced);
        } else {
            // Schedule next batch (preserving original timing)
            context.timer.restart();
            if (!m_batchTimer->isActive()) {
                m_batchTimer->start();
            }
        }
        
    } catch (const std::exception &e) {
        fmWarning() << "Exception in stream mode processing:" << e.what();
        cleanupTraversal(context.request.requestId());
        emit traversalError(context.request.requestId(), QString("Stream processing error: %1").arg(e.what()));
    } catch (...) {
        fmWarning() << "Unknown exception in stream mode processing";
        cleanupTraversal(context.request.requestId());
        emit traversalError(context.request.requestId(), "Unknown stream processing error");
    }
}

FileItem TraversalWorker::convertToFileItem(const FileInfoPointer &fileInfo) const
{
    if (!fileInfo) {
        return FileItem();
    }
    
    return FileItem(
        fileInfo->urlOf(UrlInfoType::kUrl),
        fileInfo->size(),
        fileInfo->timeOf(TimeInfoType::kLastModified).value<QDateTime>(),
        fileInfo->timeOf(TimeInfoType::kLastRead).value<QDateTime>(),
        fileInfo->timeOf(TimeInfoType::kCreateTime).value<QDateTime>(),
        fileInfo->isAttributes(OptInfoType::kIsDir),
        fileInfo->isAttributes(OptInfoType::kIsHidden),
        fileInfo->isAttributes(OptInfoType::kIsSymLink),
        fileInfo->isAttributes(OptInfoType::kIsReadable),
        fileInfo->isAttributes(OptInfoType::kIsWritable),
        fileInfo->isAttributes(OptInfoType::kIsExecutable)
    );
}

DirectoryData TraversalWorker::createDirectoryData(const TraversalContext &context) const
{
    return DirectoryData(
        context.request.directoryUrl(),
        context.collectedItems,
        context.request.sortConfig(),
        context.request.filterConfig(),
        context.request.requestId(),
        true  // isComplete
    );
}

bool TraversalWorker::shouldCancel(const QString &requestId) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_activeTraversals.find(requestId);
    return it != m_activeTraversals.end() && it->isCancelled;
}

void TraversalWorker::cleanupTraversal(const QString &requestId)
{
    fmDebug() << "Cleaning up traversal for request:" << requestId;
    
    QMutexLocker locker(&m_mutex);
    m_activeTraversals.remove(requestId);
    
    // Stop batch timer if no active streaming traversals
    bool hasActiveStreaming = false;
    for (const auto &context : m_activeTraversals) {
        if (!context.isCancelled && context.iterator && context.iterator->oneByOne()) {
            hasActiveStreaming = true;
            break;
        }
    }
    
    if (!hasActiveStreaming && m_batchTimer->isActive()) {
        m_batchTimer->stop();
    }
}

