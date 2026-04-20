/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "readinessscore.h"
#include "sleeptracker.h"
#include "core/database.h"

#include <QDebug>
#include <QtMath>
#include <algorithm>

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

ReadinessScore::ReadinessScore(QObject *parent)
    : QObject(parent)
    , m_database(nullptr)
    , m_sleepTracker(nullptr)
    , m_score(0)
{
}

void ReadinessScore::setDatabase(Database *db) { m_database = db; }
void ReadinessScore::setSleepTracker(SleepTracker *st) { m_sleepTracker = st; }

// ═════════════════════════════════════════════════════════════════════════════
// Label
// ═════════════════════════════════════════════════════════════════════════════

QString ReadinessScore::label() const
{
    if (m_score >= 85) return QStringLiteral("Excellent");
    if (m_score >= 70) return QStringLiteral("Good");
    if (m_score >= 50) return QStringLiteral("Moderate");
    if (m_score >= 30) return QStringLiteral("Low");
    return QStringLiteral("Very Low");
}

// ═════════════════════════════════════════════════════════════════════════════
// History
// ═════════════════════════════════════════════════════════════════════════════

QVariantList ReadinessScore::history() const
{
    if (!m_database) return QVariantList();
    QDate today = QDate::currentDate();
    return m_database->getDailyMetrics(
        QStringLiteral("readiness_score"), today.addDays(-30), today);
}

// ═════════════════════════════════════════════════════════════════════════════
// Main computation
// ═════════════════════════════════════════════════════════════════════════════

void ReadinessScore::recompute()
{
    if (!m_database) {
        qWarning() << "ReadinessScore: no database";
        return;
    }

    QDate today = QDate::currentDate();

    // ── Gather acute values (last night) ────────────────────────────────

    double acuteHrv = 0.0;
    int acuteRestingHr = 0;
    double sleepQuality = 0.0;
    int sleepDuration = 0;

    if (m_sleepTracker) {
        acuteHrv = m_sleepTracker->overnightHrvRmssd();
        acuteRestingHr = m_sleepTracker->overnightRestingHr();
        sleepQuality = m_sleepTracker->qualityScore();
        sleepDuration = m_sleepTracker->totalMinutes();
    }

    // Fallback: try daily metrics from DB if sleep tracker has no data
    if (acuteHrv <= 0.0)
        acuteHrv = m_database->getDailyMetricValue(today, QStringLiteral("overnight_hrv"));
    if (acuteRestingHr <= 0)
        acuteRestingHr = static_cast<int>(
            m_database->getDailyMetricValue(today, QStringLiteral("resting_hr")));
    if (sleepQuality <= 0.0)
        sleepQuality = m_database->getDailyMetricValue(today, QStringLiteral("sleep_quality"));
    if (sleepDuration <= 0)
        sleepDuration = static_cast<int>(
            m_database->getDailyMetricValue(today, QStringLiteral("sleep_duration")));

    // ── Gather baselines (7-day rolling) ────────────────────────────────

    double baselineHrv = rollingMedian(QStringLiteral("overnight_hrv"), 7);
    double baselineRestingHr = rollingMean(QStringLiteral("resting_hr"), 7);

    // ── Training load (acute: 3-day, chronic: 28-day) ───────────────────
    //    Uses total calories as a proxy for training load
    double acuteLoad = rollingMean(QStringLiteral("workout_calories"), 3);
    double chronicLoad = rollingMean(QStringLiteral("workout_calories"), 28);

    // ── Score each component ────────────────────────────────────────────

    ComponentScore components[] = {
        { scoreHrvDeviation(acuteHrv, baselineHrv),           0.30, QStringLiteral("hrv") },
        { scoreRestingHrDeviation(acuteRestingHr, baselineRestingHr), 0.20, QStringLiteral("resting_hr") },
        { scoreSleepQuality(sleepQuality),                    0.25, QStringLiteral("sleep_quality") },
        { scoreSleepDuration(sleepDuration),                  0.10, QStringLiteral("sleep_duration") },
        { scoreTrainingLoad(acuteLoad, chronicLoad),          0.15, QStringLiteral("training_load") }
    };

    double totalScore = 0.0;
    m_breakdown.clear();
    for (const auto &c : components) {
        totalScore += c.value * c.weight;
        QVariantMap comp;
        comp[QStringLiteral("score")] = qRound(c.value);
        comp[QStringLiteral("weight")] = c.weight;
        m_breakdown[c.name] = comp;
    }

    m_score = qBound(0, qRound(totalScore), 100);

    // Store breakdown values for debugging
    m_breakdown[QStringLiteral("acute_hrv")] = acuteHrv;
    m_breakdown[QStringLiteral("baseline_hrv")] = baselineHrv;
    m_breakdown[QStringLiteral("acute_resting_hr")] = acuteRestingHr;
    m_breakdown[QStringLiteral("baseline_resting_hr")] = baselineRestingHr;

    // Persist to daily metrics
    m_database->storeDailyMetric(today, QStringLiteral("readiness_score"), m_score);

    emit scoreChanged();
    qInfo() << "ReadinessScore:" << m_score << "(" << label() << ")"
            << "HRV:" << acuteHrv << "/" << baselineHrv
            << "RHR:" << acuteRestingHr << "/" << baselineRestingHr
            << "Sleep:" << sleepQuality << "/" << sleepDuration << "min";
}

