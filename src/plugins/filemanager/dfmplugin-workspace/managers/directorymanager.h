// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DIRECTORYMANAGER_H
#define DIRECTORYMANAGER_H

#include "dfmplugin_workspace_global.h"
#include "data/directoryrequest.h"
#include "data/directorydata.h"
#include "data/filechange.h"
#include "data/configstructs.h"
#include "data/fileitem.h"

#include <QObject>
#include <QUrl>
#include <QThread>
#include <QMutex>
#include <QMap>

namespace dfmplugin_workspace {

class DirectoryDataManager;
class FileWatchManager;
class TraversalWorker;

/**
 * @brief Main coordinator class for directory operations (replaces RootInfo)
 * 
 * This class serves as the unified entry point for all directory operations,
 * coordinating between specialized components while maintaining a simple interface.
 * 
 * Key design principles:
 * - Single Responsibility: Coordinates between specialized components
 * - Simplified Interface: From 25 methods in RootInfo to 7 main methods
 * - Thread Safety: Uses Qt's signal-slot mechanism for inter-thread communication
 * - Immutable Data: All cross-thread data is immutable
 */
class DirectoryManager : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryManager(QObject *parent = nullptr);
    DirectoryManager(const QUrl& rootUrl, bool enableCache, QObject *parent = nullptr);
    ~DirectoryManager() override;

    // Main interface (as designed in refactor_rootinfo2.md)
    
    /**
     * @brief Request directory data with specified sorting and filtering
     * @param directoryUrl Directory URL to traverse
     * @param sortConfig Sorting configuration
     * @param filterConfig Filtering configuration  
     * @param useCache Whether to use cached data
     * @return QString Unique request ID for tracking this request
     */
    QString requestDirectoryData(const QUrl& directoryUrl,
                                const SortConfig& sortConfig = {},
                                const FilterConfig& filterConfig = {},
                                bool useCache = true);

    /**
     * @brief Cancel an ongoing directory request
     * @param requestId Request ID to cancel
     */
    void cancelRequest(const QString& requestId);

    /**
     * @brief Refresh directory data (clear cache and re-traverse)
     * @param directoryUrl Directory URL to refresh
     */
    void refreshDirectory(const QUrl& directoryUrl);

    /**
     * @brief Enable or disable file watching for a directory
     * @param directoryUrl Directory URL to watch
     * @param enabled Whether to enable watching
     */
    void enableWatching(const QUrl& directoryUrl, bool enabled);

    // Data access methods
    
    /**
     * @brief Get cached file items for a directory
     * @param directoryUrl Directory URL
     * @return QList<FileItem> List of file items, empty if not cached
     */
    QList<FileItem> getFileItems(const QUrl& directoryUrl) const;

    /**
     * @brief Get search keywords for a directory (if it's a search URL)
     * @param directoryUrl Directory URL (may be search URL)
     * @return QStringList List of search keywords
     */
    QStringList getSearchKeywords(const QUrl& directoryUrl) const;

signals:
    // Simplified external signals (as designed in refactor_rootinfo2.md)
    
    /**
     * @brief Emitted when directory data is ready
     * @param requestId Request ID
     * @param data Complete directory data
     */
    void directoryDataReady(const QString& requestId, const DirectoryData& data);

    /**
     * @brief Emitted when directory data is updated due to file system changes
     * @param directoryUrl Directory URL that was updated
     * @param changes List of file changes
     */
    void directoryDataUpdated(const QUrl& directoryUrl, const QList<FileChange>& changes);

    /**
     * @brief Emitted when a request encounters an error
     * @param requestId Request ID
     * @param errorMessage Error description
     */
    void requestError(const QString& requestId, const QString& errorMessage);

private slots:
    void onDataManagerDataReady(const QString& requestId, const DirectoryData& data);
    void onWatchManagerChangesDetected(const QUrl& directoryUrl, const QList<FileChange>& changes);
    
    // TraversalWorker signal handlers
    void onTraversalProgress(const QString& requestId, const QList<FileItem>& items, bool isFirstBatch);
    void onTraversalCompleted(const QString& requestId, const DirectoryData& data, bool noDataProduced);
    void onTraversalError(const QString& requestId, const QString& errorMessage);

signals:
    // Internal signals to TraversalWorker (Qt::QueuedConnection)
    void requestTraversal(const DirectoryRequest& request);
    void requestTraversalCancellation(const QString& requestId);

private:
    void setupConnections();
    void initializeTraversalWorker();
    void cleanupTraversalWorker();
    QString generateRequestId() const;

private:
    QUrl m_rootUrl;
    bool m_cacheEnabled;
    
    DirectoryDataManager* m_dataManager;
    FileWatchManager* m_watchManager;
    
    // TraversalWorker thread management
    QThread* m_traversalThread;
    TraversalWorker* m_traversalWorker;
    
    QMap<QUrl, DirectoryData> m_cachedData;
    QMap<QString, QUrl> m_activeRequests;
    
    static int s_requestCounter;
};

} // namespace dfmplugin_workspace

#endif // DIRECTORYMANAGER_H 