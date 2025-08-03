// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRAVERSALWORKER_H
#define TRAVERSALWORKER_H

#include "dfmplugin_workspace_global.h"
#include "data/directoryrequest.h"
#include "data/directorydata.h"
#include "data/fileitem.h"

#include <dfm-base/dfm_base_global.h>
#include <dfm-base/interfaces/abstractdiriterator.h>

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QMap>

DFMBASE_USE_NAMESPACE

namespace dfmplugin_workspace {

/**
 * @brief Worker class for directory traversal in dedicated thread
 * 
 * This class handles directory traversal operations in a separate worker thread,
 * preserving the original iteratorAll (batch mode) and iteratorOneByOne (streaming mode)
 * mechanisms from TraversalDirThreadManager.
 * 
 * The worker automatically chooses between batch and streaming modes based on the
 * directory iterator's oneByOne() method, ensuring optimal performance for different
 * file system types (local vs remote/search).
 * 
 * Key features:
 * - Preserves iteratorAll (batch mode) for local directories - high performance
 * - Preserves iteratorOneByOne (streaming mode) for SMB/search - continuous updates  
 * - Uses pure Qt signal/slot communication for thread safety
 * - Supports cancellation and progress reporting
 * - Maintains original 200ms/500files batching logic
 * 
 * Thread Safety: This class is designed to run in a worker thread and communicates
 * with the main thread only through Qt's signal-slot mechanism using Qt::QueuedConnection.
 */
class TraversalWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new TraversalWorker
     * @param parent Parent object (should be nullptr for worker thread objects)
     */
    explicit TraversalWorker(QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~TraversalWorker() override;

public slots:
    /**
     * @brief Start directory traversal for the given request
     * 
     * This method starts the traversal process and automatically determines
     * whether to use batch mode (iteratorAll) or streaming mode (iteratorOneByOne)
     * based on the directory iterator's characteristics.
     * 
     * @param request Directory request containing URL, sorting, filtering parameters
     */
    void startTraversal(const DirectoryRequest &request);

    /**
     * @brief Cancel ongoing traversal for the specified request
     * @param requestId Request ID to cancel
     */
    void cancelTraversal(const QString &requestId);

    /**
     * @brief Cancel all ongoing traversals
     */
    void cancelAllTraversals();

signals:
    /**
     * @brief Emitted during traversal to report progress
     * 
     * This signal is emitted periodically during streaming mode traversal
     * to provide continuous updates to the UI.
     * 
     * @param requestId Request ID
     * @param items List of file items found so far
     * @param isFirstBatch Whether this is the first batch of results
     */
    void traversalProgress(const QString &requestId, const QList<FileItem> &items, bool isFirstBatch);

    /**
     * @brief Emitted when traversal is completed
     * 
     * @param requestId Request ID
     * @param data Complete directory data
     * @param noDataProduced Whether no data was produced during traversal
     */
    void traversalCompleted(const QString &requestId, const DirectoryData &data, bool noDataProduced);

    /**
     * @brief Emitted when traversal encounters an error
     * 
     * @param requestId Request ID
     * @param errorMessage Error description
     */
    void traversalError(const QString &requestId, const QString &errorMessage);

    /**
     * @brief Emitted when sort operation is requested
     * 
     * @param requestId Request ID
     * @param directoryUrl Directory URL that needs sorting
     */
    void traversalSortRequested(const QString &requestId, const QUrl &directoryUrl);

private slots:
    /**
     * @brief Process the next batch of files in streaming mode
     */
    void processNextBatch();

private:
    /**
     * @brief Context information for an active traversal
     */
    struct TraversalContext {
        DirectoryRequest request;                    ///< Original request
        AbstractDirIteratorPointer iterator;        ///< Directory iterator
        QList<FileItem> collectedItems;            ///< Items collected so far
        QElapsedTimer timer;                        ///< Timer for batch processing
        bool isFirstBatch;                          ///< Whether next batch is first
        bool isCancelled;                           ///< Whether traversal is cancelled
        int itemCount;                              ///< Number of items processed
        
        TraversalContext() : isFirstBatch(true), isCancelled(false), itemCount(0) {}
        
        // Constructor with request
        explicit TraversalContext(const DirectoryRequest &req) 
            : request(req), isFirstBatch(true), isCancelled(false), itemCount(0) {}
    };

    /**
     * @brief Process directory traversal in batch mode (iteratorAll equivalent)
     * 
     * This method is used for local directories where all file information
     * can be retrieved quickly in a single operation.
     * 
     * @param context Traversal context
     */
    void processBatchMode(TraversalContext &context);

    /**
     * @brief Process directory traversal in streaming mode (iteratorOneByOne equivalent)
     * 
     * This method is used for remote directories, search results, or other scenarios
     * where files should be processed incrementally to provide continuous UI updates.
     * 
     * @param context Traversal context
     */
    void processStreamMode(TraversalContext &context);

    /**
     * @brief Convert FileInfoPointer to FileItem
     * @param fileInfo Source file info
     * @return FileItem Converted file item
     */
    FileItem convertToFileItem(const FileInfoPointer &fileInfo) const;

    /**
     * @brief Create directory data from traversal context
     * @param context Traversal context
     * @return DirectoryData Complete directory data
     */
    DirectoryData createDirectoryData(const TraversalContext &context) const;

    /**
     * @brief Check if traversal should be cancelled
     * @param requestId Request ID to check
     * @return bool true if cancelled, false otherwise
     */
    bool shouldCancel(const QString &requestId) const;

    /**
     * @brief Clean up completed or cancelled traversal
     * @param requestId Request ID to clean up
     */
    void cleanupTraversal(const QString &requestId);

private:
    QMap<QString, TraversalContext> m_activeTraversals;  ///< Active traversal contexts
    QTimer *m_batchTimer;                               ///< Timer for batch processing
    mutable QMutex m_mutex;                             ///< Mutex for thread safety
    
    // Constants for batch processing (preserving original behavior)
    static constexpr int kBatchIntervalMs = 200;        ///< Batch interval in milliseconds
    static constexpr int kBatchSizeLimit = 500;         ///< Maximum items per batch
};

} // namespace dfmplugin_workspace

#endif // TRAVERSALWORKER_H 