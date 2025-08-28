#include "filesystemmodel.h"
#include <QFileInfo>
#include <QMimeDatabase>
#include <QDebug>
#include <QApplication>
#include <QFont>
#include <QColor>
#include <algorithm>

DPFILEGROUP_BEGIN_NAMESPACE

FileSystemModel::FileSystemModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_groupingStrategy(std::make_unique<NoGroupingStrategy>()),
      m_groupOrder(GroupingStrategy::GroupOrder::Ascending),
      m_watcher(new QFileSystemWatcher(this)),
      m_iconProvider(new QFileIconProvider())
{
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FileSystemModel::onDirectoryChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &FileSystemModel::onFileChanged);
}

FileSystemModel::~FileSystemModel()
{
    delete m_iconProvider;
}

QModelIndex FileSystemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    // This is a flat model (no parent-child relationships in the traditional sense)
    if (parent.isValid()) {
        return QModelIndex();
    }

    return createIndex(row, column, static_cast<quintptr>(row));
}

QModelIndex FileSystemModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)
    return QModelIndex();   // Flat model
}

int FileSystemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;   // Flat model
    }

    return getModelItemCount();
}

int FileSystemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return ColumnCount;
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= getModelItemCount()) {
        return QVariant();
    }

    const ModelItem &item = getModelItem(index.row());

    // Handle group headers
    if (item.type == ModelItem::GroupHeaderType) {
        switch (role) {
        case Qt::DisplayRole:
            if (index.column() == NameColumn) {
                auto groupIt = std::find_if(m_groups.begin(), m_groups.end(),
                                            [&item](const GroupInfo &group) { return group.name == item.groupName; });
                if (groupIt != m_groups.end()) {
                    return groupIt->getHeaderText();
                }
            }
            return QVariant();

        case Qt::FontRole: {
            QFont font = QApplication::font();
            font.setBold(true);
            return font;
        }

        case Qt::BackgroundRole:
            return QColor(240, 240, 240);

        case IsGroupHeaderRole:
            return true;

        case GroupNameRole:
            return item.groupName;

        case IsExpandedRole: {
            auto groupIt = std::find_if(m_groups.begin(), m_groups.end(),
                                        [&item](const GroupInfo &group) { return group.name == item.groupName; });
            return groupIt != m_groups.end() ? groupIt->isExpanded : true;
        }

        default:
            return QVariant();
        }
    }

    // Handle file items
    if (item.type == ModelItem::FileType) {
        auto groupIt = std::find_if(m_groups.begin(), m_groups.end(),
                                    [&item](const GroupInfo &group) { return group.name == item.groupName; });

        if (groupIt == m_groups.end() || item.fileIndex >= groupIt->items.size()) {
            return QVariant();
        }

        const FileItem &fileItem = groupIt->items[item.fileIndex];

        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case NameColumn:
                return fileItem.name;
            case ModifiedColumn:
                return fileItem.getFormattedModifiedTime();
            case SizeColumn:
                return fileItem.getFormattedSize();
            case TypeColumn:
                return fileItem.getTypeString();
            }
            break;

        case Qt::DecorationRole:
            if (index.column() == NameColumn) {
                return fileItem.icon;
            }
            break;

        case Qt::ToolTipRole:
            return QString("%1\nSize: %2\nModified: %3")
                    .arg(fileItem.path)
                    .arg(fileItem.getFormattedSize())
                    .arg(fileItem.dateModified.toString());

        case FileItemRole:
            return QVariant::fromValue(fileItem);

        case IsGroupHeaderRole:
            return false;

        default:
            break;
        }
    }

    return QVariant();
}

QVariant FileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (section) {
    case NameColumn:
        return tr("Name");
    case ModifiedColumn:
        return tr("Date Modified");
    case SizeColumn:
        return tr("Size");
    case TypeColumn:
        return tr("Type");
    default:
        return QVariant();
    }
}

