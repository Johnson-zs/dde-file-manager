// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexstatisticsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QProgressBar>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QSet>
#include <QMap>
#include <QApplication>
#include <algorithm>

#include <lucene++/LuceneHeaders.h>
#include <lucene++/TermFreqVector.h>

using namespace Lucene;

IndexStatisticsDialog::IndexStatisticsDialog(IndexManager* manager, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setupUI();
    refreshStats();
}

IndexStatisticsDialog::~IndexStatisticsDialog()
{
}

void IndexStatisticsDialog::setupUI()
{
    setWindowTitle(tr("Index Statistics - %1").arg(m_manager->analyzerId()));
    resize(900, 700);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Tab widget
    QTabWidget* tabWidget = new QTabWidget();
    mainLayout->addWidget(tabWidget);

    // === Overview Tab ===
    QWidget* overviewTab = new QWidget();
    QVBoxLayout* overviewLayout = new QVBoxLayout(overviewTab);

    m_overviewLabel = new QLabel();
    m_overviewLabel->setWordWrap(true);
    m_overviewLabel->setTextFormat(Qt::RichText);
    overviewLayout->addWidget(m_overviewLabel);

    overviewLayout->addStretch();
    tabWidget->addTab(overviewTab, tr("Overview"));

    // === Fields Tab ===
    QWidget* fieldsTab = new QWidget();
    QVBoxLayout* fieldsLayout = new QVBoxLayout(fieldsTab);

    m_fieldsTable = new QTableWidget();
    m_fieldsTable->setColumnCount(6);
    m_fieldsTable->setHorizontalHeaderLabels({
        tr("Field Name"), tr("Indexed"), tr("Stored"), tr("Tokenized"),
        tr("Term Count"), tr("Doc Count")
    });
    m_fieldsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fieldsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fieldsTable->horizontalHeader()->setStretchLastSection(true);
    m_fieldsTable->verticalHeader()->setVisible(false);
    fieldsLayout->addWidget(m_fieldsTable);

    tabWidget->addTab(fieldsTab, tr("Fields"));

    // === Terms Tab ===
    QWidget* termsTab = new QWidget();
    QVBoxLayout* termsLayout = new QVBoxLayout(termsTab);

    // Term field selection
    QHBoxLayout* termSearchLayout = new QHBoxLayout();
    termSearchLayout->addWidget(new QLabel(tr("Field:")));
    m_termFieldCombo = new QComboBox();
    termSearchLayout->addWidget(m_termFieldCombo);

    QPushButton* refreshTermsBtn = new QPushButton(tr("Load Terms"));
    refreshTermsBtn->setToolTip(tr("Click to load terms (may take time for large indexes)"));
    termSearchLayout->addWidget(refreshTermsBtn);
    connect(refreshTermsBtn, &QPushButton::clicked, this, &IndexStatisticsDialog::onTermSearch);

    termSearchLayout->addStretch();
    termsLayout->addLayout(termSearchLayout);

    m_termsStatsLabel = new QLabel(tr("Select field and click 'Load Terms' to view term statistics"));
    termsLayout->addWidget(m_termsStatsLabel);

    m_termsTable = new QTableWidget();
    m_termsTable->setColumnCount(2);
    m_termsTable->setHorizontalHeaderLabels({ tr("Term"), tr("Doc Count") });
    m_termsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_termsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_termsTable->horizontalHeader()->setStretchLastSection(true);
    m_termsTable->verticalHeader()->setVisible(false);
    m_termsTable->setToolTip(tr("Double-click a term to view related documents"));
    connect(m_termsTable, &QTableWidget::cellDoubleClicked,
            this, &IndexStatisticsDialog::onTermDoubleClicked);
    termsLayout->addWidget(m_termsTable);

    tabWidget->addTab(termsTab, tr("Terms"));

    // === Documents Tab ===
    QWidget* docsTab = new QWidget();
    QVBoxLayout* docsLayout = new QVBoxLayout(docsTab);

    QHBoxLayout* docSearchLayout = new QHBoxLayout();
    docSearchLayout->addWidget(new QLabel(tr("Filter (path):")));
    m_docFilterEdit = new QLineEdit();
    m_docFilterEdit->setPlaceholderText(tr("Enter path filter..."));
    connect(m_docFilterEdit, &QLineEdit::textChanged,
            this, &IndexStatisticsDialog::onDocumentFilterChanged);
    docSearchLayout->addWidget(m_docFilterEdit);
    docsLayout->addLayout(docSearchLayout);

    m_docCountLabel = new QLabel();
    docsLayout->addWidget(m_docCountLabel);

    m_documentsTable = new QTableWidget();
    m_documentsTable->setColumnCount(4);
    m_documentsTable->setHorizontalHeaderLabels({
        tr("Path"), tr("Filename"), tr("Modified"), tr("Hidden")
    });
    m_documentsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_documentsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_documentsTable->horizontalHeader()->setStretchLastSection(true);
    m_documentsTable->verticalHeader()->setVisible(false);
    connect(m_documentsTable, &QTableWidget::cellDoubleClicked,
            this, &IndexStatisticsDialog::onDocumentSelected);
    docsLayout->addWidget(m_documentsTable);

    QLabel* hintLabel = new QLabel(tr("Double-click a document to view details"));
    hintLabel->setStyleSheet("color: gray; font-style: italic;");
    docsLayout->addWidget(hintLabel);

    tabWidget->addTab(docsTab, tr("Documents"));

    // === Top Terms Tab ===
    QWidget* topTermsTab = new QWidget();
    QVBoxLayout* topTermsLayout = new QVBoxLayout(topTermsTab);

    QHBoxLayout* topTermsCtrlLayout = new QHBoxLayout();
    topTermsCtrlLayout->addWidget(new QLabel(tr("Field:")));
    m_topTermsFieldCombo = new QComboBox();
    topTermsCtrlLayout->addWidget(m_topTermsFieldCombo);

    topTermsCtrlLayout->addWidget(new QLabel(tr("Top N:")));
    m_topTermsCountSpin = new QSpinBox();
    m_topTermsCountSpin->setRange(10, 1000);
    m_topTermsCountSpin->setValue(50);
    topTermsCtrlLayout->addWidget(m_topTermsCountSpin);

    QPushButton* topTermsBtn = new QPushButton(tr("Show"));
    topTermsCtrlLayout->addWidget(topTermsBtn);
    connect(topTermsBtn, &QPushButton::clicked, this, [this]() {
        loadTopTermsData();
    });

    topTermsCtrlLayout->addStretch();
    topTermsLayout->addLayout(topTermsCtrlLayout);

    m_topTermsTable = new QTableWidget();
    m_topTermsTable->setColumnCount(3);
    m_topTermsTable->setHorizontalHeaderLabels({ tr("Rank"), tr("Term"), tr("Frequency") });
    m_topTermsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_topTermsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_topTermsTable->horizontalHeader()->setStretchLastSection(true);
    m_topTermsTable->verticalHeader()->setVisible(false);
    topTermsLayout->addWidget(m_topTermsTable);

    tabWidget->addTab(topTermsTab, tr("Top Terms"));

    // Refresh button at bottom
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    QPushButton* refreshBtn = new QPushButton(tr("Refresh"));
    connect(refreshBtn, &QPushButton::clicked, this, &IndexStatisticsDialog::refreshStats);
    bottomLayout->addWidget(refreshBtn);

    QPushButton* closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);

    mainLayout->addLayout(bottomLayout);
}

