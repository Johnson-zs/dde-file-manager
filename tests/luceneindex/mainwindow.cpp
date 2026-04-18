// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QTreeWidget>
#include <QLabel>
#include <QFileDialog>
#include <QHeaderView>
#include <QDateTime>
#include <QMessageBox>
#include <QApplication>
#include <QtConcurrent>

// AnalyzerTab implementation

AnalyzerTab::AnalyzerTab(const QString& analyzerId, QWidget* parent)
    : QWidget(parent)
    , m_analyzerId(analyzerId)
{
    m_indexManager = new IndexManager(analyzerId, this);

    connect(m_indexManager, &IndexManager::progressChanged,
            this, &AnalyzerTab::onProgressChanged);
    connect(m_indexManager, &IndexManager::indexingFinished,
            this, &AnalyzerTab::onIndexingFinished);

    setupUI();

    // Load saved config
    if (m_indexManager->loadConfig()) {
        m_sourcePathEdit->setText(m_indexManager->sourceDirectory());
    }

    updateStats();
    updateButtonStates();
}

AnalyzerTab::~AnalyzerTab()
{
}

void AnalyzerTab::setupUI()
{
    AnalyzerInfo info = AnalyzerFactory::instance()->getAnalyzerInfo(m_analyzerId);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Header: Analyzer info
    QLabel* headerLabel = new QLabel(
        QString("<b>%1</b><br/><small>%2</small>")
        .arg(info.displayName)
        .arg(info.description)
    );
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);

    // Source directory selection
    QHBoxLayout* sourceLayout = new QHBoxLayout();
    m_sourcePathEdit = new QLineEdit();
    m_sourcePathEdit->setPlaceholderText(tr("Select source directory to index..."));
    m_sourcePathEdit->setReadOnly(true);

    m_browseBtn = new QPushButton(tr("Browse"));
    connect(m_browseBtn, &QPushButton::clicked, this, &AnalyzerTab::onSelectSourceDirectory);

    sourceLayout->addWidget(new QLabel(tr("Source:")));
    sourceLayout->addWidget(m_sourcePathEdit);
    sourceLayout->addWidget(m_browseBtn);
    mainLayout->addLayout(sourceLayout);

    // Index operations
    QHBoxLayout* indexOpsLayout = new QHBoxLayout();
    m_createIndexBtn = new QPushButton(tr("Create Index"));
    m_deleteIndexBtn = new QPushButton(tr("Delete Index"));

    m_createIndexBtn->setMinimumWidth(120);
    m_deleteIndexBtn->setMinimumWidth(120);

    connect(m_createIndexBtn, &QPushButton::clicked, this, &AnalyzerTab::onCreateIndex);
    connect(m_deleteIndexBtn, &QPushButton::clicked, this, &AnalyzerTab::onDeleteIndex);

    m_progressBar = new QProgressBar();
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("%v / %m files"));
    m_progressBar->hide();

    indexOpsLayout->addWidget(m_createIndexBtn);
    indexOpsLayout->addWidget(m_deleteIndexBtn);
    indexOpsLayout->addStretch();
    mainLayout->addLayout(indexOpsLayout);
    mainLayout->addWidget(m_progressBar);

    // Stats
    m_statsLabel = new QLabel();
    m_statsLabel->setWordWrap(true);
    mainLayout->addWidget(m_statsLabel);

    // Search section
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Enter search query..."));
    m_searchEdit->setClearButtonEnabled(true);

    m_searchBtn = new QPushButton(tr("Search"));
    m_searchBtn->setMinimumWidth(100);

    connect(m_searchBtn, &QPushButton::clicked, this, &AnalyzerTab::onSearch);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &AnalyzerTab::onSearch);

    searchLayout->addWidget(new QLabel(tr("Search:")));
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_searchBtn);
    mainLayout->addLayout(searchLayout);

    // Search time label
    m_timeLabel = new QLabel();
    mainLayout->addWidget(m_timeLabel);

    // Results tree
    m_resultsTree = new QTreeWidget();
    m_resultsTree->setHeaderLabels({ tr("File"), tr("Path"), tr("Modified"), tr("Score") });
    m_resultsTree->setRootIsDecorated(false);
    m_resultsTree->setAlternatingRowColors(true);
    m_resultsTree->setSortingEnabled(true);
    m_resultsTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_resultsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultsTree->setMinimumHeight(200);

    mainLayout->addWidget(m_resultsTree, 1);

    // Log area
    m_logEdit = new QTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(100);
    m_logEdit->setPlaceholderText(tr("Log output..."));
    mainLayout->addWidget(m_logEdit);
}

void AnalyzerTab::onSelectSourceDirectory()
{
    QString path = QFileDialog::getExistingDirectory(
        this,
        tr("Select Source Directory"),
        m_sourcePathEdit->text().isEmpty() ? QDir::homePath() : m_sourcePathEdit->text()
    );

    if (!path.isEmpty()) {
        m_sourcePathEdit->setText(path);
        m_indexManager->setSourceDirectory(path);
        updateButtonStates();
    }
}