Qt::ItemFlags FileSystemModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    const ModelItem &item = getModelItem(index.row());

    if (item.type == ModelItem::GroupHeaderType) {
        return Qt::ItemIsEnabled;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void FileSystemModel::setRootPath(const QString &path)
{
    if (m_rootPath == path) {
        return;
    }

    beginResetModel();

    // Stop watching the old path
    if (!m_rootPath.isEmpty()) {
        m_watcher->removePath(m_rootPath);
    }

    m_rootPath = path;

    // Start watching the new path
    if (!m_rootPath.isEmpty()) {
        m_watcher->addPath(m_rootPath);
    }

    loadDirectory();
    endResetModel();

    emit directoryLoaded(m_rootPath);
}

void FileSystemModel::refreshData()
{
    beginResetModel();
    loadDirectory();
    endResetModel();
}

void FileSystemModel::setGroupingStrategy(std::unique_ptr<GroupingStrategy> strategy)
{
    if (!strategy) {
        return;
    }

    beginResetModel();
    m_groupingStrategy = std::move(strategy);
    organizeFiles();
    endResetModel();
}

void FileSystemModel::setGroupingStrategy(std::unique_ptr<GroupingStrategy> strategy, GroupingStrategy::GroupOrder groupOrder)
{
    if (!strategy) {
        return;
    }

    beginResetModel();
    m_groupingStrategy = std::move(strategy);
    m_groupOrder = groupOrder;
    organizeFiles();
    endResetModel();
}

void FileSystemModel::setGroupOrder(GroupingStrategy::GroupOrder groupOrder)
{
    if (m_groupOrder != groupOrder) {
        beginResetModel();
        m_groupOrder = groupOrder;
        organizeFiles();
        endResetModel();
    }
}

void FileSystemModel::toggleGroupExpansion(const QString &groupName)
{
    auto groupIt = std::find_if(m_groups.begin(), m_groups.end(),
                                [&groupName](GroupInfo &group) { return group.name == groupName; });

    if (groupIt != m_groups.end()) {
        groupIt->isExpanded = !groupIt->isExpanded;
        beginResetModel();
        rebuildModelItems();
        endResetModel();
    }
}

bool FileSystemModel::isGroupExpanded(const QString &groupName) const
{
    auto groupIt = std::find_if(m_groups.begin(), m_groups.end(),
                                [&groupName](const GroupInfo &group) { return group.name == groupName; });

    return groupIt != m_groups.end() ? groupIt->isExpanded : true;
}

FileItem FileSystemModel::getFileItem(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return FileItem();
    }

    QVariant variant = data(index, FileItemRole);
    if (variant.canConvert<FileItem>()) {
        return variant.value<FileItem>();
    }

    return FileItem();
}

bool FileSystemModel::isGroupHeader(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    return data(index, IsGroupHeaderRole).toBool();
}

QString FileSystemModel::getGroupName(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QString();
    }

    return data(index, GroupNameRole).toString();
}

void FileSystemModel::onDirectoryChanged(const QString &path)
{
    if (path == m_rootPath) {
        refreshData();
    }
}

void FileSystemModel::onFileChanged(const QString &path)
{
    Q_UNUSED(path)
    // For now, just refresh the entire directory
    // In the future, we could implement more granular updates
    refreshData();
}

void FileSystemModel::loadDirectory()
{
    emit loadingStarted();

    m_groups.clear();
    m_modelItems.clear();

    if (m_rootPath.isEmpty()) {
        emit loadingFinished();
        return;
    }

    QDir dir(m_rootPath);
    if (!dir.exists()) {
        qWarning() << "Directory does not exist:" << m_rootPath;
        emit loadingFinished();
        return;
    }

    // Get all entries in the directory
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::NoSort);

    QList<FileItem> files;
    files.reserve(entries.size());

    for (const QFileInfo &info : entries) {
        FileItem item = createFileItem(info);
        files.append(item);
    }

    applyGrouping(files);
    rebuildModelItems();

    emit loadingFinished();
}

