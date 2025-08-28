// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEGROUP_H
#define FILEGROUP_H

#include "dfmplugin_filegroup_global.h"

#include <dfm-framework/dpf.h>

namespace dfmplugin_filegroup {

class FileGroup : public dpf::Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.plugin.filemanager" FILE "filegroup.json")

public:
    virtual void initialize() override;
    virtual bool start() override;

private:
    void registerMenuScene();

private slots:
    void onAllPluginsStarted();
};

}

#endif   // FILEGROUP_H
