// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEWATCHMANAGER_H
#define FILEWATCHMANAGER_H

#include "dfmplugin_workspace_global.h"
#include "data/filechange.h"

#include <dfm-base/dfm_base_global.h>
#include <dfm-base/interfaces/abstractfilewatcher.h>

#include <QObject>
#include <QUrl>
#include <QMap>
#include <QThread>
#include <QMutex>

DFMBASE_USE_NAMESPACE

namespace dfmplugin_workspace {

class FileWatchWorker;

/**
 * @brief Simple manager class for file system monitoring
 * 
 * This class manages file system monitoring by coordinating between the main thread
 * (where AbstractFileWatcher must be created) and a worker thread for event processing.
 */
class FileWatchManager : public QObject
{
    Q_OBJECT

public:
    explicit FileWatchManager(QObject *parent = nullptr);
    ~FileWatchManager() override;

    /**
     * @brief Start watching a directory
     * @param directoryUrl Directory URL to watch
     * @param watcherId Unique identifier for this watcher
     * @return bool true if watcher was successfully created
     */
    bool startWatching(const QUrl &directoryUrl, const QString &watcherId);

    /**
     * @brief Stop watching a directory
     * @param watcherId Watcher ID to stop
     */
    void stopWatching(const QString &watcherId);

signals:
    /**
     * @brief Emitted when batched file changes are received from worker
     * @param directoryUrl Directory URL that produced the changes  
     * @param changes List of file changes
     */
    void fileChangesBatched(const QUrl &directoryUrl, const QList<FileChange> &changes);

    /**
     * @brief Emitted when a watch operation encounters an error
     * @param watcherId Watcher ID
     * @param errorMessage Error description
     */
    void watchError(const QString &watcherId, const QString &errorMessage);

private slots:
    void onFileCreated(const QUrl &url);
    void onFileDeleted(const QUrl &url);
    void onFileModified(const QUrl &url);
    void onFileMoved(const QUrl &fromUrl, const QUrl &toUrl);
    void onWorkerFileChangesBatched(const QString &watcherId, const QList<FileChange> &changes);
    void onWorkerWatchError(const QString &watcherId, const QString &errorMessage);

private:
    struct WatcherInfo {
        QUrl directoryUrl;
        QString watcherId;
        AbstractFileWatcherPointer watcher;
        
        WatcherInfo() = default;
        WatcherInfo(const QUrl &url, const QString &id, AbstractFileWatcherPointer w)
            : directoryUrl(url), watcherId(id), watcher(w) {}
    };

    AbstractFileWatcherPointer createWatcher(const QUrl &directoryUrl);
    void connectWatcherSignals(AbstractFileWatcherPointer watcher);
    void initializeWorkerThread();
    void cleanupWorkerThread();
    
    /**
     * @brief Find the watcher ID that monitors the given URL
     * @param url File URL to find watcher for
     * @return QString Watcher ID, or empty string if not found
     */
    QString findWatcherIdForUrl(const QUrl &url) const;
    
    /**
     * @brief Check if a URL is within the watched directory
     * @param watchedDir Directory being watched
     * @param fileUrl File URL to check
     * @return bool true if file is within watched directory
     */
    bool isUrlWithinDirectory(const QUrl &watchedDir, const QUrl &fileUrl) const;
    
    /**
     * @brief Get directory URL for a given watcher ID
     * @param watcherId Watcher ID to look up
     * @return QUrl Directory URL, or empty URL if watcher not found
     */
    QUrl getDirectoryUrlForWatcher(const QString &watcherId) const;

private:
    QMap<QString, WatcherInfo> m_watchers;
    mutable QMutex m_mutex;
    
    // Worker thread management
    QThread *m_workerThread;
    FileWatchWorker *m_worker;
};

} // namespace dfmplugin_workspace

#endif // FILEWATCHMANAGER_H 