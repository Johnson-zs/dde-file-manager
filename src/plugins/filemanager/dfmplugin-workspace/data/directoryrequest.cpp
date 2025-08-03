 // SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directoryrequest.h"

using namespace dfmplugin_workspace;

DirectoryRequest::DirectoryRequest()
    : m_timestamp(QDateTime::currentDateTime())
    , m_useCache(true)
{
}

DirectoryRequest::DirectoryRequest(const QUrl &url, 
                                 const QString &requestId,
                                 const SortConfig &sortConfig,
                                 const FilterConfig &filterConfig,
                                 bool useCache,
                                 bool needWatch)
    : m_directoryUrl(url)
    , m_requestId(requestId)
    , m_sortConfig(sortConfig)
    , m_filterConfig(filterConfig)
    , m_timestamp(QDateTime::currentDateTime())
    , m_useCache(useCache)
    , m_refreshCache(needWatch)
{
}


