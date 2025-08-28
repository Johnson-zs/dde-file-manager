// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEGROUPMENUSCENE_H
#define FILEGROUPMENUSCENE_H

#include "dfmplugin_filegroup_global.h"

#include <dfm-base/interfaces/abstractmenuscene.h>
#include <dfm-base/interfaces/abstractscenecreator.h>

namespace dfmplugin_filegroup {

class FileGroupMenuSceneCreator : public DFMBASE_NAMESPACE::AbstractSceneCreator
{
public:
    static QString name()
    {
        return "FileGroupMenu";
    }
    DFMBASE_NAMESPACE::AbstractMenuScene *create() override;
};

class FileGroupMenuScenePrivate;
class FileGroupMenuScene : public DFMBASE_NAMESPACE::AbstractMenuScene
{
    Q_OBJECT
public:
    explicit FileGroupMenuScene(QObject *parent = nullptr);
    QString name() const override;
    bool initialize(const QVariantHash &params) override;
    AbstractMenuScene *scene(QAction *action) const override;
    bool create(QMenu *parent) override;
    void updateState(QMenu *parent) override;
    bool triggered(QAction *action) override;

private:
    FileGroupMenuScenePrivate *const d = nullptr;
};

}

#endif   // FILEGROUPMENUSCENE_H
