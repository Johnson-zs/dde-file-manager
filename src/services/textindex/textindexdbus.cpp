// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textindexdbus.h"
#include "private/textindexdbus_p.h"
#include "utils/indexutility.h"
#include "utils/systemdcpuutils.h"
#include "utils/textindexconfig.h"

#include <QDir>
#include <QDBusConnection>

SERVICETEXTINDEX_BEGIN_NAMESPACE
DFM_LOG_REGISTER_CATEGORY(SERVICETEXTINDEX_NAMESPACE)
SERVICETEXTINDEX_END_NAMESPACE

SERVICETEXTINDEX_USE_NAMESPACE
using namespace Lucene;

void TextIndexDBusPrivate::initialize()
{
    runtime->fsEventController()->setupFSEventCollector();
    initializeSupportedExtensions();

    // Check for dirty state at startup and set recovery pending flag
    // This must be done before any incremental task can complete and clear the Dirty state
    const IndexUtility::IndexState state = runtime->stateStore().getIndexState();
    if (state != IndexUtility::IndexState::Clean) {
        fmInfo() << "TextIndexDBus: Dirty state detected at startup, setting recovery pending flag";
        runtime->taskManager()->setRecoveryPending(true);
    }
}

void TextIndexDBusPrivate::initConnect()
{
    // Task lifecycle & status notifications come from the scheduler, which
    // centralises CPU-quota reset (process-level cgroup, design R8) and
    // pause/resume bookkeeping.
    QObject::connect(runtime->scheduler(), &TaskScheduler::taskFinished,
                     q, [this](const QString &type, const QString &path, bool success) {
                         emit q->TaskFinished(type, path, success);
                     });

    QObject::connect(runtime->scheduler(), &TaskScheduler::taskProgressChanged,
                     q, [this](const QString &type, const QString &path, qint64 count, qint64 total) {
                         emit q->TaskProgressChanged(type, path, count, total);
                     });

    QObject::connect(runtime->scheduler(), &TaskScheduler::statusChanged,
                     q, &TextIndexDBus::IndexStatusChanged);

    QObject::connect(runtime->fsEventController(), &FSEventController::requestProcessFileChanges,
                     q, &TextIndexDBus::ProcessFileChanges);
    QObject::connect(runtime->fsEventController(), &FSEventController::requestProcessFileMoves,
                     q, &TextIndexDBus::ProcessFileMoves);
    QObject::connect(runtime->fsEventController(), &FSEventController::monitoring,
                     q, [this](bool start) {
                         handleMonitoring(start);
                     });
    QObject::connect(runtime->fsEventController(), &FSEventController::requestSlientStart,
                     q, [this]() {
                         handleSlientStart();
                     });

    // Connect to TextIndexConfig changes
    QObject::connect(&TextIndexConfig::instance(), &TextIndexConfig::configChanged,
                     q, [this]() {
                         handleConfigChanged();
                     });
    QObject::connect(IndexUtility::AnythingConfigWatcher::instance(), &IndexUtility::AnythingConfigWatcher::rebuildRequired,
                     q, [this](const QString &reason) {
                         fmInfo() << "TextIndexDBus: ANYTHING config changed, marking rebuild required. reason:" << reason;
                         runtime->stateStore().setNeedsRebuild(true);
                     });
    QObject::connect(IndexUtility::DlnfsConfigWatcher::instance(), &IndexUtility::DlnfsConfigWatcher::rebuildRequired,
                     q, [this](const QString &reason) {
                         fmInfo() << "TextIndexDBus: DLNFS config changed, marking rebuild required. reason:" << reason;
                         runtime->stateStore().setNeedsRebuild(true);
                     });
}

void TextIndexDBusPrivate::handleMonitoring(bool start)
{
    fmInfo() << "TextIndexDBus: FS event monitoring state changed to:" << start;
    if (!start) {
        runtime->fsEventController()->stopFSMonitoring();
        return;
    }

    runtime->fsEventController()->startFSMonitoring();
}

