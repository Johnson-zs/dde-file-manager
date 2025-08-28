// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filegroup.h"
#include "menus/filegroupmenuscene.h"

#include "plugins/common/dfmplugin-menu/menu_eventinterface_helper.h"

#include <dfm-base/dfm_event_defines.h>

#include <dfm-framework/event/event.h>
#include <dfm-framework/dpf.h>

using namespace dfmplugin_filegroup;

namespace dfmplugin_filegroup {
DFM_LOG_REGISTER_CATEGORY(DPFILEGROUP_NAMESPACE)
}

void FileGroup::initialize()
{
    fmInfo() << "FileGroup plugin initialize";
}

bool FileGroup::start()
{
    fmInfo() << "FileGroup plugin start";
    registerMenuScene();
    return true;
}

void FileGroup::registerMenuScene()
{
    // 右键菜单初始化
    fmDebug() << "Registering FileGroup menu scene";
    dfmplugin_menu_util::menuSceneRegisterScene(FileGroupMenuSceneCreator::name(), new FileGroupMenuSceneCreator);
    onAllPluginsStarted();
}

void FileGroup::onAllPluginsStarted()
{
    fmDebug() << "All plugins started, FileGroup plugin ready";
    static constexpr auto kParentMenu { "WorkspaceMenu" };
    if (!dfmplugin_menu_util::menuSceneContains(kParentMenu)) {
        fmWarning() << "WorkspaceMenu is not contained, register filegroup menu failed";
        return;
    }

    // Bind our menu scene to the workspace menu
    dfmplugin_menu_util::menuSceneBind(FileGroupMenuSceneCreator::name(), kParentMenu);
    fmInfo() << "FileGroup menu scene bound to WorkspaceMenu successfully";
}
