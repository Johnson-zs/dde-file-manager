// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DIRECTORYDATAMANAGER_H
#define DIRECTORYDATAMANAGER_H

#include "dfmplugin_workspace_global.h"
#include "data/directorydata.h"
#include "data/directoryrequest.h"

#include <QObject>
#include <QUrl>
#include <QMap>
#include <QMutex>
#include <QDateTime>

namespace dfmplugin_workspace {

/**
 * @brief Simple manager class for directory data caching
 * 
 * This class provides basic caching of directory data with simple TTL-based expiration.
 * It is designed to be lightweight and focused only on caching functionality.
 */
class DirectoryDataManager : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryDataManager(QObject *parent = nullptr);
    ~DirectoryDataManager() override;

    /**
     * @brief Get cached directory data if available and valid
     * @param request Directory request to check
     * @return DirectoryData Cached data, or invalid data if not found/expired
     */
    DirectoryData getCachedData(const DirectoryRequest &request) const;

    /**
     * @brief Store directory data in cache
     * @param data Directory data to cache
     */
    void setCachedData(const DirectoryData &data);

    /**
     * @brief Check if valid cached data exists for the request
     * @param request Directory request to check
     * @return bool true if valid cached data exists
     */
    bool hasCachedData(const DirectoryRequest &request) const;

    /**
     * @brief Invalidate cached data for a specific directory
     * @param directoryUrl Directory URL to invalidate
     */
    void invalidateCache(const QUrl &directoryUrl);

signals:
    /**
     * @brief Emitted when directory data is ready
     * @param requestId Request ID
     * @param data Directory data
     */
    void dataReady(const QString &requestId, const DirectoryData &data);

    /**
     * @brief Emitted when data request encounters an error
     * @param requestId Request ID
     * @param errorMessage Error description
     */
    void dataError(const QString &requestId, const QString &errorMessage);

private:
    /**
     * @brief Simple cache entry
     */
    struct CacheEntry {
        DirectoryData data;
        QDateTime createdTime;
        
        CacheEntry() = default;
        explicit CacheEntry(const DirectoryData &d) 
            : data(d), createdTime(QDateTime::currentDateTime()) {}
            
        bool isExpired(int ttlMs) const {
            return createdTime.msecsTo(QDateTime::currentDateTime()) > ttlMs;
        }
    };

    QString generateCacheKey(const DirectoryRequest &request) const;
    QString generateCacheKey(const QUrl &directoryUrl, 
                            const SortConfig *sortConfig = nullptr,
                            const FilterConfig *filterConfig = nullptr) const;

private:
    QMap<QString, CacheEntry> m_cache;
    mutable QMutex m_mutex;
    
    static constexpr int kDefaultCacheTTLMs = 30000; // 30 seconds
};

} // namespace dfmplugin_workspace

#endif // DIRECTORYDATAMANAGER_H 