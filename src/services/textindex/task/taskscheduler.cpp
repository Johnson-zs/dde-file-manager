// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "task/taskscheduler.h"
#include "utils/systemdcpuutils.h"
#include "utils/textindexconfig.h"
#include "utils/indexutility.h"

#include <dfm-search/dsearch_global.h>

#include <QFileInfo>
#include <QSet>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>

SERVICETEXTINDEX_USE_NAMESPACE

namespace {

QStringList defaultIndexedPaths()
{
    const auto &configured = DFMSEARCH::Global::defaultIndexedDirectory();
    if (configured.isEmpty())
        return { QDir::homePath() };
    return configured;
}

}   // namespace

TaskScheduler::TaskScheduler(TaskManager *taskManager,
                             EnvDetector *envDetector,
                             const IndexStateStore *stateStore,
                             IndexProfile profile,
                             QObject *parent)
    : QObject(parent),
      m_tm(taskManager),
      m_env(envDetector),
      m_store(stateStore),
      m_profile(std::move(profile)),
      m_grader(m_profile.type() == IndexProfile::Type::Ocr)
{
    Q_ASSERT(m_tm);
    qRegisterMetaType<EnvState>();   // required for the queued envStateChanged connection
    fmInfo() << "[TaskScheduler] Created for profile:" << m_profile.id()
             << "isOcr:" << (m_profile.type() == IndexProfile::Type::Ocr);

    connect(m_tm, &TaskManager::taskFinished, this, &TaskScheduler::onTaskFinished, Qt::QueuedConnection);
    connect(m_tm, &TaskManager::taskProgressChanged, this, &TaskScheduler::onTaskProgress, Qt::QueuedConnection);

    if (m_env) {
        m_envState = m_env->currentState();
        connect(m_env, &EnvDetector::envStateChanged, this, &TaskScheduler::onEnvStateChanged, Qt::QueuedConnection);
    }
}

// ---- submission ----

bool TaskScheduler::submit(IndexTask::Type type, const QStringList &paths,
                           const QVariantMap &options)
{
    const bool manualImmediate = options.value("manualImmediate", false).toBool();
    const bool bypassOpt = options.value("bypassEnv", false).toBool();

    TaskGrade grade;
    bool bypassEnv = false;
    bool resourceControl = true;

    if (manualImmediate) {
        // Decision 1: Manual grade, full bypass, no rate limiting.
        grade = TaskGrade::Manual;
        bypassEnv = true;
        resourceControl = TextIndexConfig::instance().manualImmediateResourceControl();
    } else {
        grade = m_grader.gradeFor(type, /*bootRecovery*/ false,
                                  m_store ? m_store->needsRebuild() : false,
                                  m_store ? m_store->isCreateInProgress() : false);
        bypassEnv = bypassOpt;
        resourceControl = options.value("resourceControl", policyFor(grade).resourceControl).toBool();
    }

    TaskQueueItem item;
    item.type = type;
    item.pathList = paths;
    item.path = paths.isEmpty() ? QString() : paths.first();
    item.silent = options.value("silent", false).toBool();
    item.grade = grade;
    item.bypassEnv = bypassEnv;
    item.resourceControl = resourceControl;
    item.manualImmediate = manualImmediate;

    fmInfo() << "[TaskScheduler::submit] type:" << static_cast<int>(type)
             << "grade:" << taskGradeToString(grade)
             << "bypassEnv:" << bypassEnv << "resourceControl:" << resourceControl;
    return tryDispatch(item);
}

