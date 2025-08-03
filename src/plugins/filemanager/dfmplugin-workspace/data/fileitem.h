// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEITEM_H
#define FILEITEM_H

#include "dfmplugin_workspace_global.h"

#include <dfm-base/dfm_base_global.h>
#include <dfm-base/interfaces/fileinfo.h>
#include <dfm-base/interfaces/sortfileinfo.h>

#include <QUrl>
#include <QDateTime>
#include <QMetaType>

DFMBASE_USE_NAMESPACE

namespace dfmplugin_workspace {

/**
 * @brief Immutable file item data structure for thread-safe file information
 * 
 * This class represents a single file or directory with all its essential properties.
 * It is designed to be completely immutable after construction, making it safe to
 * pass between threads without synchronization.
 * 
 * Key features:
 * - All members are const and set during construction
 * - Thread-safe for read operations across multiple threads
 * - Can be converted to SortInfoPointer for compatibility with existing code
 * - Supports Qt's meta-object system for signal/slot usage
 * 
 * Thread Safety: This class is immutable and thread-safe for concurrent read access.
 */
class FileItem
{
public:
    /**
     * @brief Construct a FileItem from FileInfoPointer
     * 
     * @param fileInfo Source file information
     */
    explicit FileItem(const FileInfoPointer &fileInfo);

    /**
     * @brief Construct a FileItem with explicit parameters
     * 
     * @param url File URL
     * @param size File size in bytes
     * @param lastModified Last modification time
     * @param lastRead Last read time
     * @param createTime Creation time
     * @param isDirectory Whether this is a directory
     * @param isHidden Whether this file is hidden
     * @param isSymlink Whether this is a symbolic link
     * @param isReadable Whether this file is readable
     * @param isWriteable Whether this file is writeable
     * @param isExecutable Whether this file is executable
     */
    FileItem(const QUrl &url,
             qint64 size,
             const QDateTime &lastModified,
             const QDateTime &lastRead,
             const QDateTime &createTime,
             bool isDirectory,
             bool isHidden,
             bool isSymlink,
             bool isReadable,
             bool isWriteable,
             bool isExecutable);

    /**
     * @brief Default constructor for Qt meta-type system
     */
    FileItem() = default;

    /**
     * @brief Copy constructor
     */
    FileItem(const FileItem &other) = default;

    /**
     * @brief Move constructor
     */
    FileItem(FileItem &&other) noexcept = default;

    /**
     * @brief Assignment operators
     */
    FileItem &operator=(const FileItem &other) = default;
    FileItem &operator=(FileItem &&other) noexcept = default;

    // Const accessors for all properties
    const QUrl &url() const { return m_url; }
    qint64 size() const { return m_size; }
    const QDateTime &lastModified() const { return m_lastModified; }
    const QDateTime &lastRead() const { return m_lastRead; }
    const QDateTime &createTime() const { return m_createTime; }
    bool isDirectory() const { return m_isDirectory; }
    bool isHidden() const { return m_isHidden; }
    bool isSymlink() const { return m_isSymlink; }
    bool isReadable() const { return m_isReadable; }
    bool isWriteable() const { return m_isWriteable; }
    bool isExecutable() const { return m_isExecutable; }

    /**
     * @brief Get file name from URL
     * @return QString File name
     */
    QString fileName() const;

    /**
     * @brief Get file suffix/extension
     * @return QString File suffix
     */
    QString suffix() const;

    /**
     * @brief Create a SortInfoPointer for compatibility with existing code
     * 
     * This method creates a SortInfo object from this FileItem's data,
     * allowing integration with existing FileSortWorker logic.
     * 
     * @return SortInfoPointer Compatible sort info object
     */
    SortInfoPointer createSortInfo() const;

    /**
     * @brief Equality comparison operator
     * 
     * @param other Other FileItem to compare with
     * @return bool true if items represent the same file
     */
    bool operator==(const FileItem &other) const {
        return m_url == other.m_url;
    }

    /**
     * @brief Inequality comparison operator
     * 
     * @param other Other FileItem to compare with
     * @return bool true if items represent different files
     */
    bool operator!=(const FileItem &other) const {
        return !(*this == other);
    }

private:
    QUrl m_url;                          ///< File URL
    qint64 m_size = 0;                   ///< File size in bytes
    QDateTime m_lastModified;            ///< Last modification time
    QDateTime m_lastRead;                ///< Last read time
    QDateTime m_createTime;              ///< Creation time
    bool m_isDirectory = false;          ///< Whether this is a directory
    bool m_isHidden = false;             ///< Whether this file is hidden
    bool m_isSymlink = false;            ///< Whether this is a symbolic link
    bool m_isReadable = false;           ///< Whether this file is readable
    bool m_isWriteable = false;          ///< Whether this file is writeable
    bool m_isExecutable = false;         ///< Whether this file is executable
};

} // namespace dfmplugin_workspace

Q_DECLARE_METATYPE(dfmplugin_workspace::FileItem)

#endif // FILEITEM_H 