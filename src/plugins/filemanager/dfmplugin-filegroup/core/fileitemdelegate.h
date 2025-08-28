#ifndef FILEITEMDELEGATE_H
#define FILEITEMDELEGATE_H

#include "dfmplugin_filegroup_global.h"

#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>

class QHeaderView;

DPFILEGROUP_BEGIN_NAMESPACE

/**
 * @brief Custom delegate for rendering file items and group headers
 *
 * This delegate handles the custom painting of both group headers and individual
 * file items in a 4-column layout, with proper alignment and styling.
 */
class FileItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit FileItemDelegate(QObject *parent = nullptr);
    ~FileItemDelegate() override;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

    // Set the header view to synchronize column widths
    void setHeaderView(QHeaderView *headerView);

private:
    void paintGroupHeader(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void paintFileItem(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QRect getExpandCollapseButtonRect(const QStyleOptionViewItem &option) const;
    bool isPointInExpandCollapseButton(const QPoint &point, const QStyleOptionViewItem &option) const;
    void calculateColumnWidths(const QStyleOptionViewItem &option, int &nameWidth, int &modifiedWidth, int &sizeWidth, int &typeWidth) const;

private:
    mutable int m_groupHeaderHeight;
    mutable int m_fileItemHeight;
    QHeaderView *m_headerView;
};

DPFILEGROUP_END_NAMESPACE

#endif   // FILEITEMDELEGATE_H
