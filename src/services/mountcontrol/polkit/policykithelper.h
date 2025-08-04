// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef POLICYKITHELPER_H
#define POLICYKITHELPER_H

#include "service_mountcontrol_global.h"

#include <QObject>
#include <polkit-qt6-1/PolkitQt1/Authority>

SERVICEMOUNTCONTROL_BEGIN_NAMESPACE

class PolicyKitHelper
{
public:
    static PolicyKitHelper *instance();

    bool checkAuthorization(const QString &actionId, const QString &appBusName);

private:
    PolicyKitHelper();
    ~PolicyKitHelper();

    Q_DISABLE_COPY(PolicyKitHelper)
};

SERVICEMOUNTCONTROL_END_NAMESPACE

#endif   // POLICYKITHELPER_H 