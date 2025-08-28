#include "toolbarwidget.h"

#include <QLabel>

namespace dfmplugin_filegroup {

ToolbarWidget::ToolbarWidget(QWidget *parent)
    : QWidget { parent }
{
    setupUI();
}

ToolbarWidget::ClassificationType ToolbarWidget::currentClassificationType() const
{
    return static_cast<ClassificationType>(m_classificationComboBox->currentIndex());
}

void ToolbarWidget::setClassificationType(ClassificationType type)
{
    m_classificationComboBox->setCurrentIndex(static_cast<int>(type));
}

void ToolbarWidget::onComboBoxCurrentIndexChanged(int index)
{
    emit classificationTypeChanged(static_cast<ClassificationType>(index));
}

void ToolbarWidget::setupUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 5, 10, 5);
    m_mainLayout->setSpacing(10);

    // Add label for the combobox
    QLabel *label = new QLabel(tr("Classification:"), this);
    m_mainLayout->addWidget(label);

    // Create and setup the classification combobox
    m_classificationComboBox = new QComboBox(this);
    m_classificationComboBox->addItem(tr("None"));
    m_classificationComboBox->addItem(tr("Type"));
    m_classificationComboBox->addItem(tr("Modification Time"));
    m_classificationComboBox->addItem(tr("Size"));

    // Set default selection to "None"
    m_classificationComboBox->setCurrentIndex(0);

    // Connect signal for index changes
    connect(m_classificationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolbarWidget::onComboBoxCurrentIndexChanged);

    m_mainLayout->addWidget(m_classificationComboBox);
    m_mainLayout->addStretch();   // Add stretch to push content to the left

    setLayout(m_mainLayout);
}

}
