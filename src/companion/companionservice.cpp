/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "companionservice.h"
#include "metricsbroadcaster.h"
#include "core/database.h"
#include "export/exportmanager.h"
#include "domain/trendsmanager.h"

#include <QDBusConnection>
#include <QDate>
#include <QDateTime>

static const char *SERVICE_NAME = "org.bolide.fitness.Companion";
static const char *OBJECT_PATH  = "/org/bolide/fitness/Companion";

CompanionService::CompanionService(QObject *parent)
    : QObject(parent)
    , m_database(nullptr)
    , m_exportManager(nullptr)
    , m_trendsManager(nullptr)
    , m_metricsBroadcaster(nullptr)
    , m_registered(false)
{
}

CompanionService::~CompanionService()
{
    unregisterService();
}

void CompanionService::setDatabase(Database *db)
{
    m_database = db;
}

void CompanionService::setExportManager(ExportManager *em)
{
    m_exportManager = em;
}

void CompanionService::setTrendsManager(TrendsManager *tm)
{
    m_trendsManager = tm;
}

void CompanionService::setMetricsBroadcaster(MetricsBroadcaster *mb)
{
    m_metricsBroadcaster = mb;
}

bool CompanionService::registerService()
{
    if (m_registered)
        return true;

    QDBusConnection bus = QDBusConnection::sessionBus();

    if (!bus.registerService(QLatin1String(SERVICE_NAME))) {
        qWarning("CompanionService: Failed to register D-Bus service '%s'", SERVICE_NAME);
        return false;
    }

    if (!bus.registerObject(QLatin1String(OBJECT_PATH), this,
                            QDBusConnection::ExportAllSlots |
                            QDBusConnection::ExportAllSignals)) {
        qWarning("CompanionService: Failed to register D-Bus object '%s'", OBJECT_PATH);
        bus.unregisterService(QLatin1String(SERVICE_NAME));
        return false;
    }

    m_registered = true;
    return true;
}

void CompanionService::unregisterService()
{
    if (!m_registered)
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(QLatin1String(OBJECT_PATH));
    bus.unregisterService(QLatin1String(SERVICE_NAME));
    m_registered = false;
}

// ── D-Bus method implementations ────────────────────────────────────────────

QVariantMap CompanionService::GetInfo()
{
    QVariantMap info;
    info[QStringLiteral("name")]    = QStringLiteral("Bolide Fitness");
    info[QStringLiteral("version")] = QStringLiteral("1.0.0");
    info[QStringLiteral("api")]     = QStringLiteral("1");

    QVariantList caps;
    caps << QStringLiteral("workouts")
         << QStringLiteral("sleep")
         << QStringLiteral("readiness")
         << QStringLiteral("trends")
         << QStringLiteral("export_fit")
         << QStringLiteral("export_tcx")
         << QStringLiteral("export_gpx");
    info[QStringLiteral("capabilities")] = caps;

    return info;
}

QVariantList CompanionService::GetRecentWorkouts(int limit)
{
    if (!m_database)
        return QVariantList();

    return m_database->recentWorkouts(limit);
}

QVariantMap CompanionService::GetWorkout(qlonglong workoutId)
{
    if (!m_database)
        return QVariantMap();

    return m_database->getWorkout(static_cast<qint64>(workoutId));
}

QVariantList CompanionService::GetGpsTrack(qlonglong workoutId)
{
    if (!m_database)
        return QVariantList();

    QVector<Database::GpsPoint> points = m_database->getGpsTrack(static_cast<qint64>(workoutId));
    QVariantList result;
    result.reserve(points.size());

    for (const auto &pt : points) {
        QVariantMap p;
        p[QStringLiteral("timestamp")] = pt.timestamp;
        p[QStringLiteral("latitude")]  = pt.latitude;
        p[QStringLiteral("longitude")] = pt.longitude;
        p[QStringLiteral("altitude")]  = pt.altitude;
        p[QStringLiteral("speed")]     = pt.speed;
        p[QStringLiteral("bearing")]   = pt.bearing;
        p[QStringLiteral("accuracy")]  = pt.accuracy;
        result.append(p);
    }

    return result;
}

QVariantList CompanionService::GetWorkoutMetrics(qlonglong workoutId)
{
    if (!m_database)
        return QVariantList();

    return m_database->getWorkoutMetrics(static_cast<qint64>(workoutId));
}

QByteArray CompanionService::ExportWorkout(qlonglong workoutId, const QString &format)
{
    if (!m_exportManager)
        return QByteArray();

    return m_exportManager->exportBytes(static_cast<qint64>(workoutId), format);
}