void IndexStatisticsDialog::refreshStats()
{
    loadOverviewData();
    loadFieldsData();

    // Populate field combos
    QStringList fields;
    for (int i = 0; i < m_fieldsTable->rowCount(); ++i) {
        QString fieldName = m_fieldsTable->item(i, 0)->text();
        fields.append(fieldName);
    }

    m_termFieldCombo->blockSignals(true);
    m_termFieldCombo->clear();
    m_termFieldCombo->addItems(fields);
    m_termFieldCombo->blockSignals(false);

    m_topTermsFieldCombo->clear();
    m_topTermsFieldCombo->addItems(fields);

    // Don't auto-load terms data - it can be slow for large indexes
    // User needs to click "Load Terms" button manually
    m_termsStatsLabel->setText(tr("Select field and click 'Load Terms' to view term statistics"));

    loadDocumentsData();
}

void IndexStatisticsDialog::loadOverviewData()
{
    IndexStats stats = m_manager->getIndexStats();

    QString html = QString(
        "<h2>%1</h2>"
        "<table cellspacing='8'>"
        "<tr><td><b>%2</b></td><td>%3</td></tr>"
        "<tr><td><b>%4</b></td><td>%5</td></tr>"
        "<tr><td><b>%6</b></td><td>%7</td></tr>"
        "<tr><td><b>%8</b></td><td>%9</td></tr>"
        "<tr><td><b>%10</b></td><td>%11</td></tr>"
        "</table>"
    ).arg(tr("Index Overview"))
     .arg(tr("Index Path:"))
     .arg(m_manager->indexDirectory())
     .arg(tr("Total Documents:"))
     .arg(stats.totalDocuments)
     .arg(tr("Index Size:"))
     .arg(formatSize(stats.indexSizeBytes))
     .arg(tr("Index Exists:"))
     .arg(stats.exists ? tr("Yes") : tr("No"))
     .arg(tr("Analyzer:"))
     .arg(m_manager->analyzerId());

    // Add more detailed stats from Lucene
    try {
        QString indexPath = m_manager->indexDirectory();
        if (QDir(indexPath).exists()) {
            IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);

            html += QString(
                "<h3>%1</h3>"
                "<table cellspacing='8'>"
                "<tr><td><b>%2</b></td><td>%3</td></tr>"
                "<tr><td><b>%4</b></td><td>%5</td></tr>"
                "<tr><td><b>%6</b></td><td>%7</td></tr>"
                "<tr><td><b>%8</b></td><td>%9</td></tr>"
                "</table>"
            ).arg(tr("Lucene Statistics"))
             .arg(tr("Num Docs:"))
             .arg(reader->numDocs())
             .arg(tr("Max Doc:"))
             .arg(reader->maxDoc())
             .arg(tr("Num Deleted Docs:"))
             .arg(reader->numDeletedDocs())
             .arg(tr("Has Deletions:"))
             .arg(reader->hasDeletions() ? tr("Yes") : tr("No"));

            // Note: Term counting removed for performance reasons
            // Large indexes can have millions of terms, making this too slow

            reader->close();
        }
    } catch (...) {
        // Ignore errors
    }

    m_overviewLabel->setText(html);
}

