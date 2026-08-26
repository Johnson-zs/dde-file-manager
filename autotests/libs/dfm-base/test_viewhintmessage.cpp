// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_viewhintmessage.cpp
 * @brief Unit tests for ViewHintMessage (viewhintmessage.cpp)
 *
 * ViewHintMessage is a QObject controller for floating hint messages. The
 * setter methods and isVisible() are pure data operations that don't need
 * a visible widget. The show() method is NOT called to avoid needing a real
 * host widget.
 */

#include <gtest/gtest.h>
#include <dfm-base/widgets/viewhintmessage/viewhintmessage.h>

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <QWidget>

using namespace dfmbase;

TEST(ViewHintMessageTest, ConstructAndDestructWithoutCrash)
{
    {
        ViewHintMessage msg;
        (void)msg;
    }
    SUCCEED();
}

TEST(ViewHintMessageTest, IsVisibleReturnsFalseBeforeShow)
{
    ViewHintMessage msg;
    EXPECT_FALSE(msg.isVisible());
}

TEST(ViewHintMessageTest, SetIconDoesNotCrash)
{
    ViewHintMessage msg;
    msg.setIcon(QStringLiteral("dialog-warning"));
}

TEST(ViewHintMessageTest, SetTextDoesNotCrash)
{
    ViewHintMessage msg;
    msg.setText(QStringLiteral("test message"));
}

TEST(ViewHintMessageTest, SetActionsDoesNotCrash)
{
    ViewHintMessage msg;
    QList<QPair<QString, QString>> actions;
    actions << QPair<QString, QString>(QStringLiteral("ok"), QStringLiteral("OK"));
    actions << QPair<QString, QString>(QStringLiteral("cancel"), QStringLiteral("Cancel"));
    msg.setActions(actions);
}

TEST(ViewHintMessageTest, SetAutoDismissOnActionDoesNotCrash)
{
    ViewHintMessage msg;
    msg.setAutoDismissOnAction(false);
    msg.setAutoDismissOnAction(true);
}

// ============================================================
// Additional coverage for ViewHintMessage
// ============================================================

TEST(ViewHintMessageTest, ShowWithNullHostDoesNotCrash)
{
    ViewHintMessage msg;
    EXPECT_NO_FATAL_FAILURE({ msg.show(nullptr); });
}

TEST(ViewHintMessageTest, ShowWithRealWidget)
{
    ViewHintMessage msg;
    msg.setText("Hello");
    msg.setIcon("dialog-warning");
    QWidget host;
    EXPECT_NO_FATAL_FAILURE({ msg.show(&host); });
}

TEST(ViewHintMessageTest, ShowWithActions)
{
    ViewHintMessage msg;
    msg.setText("Action test");
    QList<QPair<QString, QString>> actions;
    actions << QPair<QString, QString>("ok", "OK");
    actions << QPair<QString, QString>("cancel", "Cancel");
    msg.setActions(actions);
    QWidget host;
    EXPECT_NO_FATAL_FAILURE({ msg.show(&host); });
}

TEST(ViewHintMessageTest, CloseWithoutShowDoesNotCrash)
{
    ViewHintMessage msg;
    EXPECT_NO_FATAL_FAILURE({ msg.close(); });
}

TEST(ViewHintMessageTest, ShowTwiceSecondIgnored)
{
    ViewHintMessage msg;
    QWidget host;
    msg.show(&host);
    // Second show should be ignored (message already exists)
    EXPECT_NO_FATAL_FAILURE({ msg.show(&host); });
}

// ============================================================
// Live-update and left/right custom widget factory coverage
// ============================================================

TEST(ViewHintMessageTest, SetIconAfterShowUpdatesLiveWidget)
{
    ViewHintMessage msg;
    msg.setText("initial");
    msg.setIcon("dialog-warning");
    QWidget host;
    msg.show(&host);
    EXPECT_NO_FATAL_FAILURE({ msg.setIcon("dialog-information"); });
}

TEST(ViewHintMessageTest, SetTextAfterShowUpdatesLiveWidget)
{
    ViewHintMessage msg;
    msg.setText("initial");
    QWidget host;
    msg.show(&host);
    EXPECT_NO_FATAL_FAILURE({ msg.setText("updated"); });
}

