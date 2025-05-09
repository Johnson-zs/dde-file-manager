// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "searcheditwidget.h"
#include "utils/titlebarhelper.h"
#include "utils/searchhistroymanager.h"
#include "events/titlebareventcaller.h"
#include "views/completerview.h"
#include "views/completerviewdelegate.h"
#include "models/completerviewmodel.h"

#include <dfm-base/utils/fileutils.h>
#include <dfm-base/base/urlroute.h>
#include <dfm-base/base/configs/dconfig/dconfigmanager.h>
#include <dfm-base/base/application/application.h>
#include <dfm-base/widgets/filemanagerwindowsmanager.h>
#include <dfm-base/widgets/dfmcustombuttons/customiconbutton.h>
#include <dfm-base/widgets/dfmcustombuttons/customdtoolbutton.h>

#include <DToolButton>
#include <DIconButton>
#include <DSearchEdit>
#include <DSpinner>
#include <DDialog>
#include <DGuiApplicationHelper>
#include <DPalette>

#include <QHBoxLayout>
#include <QResizeEvent>

DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE
DFMBASE_USE_NAMESPACE
DPTITLEBAR_USE_NAMESPACE

inline constexpr int kSearchEditMaxWidth { 240 };   // Maximum width of search box
inline constexpr int kSearchEditMediumWidth { 200 };   // Medium width of search box
inline constexpr int kWidthThresholdCollapse { 900 };   // Threshold width to collapse search box
inline constexpr int kWidthThresholdExpand { 1100 };   // Threshold width to expand search box

SearchEditWidget::SearchEditWidget(QWidget *parent)
    : QWidget(parent)
{
    initUI();
    initConnect();
    initData();
    searchEdit->lineEdit()->installEventFilter(this);
    searchEdit->installEventFilter(this);
    advancedButton->installEventFilter(this);
}

void SearchEditWidget::activateEdit(bool setAdvanceBtn)
{
    if (!searchEdit || !advancedButton || !searchButton)
        return;

    if (parentWidget() && parentWidget()->width() >= kWidthThresholdExpand)
        setSearchMode(SearchMode::kExtraLarge);
    else
        setSearchMode(SearchMode::kExpanded);

    if (searchEdit->hasFocus() && setAdvanceBtn) {
        advancedButton->setChecked(!advancedButton->isChecked());
        TitleBarEventCaller::sendShowFilterView(this, advancedButton->isChecked());
    } else {
        searchEdit->lineEdit()->setFocus();
    }
}

void SearchEditWidget::deactivateEdit()
{
    if (!searchEdit || !advancedButton)
        return;

    advancedButton->setChecked(false);
    advancedButton->setVisible(false);

    searchEdit->clearEdit();
    searchEdit->clearFocus();
    if (parentWidget())
        updateSearchEditWidget(parentWidget()->width());
}

bool SearchEditWidget::isAdvancedButtonChecked() const
{
    return advancedButton->isChecked();
}

void SearchEditWidget::setAdvancedButtonChecked(bool checked)
{
    advancedButton->setVisible(checked);
    advancedButton->setChecked(checked);
}

void SearchEditWidget::setAdvancedButtonVisible(bool visible)
{
    advancedButton->setVisible(visible);
}

bool SearchEditWidget::completerViewVisible()
{
    return completerView->isVisible();
}

void SearchEditWidget::updateSearchEditWidget(int parentWidth)
{
    if (parentWidth >= kWidthThresholdExpand) {
        setSearchMode(SearchMode::kExtraLarge);
    } else if (parentWidth > kWidthThresholdCollapse) {
        setSearchMode(SearchMode::kExpanded);
    } else {
        setSearchMode(SearchMode::kCollapsed);
    }
}

void SearchEditWidget::setSearchMode(SearchMode mode)
{
    if (advancedButton->isChecked() || searchEdit->hasFocus())
        return;

    currentMode = mode;
    updateSearchWidgetLayout();
}

void SearchEditWidget::setText(const QString &text)
{
    searchEdit->setText(text);
}