void IndexStatisticsDialog::loadFieldsData()
{
    m_fieldsTable->setRowCount(0);

    try {
        QString indexPath = m_manager->indexDirectory();
        if (!QDir(indexPath).exists()) return;

        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);

        // Get field info from first document
        if (reader->numDocs() > 0) {
            DocumentPtr doc = reader->document(0);
            Collection<FieldablePtr> fields = doc->getFields();

            QSet<QString> seenFields;

            // Also get all unique field names from term enum
            TermEnumPtr termEnum = reader->terms();
            QMap<QString, int64_t> termCounts;
            QMap<QString, int> docCounts;

            while (termEnum->next()) {
                TermPtr term = termEnum->term();
                QString fieldName = QString::fromStdWString(term->field());
                termCounts[fieldName] = termCounts.value(fieldName, 0) + 1;
                docCounts[fieldName] = docCounts.value(fieldName, 0) + termEnum->docFreq();
            }
            termEnum->close();

            for (auto it = termCounts.begin(); it != termCounts.end(); ++it) {
                QString fieldName = it.key();
                int row = m_fieldsTable->rowCount();
                m_fieldsTable->insertRow(row);

                m_fieldsTable->setItem(row, 0, new QTableWidgetItem(fieldName));
                m_fieldsTable->setItem(row, 1, new QTableWidgetItem(tr("Yes")));  // Indexed
                m_fieldsTable->setItem(row, 2, new QTableWidgetItem(tr("-")));     // Stored (unknown)
                m_fieldsTable->setItem(row, 3, new QTableWidgetItem(tr("Yes")));  // Tokenized
                m_fieldsTable->setItem(row, 4, new QTableWidgetItem(QString::number(it.value())));
                m_fieldsTable->setItem(row, 5, new QTableWidgetItem(QString::number(docCounts.value(fieldName))));
            }
        }

        reader->close();
    } catch (const std::exception& e) {
        qWarning() << "Error loading fields:" << e.what();
    }
}

