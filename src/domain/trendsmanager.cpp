/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "trendsmanager.h"
#include "core/database.h"
#include <QDateTime>
#include <QtMath>
#include <algorithm>

// ── Metric definitions for availableMetrics() ───────────────────────────────

struct MetricDef {
    const char *id;
    const char *name;
    const char *unit;
    const char *category;
    bool higherIsBetter;
};

static const MetricDef kMetrics[] = {
    { "resting_hr",      "Resting HR",      "bpm",        "heart",    false },
    { "overnight_hrv",   "Overnight HRV",   "ms",         "heart",    true  },
    { "body_hrv",        "Daytime HRV",     "ms",         "heart",    true  },
    { "sleep_duration",  "Sleep Duration",   "min",        "sleep",    true  },
    { "sleep_quality",   "Sleep Quality",    "score",      "sleep",    true  },
    { "sleep_deep",      "Deep Sleep",       "min",        "sleep",    true  },
    { "sleep_rem",       "REM Sleep",        "min",        "sleep",    true  },
    { "readiness_score", "Readiness",        "score",      "recovery", true  },
    { "stress_index",    "Stress",           "index",      "recovery", false },
    { "daily_steps",     "Steps",            "steps",      "activity", true  },
    { "calories",        "Calories",         "kcal",       "activity", true  },
    { "vo2max",          "VO2max",           "ml/kg/min",  "fitness",  true  },
    { "training_load",   "Training Load",    "au",         "fitness",  true  },
    { "spo2",            "SpO2",             "%",          "health",   true  },
    { "breathing_rate",  "Breathing Rate",   "br/min",     "health",   false },
};
static const int kMetricCount = sizeof(kMetrics) / sizeof(kMetrics[0]);

// ─────────────────────────────────────────────────────────────────────────────

TrendsManager::TrendsManager(QObject *parent)
    : QObject(parent)
    , m_database(nullptr)
    , m_recomputeTimer(new QTimer(this))
{
    // Refresh baselines every 30 minutes
    m_recomputeTimer->setInterval(30 * 60 * 1000);
    m_recomputeTimer->setSingleShot(false);
    connect(m_recomputeTimer, &QTimer::timeout, this, &TrendsManager::recomputeBaselines);
}

TrendsManager::~TrendsManager() = default;

void TrendsManager::setDatabase(Database *db)
{
    m_database = db;
    if (m_database) {
        recomputeBaselines();
        m_recomputeTimer->start();
    }
}

// ── Trend data queries ──────────────────────────────────────────────────────

QVariantList TrendsManager::trendData(const QString &metricId, int days)
{
    if (!m_database)
        return QVariantList();

    const QDate today = QDate::currentDate();
    const QDate from  = today.addDays(-days);

    QVariantList raw = m_database->getDailyMetrics(metricId, from, today);

    // Convert to standardized format: { "date": "YYYY-MM-DD", "value": double }
    QVariantList result;
    result.reserve(raw.size());
    for (const QVariant &v : raw) {
        QVariantMap row = v.toMap();
        QVariantMap point;
        point[QStringLiteral("date")]  = row.value(QStringLiteral("date")).toString();
        point[QStringLiteral("value")] = row.value(QStringLiteral("value")).toDouble();
        result.append(point);
    }
    return result;
}

