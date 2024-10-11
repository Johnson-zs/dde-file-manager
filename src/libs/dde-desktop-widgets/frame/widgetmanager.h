// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0
#ifndef WIDGETMANAGER_H
#define WIDGETMANAGER_H

#include <dde-desktop-widgets/dde-desktop-widgets-global.h>

#include <QObject>

WIDGETS_BEGIN_NAMESPACE

class WidgetManager : public QObject
{
    Q_OBJECT
public:
    explicit WidgetManager(QObject *parent = nullptr);

signals:
};

WIDGETS_END_NAMESPACE

#endif   // WIDGETMANAGER_H
