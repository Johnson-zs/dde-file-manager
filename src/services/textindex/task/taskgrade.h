// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TASKGRADE_H
#define TASKGRADE_H

#include "service_textindex_global.h"
#include "task/indextask.h"

#include <QString>
#include <QtGlobal>

SERVICETEXTINDEX_BEGIN_NAMESPACE

/**
 * @brief Coarse classification of an indexing task used for environment
 *        gating and resource control.
 *
 * The grade is computed by TaskGrader from the task type, the incremental
 * scale (for file-list tasks) and the boot-recovery context. It is then
 * mapped to a GradePolicy that decides which environment conditions must
 * hold before the task is allowed to run.
 */
enum class TaskGrade {
    Light,   ///< Lightweight incremental (small file-list / dirty compare-update)
    Medium,   ///< Medium incremental (file-list above the light threshold)
    Heavy,   ///< Heavy (first build / full rebuild / version migration / createInProgress recovery)
    Manual   ///< User-triggered immediate update (one-shot env bypass, no resource control)
};

/**
 * @brief Size measurement of a 3-second de-duplicated flush window.
 *
 * Only files that need content/OCR parsing (created + modified) are counted.
 * Remove/Move tasks are always Light and do not contribute to the scale.
 */
struct IncrementalScale
{
    int parseFileCount { 0 };   ///< Number of parseable files (created ∪ modified)
    qint64 parseTotalBytes { 0 };   ///< Total size in bytes of those files
};

/**
 * @brief Environment requirements and resource-control switch for a grade.
 */
struct GradePolicy
{
    bool requirePowerOff { false };   ///< Pause on battery (require !onBattery)
    bool requirePowerSaveOff { false };   ///< Pause in power-save mode
    bool requireIdle { false };   ///< Require system idle
    bool resourceControl { true };   ///< Enable CPU/IO/memory resource control
    bool bypassable { false };   ///< Allow one-shot manual env bypass
};

/**
 * @brief Convert a grade to a stable string for logging / JSON.
 */
inline QString taskGradeToString(TaskGrade g)
{
    switch (g) {
    case TaskGrade::Light:
        return QStringLiteral("light");
    case TaskGrade::Medium:
        return QStringLiteral("medium");
    case TaskGrade::Heavy:
        return QStringLiteral("heavy");
    case TaskGrade::Manual:
        return QStringLiteral("manual");
    }
    return QStringLiteral("unknown");
}

/**
 * @brief The fixed grade → policy mapping (see design §4.2).
 *
 *  | Grade     | PowerOff | PowerSaveOff | Idle | ResourceCtrl | Bypassable |
 *  |-----------|:--------:|:------------:|:----:|:------------:|:----------:|
 *  | Light     |    ✗     |      ✓       |  ✗   |      ✓       |     ✓      |
 *  | Medium    |    ✓     |      ✓       |  ✓   |      ✓       |     ✓      |
 *  | Heavy     |    ✓     |      ✓       |  ✓   |      ✓       |     ✓      |
 *  | Manual    |    ✗     |      ✗       |  ✗   |      ✗       |     —      |
 */
inline GradePolicy policyFor(TaskGrade grade)
{
    switch (grade) {
    case TaskGrade::Light:
        return { false, true, false, true, true };
    case TaskGrade::Medium:
        return { true, true, true, true, true };
    case TaskGrade::Heavy:
        return { true, true, true, true, true };
    case TaskGrade::Manual:
        return { false, false, false, false, false };
    }
    return { false, false, false, true, true };
}

/**
 * @brief Stateless task grader.
 *
 * The grader turns a task description into a TaskGrade. It reads the
 * light-incremental thresholds from TextIndexConfig at call time so that
 * dconfig changes take effect immediately (design R9).
 */
class TaskGrader
{
public:
    /**
     * @brief Construct a grader bound to a profile type.
     * @param isOcr Whether the grader serves the OCR profile (selects the
     *              OCR file-count limit instead of the content limit).
     */
    explicit TaskGrader(bool isOcr)
        : m_isOcr(isOcr) { }

    /**
     * @brief Grade a full-scan or boot-recovery task.
     *
     * Mapping (design §6.1):
     *  - Create                                 → Heavy
     *  - Update (boot recovery, createInProgress) → Heavy
     *  - Update (boot recovery, dirty only)     → Light
     *  - Update (needsRebuild / config change)  → Heavy
     *  - Update (plain compare)                 → Light
     */
    TaskGrade gradeFor(IndexTask::Type type,
                       bool bootRecovery,
                       bool needsRebuild,
                       bool createInProgress) const
    {
        switch (type) {
        case IndexTask::Type::Create:
            return TaskGrade::Heavy;
        case IndexTask::Type::Update:
            if (bootRecovery && createInProgress)
                return TaskGrade::Heavy;
            if (needsRebuild)
                return TaskGrade::Heavy;
            // dirty-only compare or plain compare → Light
            return TaskGrade::Light;
        default:
            return TaskGrade::Light;
        }
    }

    /**
     * @brief Grade an incremental file-list task from its measured scale.
     *
     * RemoveFileList / MoveFileList are always Light (no parsing).
     * CreateFileList / UpdateFileList are Light when below both the
     * file-count and size thresholds, otherwise Medium.
     */
    TaskGrade gradeForFileList(IndexTask::Type type, const IncrementalScale &scale) const
    {
        if (type == IndexTask::Type::RemoveFileList || type == IndexTask::Type::MoveFileList)
            return TaskGrade::Light;

        const int fileLimit = m_isOcr ? lightIncrementalOcrFileLimit()
                                      : lightIncrementalContentFileLimit();
        const qint64 sizeLimitBytes = static_cast<qint64>(lightIncrementalSizeLimitMB()) * 1024 * 1024;

        if (scale.parseFileCount <= fileLimit && scale.parseTotalBytes <= sizeLimitBytes)
            return TaskGrade::Light;
        return TaskGrade::Medium;
    }

private:
    int lightIncrementalContentFileLimit() const;
    int lightIncrementalOcrFileLimit() const;
    int lightIncrementalSizeLimitMB() const;

    bool m_isOcr;
};

SERVICETEXTINDEX_END_NAMESPACE

#endif   // TASKGRADE_H
