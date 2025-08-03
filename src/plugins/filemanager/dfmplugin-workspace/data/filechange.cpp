// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filechange.h"

namespace dfmplugin_workspace {

FileChange::FileChange(const QUrl &fileUrl,
           ChangeType changeType,
           const QDateTime &timestamp,
           const QUrl &oldUrl)
    : m_fileUrl(fileUrl),
      m_changeType(changeType),
      m_timestamp(timestamp),
      m_oldUrl(oldUrl)
{

}

} // namespace dfmplugin_workspace
