// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0
#ifndef WIDGETPLUGINSPEC_H
#define WIDGETPLUGINSPEC_H

#include <dde-desktop-widgets/dde-desktop-widgets-global.h>
#include <dde-desktop-widgets/interfaces/widgetplugin.h>

#include <QObject>
#include <QVariantMap>
#include <QIcon>
#include <QVector>

WIDGETS_BEGIN_NAMESPACE

struct PluginMetaData
{
    // from json
    // uniquely identifies
    QString id;

    // from json
    QString version;

    // from json
    // Reserved, ref:
    // https://develop.kde.org/docs/plasma/widget/properties/#category
    QString category;

    // from json
    // {
    //     "Email": "myemail@gmail.com",
    //     "Name": "My Name"
    // }
    QVariantMap authors;

    // display in the title bar
    // displayed in the list of Edit Mode components
    QString title;

    // display description information in the component list,
    // description length limit: 50 bytes
    QString description;

    // Standard sizes can be selected for new and adjusted widgets
    QVector<WidgetContext::SizeType> supportedSizeTypes;

    // Optional standard sizes, or customized sizes
    WidgetContext::SizeType defaultSzieType;
};

class WidgetPluginSpec : public QObject
{
    Q_OBJECT
public:
    explicit WidgetPluginSpec(QObject *parent = nullptr);

signals:
};

WIDGETS_END_NAMESPACE

#endif   // WIDGETPLUGINSPEC_H
