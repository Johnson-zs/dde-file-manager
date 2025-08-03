// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DIRECTORYREQUEST_H
#define DIRECTORYREQUEST_H

#include "dfmplugin_workspace_global.h"
#include "configstructs.h"

#include <QUrl>
#include <QString>
#include <QDateTime>
#include <QMetaType>

namespace dfmplugin_workspace {

/**
 * @brief Immutable request object for directory data operations
 * 
 * This class encapsulates all parameters needed to request directory data.
 * It is designed to be completely immutable after construction for thread safety.
 * 
 * Key features:
 * - Contains all parameters for directory traversal requests
 * - Thread-safe for concurrent access
 * - Includes sorting and filtering configurations
 * - Supports caching options and request tracking
 * 
 * Thread Safety: This class is immutable and thread-safe for concurrent read access.
 */
class DirectoryRequest
{
public:
    /**
     * @brief Construct a DirectoryRequest with all parameters
     * 
     * @param directoryUrl URL of the directory to request
     * @param requestId Unique identifier for this request
     * @param sortConfig Sorting configuration to apply
     * @param filterConfig Filter configuration to apply
     * @param useCache Whether to use cached data if available
     * @param refreshCache Whether to refresh cache even if valid data exists
     */
    DirectoryRequest(const QUrl &directoryUrl,
                     const QString &requestId,
                     const SortConfig &sortConfig,
                     const FilterConfig &filterConfig,
                     bool useCache = true,
                     bool refreshCache = false);

    /**
     * @brief Default constructor for Qt meta-type system
     */
    DirectoryRequest();

    /**
     * @brief Copy constructor
     */
    DirectoryRequest(const DirectoryRequest &other) = default;

    /**
     * @brief Move constructor
     */
    DirectoryRequest(DirectoryRequest &&other) noexcept = default;

    /**
     * @brief Assignment operators
     */
    DirectoryRequest &operator=(const DirectoryRequest &other) = default;
    DirectoryRequest &operator=(DirectoryRequest &&other) noexcept = default;

    // Const accessors for all properties
    const QUrl &directoryUrl() const { return m_directoryUrl; }
    const QString &requestId() const { return m_requestId; }
    const SortConfig &sortConfig() const { return m_sortConfig; }
    const FilterConfig &filterConfig() const { return m_filterConfig; }
    const QDateTime &timestamp() const { return m_timestamp; }
    bool useCache() const { return m_useCache; }
    bool refreshCache() const { return m_refreshCache; }

    /**
     * @brief Check if request is valid
     * @return bool true if request has valid directory URL and request ID
     */
    bool isValid() const {
        return m_directoryUrl.isValid() && !m_requestId.isEmpty();
    }

    /**
     * @brief Equality comparison operator
     * 
     * @param other Other DirectoryRequest to compare with
     * @return bool true if requests are identical
     */
    bool operator==(const DirectoryRequest &other) const {
        return m_directoryUrl == other.m_directoryUrl &&
               m_requestId == other.m_requestId &&
               m_sortConfig == other.m_sortConfig &&
               m_filterConfig == other.m_filterConfig &&
               m_useCache == other.m_useCache &&
               m_refreshCache == other.m_refreshCache;
    }

    /**
     * @brief Inequality comparison operator
     * 
     * @param other Other DirectoryRequest to compare with
     * @return bool true if requests are different
     */
    bool operator!=(const DirectoryRequest &other) const {
        return !(*this == other);
    }

private:
    QUrl m_directoryUrl;                    ///< Directory URL to request
    QString m_requestId;                    ///< Unique request identifier
    SortConfig m_sortConfig;                ///< Sorting configuration
    FilterConfig m_filterConfig;            ///< Filter configuration
    QDateTime m_timestamp;                  ///< Request creation timestamp
    bool m_useCache = true;                 ///< Whether to use cached data
    bool m_refreshCache = false;            ///< Whether to refresh cache
};

} // namespace dfmplugin_workspace

Q_DECLARE_METATYPE(dfmplugin_workspace::DirectoryRequest)

#endif // DIRECTORYREQUEST_H 
