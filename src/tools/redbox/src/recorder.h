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
#ifndef RECORDER_H
#define RECORDER_H

#include <string>
#include <mutex>
#include <map>
#include <thread>
#include <condition_variable>
#include <queue>

namespace redbox {

struct Event
{
    Event() {}
    std::string topic;
    std::string point;
    int index;
    std::map<std::string, std::string> values;
};

class Recorder
{
public:
    static Recorder *instance();
    bool openFile(const std::string &file);
    void closeFile(int ms);
    bool createTopic(const std::string &topic);
    bool addPoint(const std::string &topic, const std::string &point, const std::map<std::string, std::string> &prop);
protected:
    explicit Recorder();
    ~Recorder();
    inline bool isOpend() const { return logFile;}
    static void run();
    std::string toJson(const Event &e) const;
private:
    FILE *logFile = nullptr;
    std::mutex fileMtx;
    std::map<std::string, int> checkPoint;
    std::mutex cpMtx;
    volatile bool runFlag = false;

    std::thread *work = nullptr;
    std::condition_variable cv;

    std::mutex queMtx;
    std::queue<Event> eventQue;
};

}
#define RecorderIns redbox::Recorder::instance()

#endif // RECORDER_H