bool TaskScheduler::submitIncrementalBatch(const QStringList &created,
                                           const QStringList &modified,
                                           const QStringList &deleted,
                                           const QHash<QString, QString> &moved)
{
    if (created.isEmpty() && modified.isEmpty() && deleted.isEmpty() && moved.isEmpty())
        return false;

    // Grade the parseable set (created ∪ modified) as a whole (design §6.1).
    const IncrementalScale scale = computeScale(created, modified);
    const TaskGrade parseGrade = m_grader.gradeForFileList(IndexTask::Type::CreateFileList, scale);
    const bool parseResourceControl = policyFor(parseGrade).resourceControl;
    const bool lightResourceControl = policyFor(TaskGrade::Light).resourceControl;

    fmInfo() << "[TaskScheduler::submitIncrementalBatch] scale files:" << scale.parseFileCount
             << "bytes:" << scale.parseTotalBytes << "parseGrade:" << taskGradeToString(parseGrade)
             << "created:" << created.size() << "modified:" << modified.size()
             << "deleted:" << deleted.size() << "moved:" << moved.size();

    bool accepted = false;

    // Moves and removes are always Light (no content parsing).
    if (!moved.isEmpty()) {
        TaskQueueItem item;
        item.type = IndexTask::Type::MoveFileList;
        item.path = QString("MoveList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
        item.movedFiles = moved;
        item.silent = true;
        item.grade = TaskGrade::Light;
        item.resourceControl = lightResourceControl;
        tryDispatch(item);
        accepted = true;
    }

    if (!deleted.isEmpty()) {
        TaskQueueItem item;
        item.type = IndexTask::Type::RemoveFileList;
        item.path = QString("FileList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
        item.fileList = deleted;
        item.silent = true;
        item.grade = TaskGrade::Light;
        item.resourceControl = lightResourceControl;
        tryDispatch(item);
        accepted = true;
    }

    if (!created.isEmpty()) {
        TaskQueueItem item;
        item.type = IndexTask::Type::CreateFileList;
        item.path = QString("FileList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
        item.fileList = created;
        item.silent = true;
        item.grade = parseGrade;
        item.resourceControl = parseResourceControl;
        tryDispatch(item);
        accepted = true;
    }

    if (!modified.isEmpty()) {
        TaskQueueItem item;
        item.type = IndexTask::Type::UpdateFileList;
        item.path = QString("FileList-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
        item.fileList = modified;
        item.silent = true;
        item.grade = parseGrade;
        item.resourceControl = parseResourceControl;
        tryDispatch(item);
        accepted = true;
    }

    return accepted;
}

bool TaskScheduler::submitBootRecovery(const QStringList &paths)
{
    const bool indexExists = m_profile.isIndexAvailable();
    const bool versionOk = m_store && m_store->isCompatibleVersion();
    const bool hasUpdateTime = m_store && !m_store->getLastUpdateTime().isEmpty();

    const bool needsRebuild = m_store && m_store->needsRebuild();
    const bool createInProgress = m_store && m_store->isCreateInProgress();
    const bool dirty = m_store && m_store->getIndexState() == IndexUtility::IndexState::Dirty;

    // Directional version check (design §4.5 / §7.1 "等待索引服务升级"):
    //   storedVer > runtimeVer → index built by a NEWER service than the one
    //     running. Auto-rebuilding would destroy a newer-format index, so we
    //     must NOT rebuild; instead report WaitingUpgrade and let the user
    //     act (UpdateImmediately / RebuildIndex) or upgrade the service.
    //   storedVer < runtimeVer → outdated index, rebuild to upgrade schema.
    const int storedVer = m_store ? m_store->getIndexVersion() : -1;
    const int runtimeVer = m_profile.runtimeIndexVersion();
    const bool indexNewerThanService = indexExists && storedVer > runtimeVer;

    if (indexNewerThanService) {
        // Do NOT auto-submit any task: leave the scheduler idle so that
        // recomputeStatus() reports IndexPhase::WaitingUpgrade (§7.1). The
        // user is prompted "等待索引服务升级 → 立即更新索引".
        fmInfo() << "[TaskScheduler::submitBootRecovery] Index built by newer service"
                 << "(stored=" << storedVer << " > runtime=" << runtimeVer << "),"
                 << "reporting WaitingUpgrade instead of auto-rebuild";
        recomputeStatus(false, true);
        emitStatusChanged();
        return true;   // handled (deliberately left in WaitingUpgrade)
    }

    IndexTask::Type type = IndexTask::Type::Update;
    TaskGrade grade = TaskGrade::Light;

    if (!indexExists || (indexExists && !versionOk)) {
        // No physical index, or index built by an older service (storedVer <
        // runtimeVer): rebuild via Create(Heavy) to (re)create / upgrade the
        // schema. This preserves the original "!IndexDatabaseExists → Create"
        // behaviour for the upgrade and missing-index cases (design §6.5).
        type = IndexTask::Type::Create;
        grade = TaskGrade::Heavy;
        fmInfo() << "[TaskScheduler::submitBootRecovery] Index unavailable or outdated"
                 << "(stored=" << storedVer << " runtime=" << runtimeVer << "), Create(Heavy)";
    } else if (!hasUpdateTime) {
        // Physical index with compatible version but no lastUpdateTime: the
        // index is incomplete. Rebuild to guarantee completeness.
        type = IndexTask::Type::Create;
        grade = TaskGrade::Heavy;
        fmInfo() << "[TaskScheduler::submitBootRecovery] Compatible index but no update time, Create(Heavy)";
    } else if (needsRebuild) {
        type = IndexTask::Type::Update;
        grade = TaskGrade::Heavy;
        fmInfo() << "[TaskScheduler::submitBootRecovery] Config changed, Update(Heavy)";
        if (m_store)
            m_store->setNeedsRebuild(false);
    } else if (createInProgress) {
        type = IndexTask::Type::Update;
        grade = TaskGrade::Heavy;
        fmInfo() << "[TaskScheduler::submitBootRecovery] Create in progress, Update(Heavy)";
    } else if (dirty) {
        type = IndexTask::Type::Update;
        grade = TaskGrade::Light;
        fmInfo() << "[TaskScheduler::submitBootRecovery] Dirty state, Update(Light) compare";
    } else {
        fmInfo() << "[TaskScheduler::submitBootRecovery] Clean state, skipping recovery";
        return false;
    }

    TaskQueueItem item;
    item.type = type;
    item.pathList = paths;
    item.path = paths.isEmpty() ? QString() : paths.first();
    item.silent = true;
    item.grade = grade;
    item.bypassEnv = false;
    item.resourceControl = policyFor(grade).resourceControl;
    return tryDispatch(item);
}

// ---- manual operations ----

bool TaskScheduler::continueUpdate()
{
    // Decision 2: full environment bypass (runs even on battery / power-save / non-idle).
    const bool resourceControl = TextIndexConfig::instance().manualResumeResourceControl();
    bool didSomething = false;

    if (m_paused) {
        fmInfo() << "[TaskScheduler::continueUpdate] Resuming paused task with env bypass";
        resumePaused(/*bypassEnv*/ true, resourceControl);
        didSomething = true;
    }

    // Flush parked tasks with a one-shot env bypass as well.
    if (!m_parked.isEmpty()) {
        fmInfo() << "[TaskScheduler::continueUpdate] Dispatching" << m_parked.size() << "parked task(s) with env bypass";
        QList<ParkedTask> parked;
        parked.swap(m_parked);
        m_tm->setDispatchEnabled(true);
        for (const ParkedTask &p : std::as_const(parked)) {
            TaskQueueItem item = p.item;
            item.bypassEnv = true;
            m_tm->submit(item);
        }
        didSomething = true;
    }

    m_tm->setDispatchEnabled(true);
    m_tm->tryDispatchNext();

    recomputeStatus(false, true);
    emitStatusChanged();
    return didSomething;
}

bool TaskScheduler::updateImmediately(const QStringList &paths)
{
    // Decision 1: Manual grade, full bypass, no resource control.
    const bool resourceControl = TextIndexConfig::instance().manualImmediateResourceControl();
    const QStringList usePaths = paths.isEmpty() ? defaultIndexedPaths() : paths;

    fmInfo() << "[TaskScheduler::updateImmediately] Triggering immediate Update(Manual) for" << usePaths.size()
             << "path(s), resourceControl:" << resourceControl;

    // A manual immediate update abandons any paused state and takes priority
    // (mirrors startTask's stop-current-and-clear-queue semantics).
    m_paused.reset();
    m_pauseReason = WaitReason::None;
    m_tm->setDispatchEnabled(true);

    TaskQueueItem item;
    item.type = IndexTask::Type::Update;
    item.pathList = usePaths;
    item.path = usePaths.first();
    item.silent = false;
    item.grade = TaskGrade::Manual;
    item.bypassEnv = true;
    item.resourceControl = resourceControl;
    item.manualImmediate = true;
    m_tm->submit(item);

    recomputeStatus(false, true);
    emitStatusChanged();
    return true;
}

bool TaskScheduler::rebuildIndex(const QStringList &paths, const QVariantMap &options)
{
    const bool bypassEnv = options.value("bypassEnv", false).toBool();
    const QStringList usePaths = paths.isEmpty() ? defaultIndexedPaths() : paths;

    fmInfo() << "[TaskScheduler::rebuildIndex] Triggering Create(Heavy) rebuild for" << usePaths.size()
             << "path(s), bypassEnv:" << bypassEnv;

    m_paused.reset();
    m_pauseReason = WaitReason::None;
    m_tm->setDispatchEnabled(true);

    TaskQueueItem item;
    item.type = IndexTask::Type::Create;
    item.pathList = usePaths;
    item.path = usePaths.first();
    item.silent = false;
    item.grade = TaskGrade::Heavy;
    item.bypassEnv = bypassEnv;
    item.resourceControl = true;   // Heavy always resource-controlled
    // Route through the environment gate (design §5.1.4 / §6.4): bypassEnv
    // defaults to false → a Heavy rebuild is Parked when the environment
    // (power / power-save / idle) is not met; bypassEnv=true → immediate.
    // tryDispatch() already calls recomputeStatus() + emitStatusChanged().
    return tryDispatch(item);
}

// ---- status ----

IndexStatus TaskScheduler::status() const
{
    return m_status;
}

QString TaskScheduler::statusJson() const
{
    return indexStatusToJsonString(m_status);
}

bool TaskScheduler::isActive() const
{
    return m_tm->hasRunningTask() || m_tm->hasQueuedTasks() || m_paused.has_value() || !m_parked.isEmpty();
}

// ---- internal dispatch ----

bool TaskScheduler::tryDispatch(const TaskQueueItem &item)
{
    const GradePolicy policy = policyFor(item.grade);
    const GateDecision decision = evaluate(policy, item.bypassEnv);

    if (!decision.allow) {
        m_parked.append({item, decision.reason});
        fmInfo() << "[TaskScheduler::tryDispatch] Task parked - grade:" << taskGradeToString(item.grade)
                 << "reason:" << waitReasonToString(decision.reason);
        recomputeStatus(false, true);
        emitStatusChanged();
        return true;   // accepted (parked, will run when env allows)
    }

    m_tm->setDispatchEnabled(true);
    m_tm->submit(item);
    recomputeStatus(false, true);
    emitStatusChanged();
    return true;
}

GateDecision TaskScheduler::evaluate(const GradePolicy &policy, bool bypassEnv) const
{
    return evaluateGate(m_envState, policy, bypassEnv);
}

IncrementalScale TaskScheduler::computeScale(const QStringList &created, const QStringList &modified) const
{
    QSet<QString> parseSet;
    parseSet.reserve(created.size() + modified.size());
    for (const QString &p : created)
        parseSet.insert(p);
    for (const QString &p : modified)
        parseSet.insert(p);

    IncrementalScale scale;
    scale.parseFileCount = parseSet.size();
    qint64 bytes = 0;
    for (const QString &p : std::as_const(parseSet)) {
        const QFileInfo fi(p);
        if (fi.exists())
            bytes += fi.size();
    }
    scale.parseTotalBytes = bytes;
    return scale;
}

// ---- environment change handling ----

void TaskScheduler::onEnvStateChanged(const EnvState &state)
{
    fmInfo() << "[TaskScheduler::onEnvStateChanged] battery:" << state.onBattery
             << "powerSave:" << state.powerSaveMode << "idle:" << state.idle;
    m_envState = state;

    // 1. Pause a running task whose policy no longer holds (unless bypassed).
    if (m_tm->hasRunningTask() && !m_paused && !m_tm->runningBypassEnv()) {
        const GateDecision d = evaluate(policyFor(m_tm->runningGrade()), false);
        if (!d.allow) {
            requestPauseRunning(d.reason);
        }
    }

    // 2. Resume a paused task when the environment allows again.
    if (m_paused && !m_tm->hasRunningTask()) {
        const TaskGrade grade = m_grader.gradeFor(IndexTask::Type::Update, false,
                                                  m_store ? m_store->needsRebuild() : false,
                                                  m_store ? m_store->isCreateInProgress() : false);
        const GateDecision d = evaluate(policyFor(grade), false);
        if (d.allow) {
            fmInfo() << "[TaskScheduler::onEnvStateChanged] Environment allows, resuming paused task";
            resumePaused(/*bypassEnv*/ false, policyFor(grade).resourceControl);
        } else {
            m_pauseReason = d.reason;
        }
    }

    // 3. Re-evaluate parked tasks: move allowed ones into the executor.
    if (!m_parked.isEmpty()) {
        dispatchParked();
    }

    recomputeStatus(false, true);
    emitStatusChanged();
}

void TaskScheduler::dispatchParked()
{
    if (m_parked.isEmpty())
        return;

    QList<ParkedTask> stillParked;
    bool dispatchedAny = false;
    for (ParkedTask &p : m_parked) {
        const GateDecision d = evaluate(policyFor(p.item.grade), p.item.bypassEnv);
        if (d.allow) {
            m_tm->submit(p.item);
            dispatchedAny = true;
        } else {
            p.reason = d.reason;
            stillParked.append(std::move(p));
        }
    }
    m_parked = std::move(stillParked);

    if (dispatchedAny) {
        m_tm->setDispatchEnabled(true);
        m_tm->tryDispatchNext();
    }
}

void TaskScheduler::requestPauseRunning(WaitReason reason)
{
    if (!m_tm->hasRunningTask())
        return;

    const QStringList paths = m_tm->runningPathList().isEmpty()
            ? defaultIndexedPaths() : m_tm->runningPathList();

    fmInfo() << "[TaskScheduler::requestPauseRunning] Pausing running task - reason:"
             << waitReasonToString(reason) << "paths:" << paths.size();

    // Decision 3: no checkpoint is persisted. The running task is
    // cooperatively stopped via TaskState::stop() (m_running=false); the
    // worker checks isRunning() at batch boundaries and exits with
    // interrupted=true, and Dirty state is preserved. A dedicated
    // pauseRequested flag is unnecessary here because decision 3 (resume
    // always runs a fresh idempotent Update) means there is no need to
    // distinguish "paused by environment" from "stopped by user" inside the
    // worker — the scheduler records the pause separately in m_paused. On
    // resume a fresh idempotent Update(compare) is run, which re-checks
    // mtime and is safe to re-execute.
    m_paused = PausedInfo { paths, m_tm->runningGrade() };
    m_pauseReason = reason;
    m_tm->setDispatchEnabled(false);   // keep queued tasks from auto-starting
    m_tm->stopCurrentTask();           // cooperative stop at batch boundary
}

void TaskScheduler::resumePaused(bool bypassEnv, bool resourceControl)
{
    if (!m_paused)
        return;

    const PausedInfo info = *m_paused;
    m_paused.reset();
    m_pauseReason = WaitReason::None;

    // Resume always runs an Update(compare) – idempotent (decision 3).
    const TaskGrade grade = m_grader.gradeFor(IndexTask::Type::Update, false,
                                              m_store ? m_store->needsRebuild() : false,
                                              m_store ? m_store->isCreateInProgress() : false);

    TaskQueueItem item;
    item.type = IndexTask::Type::Update;
    item.pathList = info.paths;
    item.path = info.paths.isEmpty() ? QString() : info.paths.first();
    item.silent = true;
    item.grade = grade;
    item.bypassEnv = bypassEnv;
    item.resourceControl = resourceControl;

    fmInfo() << "[TaskScheduler::resumePaused] Resuming with Update(compare) - grade:" << taskGradeToString(grade)
             << "bypassEnv:" << bypassEnv << "resourceControl:" << resourceControl;

    m_tm->setDispatchEnabled(true);
    m_tm->submit(item);
}

// ---- task lifecycle ----

void TaskScheduler::onTaskFinished(const QString &type, const QString &path, bool success)
{
    // Always release CPU quota at task end (process-level cgroup).
    QString msg;
    if (!SystemdCpuUtils::resetCpuQuota(Defines::kCgroupServiceName, &msg)) {
        fmWarning() << "[TaskScheduler::onTaskFinished] Failed to reset CPU quota:" << msg;
    }

    if (m_paused) {
        // The task was cooperatively paused (interrupted). Do not treat as
        // failure; Dirty state is already preserved by TaskManager.
        fmInfo() << "[TaskScheduler::onTaskFinished] Task paused (not failed) - type:" << type << "path:" << path;

        // If the environment already allows, resume immediately; otherwise
        // keep waiting (m_pauseReason was set when pausing).
        const TaskGrade grade = m_grader.gradeFor(IndexTask::Type::Update, false,
                                                  m_store ? m_store->needsRebuild() : false,
                                                  m_store ? m_store->isCreateInProgress() : false);
        const GateDecision d = evaluate(policyFor(grade), false);
        if (d.allow) {
            resumePaused(false, policyFor(grade).resourceControl);
        } else {
            m_pauseReason = d.reason;
        }

        recomputeStatus(false, true);
        emitStatusChanged();
        emit taskFinished(type, path, /*success*/ true);   // paused ≠ failure for the UI
        return;
    }

    // Normal completion.
    if (success) {
        m_lastFailed = false;
    } else {
        // Not paused (paused is handled above) → treat as a real failure.
        m_lastFailed = true;
    }

    // The environment may now allow previously parked tasks.
    if (!m_parked.isEmpty()) {
        dispatchParked();
    }
    m_tm->setDispatchEnabled(true);
    m_tm->tryDispatchNext();

    recomputeStatus(false, success);
    emitStatusChanged();
    emit taskFinished(type, path, success);
}

void TaskScheduler::onTaskProgress(const QString &type, const QString &path, qint64 count, qint64 total)
{
    m_status.progressCount = count;
    m_status.progressTotal = total;
    emit taskProgressChanged(type, path, count, total);
    // Progress-only changes are not emitted as a full status JSON to avoid
    // spamming listeners; the DBus TaskProgressChanged signal covers this.
}

void TaskScheduler::recomputeStatus(bool taskJustFailed, bool success)
{
    Q_UNUSED(taskJustFailed)
    if (m_store)
        m_status.lastUpdateTime = m_store->getLastUpdateTime();

    const bool indexExists = m_profile.isIndexAvailable();
    const bool versionOk = m_store && m_store->isCompatibleVersion();
    const bool hasUpdateTime = m_store && !m_store->getLastUpdateTime().isEmpty();
    const bool dbAvailable = indexExists && versionOk && hasUpdateTime;
    m_status.ready = dbAvailable && !m_lastFailed;

    // Determine phase with priority: Paused > Failed > Updating > WaitingUpgrade > Idle.
    if (m_paused || !m_parked.isEmpty()) {
        m_status.phase = IndexPhase::Paused;
        m_status.waitReason = m_paused ? m_pauseReason
                                       : (m_parked.isEmpty() ? WaitReason::None : m_parked.first().reason);
        m_status.grade = m_paused ? m_paused->grade
                                  : (m_parked.isEmpty() ? TaskGrade::Light : m_parked.first().item.grade);
    } else if (m_tm->hasRunningTask()) {
        m_status.phase = IndexPhase::Updating;
        m_status.waitReason = WaitReason::None;
        m_status.grade = m_tm->runningGrade();
    } else if (m_lastFailed) {
        m_status.phase = IndexPhase::Failed;
        m_status.waitReason = WaitReason::None;
        m_status.lastError = QStringLiteral("index update failed");
    } else if (indexExists && !versionOk) {
        // Index present but schema version mismatch → waiting for service upgrade.
        m_status.phase = IndexPhase::WaitingUpgrade;
        m_status.waitReason = WaitReason::Upgrade;
    } else {
        m_status.phase = IndexPhase::Idle;
        m_status.waitReason = WaitReason::None;
    }

    if (success)
        m_status.lastError.clear();
}

void TaskScheduler::emitStatusChanged()
{
    emit statusChanged(statusJson());
}