void IndexStatisticsDialog::loadTermsData(const QString& field)
{
    m_termsTable->setRowCount(0);

    if (field.isEmpty()) return;

    const int maxTerms = 500;
    QElapsedTimer timer;
    timer.start();

    m_termsStatsLabel->setText(tr("Loading terms..."));
    QApplication::processEvents();

    try {
        QString indexPath = m_manager->indexDirectory();
        if (!QDir(indexPath).exists()) return;

        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);

        // Collect terms with doc frequency (fast, no TermDocs iteration needed)
        struct TermStats {
            QString text;
            int docFreq;
        };
        QVector<TermStats> allTerms;

        TermEnumPtr termEnum = reader->terms(newLucene<Term>(field.toStdWString(), L""));

        while (termEnum->next()) {
            TermPtr term = termEnum->term();

            // Check if still in correct field
            if (QString::fromStdWString(term->field()) != field)
                break;

            allTerms.append({ QString::fromStdWString(term->text()), termEnum->docFreq() });
        }

        termEnum->close();
        reader->close();

        // Sort by doc frequency descending
        std::sort(allTerms.begin(), allTerms.end(),
            [](const TermStats& a, const TermStats& b) {
                return a.docFreq > b.docFreq;
            });

        // Show top N
        int count = qMin(maxTerms, allTerms.size());
        for (int i = 0; i < count; ++i) {
            int row = m_termsTable->rowCount();
            m_termsTable->insertRow(row);

            m_termsTable->setItem(row, 0, new QTableWidgetItem(allTerms[i].text));
            m_termsTable->setItem(row, 1, new QTableWidgetItem(QString::number(allTerms[i].docFreq)));
        }

        m_termsStatsLabel->setText(tr("Top %1 terms by doc count (of %2 total) - loaded in %3ms")
                                    .arg(count)
                                    .arg(allTerms.size())
                                    .arg(timer.elapsed()));

    } catch (const std::exception& e) {
        qWarning() << "Error loading terms:" << e.what();
        m_termsStatsLabel->setText(QString("Error: %1").arg(e.what()));
    }
}

void IndexStatisticsDialog::onTermSearch()
{
    QString field = m_termFieldCombo->currentText();
    if (field.isEmpty()) return;
    loadTermsData(field);
}

void IndexStatisticsDialog::onTermDoubleClicked(int row, int column)
{
    Q_UNUSED(column)

    QString term = m_termsTable->item(row, 0)->text();
    QString field = m_termFieldCombo->currentText();

    // Create a dialog to show related documents
    QDialog* docDialog = new QDialog(this);
    docDialog->setWindowTitle(tr("Documents containing '%1'").arg(term));
    docDialog->resize(800, 500);

    QVBoxLayout* layout = new QVBoxLayout(docDialog);

    QLabel* infoLabel = new QLabel();
    layout->addWidget(infoLabel);

    QTableWidget* docTable = new QTableWidget();
    docTable->setColumnCount(3);
    docTable->setHorizontalHeaderLabels({ tr("Path"), tr("Filename"), tr("Term Freq") });
    docTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    docTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    docTable->horizontalHeader()->setStretchLastSection(true);
    docTable->verticalHeader()->setVisible(false);
    layout->addWidget(docTable);

    QPushButton* closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, docDialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    // Load documents containing this term
    try {
        QString indexPath = m_manager->indexDirectory();
        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);

        TermDocsPtr termDocs = reader->termDocs(
            newLucene<Term>(field.toStdWString(), term.toStdWString()));

        int docCount = 0;
        while (termDocs->next()) {
            int32_t docId = termDocs->doc();
            int32_t freq = termDocs->freq();

            if (reader->isDeleted(docId)) continue;

            DocumentPtr doc = reader->document(docId);
            QString path = QString::fromStdWString(doc->get(L"path"));
            QString filename = QString::fromStdWString(doc->get(L"filename"));

            int row = docTable->rowCount();
            docTable->insertRow(row);

            docTable->setItem(row, 0, new QTableWidgetItem(path));
            docTable->setItem(row, 1, new QTableWidgetItem(filename));
            docTable->setItem(row, 2, new QTableWidgetItem(QString::number(freq)));

            ++docCount;
        }

        termDocs->close();
        reader->close();

        infoLabel->setText(tr("Found in %1 document(s)").arg(docCount));

    } catch (const std::exception& e) {
        infoLabel->setText(QString("Error: %1").arg(e.what()));
    }

    docDialog->setAttribute(Qt::WA_DeleteOnClose);
    docDialog->show();
}

