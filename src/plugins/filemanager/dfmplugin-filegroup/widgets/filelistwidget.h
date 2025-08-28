#ifndef FILELISTWIDGET_H
#define FILELISTWIDGET_H

#include "dfmplugin_filegroup_global.h"
#include "widgets/toolbarwidget.h"

#include <QWidget>
#include <QListView>
#include <QHeaderView>
#include <QVBoxLayout>

namespace dfmplugin_filegroup {

class FileSystemModel;
class FileItemDelegate;

class FileListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FileListWidget(QWidget *parent = nullptr);

    /**
     * @brief Set the directory path to display
     * @param path The directory path
     */
    void setRootPath(const QString &path);

    /**
     * @brief Get the current root path
     * @return The current directory path
     */
    QString rootPath() const;

    /**
     * @brief Set the classification type for grouping
     * @param type The classification type
     */
    void setClassificationType(ToolbarWidget::ClassificationType type);

    /**
     * @brief Get the current classification type
     * @return The current classification type
     */
    ToolbarWidget::ClassificationType classificationType() const;

signals:
    /**
     * @brief Emitted when directory loading starts
     */
    void loadingStarted();

    /**
     * @brief Emitted when directory loading finishes
     */
    void loadingFinished();

private slots:
    void onModelLoadingStarted();
    void onModelLoadingFinished();

private:
    void setupUI();
    void setupModel();
    void updateGroupingStrategy();

private:
    QVBoxLayout *m_mainLayout;
    QListView *m_listView;
    QHeaderView *m_headerView;
    
    FileSystemModel *m_model;
    FileItemDelegate *m_delegate;
    
    ToolbarWidget::ClassificationType m_classificationType;
};

}

#endif   // FILELISTWIDGET_H