QVariantList TrendsManager::weeklyAggregates(const QString &metricId, int weeks)
{
    if (!m_database)
        return QVariantList();

    const QDate today = QDate::currentDate();
    const QDate from  = today.addDays(-weeks * 7);

    QVariantList raw = m_database->getDailyMetrics(metricId, from, today);

    // Group by ISO week
    QMap<QString, QVector<double>> weekBuckets;
    for (const QVariant &v : raw) {
        QVariantMap row = v.toMap();
        QDate d = QDate::fromString(row[QStringLiteral("date")].toString(), Qt::ISODate);
        if (!d.isValid()) continue;

        // Find Monday of that week
        int dayOfWeek = d.dayOfWeek(); // 1=Monday
        QDate weekStart = d.addDays(-(dayOfWeek - 1));
        QString key = weekStart.toString(Qt::ISODate);
        weekBuckets[key].append(row[QStringLiteral("value")].toDouble());
    }

    QVariantList result;
    for (auto it = weekBuckets.constBegin(); it != weekBuckets.constEnd(); ++it) {
        const QVector<double> &vals = it.value();
        if (vals.isEmpty()) continue;

        double sum = 0, mn = vals.first(), mx = vals.first();
        for (double v : vals) {
            sum += v;
            mn = qMin(mn, v);
            mx = qMax(mx, v);
        }

        QVariantMap agg;
        agg[QStringLiteral("weekStart")] = it.key();
        agg[QStringLiteral("mean")]      = sum / vals.size();
        agg[QStringLiteral("min")]       = mn;
        agg[QStringLiteral("max")]       = mx;
        agg[QStringLiteral("count")]     = vals.size();
        result.append(agg);
    }

    // Sort by weekStart ascending
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()[QStringLiteral("weekStart")].toString()
             < b.toMap()[QStringLiteral("weekStart")].toString();
    });

    return result;
}

QVariantList TrendsManager::monthlyAggregates(const QString &metricId, int months)
{
    if (!m_database)
        return QVariantList();

    const QDate today = QDate::currentDate();
    const QDate from  = today.addMonths(-months);

    QVariantList raw = m_database->getDailyMetrics(metricId, from, today);

    // Group by year-month
    QMap<QString, QVector<double>> monthBuckets;
    for (const QVariant &v : raw) {
        QVariantMap row = v.toMap();
        QDate d = QDate::fromString(row[QStringLiteral("date")].toString(), Qt::ISODate);
        if (!d.isValid()) continue;

        QString key = d.toString(QStringLiteral("yyyy-MM"));
        monthBuckets[key].append(row[QStringLiteral("value")].toDouble());
    }

    QVariantList result;
    for (auto it = monthBuckets.constBegin(); it != monthBuckets.constEnd(); ++it) {
        const QVector<double> &vals = it.value();
        if (vals.isEmpty()) continue;

        double sum = 0, mn = vals.first(), mx = vals.first();
        for (double v : vals) {
            sum += v;
            mn = qMin(mn, v);
            mx = qMax(mx, v);
        }

        QVariantMap agg;
        agg[QStringLiteral("month")] = it.key();
        agg[QStringLiteral("mean")]  = sum / vals.size();
        agg[QStringLiteral("min")]   = mn;
        agg[QStringLiteral("max")]   = mx;
        agg[QStringLiteral("count")] = vals.size();
        result.append(agg);
    }

    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()[QStringLiteral("month")].toString()
             < b.toMap()[QStringLiteral("month")].toString();
    });

    return result;
}

// ── Baselines ───────────────────────────────────────────────────────────────

QVariantMap TrendsManager::baselines() const
{
    return m_baselines;
}

void TrendsManager::recomputeBaselines()
{
    if (!m_database)
        return;

    m_baselines.clear();

    // Resting HR
    m_baselines[QStringLiteral("resting_hr_7d")]  = rollingMean(QStringLiteral("resting_hr"), 7);
    m_baselines[QStringLiteral("resting_hr_30d")] = rollingMean(QStringLiteral("resting_hr"), 30);

    // HRV
    m_baselines[QStringLiteral("hrv_7d")]  = rollingMedian(QStringLiteral("overnight_hrv"), 7);
    m_baselines[QStringLiteral("hrv_30d")] = rollingMedian(QStringLiteral("overnight_hrv"), 30);

    // Sleep
    m_baselines[QStringLiteral("sleep_quality_7d")]  = rollingMean(QStringLiteral("sleep_quality"), 7);
    m_baselines[QStringLiteral("sleep_duration_7d")] = rollingMean(QStringLiteral("sleep_duration"), 7);

    // Steps
    m_baselines[QStringLiteral("steps_7d")]  = rollingMean(QStringLiteral("daily_steps"), 7);
    m_baselines[QStringLiteral("steps_30d")] = rollingMean(QStringLiteral("daily_steps"), 30);

    // Training load (ACWR)
    double acuteLoad  = rollingMean(QStringLiteral("training_load"), 7);
    double chronicLoad = rollingMean(QStringLiteral("training_load"), 28);
    m_baselines[QStringLiteral("training_load_acute")]   = acuteLoad;
    m_baselines[QStringLiteral("training_load_chronic")]  = chronicLoad;
    m_baselines[QStringLiteral("training_load_acwr")]     = (chronicLoad > 0.0) ? acuteLoad / chronicLoad : 0.0;

    // VO2max (latest value from last 7 days)
    m_baselines[QStringLiteral("vo2max_latest")] = rollingMean(QStringLiteral("vo2max"), 7);

    // Stress
    m_baselines[QStringLiteral("stress_7d")] = rollingMean(QStringLiteral("stress_index"), 7);

    // Readiness
    m_baselines[QStringLiteral("readiness_7d")] = rollingMean(QStringLiteral("readiness_score"), 7);

    emit baselinesChanged();
}

