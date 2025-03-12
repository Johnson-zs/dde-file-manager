// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TITLEBARHELPER_H
#define TITLEBARHELPER_H

#include "dfmplugin_titlebar_global.h"

#include <QMap>
#include <QMutex>
#include <QWidget>
#include <QMenu>

namespace dfmplugin_titlebar {

// 定义输入类型枚举
enum class InputType {
    Navigation,  // 导航到URL
    Search,      // 执行搜索
    Disabled     // 搜索被禁用
};

class TitleBarWidget;
class TitleBarHelper
{
public:
    static TitleBarWidget *findTileBarByWindowId(quint64 windowId);
    static void addTileBar(quint64 windowId, TitleBarWidget *titleBar);
    static void removeTitleBar(quint64 windowId);
    static quint64 windowId(QWidget *sender);

    static void createSettingsMenu(quint64 id);
    static QList<CrumbData> crumbSeprateUrl(const QUrl &url);
    static QList<CrumbData> tansToCrumbDataList(const QList<QVariantMap> &mapGroup);
    
    // 新增方法：判断输入类型
    static InputType determineInputType(QWidget *sender, const QString &text, QUrl *outUrl = nullptr);
    
    // 保留原有的处理方法
    static QUrl getCurrentUrl(QWidget *sender);
    static bool isSearchDisabled(const QUrl &currentUrl);
    static void handleNavigation(QWidget *sender, const QUrl &url);
    static void handleSearch(QWidget *sender, const QString &text);

    static void openCurrentUrlInNewTab(quint64 windowId);
    static void showSettingsDialog(quint64 windowId);
    static void showConnectToServerDialog(quint64 windowId);
    static void showUserSharePasswordSettingDialog(quint64 windowId);
    static void showDiskPasswordChangingDialog(quint64 windowId);

public:
    static bool newWindowAndTabEnabled;

private:
    static QMutex &mutex();
    static void handleSettingMenuTriggered(quint64 windowId, int action);
    static QString getDisplayName(const QString &name);
    static QMap<quint64, TitleBarWidget *> kTitleBarMap;
};

}

#endif   // TITLEBARHELPER_H
