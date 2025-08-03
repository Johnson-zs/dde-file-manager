// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileitem.h"

#include <dfm-base/base/urlroute.h>

#include <QFileInfo>

using namespace dfmplugin_workspace;

FileItem::FileItem(const FileInfoPointer &fileInfo)
    : m_url(fileInfo ? fileInfo->urlOf(UrlInfoType::kUrl) : QUrl())
    , m_size(fileInfo ? fileInfo->size() : 0)
    , m_lastModified(fileInfo ? fileInfo->timeOf(TimeInfoType::kLastModified).value<QDateTime>() : QDateTime())
    , m_lastRead(fileInfo ? fileInfo->timeOf(TimeInfoType::kLastRead).value<QDateTime>() : QDateTime())
    , m_createTime(fileInfo ? fileInfo->timeOf(TimeInfoType::kCreateTime).value<QDateTime>() : QDateTime())
    , m_isDirectory(fileInfo ? fileInfo->isAttributes(OptInfoType::kIsDir) : false)
    , m_isHidden(fileInfo ? fileInfo->isAttributes(OptInfoType::kIsHidden) : false)
    , m_isSymlink(fileInfo ? fileInfo->isAttributes(OptInfoType::kIsSymLink) : false)
    , m_isReadable(fileInfo ? fileInfo->isAttributes(OptInfoType::kIsReadable) : false)
    , m_isWriteable(fileInfo ? fileInfo->isAttributes(OptInfoType::kIsWritable) : false)
    , m_isExecutable(fileInfo ? fileInfo->isAttributes(OptInfoType::kIsExecutable) : false)
{
}

FileItem::FileItem(const QUrl &url,
                   qint64 size,
                   const QDateTime &lastModified,
                   const QDateTime &lastRead,
                   const QDateTime &createTime,
                   bool isDirectory,
                   bool isHidden,
                   bool isSymlink,
                   bool isReadable,
                   bool isWriteable,
                   bool isExecutable)
    : m_url(url)
    , m_size(size)
    , m_lastModified(lastModified)
    , m_lastRead(lastRead)
    , m_createTime(createTime)
    , m_isDirectory(isDirectory)
    , m_isHidden(isHidden)
    , m_isSymlink(isSymlink)
    , m_isReadable(isReadable)
    , m_isWriteable(isWriteable)
    , m_isExecutable(isExecutable)
{
}

QString FileItem::fileName() const
{
    if (!m_url.isValid()) {
        return QString();
    }
    
    QString path = m_url.path();
    int lastSlash = path.lastIndexOf('/');
    return lastSlash >= 0 ? path.mid(lastSlash + 1) : path;
}

QString FileItem::suffix() const
{
    QString name = fileName();
    int lastDot = name.lastIndexOf('.');
    return (lastDot > 0) ? name.mid(lastDot + 1) : QString();
}

SortInfoPointer FileItem::createSortInfo() const
{
    auto sortInfo = SortInfoPointer(new SortFileInfo);
    
    // Set basic properties
    sortInfo->setUrl(m_url);
    sortInfo->setSize(m_size);
    sortInfo->setFile(!m_isDirectory);
    sortInfo->setDir(m_isDirectory);
    sortInfo->setHide(m_isHidden);
    sortInfo->setSymlink(m_isSymlink);
    sortInfo->setReadable(m_isReadable);
    sortInfo->setWriteable(m_isWriteable);
    sortInfo->setExecutable(m_isExecutable);
    
    // Set time properties (convert to time_t)
    if (m_lastModified.isValid()) {
        sortInfo->setLastModifiedTime(m_lastModified.toSecsSinceEpoch());
    }
    if (m_lastRead.isValid()) {
        sortInfo->setLastReadTime(m_lastRead.toSecsSinceEpoch());
    }
    if (m_createTime.isValid()) {
        sortInfo->setCreateTime(m_createTime.toSecsSinceEpoch());
    }
    
    return sortInfo;
} 