TEST(ViewHintMessageTest, SetActionsAfterShowUpdatesLiveWidget)
{
    ViewHintMessage msg;
    msg.setText("test");
    QWidget host;
    msg.show(&host);
    QList<QPair<QString, QString>> actions;
    actions << QPair<QString, QString>("ok", "OK");
    EXPECT_NO_FATAL_FAILURE({ msg.setActions(actions); });
}

TEST(ViewHintMessageTest, SetLeftCustomWidgetFactoryBeforeShow)
{
    ViewHintMessage msg;
    msg.setCustomWidgetFactory([](QWidget *parent) -> QWidget * {
        return new QWidget(parent);
    }, ViewHintMessage::Side::Left);
    msg.setText("factory test");
    QWidget host;
    EXPECT_NO_FATAL_FAILURE({ msg.show(&host); });
}

TEST(ViewHintMessageTest, SetLeftCustomWidgetFactoryAfterShow)
{
    ViewHintMessage msg;
    msg.setText("factory live test");
    QWidget host;
    msg.show(&host);
    msg.setCustomWidgetFactory([](QWidget *parent) -> QWidget * {
        return new QWidget(parent);
    }, ViewHintMessage::Side::Left);
    EXPECT_NO_FATAL_FAILURE({});
}

TEST(ViewHintMessageTest, SetLeftCustomWidgetFactoryNullAfterShow)
{
    ViewHintMessage msg;
    msg.setText("factory null test");
    msg.setIcon("dialog-warning");
    QWidget host;
    msg.show(&host);
    EXPECT_NO_FATAL_FAILURE({ msg.setCustomWidgetFactory(nullptr, ViewHintMessage::Side::Left); });
}

TEST(ViewHintMessageTest, SetRightCustomWidgetFactoryBeforeShow)
{
    ViewHintMessage msg;
    msg.setText("right factory test");
    msg.setCustomWidgetFactory([](QWidget *parent) -> QWidget * {
        return new QWidget(parent);
    }, ViewHintMessage::Side::Right);
    QWidget host;
    EXPECT_NO_FATAL_FAILURE({ msg.show(&host); });
}

TEST(ViewHintMessageTest, SetRightCustomWidgetFactoryAfterShow)
{
    ViewHintMessage msg;
    msg.setText("right factory live test");
    QWidget host;
    msg.show(&host);
    EXPECT_NO_FATAL_FAILURE({
        msg.setCustomWidgetFactory([](QWidget *parent) -> QWidget * {
            return new QWidget(parent);
        }, ViewHintMessage::Side::Right);
    });
}

TEST(ViewHintMessageTest, SetRightCustomWidgetFactoryNullAfterShow)
{
    ViewHintMessage msg;
    msg.setText("right factory null test");
    msg.setCustomWidgetFactory([](QWidget *parent) -> QWidget * {
        return new QWidget(parent);
    }, ViewHintMessage::Side::Right);
    QWidget host;
    msg.show(&host);
    EXPECT_NO_FATAL_FAILURE({ msg.setCustomWidgetFactory(nullptr, ViewHintMessage::Side::Right); });
}

TEST(ViewHintMessageTest, LeftAndRightCustomWidgetFactoryTogether)
{
    ViewHintMessage msg;
    msg.setCustomWidgetFactory([](QWidget *parent) -> QWidget * {
        return new QWidget(parent);
    }, ViewHintMessage::Side::Left);
    msg.setCustomWidgetFactory([](QWidget *parent) -> QWidget * {
        return new QWidget(parent);
    }, ViewHintMessage::Side::Right);
    msg.setText("both test");
    QWidget host;
    EXPECT_NO_FATAL_FAILURE({ msg.show(&host); });
}

TEST(ViewHintMessageTest, RefreshWithoutShowDoesNotCrash)
{
    ViewHintMessage msg;
    EXPECT_NO_FATAL_FAILURE({ msg.refresh(); });
}

TEST(ViewHintMessageTest, RefreshAfterShowDoesNotCrash)
{
    ViewHintMessage msg;
    msg.setText("refresh test");
    QWidget host;
    msg.show(&host);
    EXPECT_NO_FATAL_FAILURE({ msg.refresh(); });
}
