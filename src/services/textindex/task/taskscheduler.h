// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

#include "service_textindex_global.h"
#include "core/indexcontext.h"
#include "env/envdetector.h"
#include "state/indexstatestore.h"
#include "state/indexstatus.h"
#include "task/envgate.h"
#include "task/indextask.h"
#include "task/taskgrade.h"
#include "task/taskmanager.h"

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QVariantMap>
#include <QList>
#include <optional>

SERVICETEXTINDEX_BEGIN_NAMESPACE

/**
 * @brief Orchestrates task grading, environment gating, pause/resume and
 *        status reporting for one IndexRuntime.
 *
 * The scheduler sits between the D-Bus layer and TaskManager. Every task
 * submission is graded, gated against the current EnvState and either
 * dispatched to TaskManager or parked. When the environment changes the
 * scheduler re-evaluates parked tasks, pauses running tasks whose policy
 * no longer holds, and resumes previously paused work.
 *
 * Pause/resume follows the confirmed design (decision 3): no checkpoint is
 * persisted. A paused task is cooperatively stopped (Dirty state is
 * preserved); on resume a fresh idempotent Update (mtime compare) task is
 * submitted, which is safe to re-run.
 */
class TaskScheduler : public QObject
{
    Q_OBJECT
public:
    TaskScheduler(TaskManager *taskManager,
                  EnvDetector *envDetector,
                  const IndexStateStore *stateStore,
                  IndexProfile profile,
                  QObject *parent = nullptr);

    // ---- submission entry points ----

    /**
     * @brief Unified graded submission for full-scan tasks (Create/Update).
     * @p options may carry "bypassEnv", "resourceControl", "manualImmediate".
     */
    bool submit(IndexTask::Type type, const QStringList &paths,
                const QVariantMap &options = {});

    /**
     * @brief Incremental batch submission (from FSEventController flush).
     * Grades the parseable (created + modified) set as a whole and gates
     * each resulting sub-task. Remove/Move are always Light.
     */
    bool submitIncrementalBatch(const QStringList &created,
                                const QStringList &modified,
                                const QStringList &deleted,
                                const QHash<QString, QString> &moved);

    /**
     * @brief Boot / crash recovery submission (design §6.5).
     * Determines the grade from persisted state and gates accordingly.
     */
    bool submitBootRecovery(const QStringList &paths);

    // ---- manual operations ----

    /// Resume a paused task, one-shot bypassing the environment gate.
    /// Resource control follows manualResumeResourceControl (default true).
    bool continueUpdate();

    /// Trigger an immediate Update (compare). Bypasses the environment gate
    /// and disables resource control (Manual grade, decision 1).
    bool updateImmediately(const QStringList &paths);

    /// Trigger a full Create rebuild. Heavy grade, env-gated unless
    /// options["bypassEnv"] is true.
    bool rebuildIndex(const QStringList &paths, const QVariantMap &options = {});

    // ---- status ----

    IndexStatus status() const;
    QString statusJson() const;

    /// Whether a task is currently running or queued/parked by the scheduler.
    bool isActive() const;

Q_SIGNALS:
    void statusChanged(const QString &statusJson);
    void taskFinished(const QString &type, const QString &path, bool success);
    void taskProgressChanged(const QString &type, const QString &path, qint64 count, qint64 total);

private Q_SLOTS:
    void onEnvStateChanged(const EnvState &state);
    void onTaskFinished(const QString &type, const QString &path, bool success);
    void onTaskProgress(const QString &type, const QString &path, qint64 count, qint64 total);

private:
    struct ParkedTask
    {
        TaskQueueItem item;
        WaitReason reason { WaitReason::None };
    };

    struct PausedInfo
    {
        QStringList paths;
        TaskGrade grade { TaskGrade::Light };
        bool resourceControl { true };
    };

    void dispatchParked();
    bool tryDispatch(const TaskQueueItem &item);
    void requestPauseRunning(WaitReason reason);
    void resumePaused(bool bypassEnv, bool resourceControl);
    void recomputeStatus(bool taskJustFailed, bool success);
    void emitStatusChanged();

    GradePolicy policyFor(TaskGrade g) const { return SERVICETEXTINDEX_NAMESPACE::policyFor(g); }
    GateDecision evaluate(const GradePolicy &policy, bool bypassEnv) const;
    IncrementalScale computeScale(const QStringList &created, const QStringList &modified) const;

    TaskManager *m_tm { nullptr };
    EnvDetector *m_env { nullptr };
    const IndexStateStore *m_store { nullptr };
    IndexProfile m_profile;
    TaskGrader m_grader;

    EnvState m_envState;
    QList<ParkedTask> m_parked;
    std::optional<PausedInfo> m_paused;
    WaitReason m_pauseReason { WaitReason::None };

    IndexStatus m_status;
    bool m_lastFailed { false };
};

SERVICETEXTINDEX_END_NAMESPACE

Q_DECLARE_METATYPE(SERVICETEXTINDEX_NAMESPACE::EnvState)

#endif   // TASKSCHEDULER_H
