// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXSTATISTICSDIALOG_H
#define INDEXSTATISTICSDIALOG_H

#include "indexmanager.h"

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QTextEdit>

class IndexManager;

/**
 * @brief Dialog for displaying detailed index statistics
 * Similar to Luke tool for Lucene
 */
class IndexStatisticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IndexStatisticsDialog(IndexManager* manager, QWidget* parent = nullptr);
    ~IndexStatisticsDialog() override;

private slots:
    void refreshStats();
    void onFieldSelected(int row, int column);
    void onTermSearch();
    void onTermDoubleClicked(int row, int column);
    void onDocumentSelected(int row, int column);
    void onDocumentFilterChanged(const QString& text);

private:
    void setupUI();
    void loadOverviewData();
    void loadFieldsData();
    void loadTermsData(const QString& field);
    void loadDocumentsData(const QString& filter = QString());
    void loadTopTermsData();
    QString formatSize(qint64 bytes) const;

    IndexManager* m_manager { nullptr };

    // Overview tab
    QLabel* m_overviewLabel { nullptr };

    // Fields tab
    QTableWidget* m_fieldsTable { nullptr };

    // Terms tab
    QPushButton* m_termRefreshBtn { nullptr };
    QComboBox* m_termFieldCombo { nullptr };
    QTableWidget* m_termsTable { nullptr };
    QLabel* m_termsStatsLabel { nullptr };

    // Documents tab
    QLineEdit* m_docFilterEdit { nullptr };
    QTableWidget* m_documentsTable { nullptr };
    QLabel* m_docCountLabel { nullptr };

    // Top terms tab
    QComboBox* m_topTermsFieldCombo { nullptr };
    QSpinBox* m_topTermsCountSpin { nullptr };
    QTableWidget* m_topTermsTable { nullptr };
};

#endif // INDEXSTATISTICSDIALOG_H