void SearchEditWidget::onUrlChanged(const QUrl &url)
{
    if (TitleBarHelper::checkKeepTitleStatus(url)) {
        QUrlQuery query { url.query() };
        QString searchKey { query.queryItemValue("keyword", QUrl::FullyDecoded) };
        if (!searchKey.isEmpty()) {
            activateEdit(false);
            searchEdit->setText(searchKey);
        }
        return;
    }

    deactivateEdit();
}

void SearchEditWidget::onPauseButtonClicked()
{
    TitleBarEventCaller::sendStopSearch(this);
}

void SearchEditWidget::onAdvancedButtonClicked()
{
    TitleBarEventCaller::sendShowFilterView(this, advancedButton->isChecked());
}

void SearchEditWidget::onReturnPressed()
{
    if (urlCompleter && urlCompleter->popup()->isVisible()) {
        completerView->hide();
        completionPrefix.clear();
    }

    QString text { this->text() };
    if (text.isEmpty())
        return;

    // add search history list
    if (DConfigManager::instance()->value(DConfigSearch::kSearchCfgPath,
                                            DConfigSearch::kDisplaySearchHistory, true)
                .toBool()) {
        if (historyList.contains(text))
            historyList.removeAll(text);
        historyList.append(text);
        isHistoryInCompleterModel = false;
    }
    SearchHistroyManager::instance()->writeIntoSearchHistory(text);

    if (text == QObject::tr("Clear search history")) {
        quitSearch();
        auto result = showClearSearchHistory();
        if (result == DDialog::Accepted)
            clearSearchHistory();
        return;
    }

    TitleBarHelper::handleSearchPressed(this, text);
}

void SearchEditWidget::onTextEdited(const QString &text)
{
    lastEditedString = text;
    if (text.isEmpty()) {
        urlCompleter->setCompletionPrefix("");
        completerBaseString = "";
        completerView->hide();
        return;
    }

    updateCompletionState(text);
}

void SearchEditWidget::onClearSearchHistory(quint64 winId)
{
    quint64 id = FMWindowsIns.findWindowId(this);
    if (id != winId)
        return;

    auto result = showClearSearchHistory();
    if (result == DDialog::Accepted)
        clearSearchHistory();
}

void SearchEditWidget::onDConfigValueChanged(const QString &config, const QString &key)
{
    if (config != DConfigSearch::kSearchCfgPath || key != DConfigSearch::kDisplaySearchHistory)
        return;

    bool show = DConfigManager::instance()->value(config, key, false).toBool();
    if (show) {
        historyList.clear();
        historyList.append(SearchHistroyManager::instance()->getSearchHistroy());
    } else {
        historyList.clear();
        showHistoryList.clear();
        completerModel->setStringList(showHistoryList);
    }
    isHistoryInCompleterModel = false;
}

void SearchEditWidget::insertCompletion(const QString &completion)
{
    if (urlCompleter->widget() != searchEdit->lineEdit()) {
        return;
    }

    if (completion == QObject::tr("Clear search history")) {
        isClearSearch = true;
        onReturnPressed();
        return;
    }

    isClearSearch = false;
    searchEdit->setText(completerBaseString + completion);
}

void SearchEditWidget::onCompletionHighlighted(const QString &highlightedCompletion)
{
    isClearSearch = false;
    int completionPrefixLen = completionPrefix.length();
    int selectBeginPos = highlightedCompletion.length() - completionPrefixLen;
    if (highlightedCompletion == QObject::tr("Clear search history")) {
        searchEdit->setText(completerBaseString + lastEditedString);
        isClearSearch = true;
    } else {
        searchEdit->setText(completerBaseString + highlightedCompletion);
        isClearSearch = false;
    }
    searchEdit->lineEdit()->setSelection(searchEdit->text().length() - selectBeginPos, searchEdit->text().length());
}

void SearchEditWidget::onCompletionModelCountChanged()
{
    if (urlCompleter->completionCount() <= 0) {
        completerView->hide();
        searchEdit->lineEdit()->setFocus();
        return;
    }

    if (searchEdit->isVisible())
        doComplete();
}

