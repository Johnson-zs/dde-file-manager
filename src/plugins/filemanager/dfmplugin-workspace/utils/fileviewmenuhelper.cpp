// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileviewmenuhelper.h"
#include "views/fileview.h"
#include "models/fileviewmodel.h"
#include "menus/workspacemenuscene.h"
#include "utils/workspacehelper.h"

#include "plugins/common/dfmplugin-menu/menu_eventinterface_helper.h"

#include <dfm-base/dfm_menu_defines.h>
#include <dfm-base/utils/systempathutil.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/widgets/filemanagerwindowsmanager.h>

#include <dfm-framework/dpf.h>

#include <QApplication>

DFMBASE_USE_NAMESPACE
using namespace dfmplugin_workspace;

FileViewMenuHelper::FileViewMenuHelper(FileView *parent)
    : QObject(parent),
      view(parent)
{
}

bool FileViewMenuHelper::disableMenu()
{
    QVariantHash params;
    // use qApp->applicationName by defalut;
    auto ret = dpfSlotChannel->push("dfmplugin_menu", "slot_Menu_IsDisable", params);

    if (ret.isValid())
        return ret.toBool();
    return false;
}

void FileViewMenuHelper::showEmptyAreaMenu()
{
    fmInfo() << "Showing empty area menu for directory:" << view->rootUrl().toString();

    auto scene = dfmplugin_menu_util::menuSceneCreateScene(currentMenuScene());
    setWaitCursor();
#ifdef ENABLE_TESTING
    dpfSlotChannel->push("dfmplugin_utils", "slot_Accessible_SetAccessibleName",
                         qobject_cast<QWidget *>(scene), AcName::kAcFileviewMenu);
#endif
    if (!scene) {
        fmWarning() << "Create scene failed, scene name: " << currentMenuScene();
        reloadCursor();
        return;
    }

    QVariantHash params;
    params[MenuParamKey::kCurrentDir] = view->rootUrl();
    params[MenuParamKey::kOnDesktop] = false;
    params[MenuParamKey::kIsEmptyArea] = true;
    params[MenuParamKey::kWindowId] = FMWindowsIns.findWindowId(view);
    setWaitCursor();
    if (!scene->initialize(params)) {
        fmWarning() << "Failed to initialize empty area menu scene";
        delete scene;
        reloadCursor();
        return;
    }

    if (menuPtr)
        delete menuPtr;

    menuPtr = new DMenu(this->view);
    scene->create(menuPtr);
    scene->updateState(menuPtr);
    reloadCursor();

    QAction *act = menuPtr->exec(QCursor::pos());
    if (act) {
        fmInfo() << "Empty area menu action triggered:" << act->text();
        QList<QUrl> urls { view->rootUrl() };
        dpfSignalDispatcher->publish("dfmplugin_workspace", "signal_ReportLog_MenuData", act->text(), urls);
        scene->triggered(act);
    } else {
        fmDebug() << "Empty area menu dismissed without selection";
    }
    delete scene;
}

void FileViewMenuHelper::showNormalMenu(const QModelIndex &index, const Qt::ItemFlags &indexFlags)
{
    setWaitCursor();
    auto scene = dfmplugin_menu_util::menuSceneCreateScene(currentMenuScene());
    if (!scene) {
        fmWarning() << "Create scene failed, scene name: " << currentMenuScene();
        reloadCursor();
        return;
    }

    QList<QUrl> selectUrls,treeSelectUrls;
    view->selectedTreeViewUrlList(selectUrls, treeSelectUrls);
    QUrl tgUrl;

    QVariantHash params;
    params[MenuParamKey::kCurrentDir] = view->rootUrl();
    setWaitCursor();

    const FileInfoPointer &focusFileInfo = view->model()->fileInfo(index);
    if (focusFileInfo) {
        tgUrl = focusFileInfo->urlOf(UrlInfoType::kUrl);
        // first is focus
        selectUrls.removeAll(tgUrl);
        selectUrls.prepend(tgUrl);
    }
    params[MenuParamKey::kSelectFiles] = QVariant::fromValue(selectUrls);
    params[MenuParamKey::kTreeSelectFiles] = QVariant::fromValue(treeSelectUrls);
    params[MenuParamKey::kIndexFlags] = QVariant::fromValue(indexFlags);
    params[MenuParamKey::kOnDesktop] = false;
    params[MenuParamKey::kIsEmptyArea] = false;
    params[MenuParamKey::kWindowId] = FMWindowsIns.findWindowId(view);
    params = dfmplugin_menu_util::menuPerfectParams(params);
    setWaitCursor();

    if (!scene->initialize(params)) {
        fmWarning() << "Failed to initialize normal menu scene";
        reloadCursor();
        delete scene;
        return;
    }

    if (menuPtr)
        delete menuPtr;

    menuPtr = new DMenu(this->view);
    setWaitCursor();
    scene->create(menuPtr);
    setWaitCursor();
    scene->updateState(menuPtr);
    reloadCursor();

    QAction *act = menuPtr->exec(QCursor::pos());
    if (act) {
        dpfSignalDispatcher->publish("dfmplugin_workspace", "signal_ReportLog_MenuData", act->text(), selectUrls);
        scene->triggered(act);
    }
    delete scene;
}

QString FileViewMenuHelper::currentMenuScene() const
{
    QString scene = WorkspaceHelper::instance()->findMenuScene(view->rootUrl().scheme());
    return scene.isEmpty() ? WorkspaceMenuCreator::name() : scene;
}

void FileViewMenuHelper::setWaitCursor()
{
    if (QApplication::overrideCursor() && QApplication::overrideCursor()->shape() == Qt::CursorShape::WaitCursor)
        return;
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

void FileViewMenuHelper::reloadCursor()
{
    while (QApplication::overrideCursor())
        QApplication::restoreOverrideCursor();
}
