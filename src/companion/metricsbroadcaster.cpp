/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "metricsbroadcaster.h"

#include "domain/bodymetrics.h"
#include "domain/sleeptracker.h"
#include "domain/readinessscore.h"
#include "domain/trendsmanager.h"
#include "core/database.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>

static const char *SERVICE_NAME = "org.bolide.fitness.Metrics";
static const char *OBJECT_PATH  = "/org/bolide/fitness/Metrics";
static const char *INTERFACE    = "org.bolide.fitness.Metrics";

MetricsBroadcaster::MetricsBroadcaster(QObject *parent)
    : QObject(parent)
    , m_bodyMetrics(nullptr)
    , m_sleepTracker(nullptr)
    , m_readiness(nullptr)
    , m_trendsManager(nullptr)
    , m_database(nullptr)
    , m_registered(false)
    , m_heartRate(0)
    , m_spo2(0)
    , m_stressIndex(0)
    , m_temperature(0.0)
    , m_breathingRate(0)
    , m_dailySteps(0)
    , m_dailyCalories(0)
    , m_dailyFloorsClimbed(0)
    , m_dailyDistance(0.0)
    , m_activeMinutes(0)
    , m_restingHr(0)
    , m_hrvRmssd(0.0)
    , m_readinessScore(0)
    , m_recoveryTime(0)
    , m_sleepQuality(0)
    , m_sleepDuration(0)
    , m_sleepDeep(0)
    , m_sleepRem(0)
    , m_sleepLight(0)
    , m_vo2max(0.0)
    , m_trainingLoad(0.0)
    , m_stepsGoal(10000)
    , m_caloriesGoal(2000)
    , m_activeMinutesGoal(30)
    , m_stepsGoalProgress(0.0)
    , m_caloriesGoalProgress(0.0)
    , m_activeMinutesGoalProgress(0.0)
    , m_lastUpdate(0)
{
}

MetricsBroadcaster::~MetricsBroadcaster()
{
    if (m_registered) {
        QDBusConnection::sessionBus().unregisterObject(OBJECT_PATH);
        QDBusConnection::sessionBus().unregisterService(SERVICE_NAME);
    }
}

void MetricsBroadcaster::setBodyMetrics(BodyMetrics *bm)
{
    m_bodyMetrics = bm;
    if (bm) {
        connect(bm, &BodyMetrics::metricsChanged,
                this, &MetricsBroadcaster::onBodyMetricsChanged);
        connect(bm, &BodyMetrics::spo2Changed,
                this, &MetricsBroadcaster::onSpo2Changed);
        // Load initial values
        onBodyMetricsChanged();
    }
}

void MetricsBroadcaster::setSleepTracker(SleepTracker *st)
{
    m_sleepTracker = st;
    if (st) {
        connect(st, &SleepTracker::morningAnalysisComplete,
                this, &MetricsBroadcaster::onSleepAnalysisComplete);
    }
}

void MetricsBroadcaster::setReadinessScore(ReadinessScore *rs)
{
    m_readiness = rs;
    if (rs) {
        connect(rs, &ReadinessScore::scoreChanged,
                this, &MetricsBroadcaster::onReadinessRecomputed);
    }
}

void MetricsBroadcaster::setTrendsManager(TrendsManager *tm)
{
    m_trendsManager = tm;
}

void MetricsBroadcaster::setDatabase(Database *db)
{
    m_database = db;
    if (db)
        loadGoals();
}

bool MetricsBroadcaster::registerService()
{
    auto bus = QDBusConnection::sessionBus();

    if (!bus.registerService(SERVICE_NAME)) {
        qWarning("MetricsBroadcaster: failed to register D-Bus service %s", SERVICE_NAME);
        return false;
    }
    if (!bus.registerObject(OBJECT_PATH, this,
                            QDBusConnection::ExportAllSlots |
                            QDBusConnection::ExportAllSignals |
                            QDBusConnection::ExportAllProperties)) {
        qWarning("MetricsBroadcaster: failed to register D-Bus object %s", OBJECT_PATH);
        bus.unregisterService(SERVICE_NAME);
        return false;
    }

    m_registered = true;
    return true;
}

// ── Slot: BodyMetrics changed ───────────────────────────────────────────────

