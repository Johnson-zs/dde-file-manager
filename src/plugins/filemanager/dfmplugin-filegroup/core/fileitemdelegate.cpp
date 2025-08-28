#include "fileitemdelegate.h"
#include "core/filesystemmodel.h"
#include "core/fileitem.h"
#include "core/groupinfo.h"

#include <QPainter>
#include <QApplication>
#include <QMouseEvent>
#include <QHeaderView>

DPFILEGROUP_BEGIN_NAMESPACE

FileItemDelegate::FileItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent), m_groupHeaderHeight(32), m_fileItemHeight(24), m_headerView(nullptr)
{
}

FileItemDelegate::~FileItemDelegate() = default;

void FileItemDelegate::setHeaderView(QHeaderView *headerView)
{
    m_headerView = headerView;
}

void FileItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return;
    }

    // Check if this is a group header
    bool isGroupHeader = index.data(FileSystemModel::IsGroupHeaderRole).toBool();

    if (isGroupHeader) {
        paintGroupHeader(painter, option, index);
    } else {
        paintFileItem(painter, option, index);
    }
}

QSize FileItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)

    if (!index.isValid()) {
        return QSize(0, 0);
    }

    bool isGroupHeader = index.data(FileSystemModel::IsGroupHeaderRole).toBool();

    if (isGroupHeader) {
        return QSize(option.rect.width(), m_groupHeaderHeight);
    } else {
        return QSize(option.rect.width(), m_fileItemHeight);
    }
}

bool FileItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

        if (mouseEvent->button() == Qt::LeftButton) {
            bool isGroupHeader = index.data(FileSystemModel::IsGroupHeaderRole).toBool();

            if (isGroupHeader) {
                if (isPointInExpandCollapseButton(mouseEvent->pos(), option)) {
                    // Toggle group expansion
                    FileSystemModel *fsModel = qobject_cast<FileSystemModel *>(model);
                    if (fsModel) {
                        QString groupName = index.data(FileSystemModel::GroupNameRole).toString();
                        fsModel->toggleGroupExpansion(groupName);
                        return true;
                    }
                } else {
                    // Click on group header text - this will be handled by FileListView
                    // for group selection functionality
                    return false;   // Let the view handle it
                }
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void FileItemDelegate::paintGroupHeader(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    // Draw background
    QColor bgColor = QColor(240, 240, 240);
    if (option.state & QStyle::State_MouseOver) {
        bgColor = bgColor.darker(110);
    }
    painter->fillRect(option.rect, bgColor);

    // Draw border
    painter->setPen(QColor(200, 200, 200));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

    // Get group information
    QString headerText = index.data(Qt::DisplayRole).toString();
    bool isExpanded = index.data(FileSystemModel::IsExpandedRole).toBool();

    // Draw expand/collapse button
    QRect buttonRect = getExpandCollapseButtonRect(option);
    painter->setPen(QColor(100, 100, 100));
    painter->setBrush(QColor(100, 100, 100));

    // Draw triangle
    QPolygon triangle;
    if (isExpanded) {
        // Down arrow
        triangle << QPoint(buttonRect.left() + 3, buttonRect.top() + 5)
                 << QPoint(buttonRect.right() - 3, buttonRect.top() + 5)
                 << QPoint(buttonRect.center().x(), buttonRect.bottom() - 3);
    } else {
        // Right arrow
        triangle << QPoint(buttonRect.left() + 3, buttonRect.top() + 3)
                 << QPoint(buttonRect.right() - 5, buttonRect.center().y())
                 << QPoint(buttonRect.left() + 3, buttonRect.bottom() - 3);
    }
    painter->drawPolygon(triangle);

    // Draw group text
    QRect textRect = option.rect.adjusted(buttonRect.width() + 8, 0, -8, 0);
    QFont font = painter->font();
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, headerText);

    painter->restore();
}

void FileItemDelegate::paintFileItem(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    // Draw selection background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
        painter->setPen(option.palette.color(QPalette::HighlightedText));
    } else {
        painter->setPen(option.palette.color(QPalette::Text));
    }

    // Get file item data
    FileItem item = index.data(FileSystemModel::FileItemRole).value<FileItem>();

    // Calculate column widths from header view
    int nameWidth, modifiedWidth, sizeWidth, typeWidth;
    calculateColumnWidths(option, nameWidth, modifiedWidth, sizeWidth, typeWidth);

    int currentX = option.rect.left();

    // Draw Name column (with icon)
    QRect nameRect(currentX, option.rect.top(), nameWidth, option.rect.height());
    QRect iconRect(nameRect.left() + 4, nameRect.top() + 2, 16, 16);
    QRect nameTextRect(iconRect.right() + 4, nameRect.top(), nameRect.width() - iconRect.width() - 8, nameRect.height());

    // Draw icon
    if (!item.icon.isNull()) {
        item.icon.paint(painter, iconRect);
    }

    // Draw name text
    painter->drawText(nameTextRect, Qt::AlignLeft | Qt::AlignVCenter, item.name);
    currentX += nameWidth;

    // Draw Modified column
    QRect modifiedRect(currentX, option.rect.top(), modifiedWidth, option.rect.height());
    QString modifiedText = item.getFormattedModifiedTime();
    painter->drawText(modifiedRect.adjusted(8, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, modifiedText);
    currentX += modifiedWidth;

    // Draw Size column
    QRect sizeRect(currentX, option.rect.top(), sizeWidth, option.rect.height());
    QString sizeText = item.getFormattedSize();
    painter->drawText(sizeRect.adjusted(8, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter, sizeText);
    currentX += sizeWidth;

    // Draw Type column
    QRect typeRect(currentX, option.rect.top(), typeWidth, option.rect.height());
    QString typeText = item.getTypeString();
    painter->drawText(typeRect.adjusted(8, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, typeText);

    painter->restore();
}

QRect FileItemDelegate::getExpandCollapseButtonRect(const QStyleOptionViewItem &option) const
{
    return QRect(option.rect.left() + 8, option.rect.top() + 8, 16, 16);
}

bool FileItemDelegate::isPointInExpandCollapseButton(const QPoint &point, const QStyleOptionViewItem &option) const
{
    QRect buttonRect = getExpandCollapseButtonRect(option);
    return buttonRect.contains(point);
}

void FileItemDelegate::calculateColumnWidths(const QStyleOptionViewItem &option, int &nameWidth, int &modifiedWidth, int &sizeWidth, int &typeWidth) const
{
    if (m_headerView) {
        // Use actual header section widths
        nameWidth = m_headerView->sectionSize(0);
        modifiedWidth = m_headerView->sectionSize(1);
        sizeWidth = m_headerView->sectionSize(2);
        typeWidth = m_headerView->sectionSize(3);
    } else {
        // Fallback to proportional widths
        int totalWidth = option.rect.width();
        nameWidth = totalWidth * 0.4;
        modifiedWidth = totalWidth * 0.25;
        sizeWidth = totalWidth * 0.15;
        typeWidth = totalWidth * 0.2;

        // Ensure minimum widths
        nameWidth = qMax(nameWidth, 150);
        modifiedWidth = qMax(modifiedWidth, 100);
        sizeWidth = qMax(sizeWidth, 80);
        typeWidth = qMax(typeWidth, 80);
    }
}

DPFILEGROUP_END_NAMESPACE
