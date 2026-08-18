// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskmanager.h"
#include "taskqueueutils.h"
#include "utils/indexutility.h"

#include <QMetaType>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QDateTime>

SERVICETEXTINDEX_USE_NAMESPACE

namespace {
void registerMetaTypes()
{
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<IndexTask::Type>();
        qRegisterMetaType<IndexTask::Type>("IndexTask::Type");
        qRegisterMetaType<SERVICETEXTINDEX_NAMESPACE::IndexTask::Type>();
        qRegisterMetaType<SERVICETEXTINDEX_NAMESPACE::IndexTask::Type>("SERVICETEXTINDEX_NAMESPACE::IndexTask::Type");
        qRegisterMetaType<HandlerResult>();
        registered = true;
        fmDebug() << "[TaskManager] Meta types registered successfully";
    }
}

TaskQueueItem createCompensationTaskItem(const QStringList &paths, bool silent)
{
    TaskQueueItem item;
    item.type = IndexTask::Type::UpdateFileList;
    item.path = paths.isEmpty() ? QString() : paths.first();
    item.fileList = paths;
    item.silent = silent;
    return item;
}

}   // namespace

TaskManager::TaskManager(const IndexContext *context, QObject *parent)
    : QObject(parent),
      m_context(context)
{
    fmInfo() << "[TaskManager] Initializing TaskManager instance";
    registerMetaTypes();
    fmInfo() << "[TaskManager] TaskManager initialization completed";
}

TaskManager::~TaskManager()
{
    fmInfo() << "[TaskManager] Destroying TaskManager instance";
    if (currentTask) {
        fmInfo() << "[TaskManager] Stopping current task before destruction";
        stopCurrentTask();
        currentTask->disconnect();
    }

    if (workerThread.isRunning()) {
        fmInfo() << "[TaskManager] Stopping worker thread";
        workerThread.quit();
        // worker 为事件循环模型(QThread::exec)，quit() 属协作式退出：当前正在处理的槽
        // (IndexWriter::commit/optimize 或文件内容提取)执行完后事件循环才会返回。Lucene++ 的长
        // 操作没有中途取消接口，进入后只能等待其执行完成。
        // 严禁使用 terminate() —— 其底层是 pthread_cancel，会向 worker 线程注入 forced-unwind
        // 异常；若 worker 此刻正处在带 catch(...) 的调用链中(如 taskhandler 的索引处理、docparser
        // 的内容提取)，异常会被 catch(...) 截留且未 rethrow，从而触发 "FATAL: exception not
        // rethrown"，导致整个进程被 SIGABRT 中止(参见历史 dde-file-manager coredump)。
        // 此处给足等待时间，超时则交由进程退出兜底(进程 _exit 时线程由内核直接终止，不走 unwind)。
        if (!workerThread.wait(5000)) {
            fmWarning() << "[TaskManager] Worker thread still running after 5s, "
                           "will be cleaned up on process exit (terminate() removed to avoid SIGABRT)";
            _exit(1);
        }
    }
    fmInfo() << "[TaskManager] TaskManager destroyed successfully";
}

// 单路径版本，调用多路径版本保持兼容性
bool TaskManager::startTask(IndexTask::Type type, const QString &path, bool silent)
{
    fmDebug() << "[TaskManager::startTask] Single path task request - type:" << static_cast<int>(type)
              << "path:" << path << "silent:" << silent;
    return startTask(type, QStringList { path }, silent);
}

// 多路径版本的startTask实现
bool TaskManager::startTask(IndexTask::Type type, const QStringList &pathList, bool silent)
{
    Q_ASSERT_X(type == IndexTask::Type::Create || type == IndexTask::Type::Update,
               "Type error", "Only create and update supported");

    fmInfo() << "[TaskManager::startTask] Multi-path task request - type:" << static_cast<int>(type)
             << "paths:" << pathList.size() << "silent:" << silent;

    TaskQueueItem item;
    item.type = type;
    item.path = pathList.isEmpty() ? QString() : pathList.first();
    item.pathList = pathList;
    item.silent = silent;
    return submit(item);
}