void TextIndexDBusPrivate::handleSlientStart()
{
    // NOTE: Used only for silent updates after the service is started for the first time!
    static std::once_flag flag;
    std::call_once(flag, [this]() {
        const auto &configuredDirs = DFMSEARCH::Global::defaultIndexedDirectory();
        QStringList pathsToProcess;

        if (configuredDirs.isEmpty()) {
            pathsToProcess.append(QDir::homePath());
        } else {
            pathsToProcess = configuredDirs;
        }

        if (!canSilentlyRefreshIndex(pathsToProcess.first())) {
            fmWarning() << "TextIndexDBus: Unable to refresh index, task already running for:" << pathsToProcess.first();
            return;
        }

        // Boot / crash recovery is graded and gated by the scheduler (design §6.5):
        //   !dbExists          → Create(Heavy)
        //   needsRebuild        → Update(Heavy)
        //   createInProgress    → Update(Heavy)
        //   dirty               → Update(Light) compare
        //   clean               → skip
        if (!runtime->scheduler()->submitBootRecovery(pathsToProcess)) {
            fmInfo() << "TextIndexDBus: Clean state and no config changes, skipping global update";
        }
    });
}

bool TextIndexDBusPrivate::canSilentlyRefreshIndex(const QString &path) const
{
    if (auto taskTypeOpt = runtime->taskManager()->currentTaskType(); taskTypeOpt.has_value()) {
        if (auto taskPathOpt = runtime->taskManager()->currentTaskPath(); taskPathOpt.has_value()) {
            const auto &type = *taskTypeOpt;
            const auto &taskPath = *taskPathOpt;

            if ((type == IndexTask::Type::Create || type == IndexTask::Type::Update) && (taskPath == path)) {
                return false;
            }
        }
    }

    return true;
}

TextIndexDBus::TextIndexDBus(EnvDetector *envDetector, QObject *parent)
    : QObject(parent), QDBusContext(), d(new TextIndexDBusPrivate(this, envDetector))
{
    QDBusConnection::RegisterOptions opts =
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties;

    QDBusConnection::sessionBus().registerObject(Defines::kTextIndexDBusObjectPath, this, opts);
}

TextIndexDBus::~TextIndexDBus() { }

void TextIndexDBus::cleanup()
{
    d->runtime->fsEventController()->setEnabledNow(false);

    // Check if there are unfinished tasks before stopping
    bool hasUnfinishedWork = d->runtime->taskManager()->hasRunningTask()
            || d->runtime->taskManager()->hasQueuedTasks();

    if (hasUnfinishedWork) {
        fmWarning() << "TextIndexDBus: Service cleanup with unfinished indexing work, marking state as dirty";
        d->runtime->stateStore().setIndexState(IndexUtility::IndexState::Dirty);
    }

    StopCurrentTask();
}

void TextIndexDBus::Init()
{
    // 预防启动时没有开启全文检索，后续手动去开启全文检索，将造成 2 次索引
    d->runtime->fsEventController()->setSilentlyRefreshStarted(true);
}

bool TextIndexDBus::IsEnabled()
{
    return d->runtime->fsEventController()->isEnabled();
}

void TextIndexDBus::SetEnabled(bool enabled)
{
    d->runtime->fsEventController()->setEnabled(enabled);
}

bool TextIndexDBus::CreateIndexTask(const QStringList &paths, const QVariantMap &options)
{
    return d->runtime->scheduler()->submit(IndexTask::Type::Create, paths, options);
}

bool TextIndexDBus::UpdateIndexTask(const QStringList &paths, const QVariantMap &options)
{
    return d->runtime->scheduler()->submit(IndexTask::Type::Update, paths, options);
}

bool TextIndexDBus::StopCurrentTask()
{
    if (!d->runtime->taskManager()->hasRunningTask())
        return false;

    d->runtime->taskManager()->stopCurrentTask();
    return true;
}

bool TextIndexDBus::HasRunningTask()
{
    return d->runtime->taskManager()->hasRunningTask();
}

