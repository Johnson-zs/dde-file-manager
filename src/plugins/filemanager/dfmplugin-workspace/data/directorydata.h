// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DIRECTORYDATA_H
#define DIRECTORYDATA_H

#include "dfmplugin_workspace_global.h"
#include "configstructs.h"
#include "fileitem.h"

#include <dfm-base/interfaces/sortfileinfo.h>

#include <QUrl>
#include <QList>
#include <QDateTime>
#include <QMetaType>

namespace dfmplugin_workspace {

/**
 * @brief Immutable container for complete directory data
 * 
 * This class encapsulates all information about a directory including its files,
 * sorting configuration, filter configuration, and metadata. It is designed to be
 * completely immutable after construction for thread safety.
 * 
 * Key features:
 * - Contains complete directory information in a single object
 * - Thread-safe for concurrent read access
 * - Can be efficiently passed between threads
 * - Supports conversion to legacy data structures
 * 
 * Thread Safety: This class is immutable and thread-safe for concurrent read access.
 */
class DirectoryData
{
public:
    /**
     * @brief Construct DirectoryData with all parameters
     * 
     * @param directoryUrl URL of the directory
     * @param files List of files in the directory
     * @param sortConfig Sorting configuration used
     * @param filterConfig Filter configuration used
     * @param requestId Request ID that generated this data
     * @param isComplete Whether traversal is complete
     */
    DirectoryData(const QUrl &directoryUrl,
                  const QList<FileItem> &files,
                  const SortConfig &sortConfig,
                  const FilterConfig &filterConfig,
                  const QString &requestId = QString(),
                  bool isComplete = true);

    /**
     * @brief Default constructor for Qt meta-type system
     */
    DirectoryData();

    /**
     * @brief Copy constructor
     */
    DirectoryData(const DirectoryData &other) = default;

    /**
     * @brief Move constructor
     */
    DirectoryData(DirectoryData &&other) noexcept = default;

    /**
     * @brief Assignment operators
     */
    DirectoryData &operator=(const DirectoryData &other) = default;
    DirectoryData &operator=(DirectoryData &&other) noexcept = default;

    // Const accessors for all properties
    const QUrl &directoryUrl() const { return m_directoryUrl; }
    const QList<FileItem> &files() const { return m_files; }
    const SortConfig &sortConfig() const { return m_sortConfig; }
    const FilterConfig &filterConfig() const { return m_filterConfig; }
    const QString &requestId() const { return m_requestId; }
    const QDateTime &timestamp() const { return m_timestamp; }
    bool isComplete() const { return m_isComplete; }

    /**
     * @brief Get number of files in this directory
     * @return int File count
     */
    int fileCount() const { return m_files.size(); }

    /**
     * @brief Check if directory is empty
     * @return bool true if no files
     */
    bool isEmpty() const { return m_files.isEmpty(); }

    /**
     * @brief Get file at specific index
     * 
     * @param index File index
     * @return const FileItem& File at index
     * @throws std::out_of_range if index is invalid
     */
    const FileItem &fileAt(int index) const;

    /**
     * @brief Find file by URL
     * 
     * @param url File URL to find
     * @return const FileItem* Pointer to file, or nullptr if not found
     */
    const FileItem *findFile(const QUrl &url) const;

    /**
     * @brief Create list of SortInfoPointer for compatibility
     * 
     * This method creates a list of SortInfo objects from the FileItem list,
     * allowing integration with existing FileSortWorker logic.
     * 
     * @return QList<SortInfoPointer> Compatible sort info list
     */
    QList<SortInfoPointer> createSortInfoList() const;

    /**
     * @brief Equality comparison operator
     * 
     * @param other Other DirectoryData to compare with
     * @return bool true if data represents the same directory state
     */
    bool operator==(const DirectoryData &other) const {
        return m_directoryUrl == other.m_directoryUrl &&
               m_requestId == other.m_requestId;
    }

    /**
     * @brief Inequality comparison operator
     * 
     * @param other Other DirectoryData to compare with
     * @return bool true if data represents different directory states
     */
    bool operator!=(const DirectoryData &other) const {
        return !(*this == other);
    }

private:
    QUrl m_directoryUrl;                    ///< Directory URL
    QList<FileItem> m_files;                ///< List of files in directory
    SortConfig m_sortConfig;                ///< Sort configuration used
    FilterConfig m_filterConfig;            ///< Filter configuration used
    QString m_requestId;                    ///< Request ID that generated this data
    QDateTime m_timestamp;                  ///< Creation timestamp
    bool m_isComplete = true;               ///< Whether traversal is complete
};

} // namespace dfmplugin_workspace

Q_DECLARE_METATYPE(dfmplugin_workspace::DirectoryData)

#endif // DIRECTORYDATA_H 