void AnalyzerTab::onCreateIndex()
{
    if (m_sourcePathEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please select a source directory first."));
        return;
    }

    if (m_indexManager->indexExists()) {
        auto reply = QMessageBox::question(
            this,
            tr("Index Exists"),
            tr("An index already exists. Do you want to rebuild it?"),
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply != QMessageBox::Yes)
            return;
    }

    m_isIndexing = true;
    updateButtonStates();

    m_progressBar->show();
    m_progressBar->setRange(0, 0);  // Indeterminate initially
    m_logEdit->append(QString("[%1] Starting index creation...")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    m_indexManager->setSourceDirectory(m_sourcePathEdit->text());

    // Run in background thread
    QtConcurrent::run([this]() {
        m_indexManager->createIndex();
    });
}

void AnalyzerTab::onDeleteIndex()
{
    auto reply = QMessageBox::question(
        this,
        tr("Confirm Delete"),
        tr("Are you sure you want to delete this index?"),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes)
        return;

    if (m_indexManager->deleteIndex()) {
        m_logEdit->append(QString("[%1] Index deleted successfully.")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
        updateStats();
        updateButtonStates();
    } else {
        m_logEdit->append(QString("[%1] Failed to delete index.")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    }
}

void AnalyzerTab::onSearch()
{
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty())
        return;

    if (!m_indexManager->indexExists()) {
        QMessageBox::warning(this, tr("Warning"), tr("No index exists. Please create an index first."));
        return;
    }

    m_logEdit->append(QString("[%1] Searching for: %2")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                      .arg(query));

    m_resultsTree->clear();

    QList<SearchResult> results = m_indexManager->search(query);

    for (const SearchResult& result : results) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_resultsTree);
        item->setText(0, result.filename);
        item->setText(1, result.filePath);
        item->setText(2, QDateTime::fromSecsSinceEpoch(result.modifiedTime.toLongLong())
                      .toString("yyyy-MM-dd hh:mm"));
        item->setText(3, QString::number(result.score, 'f', 2));
        item->setData(0, Qt::UserRole, result.filePath);
    }

    m_resultsTree->resizeColumnToContents(0);
    m_resultsTree->resizeColumnToContents(2);
    m_resultsTree->resizeColumnToContents(3);

    m_timeLabel->setText(QString(tr("Found %1 results in %2ms"))
                         .arg(results.size())
                         .arg(m_indexManager->lastSearchTimeMs()));

    m_logEdit->append(QString("[%1] Search completed: %2 results in %3ms")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                      .arg(results.size())
                      .arg(m_indexManager->lastSearchTimeMs()));
}

void AnalyzerTab::onProgressChanged(qint64 processed, qint64 total)
{
    m_progressBar->setRange(0, static_cast<int>(total));
    m_progressBar->setValue(static_cast<int>(processed));
}

void AnalyzerTab::onIndexingFinished(bool success, const QString& message)
{
    m_isIndexing = false;
    m_progressBar->hide();
    updateButtonStates();

    if (success) {
        // Save config after successful indexing
        m_indexManager->saveConfig();
        m_logEdit->append(QString("[%1] Success: %2")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                          .arg(message));
    } else {
        m_logEdit->append(QString("[%1] Error: %2")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                          .arg(message));
    }

    updateStats();
}

void AnalyzerTab::updateStats()
{
    IndexStats stats = m_indexManager->getIndexStats();

    if (stats.exists) {
        QString sizeStr = formatSize(stats.indexSizeBytes);
        m_statsLabel->setText(QString(tr("Index: %1 documents, Size: %2 | Location: %3"))
                              .arg(stats.totalDocuments)
                              .arg(sizeStr)
                              .arg(m_indexManager->indexDirectory()));
    } else {
        m_statsLabel->setText(QString(tr("No index exists | Location: %1"))
                              .arg(m_indexManager->indexDirectory()));
    }
}

void AnalyzerTab::updateButtonStates()
{
    bool hasSource = !m_sourcePathEdit->text().isEmpty();
    bool hasIndex = m_indexManager->indexExists();

    m_createIndexBtn->setEnabled(hasSource && !m_isIndexing);
    m_deleteIndexBtn->setEnabled(hasIndex && !m_isIndexing);
    m_searchBtn->setEnabled(hasIndex && !m_isIndexing);
    m_searchEdit->setEnabled(hasIndex && !m_isIndexing);
    m_browseBtn->setEnabled(!m_isIndexing);
}

QString AnalyzerTab::formatSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    else if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    else if (bytes < 1024 * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    else
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

// MainWindow implementation

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    refreshAnalyzers();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    setWindowTitle(tr("Lucene Index Performance Test"));
    resize(900, 700);

    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);

    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(tr("Lucene Index Performance Test"));
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    m_refreshBtn = new QPushButton(tr("Refresh Analyzers"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshAnalyzers);
    headerLayout->addWidget(m_refreshBtn);

    layout->addLayout(headerLayout);

    // Tab widget for each analyzer
    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(false);
    layout->addWidget(m_tabWidget);

    // Info label
    QLabel* infoLabel = new QLabel(
        tr("Each tab represents a different analyzer. Create indexes and search to compare performance.")
    );
    infoLabel->setStyleSheet("color: gray; font-style: italic;");
    layout->addWidget(infoLabel);

    setCentralWidget(centralWidget);
}

void MainWindow::refreshAnalyzers()
{
    // Clear existing tabs
    m_tabWidget->clear();
    qDeleteAll(m_tabs);
    m_tabs.clear();

    // Create tabs for each registered analyzer
    QStringList analyzers = AnalyzerFactory::instance()->registeredAnalyzers();

    for (const QString& analyzerId : analyzers) {
        AnalyzerInfo info = AnalyzerFactory::instance()->getAnalyzerInfo(analyzerId);

        AnalyzerTab* tab = new AnalyzerTab(analyzerId);
        m_tabs[analyzerId] = tab;
        m_tabWidget->addTab(tab, info.displayName);
    }
}