void IndexStatisticsDialog::onDocumentSelected(int row, int column)
{
    Q_UNUSED(column)

    QString path = m_documentsTable->item(row, 0)->text();

    // Create a dialog to show document details
    QDialog* detailDialog = new QDialog(this);
    detailDialog->setWindowTitle(tr("Document Details"));
    detailDialog->resize(700, 500);

    QVBoxLayout* layout = new QVBoxLayout(detailDialog);

    QTextEdit* contentEdit = new QTextEdit();
    contentEdit->setReadOnly(true);
    layout->addWidget(contentEdit);

    QPushButton* closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, detailDialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    try {
        QString indexPath = m_manager->indexDirectory();
        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);
        IndexSearcherPtr searcher = newLucene<IndexSearcher>(reader);

        TermQueryPtr query = newLucene<TermQuery>(
            newLucene<Term>(L"path", path.toStdWString()));

        TopDocsPtr topDocs = searcher->search(query, 1);

        if (topDocs->totalHits > 0) {
            DocumentPtr doc = searcher->doc(topDocs->scoreDocs[0]->doc);

            QString content;
            content += QString("<h3>%1</h3>").arg(tr("Document Fields"));
            content += QString("<table cellspacing='8'>");

            Collection<FieldablePtr> fields = doc->getFields();
            for (Collection<FieldablePtr>::iterator it = fields.begin(); it != fields.end(); ++it) {
                FieldablePtr field = *it;
                QString fieldName = QString::fromStdWString(field->name());
                QString value = QString::fromStdWString(field->stringValue());

                content += QString("<tr><td><b>%1</b></td>").arg(fieldName);

                if (fieldName == "contents") {
                    content += QString("<td><i>%1 chars</i><br/>")
                        .arg(value.length());
                    content += QString("<pre style='white-space: pre-wrap;'>%1</pre>")
                        .arg(value.left(5000).toHtmlEscaped());
                    if (value.length() > 5000) {
                        content += QString("<i>... truncated (%1 more chars)</i>")
                            .arg(value.length() - 5000);
                    }
                    content += "</td></tr>";
                } else {
                    content += QString("<td>%1</td></tr>")
                        .arg(value.toHtmlEscaped());
                }
            }

            content += "</table>";

            // Add term vector info for contents field
            content += QString("<h3>%1</h3>").arg(tr("Term Analysis"));

            TermFreqVectorPtr tfv = reader->getTermFreqVector(topDocs->scoreDocs[0]->doc, L"contents");
            if (tfv) {
                Collection<String> terms = tfv->getTerms();
                Collection<int32_t> freqs = tfv->getTermFrequencies();

                if (!terms.empty()) {
                    content += QString("<p>%1 unique terms in 'contents' field</p>").arg(terms.size());

                    // Show top 20 most frequent terms
                    QVector<QPair<QString, int>> termFreqList;
                    for (int i = 0; i < static_cast<int>(terms.size()); ++i) {
                        termFreqList.append({ QString::fromStdWString(terms[i]), freqs[i] });
                    }
                    std::sort(termFreqList.begin(), termFreqList.end(),
                        [](const auto& a, const auto& b) { return a.second > b.second; });

                    content += "<table cellspacing='4'><tr><th>Term</th><th>Freq</th></tr>";
                    int showCount = qMin(20, termFreqList.size());
                    for (int i = 0; i < showCount; ++i) {
                        content += QString("<tr><td>%1</td><td>%2</td></tr>")
                            .arg(termFreqList[i].first.toHtmlEscaped())
                            .arg(termFreqList[i].second);
                    }
                    content += "</table>";
                }
            } else {
                content += QString("<p><i>%1</i></p>").arg(tr("Term vectors not stored for this field"));
            }

            contentEdit->setHtml(content);
        }

        reader->close();
    } catch (const std::exception& e) {
        contentEdit->setText(QString("Error: %1").arg(e.what()));
    }

    detailDialog->setAttribute(Qt::WA_DeleteOnClose);
    detailDialog->show();
}

