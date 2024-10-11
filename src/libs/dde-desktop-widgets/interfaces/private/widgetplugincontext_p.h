// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0
#ifndef WIDGETPLUGINCONTEXT_P_H
#define WIDGETPLUGINCONTEXT_P_H

#include <dde-desktop-widgets/interfaces/widgetplugincontext.h>
#include <dde-desktop-widgets/frame/widgetpluginspec.h>

WIDGETS_BEGIN_NAMESPACE

class WidgetPluginContextPrivate
{
public:
    WidgetPluginContextPrivate(WidgetPluginContext *q);

    WidgetPluginContext *q_ptr;
    PluginMetaData metaData;

    Q_DECLARE_PUBLIC(WidgetPluginContext)
};

WIDGETS_END_NAMESPACE

#endif   // WIDGETPLUGINCONTEXT_P_H
