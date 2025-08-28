// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "smartclassificationdialog.h"
#include "widgets/filelistwidget.h"

using namespace dfmplugin_filegroup;

SmartClassificationDialog::SmartClassificationDialog(const QUrl &targetDirectory, QWidget *parent)
    : QDialog(parent), m_targetDirectory(targetDirectory)
{
    fmInfo() << "SmartClassificationDialog created for directory:" << targetDirectory;
    setupUI();
}

void SmartClassificationDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create and add toolbar widget
    m_toolbarWidget = new ToolbarWidget(this);
    m_mainLayout->addWidget(m_toolbarWidget);

    // Create and add file list widget
    m_fileListWidget = new FileListWidget(this);
    m_fileListWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mainLayout->addWidget(m_fileListWidget);

    // Set the target directory path to the file list widget
    m_fileListWidget->setRootPath(m_targetDirectory.toLocalFile());

    // Connect toolbar signals to file list widget
    connect(m_toolbarWidget, &ToolbarWidget::classificationTypeChanged,
            this, &SmartClassificationDialog::onClassificationTypeChanged);
    
    // Connect file list widget loading signals for potential progress indication
    connect(m_fileListWidget, &FileListWidget::loadingStarted,
            this, [this]() { fmDebug() << "File list loading started"; });
    connect(m_fileListWidget, &FileListWidget::loadingFinished,
            this, [this]() { fmDebug() << "File list loading finished"; });

    setLayout(m_mainLayout);
    setWindowTitle(tr("Smart Classification"));
    setMinimumSize(800, 600);
    resize(1200, 800);

    fmDebug() << "SmartClassificationDialog UI setup completed";
}

void SmartClassificationDialog::onClassificationTypeChanged(ToolbarWidget::ClassificationType type)
{
    fmInfo() << "Classification type changed to:" << static_cast<int>(type);

    // Update the file list widget's classification type
    if (m_fileListWidget) {
        m_fileListWidget->setClassificationType(type);
    }
}
