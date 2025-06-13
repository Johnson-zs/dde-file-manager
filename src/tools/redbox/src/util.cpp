/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangyu<zhangyub@uniontech.com>
 *
 * Maintainer: zhangyu<zhangyub@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "util.h"
#include "interface.h"
#include "redbox.h"

using namespace redbox;

static GobalInit kGlobalEnv;

GobalInit::GobalInit()
{
    setenv("RB_VERSION", kRbVersion, 1);

    ins = new RedBox();
    ins->open = &rbOpen;
    ins->close = &rbClose;
    ins->createTopic = &rbCreateTopic;
    ins->addCheckPoint = &rbAddCheckPoint;

    auto strIns = std::to_string((long)ins);
    setenv("RB_INSTANCE", strIns.c_str(), 1);
}

GobalInit::~GobalInit()
{
    delete ins;
    ins = nullptr;
}
