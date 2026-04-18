// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "indexmanager.h"

#include <QMainWindow>
#include <QHash>

class QTabWidget;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QProgressBar;
class QTreeWidget;
class QLabel;
class QComboBox;

class AnalyzerTab : public QWidget
{
    Q_OBJECT

public:
    explicit AnalyzerTab(const QString& analyzerId, QWidget* parent = nullptr);
    ~AnalyzerTab() override;

    QString analyzerId() const { return m_analyzerId; }

private slots:
    void onSelectSourceDirectory();
    void onCreateIndex();
    void onDeleteIndex();
    void onSearch();
    void onViewStatistics();
    void onProgressChanged(qint64 processed, qint64 total);
    void onIndexingFinished(bool success, const QString& message);

private:
    void setupUI();
    void updateStats();
    void updateButtonStates();
    QString formatSize(qint64 bytes) const;

    QString m_analyzerId;
    IndexManager* m_indexManager { nullptr };

    // UI components
    QLineEdit* m_sourcePathEdit { nullptr };
    QLineEdit* m_searchEdit { nullptr };
    QPushButton* m_browseBtn { nullptr };
    QPushButton* m_createIndexBtn { nullptr };
    QPushButton* m_deleteIndexBtn { nullptr };
    QPushButton* m_searchBtn { nullptr };
    QPushButton* m_statsBtn { nullptr };
    QProgressBar* m_progressBar { nullptr };
    QLabel* m_statsLabel { nullptr };
    QLabel* m_timeLabel { nullptr };
    QTreeWidget* m_resultsTree { nullptr };
    QTextEdit* m_logEdit { nullptr };

    bool m_isIndexing { false };
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onAnalyzeCustomIndex();

private:
    void setupUI();
    void refreshAnalyzers();

    QTabWidget* m_tabWidget { nullptr };
    QPushButton* m_refreshBtn { nullptr };
    QHash<QString, AnalyzerTab*> m_tabs;

    // Custom index analysis
    QLineEdit* m_customPathEdit { nullptr };
    QComboBox* m_customAnalyzerCombo { nullptr };
    QPushButton* m_customBrowseBtn { nullptr };
    QPushButton* m_customAnalyzeBtn { nullptr };
};

#endif // MAINWINDOW_H
