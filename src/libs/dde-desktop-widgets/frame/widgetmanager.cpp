// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0
#include "widgetmanager.h"

Q_LOGGING_CATEGORY(logDDW, "org.deepin.dde.desktop.widgets")

WIDGETS_BEGIN_NAMESPACE

WidgetManager::WidgetManager(QObject *parent)
    : QObject { parent }
{
}

WIDGETS_END_NAMESPACE
