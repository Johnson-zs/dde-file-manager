// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0
#ifndef WIDGETPLUGINCONTEXT_H
#define WIDGETPLUGINCONTEXT_H

#include <dde-desktop-widgets/dde-desktop-widgets-global.h>

#include <QObject>

#include <functional>

WIDGETS_BEGIN_NAMESPACE

class WidgetContext
{
public:
    enum SizeType {
        Invalid,
        Small = 10,
        Middle = 20,
        Large = 30,
        Custom = 64
    };
    WidgetContext();

private:
    Q_DISABLE_COPY_MOVE(WidgetContext)
};

class WidgetPluginContextPrivate;
class WidgetPluginContext
{
public:
    using CreateWidgetFunc = std::function<std::unique_ptr<WidgetContext>()>;

    ~WidgetPluginContext();

    void setTitle(const QString &title);
    void setDescription(const QString &desc);
    void setSupportedSizeTypes(const QVector<WidgetContext::SizeType> &types);
    void setDefaultSzieType(const WidgetContext::SizeType &type);

    QString id() const;
    QString version() const;

protected:
    WidgetPluginContext();

    std::unique_ptr<WidgetPluginContextPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE_D(d_ptr, WidgetPluginContext)
    Q_DISABLE_COPY_MOVE(WidgetPluginContext)
};

WIDGETS_END_NAMESPACE

#endif   // WIDGETPLUGINCONTEXT_H