bool TextIndexDBus::IndexDatabaseExists()
{
    // First check if the index files exist
    if (!d->runtime->profile().isIndexAvailable()) {
        return false;
    }

    // Then check if the version is compatible
    if (!d->runtime->stateStore().isCompatibleVersion()) {
        fmWarning() << "TextIndexDBus: Index database exists but version is incompatible."
                    << "Current version:" << d->runtime->profile().runtimeIndexVersion()
                    << "Stored version:" << d->runtime->stateStore().getIndexVersion()
                    << "Index considered invalid due to version mismatch";
        return false;
    }

    if (d->runtime->stateStore().getLastUpdateTime().isEmpty()) {
        fmWarning() << "TextIndexDBus: Last update time is empty, index may be corrupted";
        return false;
    }

    return true;
}

QString TextIndexDBus::GetLastUpdateTime()
{
    return d->runtime->stateStore().getLastUpdateTime();
}

QString TextIndexDBus::GetIndexStatus()
{
    return d->runtime->scheduler()->statusJson();
}

bool TextIndexDBus::ContinueUpdate()
{
    return d->runtime->scheduler()->continueUpdate();
}

bool TextIndexDBus::UpdateImmediately(const QStringList &paths)
{
    return d->runtime->scheduler()->updateImmediately(paths);
}

bool TextIndexDBus::RebuildIndex(const QStringList &paths, const QVariantMap &options)
{
    return d->runtime->scheduler()->rebuildIndex(paths, options);
}

bool TextIndexDBus::ProcessFileChanges(const QStringList &createdFiles,
                                       const QStringList &modifiedFiles,
                                       const QStringList &deletedFiles)
{
    return d->runtime->scheduler()->submitIncrementalBatch(createdFiles, modifiedFiles,
                                                           deletedFiles, {});
}

bool TextIndexDBus::ProcessFileMoves(const QHash<QString, QString> &movedFiles)
{
    if (movedFiles.isEmpty()) {
        fmDebug() << "TextIndexDBus: No file moves to process";
        return false;
    }

    fmInfo() << "TextIndexDBus: Processing" << movedFiles.size() << "moved files";
    return d->runtime->scheduler()->submitIncrementalBatch({}, {}, {}, movedFiles);
}

void TextIndexDBusPrivate::initializeSupportedExtensions()
{
    m_currentSupportedExtensions = TextIndexConfig::instance().supportedTextFileExtensions();
    fmInfo() << "TextIndexDBus: Initialized supported file extensions (" << m_currentSupportedExtensions.size() << "):"
             << m_currentSupportedExtensions;
}

void TextIndexDBusPrivate::handleConfigChanged()
{
    const auto newSupportedExtensions = TextIndexConfig::instance().supportedTextFileExtensions();

    // Convert to sets for order-insensitive comparison
    const QSet<QString> currentExtensionsSet(m_currentSupportedExtensions.begin(), m_currentSupportedExtensions.end());
    const QSet<QString> newExtensionsSet(newSupportedExtensions.begin(), newSupportedExtensions.end());

    // Check if supported file extensions have changed (order-insensitive)
    if (currentExtensionsSet != newExtensionsSet) {
        fmInfo() << "TextIndexDBus: Supported file extensions changed from" << m_currentSupportedExtensions.size()
                 << "to" << newSupportedExtensions.size() << "extensions";

        // Update stored extensions
        m_currentSupportedExtensions = newSupportedExtensions;

        // Trigger index update for configured directories
        const auto &configuredDirs = DFMSEARCH::Global::defaultIndexedDirectory();
        QStringList pathsToProcess;

        if (configuredDirs.isEmpty()) {
            pathsToProcess.append(QDir::homePath());
        } else {
            pathsToProcess = configuredDirs;
        }

        // Only start update task if index database exists
        if (q->IndexDatabaseExists()) {
            fmInfo() << "TextIndexDBus: Starting index update task due to supported file extensions change for paths:" << pathsToProcess;
            runtime->scheduler()->submit(IndexTask::Type::Update, pathsToProcess);
        } else {
            fmWarning() << "TextIndexDBus: Cannot start index update task, index database does not exist";
        }
    }
}