void FileSystemModel::organizeFiles()
{
    // Collect all files from existing groups
    QList<FileItem> allFiles;
    for (const GroupInfo &group : m_groups) {
        allFiles.append(group.items);
    }

    if (!allFiles.isEmpty()) {
        applyGrouping(allFiles);
        rebuildModelItems();
    }
}

void FileSystemModel::rebuildModelItems()
{
    m_modelItems.clear();

    for (const GroupInfo &group : m_groups) {
        // Add group header if we have grouping strategy that creates groups
        if (!group.name.isEmpty()) {
            m_modelItems.append(ModelItem(ModelItem::GroupHeaderType, group.name));
        }

        // Add file items if group is expanded
        if (group.isExpanded) {
            for (int i = 0; i < group.items.size(); ++i) {
                m_modelItems.append(ModelItem(ModelItem::FileType, group.name, i));
            }
        }
    }
}

void FileSystemModel::applyGrouping(const QList<FileItem> &files)
{
    m_groups.clear();

    if (!m_groupingStrategy) {
        // No grouping - put all files in one group without group name
        GroupInfo group { QString() };
        for (const FileItem &file : files) {
            group.addItem(file);
        }
        if (!group.isEmpty()) {
            m_groups.append(group);
        }
        return;
    }

    // Check if this is NoGroupingStrategy
    if (m_groupingStrategy->getStrategyName().toLower() == "none") {
        // For NoGroupingStrategy, create a single group with empty name (no header)
        GroupInfo group { QString() };
        for (const FileItem &file : files) {
            group.addItem(file);
        }
        if (!group.isEmpty()) {
            m_groups.append(group);
        }
        return;
    }

    // Group files according to strategy
    QMap<QString, GroupInfo> groupMap;

    for (const FileItem &file : files) {
        QString groupName = m_groupingStrategy->getGroupName(file);
        if (groupName.isEmpty()) {
            groupName = "Other";   // Fallback group name
        }

        if (!groupMap.contains(groupName)) {
            int order = m_groupingStrategy->getGroupDisplayOrder(groupName, m_groupOrder);
            groupMap[groupName] = GroupInfo(groupName, order);
        }

        groupMap[groupName].addItem(file);
    }

    // Convert to list and sort by display order
    for (auto it = groupMap.begin(); it != groupMap.end(); ++it) {
        if (m_groupingStrategy->isGroupVisible(it.key(), it.value().items)) {
            m_groups.append(it.value());
        }
    }

    std::sort(m_groups.begin(), m_groups.end(),
              [](const GroupInfo &a, const GroupInfo &b) {
                  return a.displayOrder < b.displayOrder;
              });
}

FileItem FileSystemModel::createFileItem(const QFileInfo &fileInfo) const
{
    FileItem item;
    item.name = fileInfo.fileName();
    item.path = fileInfo.absoluteFilePath();
    item.size = fileInfo.isDir() ? -1 : fileInfo.size();
    item.dateModified = fileInfo.lastModified();
    item.dateCreated = fileInfo.birthTime();
    item.isDirectory = fileInfo.isDir();
    item.icon = m_iconProvider->icon(fileInfo);

    if (fileInfo.isDir()) {
        item.type = "Directory";
    } else {
        // Use MIME database for better type detection
        QMimeDatabase mimeDb;
        QMimeType mimeType = mimeDb.mimeTypeForFile(fileInfo);

        if (mimeType.isValid()) {
            item.type = mimeType.name();
        } else {
            // Fallback to extension
            item.type = fileInfo.suffix().toLower();
            if (item.type.isEmpty()) {
                item.type = "application/octet-stream";
            }
        }
    }

    return item;
}

int FileSystemModel::getModelItemCount() const
{
    return m_modelItems.size();
}

const FileSystemModel::ModelItem &FileSystemModel::getModelItem(int index) const
{
    static const ModelItem invalidItem(ModelItem::FileType);
    if (index < 0 || index >= m_modelItems.size()) {
        return invalidItem;
    }
    return m_modelItems[index];
}

DPFILEGROUP_END_NAMESPACE
