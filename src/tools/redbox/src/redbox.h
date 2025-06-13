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

#ifndef REDBOX_H
#define REDBOX_H


#define Rb_Suceess 0

// status
#define Rb_Idle 1
#define Rb_Busy 2
#define Rb_Closed 3

#include <string>
#include <map>
#include <iostream>

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <vector>

namespace redbox {

typedef std::map<std::string, std::string> RbValues;
constexpr const char kRbVersion[] = "0.1";

// utils
inline void makeMap2(redbox::RbValues &ret)
{
}

template<typename ...Aargs>
void makeMap2(redbox::RbValues &ret, const std::string &a, const std::string & b, Aargs... args)
{
    ret.insert(std::make_pair(a, b));
    makeMap2(ret, args...);
}

template<typename ...Aargs>
redbox::RbValues makeMap(Aargs... args)
{
    redbox::RbValues ret;
    redbox::makeMap2(ret, args...);
    return ret;
}

// context
typedef int (*RbOpen)(const std::string &file);
typedef void (*RbClose)(int waitms);
typedef int (*RbCreateTopic)(const std::string &topic);
typedef int (*RbAddCheckPoint)(const std::string &topic, const std::string &point, const std::map<std::string, std::string> &values);

struct RedBox
{
    RbOpen open = nullptr;
    RbClose close = nullptr;
    RbCreateTopic createTopic = nullptr;
    RbAddCheckPoint addCheckPoint = nullptr;
};

inline RedBox *instance()
{
    const char *str = getenv("RB_INSTANCE");
    if (!str)
        return nullptr;
    return (RedBox *)atol(str);
}

inline std::string defaultLogFile()
{
    std::string path = "./redbox.log";
    return path;
}

inline std::string envLogFile()
{
    const char *path = getenv("RB_LOGFILE");
    if (!path)
        return "";
    return path;
}

inline void initialize(std::string str = "")
{
    const char *ver = getenv("RB_VERSION");
    if (!ver || strcmp(ver, kRbVersion) != 0) {
        std::cerr << "the version is not matched, header: " << kRbVersion
                  << " lib:" << (ver ? ver : "null") << std::endl;
    }
    if (str.empty()) {
        str = envLogFile();
        if (str.empty())
            str = defaultLogFile();
    }
    // get real path
    {
        char rp[260] = {0};
        realpath(str.c_str(), rp);
        str = rp;
    }
    str.append("." + std::to_string(getpid()));
    if (auto ins = redbox::instance()) {
        std::cout << "welcome to redbox " << kRbVersion << ". the log file is " << str << std::endl;
        if (ins->open(str) != Rb_Suceess)
            abort();
    } else {
        std::cerr << "no redbox instance " << kRbVersion << std::endl;
    }
}

// start up tools
inline std::string getStartupTime()
{
    long time = 0;
    const char *strTime = getenv("RB_STARTUP_TIME");
    if (strTime) {
        time = atol(strTime);
    }

    return std::to_string(time);
}

inline std::string getCurrentTime()
{
    timeval time;
    gettimeofday(&time, NULL);
    long startTime = time.tv_sec * 1000l + time.tv_usec / 1000l;
    return std::to_string(startTime);
}

}


// marco functions
#define RB_INITIALIZE(argc, argv) {\
    redbox::initialize(); \
}

#define RB_INITIALIZE_WITH_STARTUP(argc, argv) {\
    auto maint = redbox::getCurrentTime();\
    redbox::initialize();\
    if (auto ins = redbox::instance()) {\
        ins->createTopic("startup");\
        ins->addCheckPoint("startup", "exec", \
            redbox::makeMap("time", redbox::getStartupTime()));\
        ins->addCheckPoint("startup","main", \
        redbox::makeMap("time", maint));\
    }\
}

#define RB_CLOSE(ms) if (auto ins = redbox::instance()) \
                        ins->close(ms)

#define RB_CHECKTIME_WITH_STARTUP(point) \
    if (auto ins = redbox::instance()) {\
        ins->addCheckPoint("startup", point, \
        redbox::makeMap("time", redbox::getCurrentTime()));\
    }

#define RB_JUSTDOFIRST(func) {\
        static bool first = true; \
        if (first) { \
            func; \
            first = false; \
        }} \

#else
#define RB_INITIALIZE(argc, argv)
#define RB_INITIALIZE_WITH_STARTUP(argc, argv)
#define RB_CLOSE(ms)
#define RB_CHECKTIME_WITH_STARTUP(point)
#define RB_JUSTDOFIRST(func)
#endif


