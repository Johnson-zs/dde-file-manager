#include "filelistwidget.h"
#include "core/filesystemmodel.h"
#include "core/fileitemdelegate.h"
#include "strategies/groupingstrategy.h"

#include <QListView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDebug>

namespace dfmplugin_filegroup {

FileListWidget::FileListWidget(QWidget *parent)
    : QWidget(parent),
      m_mainLayout(nullptr),
      m_listView(nullptr),
      m_headerView(nullptr),
      m_model(nullptr),
      m_delegate(nullptr),
      m_classificationType(ToolbarWidget::ClassificationType::None)
{
    setupUI();
    setupModel();
}

void FileListWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create header view for column headers
    m_headerView = new QHeaderView(Qt::Horizontal, this);
    m_headerView->setModel(nullptr);   // Will be set when model is created
    m_headerView->setSectionResizeMode(QHeaderView::Interactive);
    m_headerView->setStretchLastSection(true);
    m_headerView->setFixedHeight(30);
    m_mainLayout->addWidget(m_headerView);

    // Create list view
    m_listView = new QListView(this);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mainLayout->addWidget(m_listView);

    setLayout(m_mainLayout);
}

void FileListWidget::setupModel()
{
    // Create model
    m_model = new FileSystemModel(this);
    
    // Create delegate
    m_delegate = new FileItemDelegate(this);
    m_delegate->setHeaderView(m_headerView);

    // Set model and delegate to views
    m_headerView->setModel(m_model);
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(m_delegate);

    // Connect HeaderView section resize signal for real-time updates
    connect(m_headerView, &QHeaderView::sectionResized,
            this, [this](int logicalIndex, int oldSize, int newSize) {
                Q_UNUSED(logicalIndex)
                Q_UNUSED(oldSize)
                Q_UNUSED(newSize)
                // Force ListView to repaint immediately during resize
                m_listView->viewport()->update();
            });

    // Connect section moved signal for column reordering
    connect(m_headerView, &QHeaderView::sectionMoved,
            this, [this](int logicalIndex, int oldVisualIndex, int newVisualIndex) {
                Q_UNUSED(logicalIndex)
                Q_UNUSED(oldVisualIndex)
                Q_UNUSED(newVisualIndex)
                // Force ListView to repaint after column reordering
                m_listView->viewport()->update();
            });

    // Connect geometry changed signal to ensure synchronization
    connect(m_headerView, &QHeaderView::geometriesChanged,
            this, [this]() {
                // Force ListView to repaint when header geometry changes
                m_listView->viewport()->update();
            });

    // Connect model signals
    connect(m_model, &FileSystemModel::loadingStarted,
            this, &FileListWidget::onModelLoadingStarted);
    connect(m_model, &FileSystemModel::loadingFinished,
            this, &FileListWidget::onModelLoadingFinished);

    fmDebug() << "FileListWidget model and delegate setup completed";
}

void FileListWidget::setRootPath(const QString &path)
{
    if (m_model) {
        fmInfo() << "Setting root path to:" << path;
        m_model->setRootPath(path);
    }
}

QString FileListWidget::rootPath() const
{
    return m_model ? m_model->rootPath() : QString();
}

void FileListWidget::setClassificationType(ToolbarWidget::ClassificationType type)
{
    if (m_classificationType != type) {
        m_classificationType = type;
        updateGroupingStrategy();
        fmInfo() << "Classification type changed to:" << static_cast<int>(type);
    }
}

ToolbarWidget::ClassificationType FileListWidget::classificationType() const
{
    return m_classificationType;
}

void FileListWidget::onModelLoadingStarted()
{
    fmDebug() << "Model loading started";
    emit loadingStarted();
}

void FileListWidget::onModelLoadingFinished()
{
    fmDebug() << "Model loading finished";
    emit loadingFinished();
}

void FileListWidget::updateGroupingStrategy()
{
    if (!m_model) {
        return;
    }

    std::unique_ptr<GroupingStrategy> strategy;

    switch (m_classificationType) {
    case ToolbarWidget::ClassificationType::None:
        strategy = std::make_unique<NoGroupingStrategy>();
        fmDebug() << "Applied NoGroupingStrategy";
        break;

    case ToolbarWidget::ClassificationType::Type:
        strategy = std::make_unique<TypeGroupingStrategy>();
        fmDebug() << "Applied TypeGroupingStrategy";
        break;

    case ToolbarWidget::ClassificationType::ModificationTime:
        strategy = std::make_unique<TimeGroupingStrategy>(TimeGroupingStrategy::ModificationTime);
        fmDebug() << "Applied TimeGroupingStrategy (ModificationTime)";
        break;

    case ToolbarWidget::ClassificationType::Size:
        strategy = std::make_unique<SizeGroupingStrategy>();
        fmDebug() << "Applied SizeGroupingStrategy";
        break;

    default:
        strategy = std::make_unique<NoGroupingStrategy>();
        fmWarning() << "Unknown classification type, falling back to NoGroupingStrategy";
        break;
    }

    if (strategy) {
        m_model->setGroupingStrategy(std::move(strategy));
    }
}

}
