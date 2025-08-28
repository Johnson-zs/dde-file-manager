#ifndef FILESYSTEMMODEL_H
#define FILESYSTEMMODEL_H

#include "core/fileitem.h"
#include "core/groupinfo.h"
#include "strategies/groupingstrategy.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFileSystemWatcher>
#include <QFileIconProvider>
#include <memory>

DPFILEGROUP_BEGIN_NAMESPACE
/**
 * @brief Core data model for the file system browser
 *
 * This model manages file system data with support for grouping and sorting.
 * It follows the Qt Model/View architecture and provides data to QListView.
 */
class FileSystemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    /**
     * @brief Column enumeration for the model
     */
    enum Column {
        NameColumn = 0,
        ModifiedColumn = 1,
        SizeColumn = 2,
        TypeColumn = 3,
        ColumnCount = 4
    };

    /**
     * @brief Custom roles for data access
     */
    enum CustomRoles {
        FileItemRole = Qt::UserRole + 1,
        IsGroupHeaderRole,
        GroupNameRole,
        IsExpandedRole
    };

    explicit FileSystemModel(QObject *parent = nullptr);
    ~FileSystemModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // File system operations
    void setRootPath(const QString &path);
    QString rootPath() const { return m_rootPath; }
    void refreshData();

    // Grouping and sorting
    void setGroupingStrategy(std::unique_ptr<GroupingStrategy> strategy);
    void setGroupingStrategy(std::unique_ptr<GroupingStrategy> strategy, GroupingStrategy::GroupOrder groupOrder);
    void setGroupOrder(GroupingStrategy::GroupOrder groupOrder);

    GroupingStrategy *groupingStrategy() const { return m_groupingStrategy.get(); }
    GroupingStrategy::GroupOrder groupOrder() const { return m_groupOrder; }

    // Group operations
    void toggleGroupExpansion(const QString &groupName);
    bool isGroupExpanded(const QString &groupName) const;

    // Utility methods
    FileItem getFileItem(const QModelIndex &index) const;
    bool isGroupHeader(const QModelIndex &index) const;
    QString getGroupName(const QModelIndex &index) const;

signals:
    void directoryLoaded(const QString &path);
    void loadingStarted();
    void loadingFinished();

private slots:
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);

private:
    // Internal data structures
    struct ModelItem
    {
        enum Type { FileType,
                    GroupHeaderType };
        Type type;
        QString groupName;
        int fileIndex;   // Index in the group's file list

        ModelItem(Type t, const QString &group = QString(), int index = -1)
            : type(t), groupName(group), fileIndex(index) { }
    };

    // Core data
    QString m_rootPath;
    QList<GroupInfo> m_groups;
    QList<ModelItem> m_modelItems;   // Flattened view for the model

    // Strategies
    std::unique_ptr<GroupingStrategy> m_groupingStrategy;
    GroupingStrategy::GroupOrder m_groupOrder;

    // File system monitoring
    QFileSystemWatcher *m_watcher;
    QFileIconProvider *m_iconProvider;

    // Private methods
    void loadDirectory();
    void organizeFiles();
    void rebuildModelItems();
    void applyGrouping(const QList<FileItem> &files);
    FileItem createFileItem(const QFileInfo &fileInfo) const;
    int getModelItemCount() const;
    const ModelItem &getModelItem(int index) const;
};

DPFILEGROUP_END_NAMESPACE
#endif   // FILESYSTEMMODEL_H
