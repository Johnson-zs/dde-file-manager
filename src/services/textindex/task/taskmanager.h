// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "service_textindex_global.h"
#include "core/indexcontext.h"
#include "indextask.h"
#include "task/taskgrade.h"

#include <QObject>
#include <QThread>
#include <QQueue>
#include <QHash>

SERVICETEXTINDEX_BEGIN_NAMESPACE

// 任务队列项
struct TaskQueueItem
{
    IndexTask::Type type;
    QString path;
    QStringList pathList;   // 当传入多个路径时使用
    QStringList fileList;   // 仅在文件列表类型任务中使用
    QHash<QString, QString> movedFiles;  // 仅在移动任务中使用 (fromPath -> toPath)
    bool silent { false };

    // —— 策略优化新增字段 ——
    TaskGrade grade { TaskGrade::Light };   ///< 任务分级（环境门控 / 资源管控依据）
    bool bypassEnv { false };               ///< 一次性绕过环境门槛（手动）
    bool resourceControl { true };         ///< 是否启用 CPU/IO 资源管控（按 grade + 手动模式）
    bool manualImmediate { false };        ///< 是否「立即更新」（影响资源管控默认值）
};

class TaskManager : public QObject
{
    Q_OBJECT
public:
    explicit TaskManager(const IndexContext *context, QObject *parent = nullptr);
    ~TaskManager();

    bool startTask(IndexTask::Type type, const QStringList &pathList, bool silent = false);
    bool startTask(IndexTask::Type type, const QString &path, bool silent = false);

    bool startFileListTask(IndexTask::Type type, const QStringList &fileList, bool silent = false);

    bool startFileMoveTask(const QHash<QString, QString> &movedFiles, bool silent = false);

    /// Unified graded submission entry used by TaskScheduler. Handles all
    /// task types and propagates grade/resourceControl to the IndexTask.
    bool submit(const TaskQueueItem &item);

    bool hasRunningTask() const;
    bool hasQueuedTasks() const;
    void stopCurrentTask();

    /// When false, the manager will not auto-dispatch queued tasks after a
    /// task finishes (used by the scheduler while paused). Pending tasks
    /// remain in the queue and are flushed via tryDispatchNext().
    void setDispatchEnabled(bool enabled);
    bool tryDispatchNext();

    /// Grade / bypass / resource flags of the currently running task.
    TaskGrade runningGrade() const { return m_runningGrade; }
    bool runningBypassEnv() const { return m_runningBypassEnv; }
    bool runningResourceControl() const { return m_runningResourceControl; }
    QStringList runningPathList() const { return m_runningPathList; }

    std::optional<IndexTask::Type> currentTaskType() const;
    std::optional<QString> currentTaskPath() const;

    // Recovery state management - used to prevent incremental tasks from
    // clearing Dirty state before recovery task completes
    void setRecoveryPending(bool pending);
    bool isRecoveryPending() const;

Q_SIGNALS:
    void taskFinished(const QString &type, const QString &path, bool success);
    void taskProgressChanged(const QString &type, const QString &path, qint64 count, qint64 total);
    void startTaskInThread();

private Q_SLOTS:
    void onTaskProgress(IndexTask::Type type, qint64 count, qint64 total);
    void onTaskFinished(IndexTask::Type type, SERVICETEXTINDEX_NAMESPACE::HandlerResult result);

private:
    void cleanupTask();
    bool startNextTask();
    TaskHandler getTaskHandler(IndexTask::Type type);
    bool isFullScanTask(IndexTask::Type type) const;
    bool enqueueCompensationTask(const QStringList &paths, bool silent);
    QStringList applyDirectoryMovePlans(const QHash<QString, QString> &movedFiles);

    /// Common worker launch: creates the IndexTask, applies silent /
    /// resourceControl / service-name, moves it to the worker thread and
    /// starts it. Records the running grade/bypass/resource flags.
    bool launchTask(IndexTask::Type type, const QString &pathId,
                    TaskHandler handler, const TaskQueueItem &item);

    // Type-specific launch paths (used by submit())
    bool startFullScanTask(const TaskQueueItem &item);
    bool startFileListTaskInternal(const TaskQueueItem &item);
    bool startFileMoveTaskInternal(const TaskQueueItem &item);

    // onTaskFinished sub-routines
    bool handleCorruptedIndex(IndexTask::Type type, const HandlerResult &result, const QString &taskPath);
    void handleRootPathFailure(bool success, bool interrupted, const QString &taskPath);
    void updateIndexStatusOnSuccess(IndexTask::Type type, const HandlerResult &result);
    void finalizeIndexState(IndexTask::Type type, const HandlerResult &result);

    const IndexContext *m_context { nullptr };
    QThread workerThread;
    IndexTask *currentTask { nullptr };

    // 保存待执行的任务信息
    QQueue<TaskQueueItem> taskQueue;

    // Recovery pending flag - set at service startup if Dirty state detected
    // Prevents incremental tasks from clearing Dirty state before recovery completes
    bool m_recoveryPending { false };

    // Scheduler-controlled dispatch gate
    bool m_dispatchEnabled { true };

    // Running task strategy flags (mirrored from the launched TaskQueueItem)
    TaskGrade m_runningGrade { TaskGrade::Light };
    bool m_runningBypassEnv { false };
    bool m_runningResourceControl { true };
    QStringList m_runningPathList;

    static QString typeToString(IndexTask::Type type);
};

SERVICETEXTINDEX_END_NAMESPACE
#endif   // TASKMANAGER_H
