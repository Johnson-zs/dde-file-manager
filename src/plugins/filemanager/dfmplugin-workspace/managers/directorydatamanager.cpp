// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directorydatamanager.h"

#include <dfm-base/dfm_log_defines.h>

#include <QMutexLocker>
#include <QCryptographicHash>

using namespace dfmplugin_workspace;

DirectoryDataManager::DirectoryDataManager(QObject *parent)
    : QObject(parent)
{
    fmDebug() << "DirectoryDataManager created";
}

DirectoryDataManager::~DirectoryDataManager()
{
    fmDebug() << "DirectoryDataManager destroyed, cache entries:" << m_cache.size();
}

DirectoryData DirectoryDataManager::getCachedData(const DirectoryRequest &request) const
{
    QMutexLocker locker(&m_mutex);
    
    QString key = generateCacheKey(request);
    auto it = m_cache.find(key);
    
    if (it == m_cache.end()) {
        fmDebug() << "Cache miss for request:" << request.requestId();
        return DirectoryData(); // Invalid data
    }
    
    // Check if entry is expired
    if (it->isExpired(kDefaultCacheTTLMs)) {
        fmDebug() << "Cache entry expired for request:" << request.requestId();
        // Remove expired entry
        const_cast<DirectoryDataManager*>(this)->m_cache.remove(key);
        return DirectoryData(); // Invalid data
    }
    
    fmDebug() << "Cache hit for request:" << request.requestId() 
             << "Files:" << it->data.fileCount();
    
    return it->data;
}

void DirectoryDataManager::setCachedData(const DirectoryData &data)
{
    QMutexLocker locker(&m_mutex);
    
    QString key = generateCacheKey(DirectoryRequest(
        data.directoryUrl(),
        data.requestId(),
        data.sortConfig(),
        data.filterConfig()
    ));
    
    // Create cache entry
    CacheEntry entry(data);
    
    fmDebug() << "Caching data for directory:" << data.directoryUrl().toString()
             << "Files:" << data.fileCount();
    
    // Insert or update entry
    m_cache[key] = entry;
}

bool DirectoryDataManager::hasCachedData(const DirectoryRequest &request) const
{
    QMutexLocker locker(&m_mutex);
    
    QString key = generateCacheKey(request);
    auto it = m_cache.find(key);
    
    if (it == m_cache.end()) {
        return false;
    }
    
    // Check if expired
    if (it->isExpired(kDefaultCacheTTLMs)) {
        // Remove expired entry
        const_cast<DirectoryDataManager*>(this)->m_cache.remove(key);
        return false;
    }
    
    return true;
}

void DirectoryDataManager::invalidateCache(const QUrl &directoryUrl)
{
    QMutexLocker locker(&m_mutex);
    
    fmDebug() << "Invalidating cache for directory:" << directoryUrl.toString();
    
    // Find all cache entries for this directory
    QStringList keysToRemove;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->data.directoryUrl() == directoryUrl) {
            keysToRemove.append(it.key());
        }
    }
    
    // Remove found entries
    for (const QString &key : keysToRemove) {
        m_cache.remove(key);
    }
    
    if (!keysToRemove.isEmpty()) {
        fmDebug() << "Removed" << keysToRemove.size() << "cache entries for directory:" 
                 << directoryUrl.toString();
    }
}

QString DirectoryDataManager::generateCacheKey(const DirectoryRequest &request) const
{
    return generateCacheKey(request.directoryUrl(), &request.sortConfig(), &request.filterConfig());
}

QString DirectoryDataManager::generateCacheKey(const QUrl &directoryUrl, 
                                              const SortConfig *sortConfig,
                                              const FilterConfig *filterConfig) const
{
    QString baseKey = directoryUrl.toString();
    
    // Add sort configuration to key
    if (sortConfig) {
        baseKey += QString("_sort_%1_%2_%3_%4")
                   .arg(static_cast<int>(sortConfig->role))
                   .arg(static_cast<int>(sortConfig->order))
                   .arg(sortConfig->sortCaseSensitive ? 1 : 0)
                   .arg(sortConfig->isMixDirAndFile ? 1 : 0);
    }
    
    // Add filter configuration to key
    if (filterConfig) {
        baseKey += QString("_filter_%1_%2_%3")
                   .arg(filterConfig->nameFilters.join(","))
                   .arg(static_cast<int>(filterConfig->dirFilters))
                   .arg(filterConfig->showHidden ? 1 : 0);
    }
    
    // Generate hash for consistent key length
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(baseKey.toUtf8());
    return QString(hash.result().toHex());
} 