void MetricsBroadcaster::onBodyMetricsChanged()
{
    if (!m_bodyMetrics)
        return;

    QStringList changed;

    int hr = m_bodyMetrics->heartRate();
    if (hr != m_heartRate) {
        m_heartRate = hr;
        changed << QStringLiteral("heartRate");
        emit heartRateChanged(hr);
    }

    int stress = m_bodyMetrics->stressIndex();
    if (stress != m_stressIndex) {
        m_stressIndex = stress;
        changed << QStringLiteral("stressIndex");
        emit stressIndexChanged(stress);
    }

    double temp = m_bodyMetrics->temperature();
    if (qAbs(temp - m_temperature) > 0.01) {
        m_temperature = temp;
        changed << QStringLiteral("temperature");
        emit temperatureChanged(temp);
    }

    int br = m_bodyMetrics->breathingRate();
    if (br != m_breathingRate) {
        m_breathingRate = br;
        changed << QStringLiteral("breathingRate");
        emit breathingRateChanged(br);
    }

    int rhr = m_bodyMetrics->restingHr();
    if (rhr != m_restingHr) {
        m_restingHr = rhr;
        changed << QStringLiteral("restingHr");
        emit restingHrChanged(rhr);
    }

    double hrv = m_bodyMetrics->hrvRmssd();
    if (qAbs(hrv - m_hrvRmssd) > 0.01) {
        m_hrvRmssd = hrv;
        changed << QStringLiteral("hrvRmssd");
        emit hrvRmssdChanged(hrv);
    }

    int steps = m_bodyMetrics->dailySteps();
    if (steps != m_dailySteps) {
        m_dailySteps = steps;
        changed << QStringLiteral("dailySteps");
        emit dailyStepsChanged(steps);
        // Update goal progress
        double prog = m_stepsGoal > 0 ? (double)steps / m_stepsGoal : 0.0;
        if (qAbs(prog - m_stepsGoalProgress) > 0.001) {
            m_stepsGoalProgress = prog;
            emit goalsChanged();
        }
    }

    int cal = m_bodyMetrics->dailyCalories();
    if (cal != m_dailyCalories) {
        m_dailyCalories = cal;
        changed << QStringLiteral("dailyCalories");
        emit dailyCaloriesChanged(cal);
        double prog = m_caloriesGoal > 0 ? (double)cal / m_caloriesGoal : 0.0;
        if (qAbs(prog - m_caloriesGoalProgress) > 0.001) {
            m_caloriesGoalProgress = prog;
            emit goalsChanged();
        }
    }

    if (!changed.isEmpty()) {
        m_lastUpdate = QDateTime::currentMSecsSinceEpoch();
        emitDBusPropertiesChanged(changed);
        for (const auto &prop : changed)
            emit anyMetricChanged(prop);
    }
}

void MetricsBroadcaster::onSpo2Changed(int percent)
{
    if (percent != m_spo2) {
        m_spo2 = percent;
        m_lastUpdate = QDateTime::currentMSecsSinceEpoch();
        emit spo2Changed(percent);
        emitDBusPropertiesChanged({QStringLiteral("spo2")});
        emit anyMetricChanged(QStringLiteral("spo2"));
    }
}

void MetricsBroadcaster::onReadinessRecomputed()
{
    if (!m_readiness)
        return;

    int score = m_readiness->score();
    if (score != m_readinessScore) {
        m_readinessScore = score;
        m_lastUpdate = QDateTime::currentMSecsSinceEpoch();
        emit readinessScoreChanged(score);
        emitDBusPropertiesChanged({QStringLiteral("readinessScore")});
        emit anyMetricChanged(QStringLiteral("readinessScore"));
    }
}

void MetricsBroadcaster::onSleepAnalysisComplete()
{
    if (!m_sleepTracker)
        return;

    m_sleepQuality  = m_sleepTracker->qualityScore();
    m_sleepDuration = m_sleepTracker->totalMinutes();
    m_sleepDeep     = m_sleepTracker->deepMinutes();
    m_sleepRem      = m_sleepTracker->remMinutes();
    m_sleepLight    = m_sleepTracker->lightMinutes();

    m_lastUpdate = QDateTime::currentMSecsSinceEpoch();
    emit sleepChanged();
    emitDBusPropertiesChanged({
        QStringLiteral("sleepQuality"),
        QStringLiteral("sleepDuration"),
        QStringLiteral("sleepDeep"),
        QStringLiteral("sleepRem"),
        QStringLiteral("sleepLight")
    });
    emit anyMetricChanged(QStringLiteral("sleepQuality"));
}