bool TaskManager::startFileListTask(IndexTask::Type type, const QStringList &fileList, bool silent)
{
    fmInfo() << "[TaskManager::startFileListTask] File list task request - type:" << static_cast<int>(type)
             << "files:" << fileList.size() << "silent:" << silent;

    TaskQueueItem item;
    item.type = type;
    item.path = QString("FileList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
    item.fileList = fileList;
    item.silent = silent;
    return submit(item);
}

bool TaskManager::startFileMoveTask(const QHash<QString, QString> &movedFiles, bool silent)
{
    fmInfo() << "[TaskManager::startFileMoveTask] File move task request - moves:" << movedFiles.size()
             << "silent:" << silent;

    TaskQueueItem item;
    item.type = IndexTask::Type::MoveFileList;
    item.path = QString("MoveList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
    item.movedFiles = movedFiles;
    item.silent = silent;
    return submit(item);
}

// 统一入口：根据任务类型分发到具体的启动逻辑，并把 grade/resourceControl 透传给 IndexTask
bool TaskManager::submit(const TaskQueueItem &item)
{
    switch (item.type) {
    case IndexTask::Type::Create:
    case IndexTask::Type::Update:
        return startFullScanTask(item);
    case IndexTask::Type::CreateFileList:
    case IndexTask::Type::UpdateFileList:
    case IndexTask::Type::RemoveFileList:
        return startFileListTaskInternal(item);
    case IndexTask::Type::MoveFileList:
        return startFileMoveTaskInternal(item);
    }

    fmWarning() << "[TaskManager::submit] Unknown task type:" << static_cast<int>(item.type);
    return false;
}

bool TaskManager::startFullScanTask(const TaskQueueItem &orig)
{
    Q_ASSERT_X(orig.type == IndexTask::Type::Create || orig.type == IndexTask::Type::Update,
               "Type error", "Only create and update supported");

    // 路径列表归一化：兼容仅设置 path 的旧调用方式
    QStringList pathList = orig.pathList.isEmpty() && !orig.path.isEmpty()
            ? QStringList { orig.path } : orig.pathList;

    if (pathList.isEmpty()) {
        fmWarning() << "[TaskManager::startFullScanTask] Cannot start task - path list is empty";
        return false;
    }

    // 所有路径都必须是默认索引目录
    for (const auto &path : pathList) {
        if (!IndexUtility::isDefaultIndexedDirectory(path)) {
            fmWarning() << "[TaskManager::startFullScanTask] Invalid path detected:" << path;
            return false;
        }
    }

    const QString primaryPath = pathList.first();

    // 如果当前有任务在运行，停止它并将新任务保存为待执行任务
    // startTask 的优先级高于 startFileListTask，因此直接重置任务队列
    if (hasRunningTask()) {
        fmInfo() << "[TaskManager::startFullScanTask] Current task running, queuing new task - paths:" << pathList.size()
                 << "primary:" << primaryPath;
        stopCurrentTask();
        if (!taskQueue.isEmpty()) {
            fmInfo() << "[TaskManager::startFullScanTask] Clearing existing task queue with" << taskQueue.size() << "pending tasks";
            taskQueue.clear();
        }

        TaskQueueItem item = orig;
        item.path = primaryPath;
        item.pathList = pathList;
        taskQueue.enqueue(item);
        fmInfo() << "[TaskManager::startFullScanTask] Task queued successfully, will execute after current task stops";
        return true;
    }

    fmInfo() << "[TaskManager::startFullScanTask] Starting new task immediately - paths:" << pathList.size()
             << "primary:" << primaryPath << "type:" << static_cast<int>(orig.type) << "silent:" << orig.silent
             << "grade:" << taskGradeToString(orig.grade);

    // status文件存储了修改时间，清除后外部无法获取时间，外部利用该特性判断索引状态
    if (orig.type == IndexTask::Type::Create) {
        fmInfo() << "[TaskManager::startFullScanTask] Create task detected, clearing existing index status";
        if (m_context && m_context->stateStore()) {
            m_context->stateStore()->removeIndexStatusFile();
            // 创建索引的任务开销巨大，避免任务未完成时进程退出后，重复进入创建任务
            m_context->stateStore()->saveIndexStatus(QDateTime::currentDateTime());
            m_context->stateStore()->setCreateInProgress(true);
        }
    }

    TaskHandler handler = getTaskHandler(orig.type);
    if (!handler) {
        fmCritical() << "[TaskManager::startFullScanTask] Unknown task type:" << static_cast<int>(orig.type);
        return false;
    }

    // 对每个路径执行原始的 handler，聚合多路径结果
    auto wrapped = [handler, pathList](const QString &, TaskState &state) -> HandlerResult {
        fmDebug() << "[TaskManager::startFullScanTask] Executing task handler for" << pathList.size() << "paths";
        HandlerResult finalResult { true, false, false, false };

        for (const auto &path : pathList) {
            if (!state.isRunning()) {
                fmInfo() << "[TaskManager::startFullScanTask] Task execution interrupted during path processing";
                finalResult.interrupted = true;
                break;
            }

            fmDebug() << "[TaskManager::startFullScanTask] Processing path:" << path;
            HandlerResult pathResult = handler(path, state);

            if (!pathResult.success) {
                fmWarning() << "[TaskManager::startFullScanTask] Path processing failed:" << path;
                finalResult.success = false;
            }

            if (pathResult.fatal) {
                fmCritical() << "[TaskManager::startFullScanTask] Fatal error occurred during path processing:" << path;
                finalResult.fatal = true;
                break;
            }

            if (pathResult.interrupted) {
                fmInfo() << "[TaskManager::startFullScanTask] Path processing interrupted:" << path;
                finalResult.interrupted = true;
                break;
            }

            if (pathResult.useAnything) {
                fmInfo() << "[TaskManager::startFullScanTask] Using ANYTHING for file discovery, skipping remaining paths";
                break;
            }

            if (pathResult.indexChanged) {
                finalResult.indexChanged = true;
            }
        }

        fmInfo() << "[TaskManager::startFullScanTask] Task handler execution completed - success:" << finalResult.success
                 << "interrupted:" << finalResult.interrupted << "fatal:" << finalResult.fatal;
        return finalResult;
    };

    TaskQueueItem item = orig;
    item.path = primaryPath;
    item.pathList = pathList;
    return launchTask(orig.type, primaryPath, wrapped, item);
}

bool TaskManager::startFileListTaskInternal(const TaskQueueItem &orig)
{
    if (orig.fileList.isEmpty()) {
        fmWarning() << "[TaskManager::startFileListTaskInternal] Cannot start task - file list is empty";
        return false;
    }

    // 如果当前有任务在运行，将新任务加入队列
    if (hasRunningTask() || currentTask) {
        fmInfo() << "[TaskManager::startFileListTaskInternal] Current task running, queuing file list task with"
                 << orig.fileList.size() << "files";
        taskQueue.enqueue(orig);
        fmDebug() << "[TaskManager::startFileListTaskInternal] File list task queued successfully";
        return true;
    }

    fmInfo() << "[TaskManager::startFileListTaskInternal] Starting file list task immediately - files:" << orig.fileList.size()
             << "type:" << static_cast<int>(orig.type) << "silent:" << orig.silent
             << "grade:" << taskGradeToString(orig.grade);

    TaskHandler handler;
    switch (orig.type) {
    case IndexTask::Type::CreateFileList:
    case IndexTask::Type::UpdateFileList:
        handler = TaskHandlers::CreateOrUpdateFileListHandler(*m_context, orig.fileList);
        break;
    case IndexTask::Type::RemoveFileList:
        handler = TaskHandlers::RemoveFileListHandler(*m_context, orig.fileList);
        break;
    default:
        fmCritical() << "[TaskManager::startFileListTaskInternal] Unknown file list task type:" << static_cast<int>(orig.type);
        return false;
    }

    const QString pathId = QString("FileList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
    return launchTask(orig.type, pathId, handler, orig);
}

bool TaskManager::startFileMoveTaskInternal(const TaskQueueItem &orig)
{
    if (orig.movedFiles.isEmpty()) {
        fmWarning() << "[TaskManager::startFileMoveTaskInternal] Cannot start task - moved files list is empty";
        return false;
    }

    const QStringList compensationPaths = applyDirectoryMovePlans(orig.movedFiles);

    // 如果当前有任务在运行，将新任务加入队列
    if (hasRunningTask() || currentTask) {
        fmInfo() << "[TaskManager::startFileMoveTaskInternal] Current task running, queuing file move task with"
                 << orig.movedFiles.size() << "moves";
        taskQueue.enqueue(orig);
        enqueueCompensationTask(compensationPaths, orig.silent);
        fmDebug() << "[TaskManager::startFileMoveTaskInternal] File move task queued successfully";
        return true;
    }

    fmInfo() << "[TaskManager::startFileMoveTaskInternal] Starting file move task immediately - moves:" << orig.movedFiles.size()
             << "silent:" << orig.silent;

    TaskHandler handler = TaskHandlers::MoveFileListHandler(*m_context, orig.movedFiles);
    if (!handler) {
        fmCritical() << "[TaskManager::startFileMoveTaskInternal] Failed to create move file list handler";
        return false;
    }

    const QString pathId = QString("MoveList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
    const bool ok = launchTask(IndexTask::Type::MoveFileList, pathId, handler, orig);
    enqueueCompensationTask(compensationPaths, orig.silent);
    return ok;
}

bool TaskManager::launchTask(IndexTask::Type type, const QString &pathId,
                             TaskHandler handler, const TaskQueueItem &item)
{
    Q_ASSERT(!currentTask);
    currentTask = new IndexTask(type, pathId, handler);
    currentTask->setSilent(item.silent);
    // 资源管控与 silent 解耦：按 grade + 手动模式决定是否限速（设计 6.6）
    currentTask->setResourceControl(item.resourceControl);
    // 进程级 cgroup 服务名：Content 与 OCR 共用同一进程 cgroup（设计 2.1）
    currentTask->setServiceName(Defines::kCgroupServiceName);
    currentTask->moveToThread(&workerThread);

    connect(currentTask, &IndexTask::progressChanged, this, &TaskManager::onTaskProgress, Qt::QueuedConnection);
    connect(currentTask, &IndexTask::finished, this, &TaskManager::onTaskFinished, Qt::QueuedConnection);
    connect(this, &TaskManager::startTaskInThread, currentTask, &IndexTask::start, Qt::QueuedConnection);
    workerThread.start();

    // 记录当前运行任务的策略标志，供 TaskScheduler 做环境门控/暂停判定
    m_runningGrade = item.grade;
    m_runningBypassEnv = item.bypassEnv;
    m_runningResourceControl = item.resourceControl;
    m_runningPathList = item.pathList.isEmpty() && !item.path.isEmpty()
            ? QStringList { item.path } : item.pathList;

    // Mark index state as dirty before starting task
    if (m_context && m_context->stateStore()) {
        m_context->stateStore()->setIndexState(IndexUtility::IndexState::Dirty);
    }

    emit startTaskInThread();
    fmInfo() << "[TaskManager::launchTask] Task started in worker thread - type:" << static_cast<int>(type)
             << "pathId:" << pathId << "grade:" << taskGradeToString(item.grade)
             << "resourceControl:" << item.resourceControl;
    return true;
}

TaskHandler TaskManager::getTaskHandler(IndexTask::Type type)
{
    if (!m_context)
        return nullptr;

    switch (type) {
    case IndexTask::Type::Create:
        return TaskHandlers::CreateIndexHandler(*m_context);
    case IndexTask::Type::Update:
        return TaskHandlers::UpdateIndexHandler(*m_context);
    default:
        fmWarning() << "[TaskManager::getTaskHandler] Unknown task type:" << static_cast<int>(type);
        return nullptr;
    }
}

QString TaskManager::typeToString(IndexTask::Type type)
{
    switch (type) {
    case IndexTask::Type::Create:
        return "create";
    case IndexTask::Type::Update:
        return "update";
    case IndexTask::Type::CreateFileList:
        return "create-file-list";
    case IndexTask::Type::UpdateFileList:
        return "update-file-list";
    case IndexTask::Type::RemoveFileList:
        return "remove-file-list";
    case IndexTask::Type::MoveFileList:
        return "move-file-list";
    default:
        fmWarning() << "[TaskManager::typeToString] Unknown task type:" << static_cast<int>(type);
        return "unknown";
    }
}

void TaskManager::onTaskProgress(IndexTask::Type type, qint64 count, qint64 total)
{
    if (!currentTask) {
        fmWarning() << "[TaskManager::onTaskProgress] Received progress update but no current task exists";
        return;
    }

    emit taskProgressChanged(typeToString(type), currentTask->taskPath(), count, total);
}

void TaskManager::onTaskFinished(IndexTask::Type type, HandlerResult result)
{
    if (!currentTask) {
        fmWarning() << "[TaskManager::onTaskFinished] Received task finished signal but no current task exists";
        return;
    }

    const QString taskPath = currentTask->taskPath();
    fmInfo() << "[TaskManager::onTaskFinished] Task finished - type:" << static_cast<int>(type)
             << "path:" << taskPath << "success:" << result.success << "interrupted:" << result.interrupted;

    // 处理索引损坏：若已启动重建任务则提前返回
    if (handleCorruptedIndex(type, result, taskPath))
        return;

    fmDebug() << "[TaskManager::onTaskFinished] Task" << typeToString(type) << "for path" << taskPath
              << (result.success ? "completed successfully" : "failed");

    handleRootPathFailure(result.success, result.interrupted, taskPath);
    updateIndexStatusOnSuccess(type, result);

    emit taskFinished(typeToString(type), taskPath, result.success);
    cleanupTask();

    if (startNextTask()) {
        fmInfo() << "[TaskManager::onTaskFinished] Started next queued task";
    } else {
        fmDebug() << "[TaskManager::onTaskFinished] No more tasks in queue";
        finalizeIndexState(type, result);
    }
}

bool TaskManager::handleCorruptedIndex(IndexTask::Type type, const HandlerResult &result, const QString &taskPath)
{
    if (result.success || type != IndexTask::Type::Update)
        return false;
    if (!currentTask->isIndexCorrupted()) {
        fmInfo() << "[TaskManager::onTaskFinished] Update task failed but index is not corrupted, skipping rebuild - path:" << taskPath;
        return false;
    }

    fmWarning() << "[TaskManager::onTaskFinished] Update task failed due to index corruption, attempting rebuild - path:" << taskPath;

    if (m_context && m_context->stateStore())
        m_context->stateStore()->clearIndexDirectory();

    cleanupTask();

    if (!taskQueue.isEmpty() && taskQueue.head().pathList.contains(taskPath)) {
        fmInfo() << "[TaskManager::onTaskFinished] Found queued task containing corrupted path, letting queue handle rebuild";
    } else {
        fmInfo() << "[TaskManager::onTaskFinished] Starting rebuild task for corrupted index - path:" << taskPath;
        if (startTask(IndexTask::Type::Create, taskPath))
            return true;   // 重建任务已启动
        fmCritical() << "[TaskManager::onTaskFinished] Failed to start rebuild task for path:" << taskPath;
    }

    return false;
}

void TaskManager::handleRootPathFailure(bool success, bool interrupted, const QString &taskPath)
{
    if (success || interrupted)
        return;
    if (!m_context || !m_context->profile().isPathInScope(taskPath))
        return;

    fmWarning() << "[TaskManager::onTaskFinished] Root indexing failed, clearing status - path:" << taskPath;
    if (m_context->stateStore())
        m_context->stateStore()->removeIndexStatusFile();
}

void TaskManager::updateIndexStatusOnSuccess(IndexTask::Type type, const HandlerResult &result)
{
    if (!result.success)
        return;
    if (result.interrupted && type != IndexTask::Type::Create)
        return;
    if (!result.indexChanged && !isFullScanTask(type)) {
        fmDebug() << "[TaskManager::onTaskFinished] Task completed with no index changes, skipping status update";
        return;
    }

    fmDebug() << "[TaskManager::onTaskFinished] Task completed with actual index changes, updating index status";
    if (!m_context || !m_context->stateStore())
        return;

    // Full-scan tasks (Create/Update) update version number;
    // Incremental tasks only update last update time to avoid version mismatch
    // when recovery from a previous interrupted full-scan task is pending.
    if (isFullScanTask(type)) {
        m_context->stateStore()->saveIndexStatus(QDateTime::currentDateTime());
    } else {
        m_context->stateStore()->saveLastUpdateTime(QDateTime::currentDateTime());
    }
}

void TaskManager::finalizeIndexState(IndexTask::Type type, const HandlerResult &result)
{
    // Only set Clean state when:
    // 1. Task completed successfully without interruption
    // 2. No recovery is pending (or this is the recovery task completing)
    if (!result.success || result.interrupted)
        return;

    if (isFullScanTask(type)) {
        m_recoveryPending = false;
        if (m_context && m_context->stateStore()) {
            m_context->stateStore()->setIndexState(IndexUtility::IndexState::Clean);
            m_context->stateStore()->setCreateInProgress(false);
        }
        fmInfo() << "[TaskManager::onTaskFinished] Full-scan task completed, index state set to clean";
    } else if (!m_recoveryPending) {
        if (m_context && m_context->stateStore())
            m_context->stateStore()->setIndexState(IndexUtility::IndexState::Clean);
        fmInfo() << "[TaskManager::onTaskFinished] Incremental task completed, index state set to clean";
    } else {
        fmInfo() << "[TaskManager::onTaskFinished] Incremental task completed but recovery is pending, keeping Dirty state";
    }
}

bool TaskManager::hasRunningTask() const
{
    return currentTask && currentTask->isRunning();
}

bool TaskManager::hasQueuedTasks() const
{
    return !taskQueue.isEmpty();
}

void TaskManager::setRecoveryPending(bool pending)
{
    m_recoveryPending = pending;
    fmInfo() << "[TaskManager] Recovery pending state set to:" << pending;
}

bool TaskManager::isRecoveryPending() const
{
    return m_recoveryPending;
}

void TaskManager::setDispatchEnabled(bool enabled)
{
    m_dispatchEnabled = enabled;
    fmInfo() << "[TaskManager] Dispatch enabled set to:" << enabled;
}

bool TaskManager::tryDispatchNext()
{
    return startNextTask();
}

void TaskManager::stopCurrentTask()
{
    if (currentTask) {
        fmInfo() << "[TaskManager::stopCurrentTask] Stopping current task - type:" << static_cast<int>(currentTask->taskType())
                 << "path:" << currentTask->taskPath();
        currentTask->stop();
    } else {
        fmDebug() << "[TaskManager::stopCurrentTask] No current task to stop";
    }
}

std::optional<IndexTask::Type> TaskManager::currentTaskType() const
{
    if (!hasRunningTask()) {
        return std::nullopt;
    }

    return currentTask->taskType();
}

std::optional<QString> TaskManager::currentTaskPath() const
{
    if (!hasRunningTask()) {
        return std::nullopt;
    }

    return currentTask->taskPath();
}

void TaskManager::cleanupTask()
{
    if (currentTask) {
        fmDebug() << "[TaskManager::cleanupTask] Cleaning up task resources - type:" << static_cast<int>(currentTask->taskType())
                  << "path:" << currentTask->taskPath();
        disconnect(this, &TaskManager::startTaskInThread, currentTask, &IndexTask::start);
        currentTask->deleteLater();
        currentTask = nullptr;
        fmDebug() << "[TaskManager::cleanupTask] Task cleanup completed";
    }
}

bool TaskManager::startNextTask()
{
    // 调度器暂停期间不自动派发队列任务，避免在环境不满足时启动任务
    if (!m_dispatchEnabled) {
        fmDebug() << "[TaskManager::startNextTask] Dispatch disabled, keeping" << taskQueue.size() << "queued tasks";
        return false;
    }

    // 检查队列是否为空
    if (taskQueue.isEmpty()) {
        fmDebug() << "[TaskManager::startNextTask] No tasks in queue";
        return false;
    }

    // 从队列中取出下一个任务
    TaskQueueItem nextTask = taskQueue.dequeue();

    fmInfo() << "[TaskManager::startNextTask] Starting next queued task - type:" << static_cast<int>(nextTask.type)
             << "remaining in queue:" << taskQueue.count();

    // 统一经 submit 分发，保留 grade/resourceControl 等策略字段
    return submit(nextTask);
}

bool TaskManager::isFullScanTask(IndexTask::Type type) const
{
    return type == IndexTask::Type::Create || type == IndexTask::Type::Update;
}

bool TaskManager::enqueueCompensationTask(const QStringList &paths, bool silent)
{
    if (paths.isEmpty()) {
        return false;
    }

    taskQueue.enqueue(createCompensationTaskItem(paths, silent));
    fmInfo() << "[TaskManager::enqueueCompensationTask] Queued directory compensation update for"
             << paths.size() << "path(s), primary:" << paths.first();
    return true;
}

QStringList TaskManager::applyDirectoryMovePlans(const QHash<QString, QString> &movedFiles)
{
    const QList<TaskQueueUtils::DirectoryMovePlan> plans = TaskQueueUtils::buildDirectoryMovePlans(movedFiles);
    if (plans.isEmpty()) {
        return {};
    }

    QStringList compensationPaths;

    for (const TaskQueueUtils::DirectoryMovePlan &plan : plans) {
        const bool rewroteQueuedTasks = TaskQueueUtils::rewriteQueuedTasksForDirectoryMove(taskQueue,
                                                                                           plan.fromPath,
                                                                                           plan.toPath);
        if (rewroteQueuedTasks) {
            fmInfo() << "[TaskManager::applyDirectoryMovePlans] Rewrote queued task paths for directory move:"
                     << plan.fromPath << "->" << plan.toPath;
        }

        if (!compensationPaths.contains(plan.toPath)) {
            compensationPaths.append(plan.toPath);
        }
    }

    return compensationPaths;
}
