/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Trends analysis module.
 * Computes rolling baselines, weekly/monthly aggregates, and time-series
 * data for all tracked metrics (HRV, RHR, sleep, training load, VO2max,
 * stress, body composition).
 */

#ifndef TRENDSMANAGER_H
#define TRENDSMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QDate>
#include <QTimer>

class Database;

/**
 * @brief Trend analysis and baseline tracking for all fitness metrics.
 *
 * Provides:
 * - Time-series data for any stored metric over configurable windows
 * - Rolling baselines (7-day, 30-day) for key health indicators
 * - Weekly and monthly aggregations with min/max/mean/trend direction
 * - Change detection (significant deviations from baseline)
 *
 * Supported metrics (by metricId):
 * - "resting_hr"       Resting heart rate (bpm)
 * - "overnight_hrv"    Overnight HRV RMSSD (ms)
 * - "body_hrv"         Daytime HRV RMSSD (ms)
 * - "sleep_duration"   Sleep duration (minutes)
 * - "sleep_quality"    Sleep quality score (0-100)
 * - "sleep_deep"       Deep sleep (minutes)
 * - "sleep_rem"        REM sleep (minutes)
 * - "readiness_score"  Daily readiness (0-100)
 * - "stress_index"     Stress index (0-100)
 * - "daily_steps"      Daily step count
 * - "calories"         Daily active calories
 * - "vo2max"           VO2max estimate (ml/kg/min)
 * - "training_load"    Training load (arbitrary units)
 * - "spo2"             SpO2 percentage
 * - "breathing_rate"   Breathing rate (breaths/min)
 */
class TrendsManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantMap baselines READ baselines NOTIFY baselinesChanged)
    Q_PROPERTY(QVariantList availableMetrics READ availableMetrics CONSTANT)

public:
    explicit TrendsManager(QObject *parent = nullptr);
    ~TrendsManager() override;

    void setDatabase(Database *db);

    // ── Trend data queries ──────────────────────────────────────────────

    /**
     * Get time-series data for a metric over the last N days.
     * Returns list of { "date": "YYYY-MM-DD", "value": double }
     */
    Q_INVOKABLE QVariantList trendData(const QString &metricId, int days = 30);

    /**
     * Get weekly aggregates for a metric.
     * Returns list of { "weekStart": "YYYY-MM-DD", "mean": double,
     *                    "min": double, "max": double, "count": int }
     */
    Q_INVOKABLE QVariantList weeklyAggregates(const QString &metricId, int weeks = 12);

    /**
     * Get monthly aggregates for a metric.
     * Returns list of { "month": "YYYY-MM", "mean": double,
     *                    "min": double, "max": double, "count": int }
     */
    Q_INVOKABLE QVariantList monthlyAggregates(const QString &metricId, int months = 6);

    // ── Baselines ───────────────────────────────────────────────────────

    /**
     * Current baselines map:
     * { "resting_hr_7d": val, "resting_hr_30d": val,
     *   "hrv_7d": val, "hrv_30d": val,
     *   "sleep_quality_7d": val, "sleep_duration_7d": val,
     *   "steps_7d": val, "steps_30d": val,
     *   "training_load_acute": val, "training_load_chronic": val,
     *   "vo2max_latest": val }
     */
    QVariantMap baselines() const;

    /** Force recompute all baselines from database. */
    Q_INVOKABLE void recomputeBaselines();

    // ── Change detection ────────────────────────────────────────────────

    /**
     * Get trend direction for a metric: -1 declining, 0 stable, +1 improving.
     * Compares 7-day mean to 30-day mean.
     */
    Q_INVOKABLE int trendDirection(const QString &metricId);

    /**
     * Percent change of 7-day mean vs 30-day mean.
     */
    Q_INVOKABLE double trendChangePercent(const QString &metricId);

    // ── Available metrics ───────────────────────────────────────────────

    QVariantList availableMetrics() const;

signals:
    void baselinesChanged();

private:
    double rollingMean(const QString &metricId, int days);
    double rollingMedian(const QString &metricId, int days);

    /** Whether a higher value is "better" for this metric. */
    bool isHigherBetter(const QString &metricId) const;

    Database *m_database;
    QVariantMap m_baselines;
    QTimer *m_recomputeTimer;   // Periodic baseline refresh (every 30 min)
};

#endif // TRENDSMANAGER_H