QVariantMap CompanionService::GetTodaySummary()
{
    if (!m_database)
        return QVariantMap();

    QVariantMap summary;
    QDate today = QDate::currentDate();

    summary[QStringLiteral("date")]           = today.toString(Qt::ISODate);
    summary[QStringLiteral("steps")]          = m_database->getDailyMetricValue(today, QStringLiteral("daily_steps"));
    summary[QStringLiteral("calories")]       = m_database->getDailyMetricValue(today, QStringLiteral("calories"));
    summary[QStringLiteral("resting_hr")]     = m_database->getDailyMetricValue(today, QStringLiteral("resting_hr"));
    summary[QStringLiteral("hrv")]            = m_database->getDailyMetricValue(today, QStringLiteral("overnight_hrv"));
    summary[QStringLiteral("sleep_quality")]  = m_database->getDailyMetricValue(today, QStringLiteral("sleep_quality"));
    summary[QStringLiteral("sleep_duration")] = m_database->getDailyMetricValue(today, QStringLiteral("sleep_duration"));
    summary[QStringLiteral("readiness")]      = m_database->getDailyMetricValue(today, QStringLiteral("readiness_score"));
    summary[QStringLiteral("stress")]         = m_database->getDailyMetricValue(today, QStringLiteral("stress_index"));
    summary[QStringLiteral("spo2")]           = m_database->getDailyMetricValue(today, QStringLiteral("spo2"));

    return summary;
}

QVariantList CompanionService::GetTrendData(const QString &metricId, int days)
{
    if (!m_trendsManager)
        return QVariantList();

    return m_trendsManager->trendData(metricId, days);
}

QVariantMap CompanionService::GetBaselines()
{
    if (!m_trendsManager)
        return QVariantMap();

    return m_trendsManager->baselines();
}

QVariantMap CompanionService::GetUserProfile()
{
    if (!m_database)
        return QVariantMap();

    return m_database->getUserProfile();
}

bool CompanionService::SetUserProfile(const QVariantMap &profile)
{
    if (!m_database)
        return false;

    return m_database->setUserProfile(profile);
}

QVariantList CompanionService::GetSleepHistory(int days)
{
    if (!m_database)
        return QVariantList();

    return m_database->getSleepHistory(days);
}

double CompanionService::GetDailyMetric(const QString &date, const QString &metricId)
{
    if (!m_database)
        return 0.0;

    QDate d = QDate::fromString(date, Qt::ISODate);
    if (!d.isValid())
        return 0.0;

    return m_database->getDailyMetricValue(d, metricId);
}

void CompanionService::TriggerSync()
{
    if (m_trendsManager)
        m_trendsManager->recomputeBaselines();

    emit SyncComplete();
}

QString CompanionService::Ping()
{
    return QStringLiteral("pong");
}

// ── Inbound sync methods (companion → watch) ───────────────────────────────

int CompanionService::PushMetrics(const QVariantMap &metrics)
{
    if (!m_metricsBroadcaster)
        return 0;

    int updated = m_metricsBroadcaster->PushMetrics(metrics);
    if (updated > 0)
        emit MetricsUpdated(metrics);

    return updated;
}

bool CompanionService::PushDailyMetrics(const QString &date, const QVariantMap &metrics)
{
    if (!m_database)
        return false;

    QDate d = QDate::fromString(date, Qt::ISODate);
    if (!d.isValid())
        return false;

    // Store each metric for the given date
    for (auto it = metrics.constBegin(); it != metrics.constEnd(); ++it) {
        m_database->storeDailyMetric(d, it.key(), it.value().toDouble());
    }

    // If this is today, also push to the live broadcaster
    if (d == QDate::currentDate() && m_metricsBroadcaster) {
        m_metricsBroadcaster->PushMetrics(metrics);
    }

    emit MetricsUpdated(metrics);
    return true;
}

qlonglong CompanionService::PushWorkout(const QVariantMap &workout, const QVariantList &samples)
{
    if (!m_database)
        return -1;

    // Create the workout record
    QString type = workout.value(QStringLiteral("type"), QStringLiteral("unknown")).toString();
    qint64 startTime = workout.value(QStringLiteral("startTime"), 0).toLongLong();
    qint64 id = m_database->createWorkout(type, startTime);
    if (id < 0)
        return -1;

    // Finish the workout with provided stats
    qint64 endTime = workout.value(QStringLiteral("endTime"), startTime).toLongLong();
    int duration = workout.value(QStringLiteral("duration"), 0).toInt();
    double distance = workout.value(QStringLiteral("distance"), 0.0).toDouble();
    double elevation = workout.value(QStringLiteral("elevationGain"), 0.0).toDouble();
    int calories = workout.value(QStringLiteral("calories"), 0).toInt();
    int avgHR = workout.value(QStringLiteral("avgHR"), 0).toInt();
    int maxHR = workout.value(QStringLiteral("maxHR"), 0).toInt();
    double avgPace = workout.value(QStringLiteral("avgPace"), 0.0).toDouble();
    m_database->finishWorkout(id, endTime, duration, distance, elevation, calories, avgHR, maxHR, avgPace);

    // Store sensor samples if provided
    if (!samples.isEmpty()) {
        QVector<QPair<qint64, float>> hrSamples;
        for (const QVariant &s : samples) {
            QVariantMap sample = s.toMap();
            qint64 ts = sample.value(QStringLiteral("timestamp"), 0).toLongLong();
            float hr = sample.value(QStringLiteral("heartRate"), 0).toFloat();
            if (ts > 0 && hr > 0)
                hrSamples.append({ts, hr});
        }
        if (!hrSamples.isEmpty())
            m_database->storeSensorBatch(id, QStringLiteral("heartRate"), hrSamples);
    }

    emit WorkoutCompleted(id, workout);
    return id;
}
