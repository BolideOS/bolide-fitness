/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Readiness scoring module (Phase 2).
 *
 * Computes a daily readiness-to-train score (0-100) by combining:
 *   1. HRV baseline deviation   (30% weight)
 *   2. Resting HR deviation     (20% weight)
 *   3. Sleep quality             (25% weight)
 *   4. Sleep duration adequacy   (10% weight)
 *   5. Recent training load      (15% weight)
 *
 * Baselines are rolling 7-day medians. Acute values are from last night's
 * sleep session data (provided by SleepTracker).
 *
 * Score interpretation:
 *   85-100  Excellent — full training
 *   70-84   Good — normal training
 *   50-69   Moderate — reduce intensity
 *   30-49   Low — active recovery only
 *    0-29   Very low — rest day
 */

#ifndef READINESSSCORE_H
#define READINESSSCORE_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QDate>
#include <QVector>

class Database;
class SleepTracker;

class ReadinessScore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int score READ score NOTIFY scoreChanged)
    Q_PROPERTY(QString label READ label NOTIFY scoreChanged)
    Q_PROPERTY(QVariantMap breakdown READ breakdown NOTIFY scoreChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY scoreChanged)

public:
    explicit ReadinessScore(QObject *parent = nullptr);
    ~ReadinessScore() override = default;

    void setDatabase(Database *db);
    void setSleepTracker(SleepTracker *st);

    // ── Properties ──────────────────────────────────────────────────────

    int score() const { return m_score; }

    QString label() const;

    QVariantMap breakdown() const { return m_breakdown; }

    QVariantList history() const;

    // ── Actions ─────────────────────────────────────────────────────────

    /** Recompute readiness from latest data. Call after sleep analysis. */
    Q_INVOKABLE void recompute();

    /** Load today's score from DB (app startup). */
    void loadToday();

signals:
    void scoreChanged();

private:
    // ── Component scoring ───────────────────────────────────────────────

    struct ComponentScore {
        double value;   // 0-100
        double weight;
        QString name;
    };

    /** HRV deviation from 7-day baseline. Higher acute → better. */
    double scoreHrvDeviation(double acuteRmssd, double baselineRmssd) const;

    /** Resting HR deviation. Lower acute → better. */
    double scoreRestingHrDeviation(int acuteHr, double baselineHr) const;

    /** Sleep quality score pass-through (already 0-100). */
    double scoreSleepQuality(double quality) const;

    /** Sleep duration vs target. */
    double scoreSleepDuration(int durationMin, int targetMin = 480) const;

    /** Training load: acute (3d) vs chronic (28d) ratio. */
    double scoreTrainingLoad(double acuteLoad, double chronicLoad) const;

    /** Compute rolling median of a metric over last N days. */
    double rollingMedian(const QString &metricId, int days) const;

    /** Compute rolling mean of a metric over last N days. */
    double rollingMean(const QString &metricId, int days) const;

    // ── Data ────────────────────────────────────────────────────────────

    Database *m_database;
    SleepTracker *m_sleepTracker;

    int m_score;
    QVariantMap m_breakdown;
};

#endif // READINESSSCORE_H
