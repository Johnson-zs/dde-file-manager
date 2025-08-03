// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONFIGSTRUCTS_H
#define CONFIGSTRUCTS_H

#include "dfmplugin_workspace_global.h"

#include <dfm-base/dfm_base_global.h>

#include <QStringList>
#include <QDir>
#include <QMetaType>

DFMBASE_USE_NAMESPACE

namespace dfmplugin_workspace {

/**
 * @brief Configuration structure for file sorting
 * 
 * This structure contains all parameters needed for sorting files and directories.
 * It is designed to be lightweight and easily comparable.
 */
struct SortConfig
{
    Global::ItemRoles role = Global::ItemRoles::kItemFileDisplayNameRole;  ///< Sort role/column
    Qt::SortOrder order = Qt::AscendingOrder;                              ///< Sort order
    bool sortCaseSensitive = false;                                        ///< Case sensitive sorting
    bool isMixDirAndFile = false;                                          ///< Mix directories and files

    /**
     * @brief Default constructor
     */
    SortConfig() = default;

    /**
     * @brief Constructor with parameters
     */
    SortConfig(Global::ItemRoles r, Qt::SortOrder o, bool caseSensitive = false, bool mixDirFile = false)
        : role(r), order(o), sortCaseSensitive(caseSensitive), isMixDirAndFile(mixDirFile) {}

    /**
     * @brief Equality comparison operator
     * 
     * @param other Other SortConfig to compare with
     * @return bool true if configurations are identical
     */
    bool operator==(const SortConfig &other) const {
        return role == other.role &&
               order == other.order &&
               sortCaseSensitive == other.sortCaseSensitive &&
               isMixDirAndFile == other.isMixDirAndFile;
    }

    /**
     * @brief Inequality comparison operator
     * 
     * @param other Other SortConfig to compare with
     * @return bool true if configurations are different
     */
    bool operator!=(const SortConfig &other) const {
        return !(*this == other);
    }
};

/**
 * @brief Configuration structure for file filtering
 * 
 * This structure contains all parameters needed for filtering files and directories.
 * It encapsulates the various filter types used by Qt's directory iteration.
 */
struct FilterConfig
{
    QStringList nameFilters;                                               ///< Name filter patterns (e.g., "*.txt")
    QDir::Filters dirFilters = QDir::AllEntries | QDir::NoDotAndDotDot;   ///< Directory filter flags
    bool showHidden = false;                                               ///< Whether to show hidden files

    /**
     * @brief Default constructor
     */
    FilterConfig() = default;

    /**
     * @brief Constructor with parameters
     */
    FilterConfig(const QStringList &nameFilters, QDir::Filters dirFilters, bool showHidden = false)
        : nameFilters(nameFilters), dirFilters(dirFilters), showHidden(showHidden) {}

    /**
     * @brief Equality comparison operator
     * 
     * @param other Other FilterConfig to compare with
     * @return bool true if configurations are identical
     */
    bool operator==(const FilterConfig &other) const {
        return nameFilters == other.nameFilters &&
               dirFilters == other.dirFilters &&
               showHidden == other.showHidden;
    }

    /**
     * @brief Inequality comparison operator
     * 
     * @param other Other FilterConfig to compare with
     * @return bool true if configurations are different
     */
    bool operator!=(const FilterConfig &other) const {
        return !(*this == other);
    }
};

} // namespace dfmplugin_workspace

Q_DECLARE_METATYPE(dfmplugin_workspace::SortConfig)
Q_DECLARE_METATYPE(dfmplugin_workspace::FilterConfig)

#endif // CONFIGSTRUCTS_H 