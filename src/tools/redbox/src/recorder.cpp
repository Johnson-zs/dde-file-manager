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

#include "recorder.h"
#include "util.h"
#include "redbox.h"

#include <stdio.h>
#include <thread>

using namespace redbox;

constexpr char kBeginChar[] = "[\n{\"redbox\":\"0.1\"}\n";
constexpr char kEndChar[] = "\n]";

Recorder *Recorder::instance()
{
    static redbox::Recorder ins;
    return &ins;
}

Recorder::Recorder()
{

}

Recorder::~Recorder()
{
    closeFile(0);
}

void Recorder::run()
{
    auto self = instance();
    while (self->runFlag) {
        MutexLocker lk(self->queMtx);
        if (self->eventQue.empty()){
            fflush(self->logFile);
            self->cv.wait(lk);
            continue;
        }

        auto e = self->eventQue.front();
        self->eventQue.pop();
        lk.unlock();

        std::string str = ","; // in json array
        str.append(self->toJson(e));
        str.append("\n");

        {
            MutexLocker lk(self->fileMtx);
            fwrite(str.c_str(), str.size(), 1, self->logFile);
        }
    }
}

std::string Recorder::toJson(const Event &e) const
{
    std::string ret = "{\"topic\":\"" + e.topic + "\"" +
            ",\"index\":\"" + std::to_string(e.index) + "\""
            + ",\"name\":\"" + e.point + "\"";
    // values
    if (!e.values.empty()) {
        ret += ",\"values\":{";
        for (auto it = e.values.begin(); it != e.values.end(); ++it) {
            if (it != e.values.begin())
                ret += ",";
            ret = ret + "\"" + it->first + "\":\"" + it->second + "\"";
        }
        ret += "}";
    }

    // end
    ret += "}";
    return ret;
}

bool Recorder::openFile(const std::string &file)
{
    bool ret = false;
    MutexLocker lk(fileMtx);
    if (!file.empty() && !isOpend()) {
        if (auto fp = fopen(file.c_str(), "w+")) {
            logFile = fp;
            fwrite(kBeginChar, strlen(kBeginChar), 1, logFile);

            ret = true;
            runFlag = true;
            eventQue = std::queue<Event>();
            work = new std::thread(Recorder::run);
        }
    }

    return ret;
}

void Recorder::closeFile(int ms)
{
    if (work && runFlag) {
        if (ms < 0) {
            bool empty = false;
            while (true) {
                {
                    MutexLocker lk(queMtx);
                    empty = eventQue.size() == 0;
                }
                if (empty)
                    break;

                usleep(100000);
            }
        } else if (ms > 0) {
            while (ms > 0) {
                bool empty = false;
                {
                    MutexLocker lk(queMtx);
                    empty = eventQue.size() == 0;
                }
                if (empty)
                    break;

                usleep(100000);
                ms -= 100;
            }
        }

        runFlag = false;
        cv.notify_one();
        work->join();
        delete work;
        work = nullptr;
    }

    MutexLocker lk(fileMtx);
    if (logFile) {
        fwrite(kEndChar, strlen(kEndChar), 1, logFile);
        fclose(logFile);
        logFile = nullptr;
    }
}

bool Recorder::createTopic(const std::string &topic)
{
    MutexLocker lk(cpMtx);
    bool ret = false;
    if (!topic.empty() && checkPoint.find(topic) == checkPoint.end()) {
        checkPoint.insert(std::make_pair(topic, 0));
        ret = true;
    }

    return ret;
}

bool Recorder::addPoint(const std::string &topic, const std::string &point, const std::map<std::string, std::string> &prop)
{
    int index = 0;
    {
        MutexLocker lk(cpMtx);
        auto it = checkPoint.find(topic);
        if (it == checkPoint.end())
            return false;
        index = it->second++;
    }

    if (point.empty())
        return false;

    Event e;
    e.topic = topic;
    e.point = point;
    e.values = prop;
    e.index = index;

    {
        MutexLocker lk(queMtx);
        eventQue.push(e);
    }

    cv.notify_one();
    return true;
}
