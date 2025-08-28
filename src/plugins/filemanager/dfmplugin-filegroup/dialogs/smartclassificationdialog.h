// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SMARTCLASSIFICATIONDIALOG_H
#define SMARTCLASSIFICATIONDIALOG_H

#include "dfmplugin_filegroup_global.h"
#include "widgets/toolbarwidget.h"

#include <QDialog>
#include <QUrl>
#include <QVBoxLayout>

namespace dfmplugin_filegroup {

class FileListWidget;

class SmartClassificationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SmartClassificationDialog(const QUrl &targetDirectory, QWidget *parent = nullptr);
    ~SmartClassificationDialog() override = default;

private slots:
    /**
     * @brief Handle classification type changes from toolbar
     * @param type The new classification type
     */
    void onClassificationTypeChanged(ToolbarWidget::ClassificationType type);

private:
    void setupUI();

private:
    QUrl m_targetDirectory;
    QVBoxLayout *m_mainLayout;
    ToolbarWidget *m_toolbarWidget;
    FileListWidget *m_fileListWidget;
};

}

#endif   // SMARTCLASSIFICATIONDIALOG_H
