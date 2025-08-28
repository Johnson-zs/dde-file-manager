// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DFMPLUGIN_FILEGROUP_GLOBAL_H
#define DFMPLUGIN_FILEGROUP_GLOBAL_H

#include <dfm-base/dfm_log_defines.h>

#define DPFILEGROUP_NAMESPACE dfmplugin_filegroup

#define DPFILEGROUP_BEGIN_NAMESPACE namespace DPFILEGROUP_NAMESPACE {
#define DPFILEGROUP_END_NAMESPACE }
#define DPFILEGROUP_USE_NAMESPACE using namespace DPFILEGROUP_NAMESPACE;

DPFILEGROUP_BEGIN_NAMESPACE
DFM_LOG_USE_CATEGORY(DPFILEGROUP_NAMESPACE)

namespace FileGroupActionId {
inline constexpr char kSmartClassification[] { "smart_classification" };
}

DPFILEGROUP_END_NAMESPACE

#endif // DFMPLUGIN_FILEGROUP_GLOBAL_H