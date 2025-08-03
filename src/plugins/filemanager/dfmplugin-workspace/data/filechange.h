// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILECHANGE_H
#define FILECHANGE_H

#include "dfmplugin_workspace_global.h"

#include <QUrl>
#include <QDateTime>
#include <QMetaType>

namespace dfmplugin_workspace {

/**
 * @brief Immutable event representing a file system change
 * 
 * This class encapsulates information about a single file system change event.
 * It is designed to be completely immutable after construction for thread safety.
 * 
 * Key features:
 * - Represents all types of file system changes (create, delete, modify, move)
 * - Thread-safe for concurrent access
 * - Lightweight and efficient for batch processing
 * - Contains timestamp for event ordering and deduplication
 * 
 * Thread Safety: This class is immutable and thread-safe for concurrent read access.
 */
class FileChange
{
public:
    /**
     * @brief Types of file system changes
     */
    enum class ChangeType {
        Created,    ///< File or directory was created
        Deleted,    ///< File or directory was deleted
        Modified,   ///< File or directory was modified
        Moved       ///< File or directory was moved/renamed
    };

    /**
     * @brief Construct a FileChange event
     * 
     * @param fileUrl URL of the file that changed
     * @param changeType Type of change that occurred
     * @param timestamp When the change occurred
     * @param oldUrl Original URL for move operations (optional)
     */
    FileChange(const QUrl &fileUrl,
               ChangeType changeType,
               const QDateTime &timestamp,
               const QUrl &oldUrl = QUrl());

    /**
     * @brief Default constructor for Qt meta-type system
     */
    FileChange() = default;

    /**
     * @brief Copy constructor
     */
    FileChange(const FileChange &other) = default;

    /**
     * @brief Move constructor
     */
    FileChange(FileChange &&other) noexcept = default;

    /**
     * @brief Assignment operators
     */
    FileChange &operator=(const FileChange &other) = default;
    FileChange &operator=(FileChange &&other) noexcept = default;

    // Const accessors for all properties
    const QUrl &fileUrl() const { return m_fileUrl; }
    ChangeType changeType() const { return m_changeType; }
    const QDateTime &timestamp() const { return m_timestamp; }
    const QUrl &oldUrl() const { return m_oldUrl; }

    /**
     * @brief Get change type as string for debugging
     * @return QString Human-readable change type
     */
    QString changeTypeString() const;

    /**
     * @brief Check if this change affects the same file as another
     * 
     * This method is useful for deduplication logic, checking if two
     * changes affect the same file (considering move operations).
     * 
     * @param other Other FileChange to compare with
     * @return bool true if changes affect the same file
     */
    bool affectsSameFile(const FileChange &other) const;

    /**
     * @brief Equality comparison operator
     * 
     * @param other Other FileChange to compare with
     * @return bool true if changes are identical
     */
    bool operator==(const FileChange &other) const {
        return m_fileUrl == other.m_fileUrl &&
               m_changeType == other.m_changeType &&
               m_timestamp == other.m_timestamp &&
               m_oldUrl == other.m_oldUrl;
    }

    /**
     * @brief Inequality comparison operator
     * 
     * @param other Other FileChange to compare with
     * @return bool true if changes are different
     */
    bool operator!=(const FileChange &other) const {
        return !(*this == other);
    }

private:
    QUrl m_fileUrl;                                 ///< URL of the file that changed
    ChangeType m_changeType = ChangeType::Modified; ///< Type of change
    QDateTime m_timestamp;                          ///< When the change occurred
    QUrl m_oldUrl;                                  ///< Original URL for move operations
};

} // namespace dfmplugin_workspace

Q_DECLARE_METATYPE(dfmplugin_workspace::FileChange)
Q_DECLARE_METATYPE(dfmplugin_workspace::FileChange::ChangeType)

#endif // FILECHANGE_H 