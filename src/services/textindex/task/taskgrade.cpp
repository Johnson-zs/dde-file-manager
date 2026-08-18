// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "task/taskgrade.h"
#include "utils/textindexconfig.h"

SERVICETEXTINDEX_BEGIN_NAMESPACE

int TaskGrader::lightIncrementalContentFileLimit() const
{
    return TextIndexConfig::instance().lightIncrementalContentFileLimit();
}

int TaskGrader::lightIncrementalOcrFileLimit() const
{
    return TextIndexConfig::instance().lightIncrementalOcrFileLimit();
}

int TaskGrader::lightIncrementalSizeLimitMB() const
{
    return TextIndexConfig::instance().lightIncrementalSizeLimitMB();
}

SERVICETEXTINDEX_END_NAMESPACE
