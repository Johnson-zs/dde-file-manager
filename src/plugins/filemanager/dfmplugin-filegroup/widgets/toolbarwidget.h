#ifndef TOOLBARWIDGET_H
#define TOOLBARWIDGET_H

#include "dfmplugin_filegroup_global.h"

#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>

namespace dfmplugin_filegroup {

class ToolbarWidget : public QWidget
{
    Q_OBJECT

public:
    enum ClassificationType {
        None,
        Type,
        ModificationTime,
        Size
    };
    Q_ENUM(ClassificationType)

    explicit ToolbarWidget(QWidget *parent = nullptr);

    /**
     * @brief Get the current selected classification type
     * @return Current classification type
     */
    ClassificationType currentClassificationType() const;

    /**
     * @brief Set the classification type
     * @param type The classification type to set
     */
    void setClassificationType(ClassificationType type);

signals:
    /**
     * @brief Emitted when classification type changes
     * @param type The new classification type
     */
    void classificationTypeChanged(ClassificationType type);

private slots:
    void onComboBoxCurrentIndexChanged(int index);

private:
    void setupUI();

private:
    QHBoxLayout *m_mainLayout;
    QComboBox *m_classificationComboBox;
};

}

#endif   // TOOLBARWIDGET_H
