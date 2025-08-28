#ifndef FILEITEM_H
#define FILEITEM_H

#include "dfmplugin_filegroup_global.h"

#include <QString>
#include <QDateTime>
#include <QIcon>
#include <QMetaType>

DPFILEGROUP_BEGIN_NAMESPACE

/**
 * @brief Core data structure representing a file or directory item
 *
 * This structure contains all necessary information about a file or directory
 * including metadata for grouping and sorting operations.
 */
struct FileItem
{
    QString name;   ///< File or directory name
    QString path;   ///< Full absolute path
    QString type;   ///< File type (extension or "Directory")
    qint64 size;   ///< File size in bytes (-1 for directories)
    QDateTime dateModified;   ///< Last modification time
    QDateTime dateCreated;   ///< Creation time
    bool isDirectory;   ///< True if this item is a directory
    QIcon icon;   ///< System icon for the file type

    // Grouping metadata
    QString groupName;   ///< Name of the group this item belongs to
    int groupIndex;   ///< Index within the group for sorting

    /**
     * @brief Default constructor
     */
    FileItem()
        : size(-1), isDirectory(false), groupIndex(-1) { }

    /**
     * @brief Constructor with basic file information
     */
    FileItem(const QString &filePath, const QString &fileName, bool isDir)
        : name(fileName), path(filePath), isDirectory(isDir), size(isDir ? -1 : 0), groupIndex(-1) { }

    /**
     * @brief Equality operator for comparison
     */
    bool operator==(const FileItem &other) const
    {
        return path == other.path;
    }

    /**
     * @brief Get formatted file size string
     */
    QString getFormattedSize() const;

    /**
     * @brief Get formatted modification time string
     */
    QString getFormattedModifiedTime() const;

    /**
     * @brief Get file type display string
     */
    QString getTypeString() const;
};

DPFILEGROUP_END_NAMESPACE

// Register with Qt's meta-object system for use in QVariant
Q_DECLARE_METATYPE(DPFILEGROUP_NAMESPACE::FileItem)

#endif   // FILEITEM_H
