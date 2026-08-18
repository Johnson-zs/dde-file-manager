// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENVGATE_H
#define ENVGATE_H

#include "service_textindex_global.h"
#include "env/envdetector.h"
#include "task/taskgrade.h"

#include <QString>

SERVICETEXTINDEX_BEGIN_NAMESPACE

/**
 * @brief Why a task is currently blocked from running.
 */
enum class WaitReason {
    None,        ///< Not blocked – task may run
    Power,       ///< Running on battery (policy requires power-off)
    PowerSave,   ///< Power-save mode active (policy requires power-save off)
    Idle,        ///< System not idle (policy requires idle)
    Upgrade      ///< Waiting for index service upgrade (version mismatch)
};

/**
 * @brief Result of evaluating the environment gate for a task.
 */
struct GateDecision
{
    bool allow { true };
    WaitReason reason { WaitReason::None };
};

inline QString waitReasonToString(WaitReason r)
{
    switch (r) {
    case WaitReason::None: return QStringLiteral("none");
    case WaitReason::Power: return QStringLiteral("power");
    case WaitReason::PowerSave: return QStringLiteral("power-save");
    case WaitReason::Idle: return QStringLiteral("idle");
    case WaitReason::Upgrade: return QStringLiteral("upgrade");
    }
    return QStringLiteral("none");
}

/**
 * @brief Evaluate the environment gate.
 *
 * When @p bypassEnv is true the gate is unconditionally open (Manual /
 * ContinueUpdate one-shot bypass). Otherwise the first failing policy
 * requirement determines the WaitReason, evaluated in a stable order:
 * power → power-save → idle.
 *
 * Note: this function is pure and has no side effects, making it trivial
 * to unit-test (see test_envgate).
 */
inline GateDecision evaluateGate(const EnvState &env,
                                 const GradePolicy &policy,
                                 bool bypassEnv)
{
    GateDecision decision;
    if (bypassEnv) {
        decision.allow = true;
        decision.reason = WaitReason::None;
        return decision;
    }

    if (policy.requirePowerOff && env.onBattery) {
        decision.allow = false;
        decision.reason = WaitReason::Power;
        return decision;
    }
    if (policy.requirePowerSaveOff && env.powerSaveMode) {
        decision.allow = false;
        decision.reason = WaitReason::PowerSave;
        return decision;
    }
    if (policy.requireIdle && !env.idle) {
        decision.allow = false;
        decision.reason = WaitReason::Idle;
        return decision;
    }

    decision.allow = true;
    decision.reason = WaitReason::None;
    return decision;
}

SERVICETEXTINDEX_END_NAMESPACE

#endif   // ENVGATE_H
