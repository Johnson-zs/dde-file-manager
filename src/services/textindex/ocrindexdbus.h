// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OCRINDEXDBUS_H
#define OCRINDEXDBUS_H

#include "service_textindex_global.h"

#include <QObject>
#include <QDBusContext>
#include <QStringList>
#include <QHash>
#include <QVariantMap>

SERVICETEXTINDEX_BEGIN_NAMESPACE
class OcrIndexDBusPrivate;
class EnvDetector;
SERVICETEXTINDEX_END_NAMESPACE

class OcrIndexDBus : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.Filemanager.OcrIndex")

public:
    explicit OcrIndexDBus(SERVICETEXTINDEX_NAMESPACE::EnvDetector *envDetector = nullptr, QObject *parent = nullptr);
    ~OcrIndexDBus();

    void cleanup();

public Q_SLOTS:
    void Init();
    bool IsEnabled();
    void SetEnabled(bool enabled);
    bool CreateIndexTask(const QStringList &paths, const QVariantMap &options = QVariantMap());
    bool UpdateIndexTask(const QStringList &paths, const QVariantMap &options = QVariantMap());
    bool StopCurrentTask();
    bool HasRunningTask();
    bool IndexDatabaseExists();
    QString GetLastUpdateTime();
    bool ProcessFileChanges(const QStringList &createdFiles, const QStringList &modifiedFiles, const QStringList &deletedFiles);
    bool ProcessFileMoves(const QHash<QString, QString> &movedFiles);
    QString GetIndexStatus();
    bool ContinueUpdate();
    bool UpdateImmediately(const QStringList &paths);
    bool RebuildIndex(const QStringList &paths, const QVariantMap &options = QVariantMap());

Q_SIGNALS:
    void TaskFinished(const QString &type, const QString &path, bool success);
    void TaskProgressChanged(const QString &type, const QString &path, qint64 count, qint64 total);
    void IndexStatusChanged(const QString &statusJson);

private:
    QScopedPointer<SERVICETEXTINDEX_NAMESPACE::OcrIndexDBusPrivate> d;
};

#endif   // OCRINDEXDBUS_H
