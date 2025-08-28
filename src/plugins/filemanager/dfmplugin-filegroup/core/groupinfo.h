#ifndef GROUPINFO_H
#define GROUPINFO_H

#include "dfmplugin_filegroup_global.h"
#include "core/fileitem.h"

#include <QString>
#include <QList>
#include <algorithm>

DPFILEGROUP_BEGIN_NAMESPACE

/**
 * @brief Data structure representing a group of files
 *
 * This structure manages a collection of files that belong to the same group
 * based on the current grouping strategy (type, time, name, size, etc.).
 */
struct GroupInfo
{
    QString name;   ///< Display name of the group
    int fileCount;   ///< Number of files in this group
    bool isExpanded;   ///< Whether the group is currently expanded
    QList<FileItem> items;   ///< List of files in this group
    int displayOrder;   ///< Order for displaying groups

    /**
     * @brief Default constructor
     */
    GroupInfo()
        : fileCount(0), isExpanded(true), displayOrder(0) { }

    /**
     * @brief Constructor with group name
     */
    explicit GroupInfo(const QString &groupName, int order = 0)
        : name(groupName), fileCount(0), isExpanded(true), displayOrder(order) { }

    /**
     * @brief Add a file item to this group
     */
    void addItem(const FileItem &item)
    {
        items.append(item);
        fileCount = items.size();
    }

    /**
     * @brief Remove a file item from this group
     */
    bool removeItem(const QString &filePath)
    {
        for (int i = 0; i < items.size(); ++i) {
            if (items[i].path == filePath) {
                items.removeAt(i);
                fileCount = items.size();
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Clear all items in this group
     */
    void clear()
    {
        items.clear();
        fileCount = 0;
    }

    /**
     * @brief Check if this group is empty
     */
    bool isEmpty() const
    {
        return items.isEmpty();
    }

    /**
     * @brief Get formatted group header text
     */
    QString getHeaderText() const
    {
        return QString("%1 (%2)").arg(name).arg(fileCount);
    }

    /**
     * @brief Sort items within this group using a comparison function
     */
    template<typename Compare>
    void sortItems(Compare comp)
    {
        std::sort(items.begin(), items.end(), comp);
    }
};

DPFILEGROUP_END_NAMESPACE

#endif   // GROUPINFO_H
