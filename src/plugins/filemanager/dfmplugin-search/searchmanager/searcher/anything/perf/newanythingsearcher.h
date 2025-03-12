// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NEWANYTHINGSEARCHER_H
#define NEWANYTHINGSEARCHER_H

#include "searcherinterface.h"

#include <QDBusInterface>
#include <QStringList>
#include <QObject>

class NewAnythingSearcher : public SearcherInterface
{
    Q_OBJECT
public:
    explicit NewAnythingSearcher(QObject *parent = nullptr);

    // 异步搜索实现
    bool requestSearch(const QString &path, const QString &text) override;

    // 同步搜索实现
    QStringList searchSync(const QString &path, const QString &text) override;

    void cancelSearch() override;

private:
    QDBusInterface *anythingInterface { nullptr };
    QDBusPendingCallWatcher *currentRequest { nullptr };
    QString m_currentQuery;
    QString m_searchPath;
    bool m_isSearching;

private slots:
    void onRequestFinished(QDBusPendingCallWatcher *watcher);
};

#endif   // NEWANYTHINGSEARCHER_H
