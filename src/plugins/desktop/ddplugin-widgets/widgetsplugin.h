// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0
#ifndef WIDGETSPLUGIN_H
#define WIDGETSPLUGIN_H

#include "ddplugin_widgets_global.h"

#include <dfm-framework/dpf.h>

DDP_WIDGETS_BEGIN_NAMESPACE

class WidgetsPlugin : public DPF_NAMESPACE::Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.plugin.desktop.qt6" FILE "widgetsplugin.json")

public:
    virtual void initialize() override;
    virtual bool start() override;
};

DDP_WIDGETS_END_NAMESPACE

#endif   // WIDGETSPLUGIN_H
