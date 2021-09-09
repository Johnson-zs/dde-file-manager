/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     Gary Wang <wzc782970009@gmail.com>
 *
 * Maintainer: Gary Wang <wangzichong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CrumbBar_H
#define CrumbBar_H

#include "dfm-base/base/urlroute.h"
#include "dfm-base/base/standardpaths.h"
#include "dfm-base/base/schemefactory.h"
#include "dfm_filemanager_service_global.h"

#include <QFrame>
#include <QUrl>

DSB_FM_BEGIN_NAMESPACE

class CrumbBarPrivate;
class CrumbBar : public QFrame
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(CrumbBar)
    QScopedPointer<CrumbBarPrivate> d_ptr;

public:
    explicit CrumbBar(QWidget *parent = nullptr);
    virtual ~CrumbBar() override;

    void setRootUrl(const QUrl &url);

Q_SIGNALS:
    void selectedUrl(const QUrl &url);

public Q_SLOTS:
    void onCustomContextMenu(const QPoint &point);

protected:
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
};

DSB_FM_END_NAMESPACE

#endif // CrumbBar_H