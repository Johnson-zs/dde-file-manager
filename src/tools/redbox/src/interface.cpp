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

#include "interface.h"
#include "recorder.h"

int rbOpen(const std::string &file)
{
    return RecorderIns->openFile(file) ? 0 : -1;
}

void rbClose(int waitms)
{
    RecorderIns->closeFile(waitms);
}


int rbCreateTopic(const std::string &topic)
{
    return RecorderIns->createTopic(topic) ? 0 : -1;
}

int rbAddCheckPoint(const std::string &topic, const std::string &point, const std::map<std::string, std::string> &prop)
{
    return RecorderIns->addPoint(topic, point, prop) ? 0 : -1;
}
