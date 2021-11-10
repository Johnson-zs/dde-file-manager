/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     huanyu<huanyu@uniontech.com>
 *
 * Maintainer: huanyu<huanyu@uniontech.com>
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

#include "config.h" //cmake

#include <dfm-framework/framework.h>

#include <DApplication>
#include <DMainWindow>

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QtGlobal>

#include <dfmio_register.h>

#include <iostream>
#include <algorithm>

DGUI_USE_NAMESPACE
USING_IO_NAMESPACE
DWIDGET_USE_NAMESPACE

/// @brief PLUGIN_INTERFACE 默认插件iid
static const char *const FM_PLUGIN_INTERFACE = "org.deepin.plugin.desktop";
static const char *const PLUGIN_CORE = "ddplugin-core";
static const char *const LIB_CORE = "libddplugin-core.so";

static bool pluginsLoad()
{
    dpfCheckTimeBegin();

    auto &&lifeCycle = dpfInstance.lifeCycle();

    // set plugin iid from qt style
    lifeCycle.setPluginIID(FM_PLUGIN_INTERFACE);

    // cmake out definitions "DFM_PLUGIN_PATH" and "DFM_BUILD_OUT_PLGUN_DIR"
    if (DApplication::applicationDirPath() == "/usr/bin") {
        // run dde-file-manager path is /usr/bin, use system install plugins
        qInfo() << "run application in /usr/bin, load system plugin";
        lifeCycle.setPluginPaths({DFM_PLUGIN_PATH});
    } else {
        // if debug and any read from cmake out build path
        qInfo() << "run application not /usr/bin, load debug plugin";
        lifeCycle.setPluginPaths({DFM_BUILD_OUT_PLGUN_DIR});
    }

    qInfo() << "Depend library paths:" << DApplication::libraryPaths();
    qInfo() << "Load plugin paths: " << dpf::LifeCycle::pluginPaths();

    // read all plugins in setting paths
    if (!lifeCycle.readPlugins())
        return false;

    // 手动初始化Core插件
    auto corePlugin = lifeCycle.pluginMetaObj(PLUGIN_CORE);
    if (corePlugin.isNull())
        return false;
    if (!corePlugin->fileName().contains(LIB_CORE)) {
        qWarning() << corePlugin->fileName() << "is not" << LIB_CORE;
        return false;
    }
    if (!lifeCycle.loadPlugin(corePlugin))
        return false;

    // load plugins without core
    if (!lifeCycle.loadPlugins())
        return false;

    dpfCheckTimeEnd();

    return true;
}

void initAbus()
{
    dpfInstance.appBus();
}

int main(int argc, char *argv[])
{
    DApplication a(argc, argv);

    dpfInstance.initialize();

    if (!pluginsLoad()) {
        qCritical() << "Load pugin failed!";
        abort();
    }

    initAbus();

    return a.exec();
}