void IndexStatisticsDialog::onDocumentFilterChanged(const QString& text)
{
    loadDocumentsData(text);
}

void IndexStatisticsDialog::loadDocumentsData(const QString& filter)
{
    m_documentsTable->setRowCount(0);

    try {
        QString indexPath = m_manager->indexDirectory();
        if (!QDir(indexPath).exists()) return;

        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);

        int32_t numDocs = reader->numDocs();
        int shown = 0;
        const int maxDocs = 1000;

        for (int32_t i = 0; i < numDocs && shown < maxDocs; ++i) {
            if (reader->isDeleted(i)) continue;

            DocumentPtr doc = reader->document(i);
            QString path = QString::fromStdWString(doc->get(L"path"));

            if (!filter.isEmpty()) {
                if (!path.contains(filter, Qt::CaseInsensitive)) continue;
            }

            int row = m_documentsTable->rowCount();
            m_documentsTable->insertRow(row);

            m_documentsTable->setItem(row, 0, new QTableWidgetItem(path));
            m_documentsTable->setItem(row, 1, new QTableWidgetItem(
                QString::fromStdWString(doc->get(L"filename"))));

            QString modified = QString::fromStdWString(doc->get(L"modified"));
            if (!modified.isEmpty()) {
                qint64 epoch = modified.toLongLong();
                m_documentsTable->setItem(row, 2, new QTableWidgetItem(
                    QDateTime::fromSecsSinceEpoch(epoch).toString("yyyy-MM-dd hh:mm")));
            } else {
                m_documentsTable->setItem(row, 2, new QTableWidgetItem("-"));
            }

            QString hidden = QString::fromStdWString(doc->get(L"is_hidden"));
            m_documentsTable->setItem(row, 3, new QTableWidgetItem(hidden == "Y" ? tr("Yes") : tr("No")));

            ++shown;
        }

        reader->close();

        m_docCountLabel->setText(tr("Showing %1 of %2 documents").arg(shown).arg(numDocs));

    } catch (const std::exception& e) {
        qWarning() << "Error loading documents:" << e.what();
    }
}

void IndexStatisticsDialog::onFieldSelected(int row, int column)
{
    Q_UNUSED(column)
    QString field = m_fieldsTable->item(row, 0)->text();

    // Update term field combo
    int index = m_termFieldCombo->findText(field);
    if (index >= 0) {
        m_termFieldCombo->setCurrentIndex(index);
    }
}

QString IndexStatisticsDialog::formatSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    else if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    else if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    else
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

void IndexStatisticsDialog::loadTopTermsData()
{
    m_topTermsTable->setRowCount(0);

    QString field = m_topTermsFieldCombo->currentText();
    if (field.isEmpty()) return;

    int topN = m_topTermsCountSpin->value();

    try {
        QString indexPath = m_manager->indexDirectory();
        if (!QDir(indexPath).exists()) return;

        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);

        // Collect terms with doc frequency (fast)
        QVector<QPair<QString, int>> termFreqs;
        TermEnumPtr termEnum = reader->terms(newLucene<Term>(field.toStdWString(), L""));

        while (termEnum->next()) {
            TermPtr term = termEnum->term();
            if (QString::fromStdWString(term->field()) != field)
                break;

            termFreqs.append({ QString::fromStdWString(term->text()), termEnum->docFreq() });
        }
        termEnum->close();

        // Sort by doc frequency descending
        std::sort(termFreqs.begin(), termFreqs.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        // Show top N
        int count = qMin(topN, termFreqs.size());
        for (int i = 0; i < count; ++i) {
            int row = m_topTermsTable->rowCount();
            m_topTermsTable->insertRow(row);

            m_topTermsTable->setItem(row, 0, new QTableWidgetItem(QString::number(i + 1)));
            m_topTermsTable->setItem(row, 1, new QTableWidgetItem(termFreqs[i].first));
            m_topTermsTable->setItem(row, 2, new QTableWidgetItem(QString::number(termFreqs[i].second)));
        }

        reader->close();

    } catch (const std::exception& e) {
        qWarning() << "Error loading top terms:" << e.what();
    }
}
