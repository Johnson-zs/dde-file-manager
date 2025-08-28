// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEGROUPMENUSCENE_P_H
#define FILEGROUPMENUSCENE_P_H

#include "filegroupmenuscene.h"

#include <dfm-base/interfaces/private/abstractmenuscene_p.h>

namespace dfmplugin_filegroup {
DFMBASE_USE_NAMESPACE

class FileGroupMenuScenePrivate : public AbstractMenuScenePrivate
{
    Q_OBJECT
public:
    friend class FileGroupMenuScene;
    explicit FileGroupMenuScenePrivate(FileGroupMenuScene *qq = nullptr);

private:
    void updateMenu(QMenu *menu);
    bool isDirectory() const;
};

}

#endif // FILEGROUPMENUSCENE_P_H 