void SearchEditWidget::appendToCompleterModel(const QStringList &stringList)
{
    for (const QString &str : stringList) {
        // 防止出现空的补全提示
        if (str.isEmpty())
            continue;

        QStandardItem *item = new QStandardItem(str);
        completerModel->appendRow(item);
    }
}

void SearchEditWidget::expandSearchEdit()
{
    setSearchMode(SearchMode::kExpanded);
    searchEdit->lineEdit()->setFocus();
}

void SearchEditWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    int rightMargin = 70;

    int spinnerX = event->size().width() - spinner->size().width() - rightMargin;
    int spinnerY = (event->size().height() - spinner->size().height()) / 2;
    spinner->setGeometry(spinnerX, spinnerY, spinner->size().width(), spinner->size().height());

    int pauseX = event->size().width() - pauseButton->size().width() - rightMargin;
    int pauseY = (event->size().height() - pauseButton->size().height()) / 2;
    pauseButton->setGeometry(pauseX, pauseY, pauseButton->size().width(), pauseButton->size().height());
}

bool SearchEditWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == searchEdit->lineEdit()) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (handleKeyPress(keyEvent))
                return true;
        } else if (event->type() == QEvent::FocusOut) {
            handleFocusOutEvent(static_cast<QFocusEvent *>(event));
        } else if (event->type() == QEvent::FocusIn) {
            handleFocusInEvent(static_cast<QFocusEvent *>(event));
        } else if (event->type() == QEvent::Leave) {
            handleLeaveEvent(static_cast<QEvent *>(event));
        } else if (event->type() == QEvent::Enter) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            handleEnterEvent(static_cast<QEvent *>(event));
#else
            handleEnterEvent(static_cast<QEnterEvent *>(event));
#endif
        } else if (event->type() == QEvent::InputMethod) {
            handleInputMethodEvent(static_cast<QInputMethodEvent *>(event));
        }
    }

    return QWidget::eventFilter(watched, event);
}

void SearchEditWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);

    if (searchEdit->lineEdit()->text().isEmpty() && !advancedButton->isChecked()) {
        advancedButton->setVisible(false);
    }
}

void SearchEditWidget::initUI()
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // search button
    searchButton = new CustomDIconButton(this);
    searchButton->setIcon(QIcon::fromTheme("dfm_search_button"));
    searchButton->setFixedSize(kToolButtonSize, kToolButtonSize);
    searchButton->setIconSize(QSize(kToolButtonIconSize, kToolButtonIconSize));
    searchButton->setFocusPolicy(Qt::NoFocus);
    searchButton->setToolTip(QObject::tr("search"));
    searchButton->setFlat(true);
    searchButton->setVisible(false);

    // search edit
    searchEdit = new DSearchEdit(this);
    searchEdit->setVisible(true);
    searchEdit->setFocusPolicy(Qt::StrongFocus);
    searchEdit->lineEdit()->setFocusPolicy(Qt::ClickFocus);

    // advanced search button
    advancedButton = new CustomDToolButton(this);
    advancedButton->setIcon(QIcon::fromTheme("dfm_view_filter"));
    advancedButton->setFixedSize(kToolButtonSize, kToolButtonSize);
    advancedButton->setFocusPolicy(Qt::NoFocus);
    advancedButton->setToolTip(QObject::tr("advanced search"));
    advancedButton->setCheckable(true);
    advancedButton->setVisible(false);

    layout->addWidget(searchButton);
    layout->addWidget(searchEdit);
    layout->addSpacing(10);
    layout->addWidget(advancedButton);
    // pause button
    pauseButton = new DIconButton(searchEdit);
    pauseButton->setFixedSize(QSize(16, 16));
    pauseButton->setIconSize(QSize(16, 16));
    pauseButton->setIcon(QIcon::fromTheme("dfm_search_pause"));
    pauseButton->setFocusPolicy(Qt::NoFocus);
    pauseButton->setCursor({ Qt::ArrowCursor });
    pauseButton->setFlat(true);
    pauseButton->setVisible(false);
    

    // spinner
    spinner = new DSpinner(searchEdit);
    spinner->setAttribute(Qt::WA_TransparentForMouseEvents);
    spinner->setFocusPolicy(Qt::NoFocus);
    spinner->setFixedSize(16, 16);
    spinner->hide();

    // Completer List
    completerView = new CompleterView(searchEdit->lineEdit());
    cpItemDelegate = new CompleterViewDelegate(completerView);
}