void MetricsBroadcaster::onPerformanceUpdated()
{
    // Called when training metrics are recomputed (e.g. after workout)
    m_lastUpdate = QDateTime::currentMSecsSinceEpoch();
    emit performanceChanged();
    emitDBusPropertiesChanged({
        QStringLiteral("vo2max"),
        QStringLiteral("trainingLoad"),
        QStringLiteral("trainingStatus")
    });
    emit anyMetricChanged(QStringLiteral("trainingLoad"));
}

// ── D-Bus methods ───────────────────────────────────────────────────────────

QVariantMap MetricsBroadcaster::GetAllMetrics() const
{
    QVariantMap m;
    m[QStringLiteral("heartRate")]         = m_heartRate;
    m[QStringLiteral("spo2")]              = m_spo2;
    m[QStringLiteral("stressIndex")]       = m_stressIndex;
    m[QStringLiteral("temperature")]       = m_temperature;
    m[QStringLiteral("breathingRate")]     = m_breathingRate;
    m[QStringLiteral("dailySteps")]        = m_dailySteps;
    m[QStringLiteral("dailyCalories")]     = m_dailyCalories;
    m[QStringLiteral("dailyFloorsClimbed")]= m_dailyFloorsClimbed;
    m[QStringLiteral("dailyDistance")]     = m_dailyDistance;
    m[QStringLiteral("activeMinutes")]     = m_activeMinutes;
    m[QStringLiteral("restingHr")]         = m_restingHr;
    m[QStringLiteral("hrvRmssd")]          = m_hrvRmssd;
    m[QStringLiteral("readinessScore")]    = m_readinessScore;
    m[QStringLiteral("recoveryTime")]      = m_recoveryTime;
    m[QStringLiteral("sleepQuality")]      = m_sleepQuality;
    m[QStringLiteral("sleepDuration")]     = m_sleepDuration;
    m[QStringLiteral("sleepDeep")]         = m_sleepDeep;
    m[QStringLiteral("sleepRem")]          = m_sleepRem;
    m[QStringLiteral("sleepLight")]        = m_sleepLight;
    m[QStringLiteral("vo2max")]            = m_vo2max;
    m[QStringLiteral("trainingLoad")]      = m_trainingLoad;
    m[QStringLiteral("trainingStatus")]    = m_trainingStatus;
    m[QStringLiteral("stepsGoalProgress")] = m_stepsGoalProgress;
    m[QStringLiteral("caloriesGoalProgress")]     = m_caloriesGoalProgress;
    m[QStringLiteral("activeMinutesGoalProgress")]= m_activeMinutesGoalProgress;
    m[QStringLiteral("lastUpdateTimestamp")]      = m_lastUpdate;
    return m;
}

QVariant MetricsBroadcaster::GetMetric(const QString &key) const
{
    return GetAllMetrics().value(key);
}