void ReadinessScore::loadToday()
{
    if (!m_database) return;

    QDate today = QDate::currentDate();
    double stored = m_database->getDailyMetricValue(today, QStringLiteral("readiness_score"));
    if (stored > 0) {
        m_score = qBound(0, qRound(stored), 100);
        emit scoreChanged();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Component scoring functions
// ═════════════════════════════════════════════════════════════════════════════

double ReadinessScore::scoreHrvDeviation(double acuteRmssd, double baselineRmssd) const
{
    /*
     * HRV deviation scoring (0-100):
     * - Acute == baseline → 75 (normal)
     * - Acute > baseline → up to 100 (better parasympathetic tone)
     * - Acute < baseline → down toward 0 (sympathetic dominance / fatigue)
     *
     * Each 10% deviation maps to ~12.5 points.
     */
    if (baselineRmssd <= 0.0) {
        // No baseline data → neutral score
        return 50.0;
    }

    double ratio = acuteRmssd / baselineRmssd;
    double score;

    if (ratio >= 1.0) {
        // Better than baseline
        score = 75.0 + qMin(25.0, (ratio - 1.0) * 125.0);
    } else {
        // Worse than baseline
        score = 75.0 * ratio;
    }

    return qBound(0.0, score, 100.0);
}

double ReadinessScore::scoreRestingHrDeviation(int acuteHr, double baselineHr) const
{
    /*
     * Resting HR deviation scoring (0-100):
     * - Acute == baseline → 80 (normal)
     * - Acute lower → better (up to 100)
     * - Acute higher → worse (down toward 0)
     *
     * Each bpm deviation counts ~5 points.
     */
    if (baselineHr <= 0.0 || acuteHr <= 0)
        return 50.0;

    double delta = static_cast<double>(acuteHr) - baselineHr;
    double score = 80.0 - delta * 5.0;

    return qBound(0.0, score, 100.0);
}

double ReadinessScore::scoreSleepQuality(double quality) const
{
    // Pass-through: sleep quality is already 0-100
    return qBound(0.0, quality, 100.0);
}

double ReadinessScore::scoreSleepDuration(int durationMin, int targetMin) const
{
    if (targetMin <= 0) return 50.0;

    double ratio = static_cast<double>(durationMin) / targetMin;

    if (ratio >= 0.95 && ratio <= 1.15)
        return 100.0;  // On target
    if (ratio < 0.95)
        return qMax(0.0, ratio / 0.95 * 100.0);
    // Overslept
    return qMax(50.0, 100.0 - (ratio - 1.15) * 200.0);
}

double ReadinessScore::scoreTrainingLoad(double acuteLoad, double chronicLoad) const
{
    /*
     * Acute:Chronic Workload Ratio (ACWR) scoring:
     * - Ratio ~0.8-1.3 → "sweet spot" (high score)
     * - Ratio <0.5 → detraining (moderate)
     * - Ratio >1.5 → overload risk (low score)
     *
     * If no data at all, return neutral.
     */
    if (chronicLoad <= 0.0) {
        if (acuteLoad <= 0.0)
            return 75.0;  // No training data at all
        return 60.0;      // Some training but no baseline
    }

    double ratio = acuteLoad / chronicLoad;

    if (ratio >= 0.8 && ratio <= 1.3)
        return 90.0;
    if (ratio < 0.8)
        return 70.0 + (ratio / 0.8) * 20.0;
    if (ratio <= 1.5)
        return 90.0 - (ratio - 1.3) * 200.0;  // 90→50 over 1.3→1.5
    // ratio > 1.5
    return qMax(0.0, 50.0 - (ratio - 1.5) * 100.0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Statistical helpers
// ═════════════════════════════════════════════════════════════════════════════

double ReadinessScore::rollingMedian(const QString &metricId, int days) const
{
    if (!m_database) return 0.0;

    QDate today = QDate::currentDate();
    QVariantList data = m_database->getDailyMetrics(
        metricId, today.addDays(-days), today.addDays(-1));

    if (data.isEmpty()) return 0.0;

    QVector<double> values;
    values.reserve(data.size());
    for (const QVariant &v : data) {
        double val = v.toMap().value(QStringLiteral("value"), 0.0).toDouble();
        if (val > 0.0) values.append(val);
    }

    if (values.isEmpty()) return 0.0;

    std::sort(values.begin(), values.end());
    int n = values.size();
    if (n % 2 == 0)
        return (values[n / 2 - 1] + values[n / 2]) / 2.0;
    return values[n / 2];
}

double ReadinessScore::rollingMean(const QString &metricId, int days) const
{
    if (!m_database) return 0.0;

    QDate today = QDate::currentDate();
    QVariantList data = m_database->getDailyMetrics(
        metricId, today.addDays(-days), today.addDays(-1));

    if (data.isEmpty()) return 0.0;

    double sum = 0.0;
    int count = 0;
    for (const QVariant &v : data) {
        double val = v.toMap().value(QStringLiteral("value"), 0.0).toDouble();
        if (val > 0.0) {
            sum += val;
            count++;
        }
    }

    return count > 0 ? sum / count : 0.0;
}