void SearchEditWidget::initConnect()
{
    connect(searchButton, &DIconButton::clicked, this, &SearchEditWidget::expandSearchEdit);
    connect(searchEdit, &DSearchEdit::textEdited, this, &SearchEditWidget::onTextEdited, Qt::QueuedConnection);
    connect(searchEdit, &DSearchEdit::searchAborted, this, &SearchEditWidget::quitSearch);
    connect(searchEdit, &DSearchEdit::returnPressed, this, &SearchEditWidget::onReturnPressed);
    connect(pauseButton, &DIconButton::clicked, this, &SearchEditWidget::onPauseButtonClicked);
    connect(advancedButton, &DToolButton::clicked, this, &SearchEditWidget::onAdvancedButtonClicked);

    connect(Application::instance(), &Application::clearSearchHistory, this,
            &SearchEditWidget::onClearSearchHistory);
    // fix bug#31692 搜索框输入中文后,全选已输入的,再次输入未覆盖之前的内容
    // 选中内容时，记录光标开始位置以及选中的长度
    connect(searchEdit, &DSearchEdit::selectionChanged, this, [=] {
        int posStart = searchEdit->lineEdit()->selectionStart();
        int posEnd = searchEdit->lineEdit()->selectionEnd();
        selectPosStart = posStart < posEnd ? posStart : posEnd;
        selectLength = searchEdit->lineEdit()->selectionLength();
    });
}

void SearchEditWidget::initData()
{
    // 设置补全组件
    urlCompleter = new QCompleter(searchEdit->lineEdit());
    urlCompleter->setWidget(searchEdit->lineEdit());

    completerModel = new CompleterViewModel(completerView);
    setCompleter(urlCompleter);

    // 设置补全选择组件为popup的焦点
    completerView->setFocus(Qt::FocusReason::PopupFocusReason);

    updateHistory();
}

void SearchEditWidget::updateHistory()
{
    if (!DConfigManager::instance()->value(DConfigSearch::kSearchCfgPath,
                                           DConfigSearch::kDisplaySearchHistory, true)
                 .toBool())
        return;

    historyList.clear();
    historyList.append(SearchHistroyManager::instance()->getSearchHistroy());
    isHistoryInCompleterModel = false;
}

int SearchEditWidget::showClearSearchHistory()
{
    QString clearSearch = QObject::tr("Are you sure clear search histories?");
    QStringList buttonTexts;
    buttonTexts.append(QObject::tr("Cancel", "button"));
    buttonTexts.append(QObject::tr("Confirm", "button"));

    DDialog d;

    if (!d.parentWidget()) {
        d.setWindowFlags(d.windowFlags() | Qt::WindowStaysOnTopHint);
    }
    d.setIcon(QIcon::fromTheme("dialog-warning"));
    d.setTitle(clearSearch);
    d.addButton(buttonTexts[0], true, DDialog::ButtonNormal);
    d.addButton(buttonTexts[1], false, DDialog::ButtonWarning);
    d.setDefaultButton(1);
    d.getButton(1)->setFocus();
    d.moveToCenter();

    int code = d.exec();
    return code;
}

void SearchEditWidget::clearSearchHistory()
{
    historyList.clear();
    SearchHistroyManager::instance()->clearHistory();
    clearCompleterModel();
}

void SearchEditWidget::startSpinner()
{
    spinner->start();
    spinner->show();
}

void SearchEditWidget::stopSpinner()
{
    pauseButton->setVisible(false);
    spinner->stop();
    spinner->hide();
}

QString SearchEditWidget::text() const
{
    if (isClearSearch)
        return QObject::tr("Clear search history");

    return searchEdit->text();
}

