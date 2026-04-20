/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Training load & performance metric plugins.
 * All plugins follow the MetricPlugin interface and are auto-registered.
 */

#ifndef TRAININGMETRICS_H
#define TRAININGMETRICS_H

#include "metricplugin.h"

/* ── Acute Load (7-day exponential weighted TRIMP) ───────────────────────── */
class AcuteLoadPlugin : public MetricPlugin
{
public:
    QString id()       const override { return QStringLiteral("acute_load"); }
    QString name()     const override { return QStringLiteral("Acute Load (7-Day)"); }
    QString unit()     const override { return QString(); }
    QString category() const override { return QStringLiteral("training_load"); }
    double  maxValue() const override { return 2000.0; }

    QStringList requiredInputs() const override {
        return { QStringLiteral("daily_loads") };
    }
    QStringList optionalInputs() const override {
        return { QStringLiteral("hr_series"), QStringLiteral("time_s") };
    }

    QVariant compute(const QVariantMap &inputs) override;
    QVariantMap computeDetailed(const QVariantMap &inputs) override;
};

/* ── Chronic Load (42-day exponential weighted TRIMP) ────────────────────── */
class ChronicLoadPlugin : public MetricPlugin
{
public:
    QString id()       const override { return QStringLiteral("chronic_load"); }
    QString name()     const override { return QStringLiteral("Chronic Load (42-Day)"); }
    QString unit()     const override { return QString(); }
    QString category() const override { return QStringLiteral("training_load"); }
    double  maxValue() const override { return 2000.0; }

    QStringList requiredInputs() const override {
        return { QStringLiteral("daily_loads") };
    }

    QVariant compute(const QVariantMap &inputs) override;
};

/* ── Load Balance (ATL / CTL ratio) ──────────────────────────────────────── */
class LoadBalancePlugin : public MetricPlugin
{
public:
    QString id()       const override { return QStringLiteral("acute_load_balance"); }
    QString name()     const override { return QStringLiteral("Load Balance (ATL/CTL)"); }
    QString unit()     const override { return QString(); }
    QString category() const override { return QStringLiteral("training_load"); }
    double  maxValue() const override { return 3.0; }

    QStringList requiredInputs() const override {
        return { QStringLiteral("acute_load"), QStringLiteral("chronic_load") };
    }

    QVariant compute(const QVariantMap &inputs) override;
    QVariantMap computeDetailed(const QVariantMap &inputs) override;
};

/* ── Zone-based TRIMP Load Focus ─────────────────────────────────────────── */
class LoadFocusPlugin : public MetricPlugin
{
public:
    enum Zone { LowAerobic, HighAerobic, Anaerobic };

    explicit LoadFocusPlugin(Zone zone);

    QString id()       const override;
    QString name()     const override;
    QString unit()     const override { return QString(); }
    QString category() const override { return QStringLiteral("training_load"); }
    double  maxValue() const override { return 1000.0; }

    QStringList requiredInputs() const override {
        return { QStringLiteral("hr_series"), QStringLiteral("hr_max"), QStringLiteral("time_s") };
    }

    QVariant compute(const QVariantMap &inputs) override;

private:
    Zone m_zone;
};

/* ── Recovery Time ───────────────────────────────────────────────────────── */
class RecoveryTimePlugin : public MetricPlugin
{
public:
    QString id()       const override { return QStringLiteral("recovery_time"); }
    QString name()     const override { return QStringLiteral("Recovery Time"); }
    QString unit()     const override { return QStringLiteral("h"); }
    QString category() const override { return QStringLiteral("recovery"); }
    double  maxValue() const override { return 96.0; }

    QStringList requiredInputs() const override {
        return { QStringLiteral("workout_load") };
    }
    QStringList optionalInputs() const override {
        return { QStringLiteral("hrv_status"), QStringLiteral("sleep_quality"),
                 QStringLiteral("stress_index") };
    }

    QVariant compute(const QVariantMap &inputs) override;
    QVariantMap computeDetailed(const QVariantMap &inputs) override;
};

/* ── Training Status (categorical) ───────────────────────────────────────── */
class TrainingStatusPlugin : public MetricPlugin
{
public:
    QString id()       const override { return QStringLiteral("training_status"); }
    QString name()     const override { return QStringLiteral("Training Status"); }
    QString unit()     const override { return QString(); }
    QString category() const override { return QStringLiteral("performance"); }

    QStringList requiredInputs() const override {
        return { QStringLiteral("vo2max_trend"), QStringLiteral("acute_load"),
                 QStringLiteral("chronic_load") };
    }
    QStringList optionalInputs() const override {
        return { QStringLiteral("hrv_status") };
    }

    QVariant compute(const QVariantMap &inputs) override;
    QVariantMap computeDetailed(const QVariantMap &inputs) override;
};

/* ── Endurance Score (0-1000) ────────────────────────────────────────────── */
class EnduranceScorePlugin : public MetricPlugin
{
public:
    QString id()       const override { return QStringLiteral("endurance_score"); }
    QString name()     const override { return QStringLiteral("Endurance Score"); }
    QString unit()     const override { return QString(); }
    QString category() const override { return QStringLiteral("performance"); }
    double  maxValue() const override { return 1000.0; }

    QStringList requiredInputs() const override {
        return { QStringLiteral("vo2max_estimate"), QStringLiteral("chronic_load") };
    }
    QStringList optionalInputs() const override {
        return { QStringLiteral("long_duration_factor") };
    }

    QVariant compute(const QVariantMap &inputs) override;
};

/* ── Race Predictor ──────────────────────────────────────────────────────── */
class RacePredictorPlugin : public MetricPlugin
{
public:
    QString id()       const override { return QStringLiteral("race_predictor"); }
    QString name()     const override { return QStringLiteral("Race Predictor"); }
    QString unit()     const override { return QString(); }
    QString category() const override { return QStringLiteral("performance"); }

    QStringList requiredInputs() const override {
        return { QStringLiteral("vo2max_estimate") };
    }
    QStringList optionalInputs() const override {
        return { QStringLiteral("running_economy") };
    }

    QVariant compute(const QVariantMap &inputs) override;
    QVariantMap computeDetailed(const QVariantMap &inputs) override;
};

/* ── Stamina (Overall & Current) ─────────────────────────────────────────── */
class StaminaPlugin : public MetricPlugin
{
public:
    enum Mode { Overall, Current };
    explicit StaminaPlugin(Mode mode);

    QString id()       const override;
    QString name()     const override;
    QString unit()     const override { return QStringLiteral("%"); }
    QString category() const override { return QStringLiteral("performance"); }

    QStringList requiredInputs() const override {
        return { QStringLiteral("hr_series"), QStringLiteral("time_s"), QStringLiteral("hr_max") };
    }
    QStringList optionalInputs() const override {
        return { QStringLiteral("distance_m"), QStringLiteral("vo2max_estimate") };
    }

    QVariant compute(const QVariantMap &inputs) override;
    QVariantMap computeDetailed(const QVariantMap &inputs) override;

private:
    Mode m_mode;
};

#endif // TRAININGMETRICS_H