int MetricsBroadcaster::PushMetrics(const QVariantMap &metrics)
{
    // Inbound sync from companion app.
    // Apply each metric if value differs from current.
    int updated = 0;
    QStringList changed;

    auto tryInt = [&](const QString &key, int &field, void (MetricsBroadcaster::*signal)(int)) {
        if (metrics.contains(key)) {
            int val = metrics[key].toInt();
            if (val != field) {
                field = val;
                changed << key;
                emit (this->*signal)(val);
                updated++;
            }
        }
    };

    auto tryDouble = [&](const QString &key, double &field, void (MetricsBroadcaster::*signal)(double)) {
        if (metrics.contains(key)) {
            double val = metrics[key].toDouble();
            if (qAbs(val - field) > 0.001) {
                field = val;
                changed << key;
                emit (this->*signal)(val);
                updated++;
            }
        }
    };

    auto tryIntNoArg = [&](const QString &key, int &field, void (MetricsBroadcaster::*signal)()) {
        if (metrics.contains(key)) {
            int val = metrics[key].toInt();
            if (val != field) {
                field = val;
                changed << key;
                emit (this->*signal)();
                updated++;
            }
        }
    };

    auto tryDoubleNoArg = [&](const QString &key, double &field, void (MetricsBroadcaster::*signal)()) {
        if (metrics.contains(key)) {
            double val = metrics[key].toDouble();
            if (qAbs(val - field) > 0.001) {
                field = val;
                changed << key;
                emit (this->*signal)();
                updated++;
            }
        }
    };

    tryInt(QStringLiteral("heartRate"), m_heartRate, &MetricsBroadcaster::heartRateChanged);
    tryInt(QStringLiteral("spo2"), m_spo2, &MetricsBroadcaster::spo2Changed);
    tryInt(QStringLiteral("stressIndex"), m_stressIndex, &MetricsBroadcaster::stressIndexChanged);
    tryInt(QStringLiteral("breathingRate"), m_breathingRate, &MetricsBroadcaster::breathingRateChanged);
    tryInt(QStringLiteral("dailySteps"), m_dailySteps, &MetricsBroadcaster::dailyStepsChanged);
    tryInt(QStringLiteral("dailyCalories"), m_dailyCalories, &MetricsBroadcaster::dailyCaloriesChanged);
    tryInt(QStringLiteral("dailyFloorsClimbed"), m_dailyFloorsClimbed, &MetricsBroadcaster::dailyFloorsClimbedChanged);
    tryInt(QStringLiteral("activeMinutes"), m_activeMinutes, &MetricsBroadcaster::activeMinutesChanged);
    tryInt(QStringLiteral("restingHr"), m_restingHr, &MetricsBroadcaster::restingHrChanged);
    tryInt(QStringLiteral("readinessScore"), m_readinessScore, &MetricsBroadcaster::readinessScoreChanged);
    tryInt(QStringLiteral("recoveryTime"), m_recoveryTime, &MetricsBroadcaster::recoveryTimeChanged);

    tryIntNoArg(QStringLiteral("sleepQuality"), m_sleepQuality, &MetricsBroadcaster::sleepChanged);
    tryIntNoArg(QStringLiteral("sleepDuration"), m_sleepDuration, &MetricsBroadcaster::sleepChanged);
    tryIntNoArg(QStringLiteral("sleepDeep"), m_sleepDeep, &MetricsBroadcaster::sleepChanged);
    tryIntNoArg(QStringLiteral("sleepRem"), m_sleepRem, &MetricsBroadcaster::sleepChanged);
    tryIntNoArg(QStringLiteral("sleepLight"), m_sleepLight, &MetricsBroadcaster::sleepChanged);

    tryDouble(QStringLiteral("temperature"), m_temperature, &MetricsBroadcaster::temperatureChanged);
    tryDouble(QStringLiteral("dailyDistance"), m_dailyDistance, &MetricsBroadcaster::dailyDistanceChanged);
    tryDouble(QStringLiteral("hrvRmssd"), m_hrvRmssd, &MetricsBroadcaster::hrvRmssdChanged);
    tryDoubleNoArg(QStringLiteral("vo2max"), m_vo2max, &MetricsBroadcaster::performanceChanged);
    tryDoubleNoArg(QStringLiteral("trainingLoad"), m_trainingLoad, &MetricsBroadcaster::performanceChanged);

    if (metrics.contains(QStringLiteral("trainingStatus"))) {
        QString val = metrics[QStringLiteral("trainingStatus")].toString();
        if (val != m_trainingStatus) {
            m_trainingStatus = val;
            changed << QStringLiteral("trainingStatus");
            emit performanceChanged();
            updated++;
        }
    }

    if (updated > 0) {
        m_lastUpdate = QDateTime::currentMSecsSinceEpoch();
        emitDBusPropertiesChanged(changed);
        for (const auto &prop : changed)
            emit anyMetricChanged(prop);
    }

    return updated;
}

// ── Private helpers ─────────────────────────────────────────────────────────

void MetricsBroadcaster::emitDBusPropertiesChanged(const QStringList &properties)
{
    if (!m_registered || properties.isEmpty())
        return;

    // Emit org.freedesktop.DBus.Properties.PropertiesChanged
    QDBusMessage signal = QDBusMessage::createSignal(
        OBJECT_PATH,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));

    QVariantMap changedProps;
    QVariantMap allMetrics = GetAllMetrics();
    for (const QString &prop : properties) {
        if (allMetrics.contains(prop))
            changedProps[prop] = allMetrics[prop];
    }

    signal << QString(INTERFACE);
    signal << changedProps;
    signal << QStringList(); // invalidated properties (none)

    QDBusConnection::sessionBus().send(signal);
}

void MetricsBroadcaster::loadGoals()
{
    if (!m_database)
        return;

    // Load user goals from profile (with defaults)
    QVariantMap profile = m_database->getUserProfile();
    m_stepsGoal = profile.value(QStringLiteral("steps_goal"), 10000).toInt();
    m_caloriesGoal = profile.value(QStringLiteral("calories_goal"), 2000).toInt();
    m_activeMinutesGoal = profile.value(QStringLiteral("active_minutes_goal"), 30).toInt();
}