void SearchEditWidget::setCompleter(QCompleter *c)
{
    if (urlCompleter)
        urlCompleter->disconnect();

    urlCompleter = c;

    if (!urlCompleter)
        return;

    urlCompleter->setModel(completerModel);
    urlCompleter->setPopup(completerView);
    urlCompleter->setCompletionMode(QCompleter::PopupCompletion);
    urlCompleter->setCaseSensitivity(Qt::CaseSensitive);
    urlCompleter->setMaxVisibleItems(10);
    completerView->setItemDelegate(cpItemDelegate);
    completerView->setAttribute(Qt::WA_InputMethodEnabled);

    connect(urlCompleter, QOverload<const QString &>::of(&QCompleter::activated),
            this, &SearchEditWidget::insertCompletion);

    connect(urlCompleter, QOverload<const QString &>::of(&QCompleter::highlighted),
            this, &SearchEditWidget::onCompletionHighlighted);

    connect(urlCompleter->completionModel(), &QAbstractItemModel::modelReset,
            this, &SearchEditWidget::onCompletionModelCountChanged);
}

void SearchEditWidget::clearCompleterModel()
{
    isHistoryInCompleterModel = false;
    completerModel->setStringList(QStringList());
}

void SearchEditWidget::updateCompletionState(const QString &text)
{
    isClearSearch = false;
    completeSearchHistory(text);
}

void SearchEditWidget::doComplete()
{
    if (completerView->isHidden()) {
        urlCompleter->complete(searchEdit->lineEdit()->rect().adjusted(0, 5, 0, 5));
    } else {
        urlCompleter->metaObject()->invokeMethod(urlCompleter, "_q_autoResizePopup");
    }

    if (urlCompleter->completionCount() == 1
        && lastPressedKey != Qt::Key_Backspace
        && lastPressedKey != Qt::Key_Delete
        && isKeyPressed   // 判断是否按键按下，时间设定的时100ms
        && !(lastPressedKey == Qt::Key_X && lastPreviousKey == Qt::Key_Control)   // 键盘剪切事件
        && searchEdit->lineEdit()->cursorPosition() == searchEdit->text().length()) {
        completerView->setCurrentIndex(urlCompleter->completionModel()->index(0, 0));
    }
    // bug: 247167
    if (urlCompleter->completionCount() > 0) {
        int h { urlCompleter->completionCount() * kItemHeight + kItemMargin * 2 };
        completerView->setFixedHeight(h < kCompleterMaxHeight ? h : kCompleterMaxHeight);
    }
    completerView->show();

    // maybe lost focus?
    // completerView->activateWindow();

    return;
}

void SearchEditWidget::completeSearchHistory(const QString &text)
{
    // set completion prefix.
    urlCompleter->setCompletionPrefix("");
    filterHistory(text);

    // Check if we already loaded history list in model
    if (isHistoryInCompleterModel)
        return;

    // Set Base String
    this->completerBaseString = "";

    // History completion.
    isHistoryInCompleterModel = true;
    completerModel->setStringList(showHistoryList);
}

void SearchEditWidget::filterHistory(const QString &text)
{
    completionPrefix = text;
    showHistoryList.clear();
    for (const auto &str : historyList) {
        if (str.startsWith(text))
            showHistoryList.push_back(str);
    }
    if (showHistoryList.count() > 0)
        showHistoryList.append(QObject::tr("Clear search history"));

    completerModel->setStringList(showHistoryList);
}

