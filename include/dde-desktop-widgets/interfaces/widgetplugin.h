// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0
#ifndef WIDGETPLUGIN_H
#define WIDGETPLUGIN_H

#include <dde-desktop-widgets/dde-desktop-widgets-global.h>
#include <dde-desktop-widgets/interfaces/widgetplugincontext.h>

#include <QObject>

#include <memory>

WIDGETS_BEGIN_NAMESPACE

class WidgetPlugin : public QObject
{
    Q_OBJECT
public:
    WidgetPlugin() = default;
    virtual ~WidgetPlugin() = default;

    virtual bool initialize(std::weak_ptr<WidgetPluginContext> context) = 0;
    virtual bool start();
};

WIDGETS_END_NAMESPACE

#define DDE_DESKTOP_WIDGETS_PLUGIN_IID "org.deepin.dde.desktop.widgets.PluginInterface"

#endif   // WIDGETPLUGIN_H
