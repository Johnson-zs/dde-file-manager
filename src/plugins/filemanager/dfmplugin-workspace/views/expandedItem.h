/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     yanghao<yanghao@uniontech.com>
 *
 * Maintainer: zhangsheng<zhangsheng@uniontech.com>
 *             liuyangming<liuyangming@uniontech.com>
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
#ifndef EXPANDEDITEM_H
#define EXPANDEDITEM_H

#include "dfmplugin_workspace_global.h"

#include <QWidget>
#include <QStyleOptionViewItem>
DPWORKSPACE_BEGIN_NAMESPACE

class IconItemDelegate;
class ExpandedItem : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(qreal opacity READ getOpacity WRITE setOpacity)

public:
    explicit ExpandedItem(IconItemDelegate *d, QWidget *parent = nullptr);
    ~ExpandedItem() override;

    bool event(QEvent *ee) override;
    void paintEvent(QPaintEvent *) override;
    QSize sizeHint() const override;
    int heightForWidth(int width) const override;

    qreal getOpacity() const;
    void setOpacity(qreal opacity);
    void setIconPixmap(const QPixmap &pixmap, int height);
    QRectF getTextBounding() const;
    void setTextBounding(QRectF textBounding);
    int getIconHeight() const;
    void setIconHeight(int iconHeight);
    bool getCanDeferredDelete() const;
    void setCanDeferredDelete(bool canDeferredDelete);
    QModelIndex getIndex() const;
    void setIndex(QModelIndex index);
    QStyleOptionViewItem getOption() const;
    void setOption(QStyleOptionViewItem opt);

private:
    QRectF textGeometry(int width = -1) const;
    QRectF iconGeometry() const;

private:
    QPixmap iconPixmap;
    int iconHeight { 0 };
    mutable QRectF textBounding;
    QModelIndex index;
    QStyleOptionViewItem option;
    qreal opacity { 1 };
    bool canDeferredDelete { true };
    IconItemDelegate *delegate { nullptr };
};
DPWORKSPACE_END_NAMESPACE

#endif   // EXPANDEDITEM_H
