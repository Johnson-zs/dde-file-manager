// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0

#include "private/widgetplugincontext_p.h"

WIDGETS_BEGIN_NAMESPACE

WidgetContext::WidgetContext()
{
}

WidgetPluginContextPrivate::WidgetPluginContextPrivate(WidgetPluginContext *q)
    : q_ptr(q)
{
}

WidgetPluginContext::~WidgetPluginContext()
{
}

void WidgetPluginContext::setTitle(const QString &title)
{
    Q_D(WidgetPluginContext);

    d->metaData.title = title;
}

void WidgetPluginContext::setDescription(const QString &desc)
{
    Q_D(WidgetPluginContext);

    d->metaData.description = desc;
}

void WidgetPluginContext::setSupportedSizeTypes(const QVector<WidgetContext::SizeType> &types)
{
    Q_D(WidgetPluginContext);

    d->metaData.supportedSizeTypes = types;
}

void WidgetPluginContext::setDefaultSzieType(const WidgetContext::SizeType &type)
{
    Q_D(WidgetPluginContext);

    d->metaData.defaultSzieType = type;
}

QString WidgetPluginContext::id() const
{
    Q_D(const WidgetPluginContext);

    return d->metaData.id;
}

QString WidgetPluginContext::version() const
{
    Q_D(const WidgetPluginContext);

    return d->metaData.version;
}

WidgetPluginContext::WidgetPluginContext()
    : d_ptr(std::make_unique<WidgetPluginContextPrivate>(this))
{
}

WIDGETS_END_NAMESPACE
