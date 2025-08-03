// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEWATCHWORKER_H
#define FILEWATCHWORKER_H

#include "dfmplugin_workspace_global.h"
#include "data/filechange.h"

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QMap>
#include <QList>
#include <QElapsedTimer>

namespace dfmplugin_workspace {

/**
 * @brief Worker class for file system monitoring in dedicated thread
 * 
 * This class handles file system monitoring operations in a separate worker thread,
 * preserving the original batching and deduplication logic from RootInfo.
 * 
 * Key features:
 * - Batches file system events (200ms intervals) to reduce UI update frequency
 * - Deduplicates events for the same file within a batch
 * - Handles file system events in worker thread to avoid blocking main thread
 * - Thread-safe communication using Qt signals with Qt::QueuedConnection
 * 
 * Thread Safety: This class is designed to run in a worker thread. All operations
 * happen within the same thread, so no mutex is needed for internal data structures.
 * Communication with the main thread happens only through Qt's signal-slot mechanism.
 */
class FileWatchWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new FileWatchWorker
     * @param parent Parent object (should be nullptr for worker thread objects)
     */
    explicit FileWatchWorker(QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~FileWatchWorker() override;

public slots:
    /**
     * @brief Start watching a directory
     * 
     * This registers the watcher context in the worker thread. The actual
     * AbstractFileWatcher is created and managed by FileWatchManager in the main thread.
     * 
     * @param directoryUrl Directory URL to watch
     * @param watcherId Unique identifier for this watcher
     */
    void startWatching(const QUrl &directoryUrl, const QString &watcherId);

    /**
     * @brief Stop watching a directory
     * @param watcherId Watcher ID to stop
     */
    void stopWatching(const QString &watcherId);

    /**
     * @brief Stop all active watchers
     */
    void stopAllWatching();

    /**
     * @brief Process file created event (connected from AbstractFileWatcher)
     * @param url File URL that was created
     */
    void onFileCreated(const QUrl &url);

    /**
     * @brief Process file deleted event (connected from AbstractFileWatcher)
     * @param url File URL that was deleted
     */
    void onFileDeleted(const QUrl &url);

    /**
     * @brief Process file modified event (connected from AbstractFileWatcher)
     * @param url File URL that was modified
     */
    void onFileModified(const QUrl &url);

    /**
     * @brief Process file moved event (connected from AbstractFileWatcher)
     * @param fromUrl Original file URL
     * @param toUrl New file URL
     */
    void onFileMoved(const QUrl &fromUrl, const QUrl &toUrl);

    /**
     * @brief Process generic file changed event (connected from AbstractFileWatcher)
     * @param url File URL that changed
     */
    void onFileChanged(const QUrl &url);

signals:
    /**
     * @brief Emitted when batched file changes are ready
     * 
     * This signal is emitted periodically (every 200ms) with a batch of
     * deduplicated file changes.
     * 
     * @param watcherId Watcher ID that produced the changes
     * @param changes List of file changes
     */
    void fileChangesBatched(const QString &watcherId, const QList<FileChange> &changes);

    /**
     * @brief Emitted when a watch operation encounters an error
     * @param watcherId Watcher ID
     * @param errorMessage Error description
     */
    void watchError(const QString &watcherId, const QString &errorMessage);

private slots:
    /**
     * @brief Process accumulated file changes in batches
     */
    void processBatchedChanges();

private:
    /**
     * @brief Context information for an active watcher
     */
    struct WatchContext {
        QUrl directoryUrl;                      ///< Directory being watched
        QString watcherId;                      ///< Unique watcher identifier
        QList<FileChange> pendingChanges;       ///< Changes waiting to be batched
        QElapsedTimer lastBatchTime;            ///< Time since last batch was sent
        bool isActive;                          ///< Whether watcher is active
        
        WatchContext() : isActive(false) {}
    };

    /**
     * @brief Add a file change to the pending batch
     * 
     * This method handles deduplication of events for the same file.
     * 
     * @param watcherId Watcher ID
     * @param change File change to add
     */
    void addPendingChange(const QString &watcherId, const FileChange &change);

    /**
     * @brief Deduplicate changes in the pending list
     * 
     * This method removes redundant changes for the same file, keeping only
     * the most recent and relevant changes.
     * 
     * @param changes List of changes to deduplicate
     * @return QList<FileChange> Deduplicated changes
     */
    QList<FileChange> deduplicateChanges(const QList<FileChange> &changes) const;

    /**
     * @brief Find the watcher ID that monitors the given URL
     * @param url File URL to find watcher for
     * @return QString Watcher ID, or empty string if not found
     */
    QString findWatcherForUrl(const QUrl &url) const;

    /**
     * @brief Check if a URL is within the watched directory
     * @param watchedDir Directory being watched
     * @param fileUrl File URL to check
     * @return bool true if file is within watched directory
     */
    bool isUrlWithinDirectory(const QUrl &watchedDir, const QUrl &fileUrl) const;

private:
    QMap<QString, WatchContext> m_watchContexts;    ///< Active watch contexts
    QTimer *m_batchTimer;                           ///< Timer for batching changes
    
    // Constants for batching (preserving original behavior)
    static constexpr int kBatchIntervalMs = 200;    ///< Batch interval in milliseconds
    static constexpr int kMaxPendingChanges = 1000; ///< Maximum pending changes per watcher
};

} // namespace dfmplugin_workspace

#endif // FILEWATCHWORKER_H 