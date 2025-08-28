// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filegroupmenuscene.h"
#include "filegroupmenuscene_p.h"
#include "dialogs/smartclassificationdialog.h"

#include <dfm-base/dfm_menu_defines.h>
#include <dfm-base/base/schemefactory.h>

#include <QMenu>
#include <QAction>

using namespace dfmplugin_filegroup;
DFMBASE_USE_NAMESPACE

AbstractMenuScene *FileGroupMenuSceneCreator::create()
{
    return new FileGroupMenuScene();
}

FileGroupMenuScenePrivate::FileGroupMenuScenePrivate(FileGroupMenuScene *qq)
    : AbstractMenuScenePrivate(qq)
{
    predicateName[FileGroupActionId::kSmartClassification] = tr("Smart Classification");
}

void FileGroupMenuScenePrivate::updateMenu(QMenu *menu)
{
    QList<QAction *> actions = menu->actions();
    if (!actions.isEmpty()) {
        QAction *smartClassificationAct = nullptr;
        for (auto act : actions) {
            if (act->isSeparator())
                continue;

            if (predicateAction.values().contains(act)) {
                smartClassificationAct = act;
                break;
            }
        }

        if (smartClassificationAct) {
            // Move the smart classification action to the end
            actions.removeOne(smartClassificationAct);
            QAction *separator = menu->addSeparator();
            actions.append(separator);
            actions.append(smartClassificationAct);
            menu->addActions(actions);
        }
    }
}

bool FileGroupMenuScenePrivate::isDirectory() const
{
    if (!focusFileInfo)
        return false;

    return focusFileInfo->isAttributes(OptInfoType::kIsDir);
}

FileGroupMenuScene::FileGroupMenuScene(QObject *parent)
    : AbstractMenuScene(parent), d(new FileGroupMenuScenePrivate(this))
{
}

QString FileGroupMenuScene::name() const
{
    return FileGroupMenuSceneCreator::name();
}

bool FileGroupMenuScene::initialize(const QVariantHash &params)
{
    d->currentDir = params.value(MenuParamKey::kCurrentDir).toUrl();
    d->isEmptyArea = params.value(MenuParamKey::kIsEmptyArea).toBool();
    d->selectFiles = params.value(MenuParamKey::kSelectFiles).value<QList<QUrl>>();
    if (!d->selectFiles.isEmpty())
        d->focusFile = d->selectFiles.first();
    d->onDesktop = params.value(MenuParamKey::kOnDesktop).toBool();

    if (!d->initializeParamsIsValid()) {
        fmWarning() << "FileGroup menu scene:" << name() << " init failed."
                    << "selectFiles empty:" << d->selectFiles.isEmpty()
                    << "focusFile:" << d->focusFile << "currentDir:" << d->currentDir;
        return false;
    }

    if (d->selectFiles.isEmpty() && d->currentDir.isValid()) {
        d->selectFiles << d->currentDir;
    }

    if (!d->isEmptyArea && !d->selectFiles.isEmpty()) {
        QString errString;
        d->focusFileInfo = DFMBASE_NAMESPACE::InfoFactory::create<FileInfo>(
                d->focusFile, Global::CreateFileInfoType::kCreateFileInfoAuto, &errString);
        if (d->focusFileInfo.isNull()) {
            fmDebug() << "Failed to create FileInfo:" << errString;
            return false;
        }
    }

    return AbstractMenuScene::initialize(params);
}

AbstractMenuScene *FileGroupMenuScene::scene(QAction *action) const
{
    if (action == nullptr)
        return nullptr;

    if (d->predicateAction.values().contains(action))
        return const_cast<FileGroupMenuScene *>(this);

    return AbstractMenuScene::scene(action);
}

bool FileGroupMenuScene::create(QMenu *parent)
{
    // Only show the Smart Classification action when right-clicking on a directory
    if (!d->isDirectory()) {
        return AbstractMenuScene::create(parent);
    }

    if (d->selectFiles.isEmpty() && !d->focusFile.isValid())
        return false;

    QAction *tempAction = parent->addAction(d->predicateName.value(FileGroupActionId::kSmartClassification));
    d->predicateAction[FileGroupActionId::kSmartClassification] = tempAction;
    tempAction->setProperty(ActionPropertyKey::kActionID, FileGroupActionId::kSmartClassification);

    fmDebug() << "Created Smart Classification action for directory:" << d->focusFile;

    return AbstractMenuScene::create(parent);
}

void FileGroupMenuScene::updateState(QMenu *parent)
{
    if (!parent)
        return;

    if (auto smartClassificationAction = d->predicateAction.value(FileGroupActionId::kSmartClassification)) {
        // Disable the action if the directory doesn't exist or is not accessible
        if (!d->isEmptyArea && d->focusFileInfo && !d->focusFileInfo->exists()) {
            smartClassificationAction->setDisabled(true);
            fmDebug() << "Smart Classification action disabled - directory doesn't exist";
        }
    }

    d->updateMenu(parent);

    AbstractMenuScene::updateState(parent);
}

bool FileGroupMenuScene::triggered(QAction *action)
{
    if (!d->predicateAction.values().contains(action))
        return false;

    QString id = d->predicateAction.key(action);
    if (id == FileGroupActionId::kSmartClassification) {
        fmInfo() << "Smart Classification action triggered for directory:" << d->focusFile;
        
        // Create and show the Smart Classification dialog
        SmartClassificationDialog *dialog = new SmartClassificationDialog(d->focusFile);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        
        return true;
    }

    return AbstractMenuScene::triggered(action);
}
