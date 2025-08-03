 // SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directorydata.h"

using namespace dfmplugin_workspace;

DirectoryData::DirectoryData(const QUrl &directoryUrl,
                             const QList<FileItem> &files,
                             const SortConfig &sortConfig,
                             const FilterConfig &filterConfig,
                             const QString &requestId,
                             bool isComplete)
     : m_directoryUrl(directoryUrl)
     , m_files(files)
     , m_sortConfig(sortConfig)
     , m_filterConfig(filterConfig)
     , m_requestId(requestId)
     , m_timestamp(QDateTime::currentDateTime())
     , m_isComplete(isComplete)
{

}

DirectoryData::DirectoryData()
   : m_timestamp(QDateTime::currentDateTime())
{
}

const FileItem &DirectoryData::fileAt(int index) const
{
    Q_ASSERT(index >= 0 && index < m_files.size());
    return m_files.at(index);
}

const FileItem *DirectoryData::findFile(const QUrl &url) const
{
    for (const auto &item : m_files) {
        if (item.url() == url) {
            return &item;
        }
    }
    return nullptr;
}

QList<SortInfoPointer> DirectoryData::createSortInfoList() const
{
    QList<SortInfoPointer> sortInfoList;
    
    for (const auto &fileItem : m_files) {
        sortInfoList.append(fileItem.createSortInfo());
    }
    
    return sortInfoList;
}





