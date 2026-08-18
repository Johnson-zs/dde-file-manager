// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXSTATUS_H
#define INDEXSTATUS_H

#include "service_textindex_global.h"
#include "task/taskgrade.h"
#include "task/envgate.h"

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

SERVICETEXTINDEX_BEGIN_NAMESPACE

/**
 * @brief High-level lifecycle phase of an index, reported to the UI.
 */
enum class IndexPhase {
    Idle,           ///< Idle / ready
    Updating,       ///< An indexing task is running
    Paused,         ///< Paused due to environment (waiting for power/save/idle)
    Failed,         ///< Last task failed
    WaitingUpgrade  ///< Waiting for index service upgrade (version mismatch)
};

inline QString indexPhaseToString(IndexPhase p)
{
    switch (p) {
    case IndexPhase::Idle: return QStringLiteral("idle");
    case IndexPhase::Updating: return QStringLiteral("updating");
    case IndexPhase::Paused: return QStringLiteral("paused");
    case IndexPhase::Failed: return QStringLiteral("failed");
    case IndexPhase::WaitingUpgrade: return QStringLiteral("waiting-upgrade");
    }
    return QStringLiteral("idle");
}

/**
 * @brief Full index status snapshot, serialised to JSON for D-Bus consumers.
 */
struct IndexStatus
{
    IndexPhase phase { IndexPhase::Idle };
    TaskGrade grade { TaskGrade::Light };
    WaitReason waitReason { WaitReason::None };
    qint64 progressCount { 0 };
    qint64 progressTotal { 0 };
    bool ready { false };          ///< Index usable for search (= exists && !Failed)
    QString lastError;
    QString lastUpdateTime;
};

inline QJsonObject indexStatusToJson(const IndexStatus &s)
{
    QJsonObject obj;
    obj[QStringLiteral("phase")] = indexPhaseToString(s.phase);
    obj[QStringLiteral("grade")] = taskGradeToString(s.grade);
    obj[QStringLiteral("waitReason")] = waitReasonToString(s.waitReason);
    obj[QStringLiteral("progressCount")] = static_cast<qint64>(s.progressCount);
    obj[QStringLiteral("progressTotal")] = static_cast<qint64>(s.progressTotal);
    obj[QStringLiteral("ready")] = s.ready;
    obj[QStringLiteral("lastError")] = s.lastError;
    obj[QStringLiteral("lastUpdateTime")] = s.lastUpdateTime;
    return obj;
}

inline QString indexStatusToJsonString(const IndexStatus &s)
{
    return QString::fromUtf8(QJsonDocument(indexStatusToJson(s)).toJson(QJsonDocument::Compact));
}

SERVICETEXTINDEX_END_NAMESPACE

#endif   // INDEXSTATUS_H