bool SearchEditWidget::handleKeyPress(QKeyEvent *keyEvent)
{
    isKeyPressed = true;
    QTimer::singleShot(100, this, [=]() {   // 设定100ms，若有问题可视情况改变
        isKeyPressed = false;
    });
    lastPreviousKey = lastPressedKey;
    lastPressedKey = keyEvent->key();

    switch (keyEvent->key()) {
    case Qt::Key_Escape:
        quitSearch();
        keyEvent->accept();
        return true;
    default:
        break;
    }

    if (urlCompleter && urlCompleter->popup()->isVisible()) {
        if (isHistoryInCompleterModel && keyEvent->modifiers() == Qt::ShiftModifier && keyEvent->key() == Qt::Key_Delete) {
            QString completeResult = completerView->currentIndex().data().toString();
            bool ret = SearchHistroyManager::instance()->removeSearchHistory(completeResult);
            if (ret && DConfigManager::instance()->value(DConfigSearch::kSearchCfgPath, DConfigSearch::kDisplaySearchHistory, true).toBool()) {
                historyList.clear();
                historyList.append(SearchHistroyManager::instance()->getSearchHistroy());
                completerModel->setStringList(historyList);
            }
        }

        // The following keys are forwarded by the completer to the widget
        switch (keyEvent->key()) {
        case Qt::Key_Backtab:
            keyEvent->ignore();
            return true;
        case Qt::Key_Tab:
            if (urlCompleter->completionCount() > 0) {
                if (searchEdit->lineEdit()->selectedText().isEmpty()) {
                    QString completeResult = urlCompleter->completionModel()->index(0, 0).data().toString();
                    insertCompletion(completeResult);
                }
            }
            keyEvent->accept();
            return true;
        // 解决bug19609文件管理器中，文件夹搜索功能中输入法在输入过程中忽然失效然后恢复
        case Qt::Key_Up:
        case Qt::Key_Down:
            completerView->keyPressEvent(keyEvent);
            return true;
        default:
            break;
        }
        searchEdit->lineEdit()->setFocus();
    } else {
        // If no compiler
        switch (keyEvent->key()) {
        case Qt::Key_Tab:
            keyEvent->accept();
            return true;
        default:
            break;
        }
    }

    return false;
}

void SearchEditWidget::handleFocusInEvent(QFocusEvent *e)
{
    if (urlCompleter)
        urlCompleter->setWidget(searchEdit->lineEdit());

    advancedButton->setVisible(true);
}

void SearchEditWidget::handleFocusOutEvent(QFocusEvent *e)
{
    if (e->reason() == Qt::PopupFocusReason
        || e->reason() == Qt::ActiveWindowFocusReason) {
        e->accept();
        if (!searchEdit->text().isEmpty())
            searchEdit->lineEdit()->setFocus();
        return;
    }

    completionPrefix.clear();
    completerView->hide();

    if (searchEdit->lineEdit()->text().isEmpty() && !advancedButton->isChecked()) {
        advancedButton->setVisible(false);
    }

    if (parentWidget()) {
        updateSearchEditWidget(parentWidget()->width());
    }
}

void SearchEditWidget::handleInputMethodEvent(QInputMethodEvent *e)
{
    if (searchEdit->lineEdit()->hasSelectedText()) {
        // fix bug#31692 搜索框输入中文后,全选已输入的,再次输入未覆盖之前的内容
        int pos = selectPosStart;
        searchEdit->setText(lastEditedString.remove(selectPosStart, selectLength));
        // 设置光标到修改处
        searchEdit->lineEdit()->setCursorPosition(pos);
    }
}

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
void SearchEditWidget::handleEnterEvent(QEvent *e)
#else
void SearchEditWidget::handleEnterEvent(QEnterEvent *e)
#endif
{
    if (spinner->isPlaying()) {
        spinner->hide();
        pauseButton->setVisible(true);
    }
}

void SearchEditWidget::handleLeaveEvent(QEvent *e)
{
    if (spinner->isPlaying()) {
        pauseButton->setVisible(false);
        spinner->show();
    }
}

void SearchEditWidget::updateSearchWidgetLayout()
{
    if (currentMode == SearchMode::kCollapsed && searchEdit->text().isEmpty()) {
        setFixedWidth(searchButton->width());
        searchEdit->setVisible(false);
        searchButton->setVisible(true);
        advancedButton->setVisible(false);
    } else {
        int width = kSearchEditMediumWidth;
        if (currentMode == SearchMode::kExtraLarge)
            width = (parentWidget()->width() - kWidthThresholdExpand) + kSearchEditMediumWidth;
        setFixedWidth(qMin(width, kSearchEditMaxWidth));
        searchEdit->setVisible(true);
        searchButton->setVisible(false);
        advancedButton->setVisible(searchEdit->hasFocus() || !searchEdit->text().isEmpty());
    }
}

void SearchEditWidget::quitSearch()
{
    stopSpinner();
    deactivateEdit();
    Q_EMIT searchQuit();
}