// ── Change detection ────────────────────────────────────────────────────────

int TrendsManager::trendDirection(const QString &metricId)
{
    double shortTerm = rollingMean(metricId, 7);
    double longTerm  = rollingMean(metricId, 30);

    if (longTerm == 0.0)
        return 0;

    double changePct = (shortTerm - longTerm) / qAbs(longTerm) * 100.0;

    // Need at least 5% change to be significant
    if (qAbs(changePct) < 5.0)
        return 0;

    bool improving = changePct > 0.0;
    if (!isHigherBetter(metricId))
        improving = !improving;

    return improving ? 1 : -1;
}

double TrendsManager::trendChangePercent(const QString &metricId)
{
    double shortTerm = rollingMean(metricId, 7);
    double longTerm  = rollingMean(metricId, 30);

    if (longTerm == 0.0)
        return 0.0;

    return (shortTerm - longTerm) / qAbs(longTerm) * 100.0;
}

// ── Available metrics ───────────────────────────────────────────────────────

QVariantList TrendsManager::availableMetrics() const
{
    QVariantList list;
    for (int i = 0; i < kMetricCount; ++i) {
        QVariantMap m;
        m[QStringLiteral("id")]       = QString::fromLatin1(kMetrics[i].id);
        m[QStringLiteral("name")]     = QString::fromLatin1(kMetrics[i].name);
        m[QStringLiteral("unit")]     = QString::fromLatin1(kMetrics[i].unit);
        m[QStringLiteral("category")] = QString::fromLatin1(kMetrics[i].category);
        list.append(m);
    }
    return list;
}

// ── Private helpers ─────────────────────────────────────────────────────────

double TrendsManager::rollingMean(const QString &metricId, int days)
{
    if (!m_database)
        return 0.0;

    const QDate today = QDate::currentDate();
    const QDate from  = today.addDays(-days);

    QVariantList data = m_database->getDailyMetrics(metricId, from, today);
    if (data.isEmpty())
        return 0.0;

    double sum = 0.0;
    int count = 0;
    for (const QVariant &v : data) {
        double val = v.toMap().value(QStringLiteral("value")).toDouble();
        if (val != 0.0) {
            sum += val;
            ++count;
        }
    }

    return (count > 0) ? sum / count : 0.0;
}

double TrendsManager::rollingMedian(const QString &metricId, int days)
{
    if (!m_database)
        return 0.0;

    const QDate today = QDate::currentDate();
    const QDate from  = today.addDays(-days);

    QVariantList data = m_database->getDailyMetrics(metricId, from, today);
    if (data.isEmpty())
        return 0.0;

    QVector<double> vals;
    vals.reserve(data.size());
    for (const QVariant &v : data) {
        double val = v.toMap().value(QStringLiteral("value")).toDouble();
        if (val != 0.0)
            vals.append(val);
    }

    if (vals.isEmpty())
        return 0.0;

    std::sort(vals.begin(), vals.end());
    int n = vals.size();
    if (n % 2 == 0)
        return (vals[n / 2 - 1] + vals[n / 2]) / 2.0;
    else
        return vals[n / 2];
}

bool TrendsManager::isHigherBetter(const QString &metricId) const
{
    for (int i = 0; i < kMetricCount; ++i) {
        if (metricId == QLatin1String(kMetrics[i].id))
            return kMetrics[i].higherIsBetter;
    }
    return true; // default
}
