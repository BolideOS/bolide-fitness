/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "metricsclient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusMessage>

static const char *SERVICE_NAME = "org.bolide.fitness.Metrics";
static const char *OBJECT_PATH  = "/org/bolide/fitness/Metrics";
static const char *INTERFACE    = "org.bolide.fitness.Metrics";

MetricsClient::MetricsClient(QObject *parent)
    : QObject(parent)
    , m_watcher(nullptr)
    , m_iface(nullptr)
    , m_connected(false)
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
    , m_stepsGoalProgress(0.0)
    , m_caloriesGoalProgress(0.0)
    , m_activeMinutesGoalProgress(0.0)
{
    // Watch for service appearing/disappearing
    m_watcher = new QDBusServiceWatcher(
        SERVICE_NAME,
        QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForRegistration |
            QDBusServiceWatcher::WatchForUnregistration,
        this);

    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered,
            this, &MetricsClient::onServiceRegistered);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &MetricsClient::onServiceUnregistered);

    // Subscribe to PropertiesChanged on the bus
    QDBusConnection::sessionBus().connect(
        SERVICE_NAME,
        OBJECT_PATH,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    // Try to connect now (service might already be running)
    connectToService();
}

MetricsClient::~MetricsClient()
{
    delete m_iface;
}

void MetricsClient::onServiceRegistered(const QString &service)
{
    Q_UNUSED(service)
    connectToService();
}

void MetricsClient::onServiceUnregistered(const QString &service)
{
    Q_UNUSED(service)
    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
}

void MetricsClient::connectToService()
{
    delete m_iface;
    m_iface = new QDBusInterface(
        SERVICE_NAME,
        OBJECT_PATH,
        INTERFACE,
        QDBusConnection::sessionBus(),
        this);

    if (m_iface->isValid()) {
        m_connected = true;
        emit connectedChanged();
        fetchAllMetrics();
    } else {
        m_connected = false;
        emit connectedChanged();
    }
}

void MetricsClient::fetchAllMetrics()
{
    if (!m_iface || !m_iface->isValid())
        return;

    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("GetAllMetrics"));
    if (reply.isValid()) {
        applyMetrics(reply.value());
    }
}

void MetricsClient::onPropertiesChanged(const QString &interface,
                                         const QVariantMap &changed,
                                         const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    if (interface != QLatin1String(INTERFACE))
        return;

    applyMetrics(changed);
}

void MetricsClient::applyMetrics(const QVariantMap &metrics)
{
    // Helper macros for brevity
#define UPDATE_INT(key, field, signal) \
    if (metrics.contains(QStringLiteral(key))) { \
        int v = metrics[QStringLiteral(key)].toInt(); \
        if (v != field) { field = v; emit signal(); emit metricsUpdated(QStringLiteral(key)); } \
    }

#define UPDATE_DOUBLE(key, field, signal) \
    if (metrics.contains(QStringLiteral(key))) { \
        double v = metrics[QStringLiteral(key)].toDouble(); \
        if (qAbs(v - field) > 0.001) { field = v; emit signal(); emit metricsUpdated(QStringLiteral(key)); } \
    }

#define UPDATE_STRING(key, field, signal) \
    if (metrics.contains(QStringLiteral(key))) { \
        QString v = metrics[QStringLiteral(key)].toString(); \
        if (v != field) { field = v; emit signal(); emit metricsUpdated(QStringLiteral(key)); } \
    }

    UPDATE_INT("heartRate",          m_heartRate,          heartRateChanged)
    UPDATE_INT("spo2",               m_spo2,              spo2Changed)
    UPDATE_INT("stressIndex",        m_stressIndex,       stressIndexChanged)
    UPDATE_INT("breathingRate",      m_breathingRate,     breathingRateChanged)
    UPDATE_INT("dailySteps",         m_dailySteps,        dailyStepsChanged)
    UPDATE_INT("dailyCalories",      m_dailyCalories,     dailyCaloriesChanged)
    UPDATE_INT("dailyFloorsClimbed", m_dailyFloorsClimbed, dailyFloorsClimbedChanged)
    UPDATE_INT("activeMinutes",      m_activeMinutes,     activeMinutesChanged)
    UPDATE_INT("restingHr",          m_restingHr,         restingHrChanged)
    UPDATE_INT("readinessScore",     m_readinessScore,    readinessScoreChanged)
    UPDATE_INT("recoveryTime",       m_recoveryTime,      recoveryTimeChanged)
    UPDATE_INT("sleepQuality",       m_sleepQuality,      sleepChanged)
    UPDATE_INT("sleepDuration",      m_sleepDuration,     sleepChanged)
    UPDATE_INT("sleepDeep",          m_sleepDeep,         sleepChanged)
    UPDATE_INT("sleepRem",           m_sleepRem,          sleepChanged)
    UPDATE_INT("sleepLight",         m_sleepLight,        sleepChanged)

    UPDATE_DOUBLE("temperature",                m_temperature,                temperatureChanged)
    UPDATE_DOUBLE("dailyDistance",              m_dailyDistance,              dailyDistanceChanged)
    UPDATE_DOUBLE("hrvRmssd",                  m_hrvRmssd,                  hrvRmssdChanged)
    UPDATE_DOUBLE("vo2max",                    m_vo2max,                    performanceChanged)
    UPDATE_DOUBLE("trainingLoad",              m_trainingLoad,              performanceChanged)
    UPDATE_DOUBLE("stepsGoalProgress",         m_stepsGoalProgress,         goalsChanged)
    UPDATE_DOUBLE("caloriesGoalProgress",      m_caloriesGoalProgress,      goalsChanged)
    UPDATE_DOUBLE("activeMinutesGoalProgress", m_activeMinutesGoalProgress, goalsChanged)

    UPDATE_STRING("trainingStatus", m_trainingStatus, performanceChanged)

#undef UPDATE_INT
#undef UPDATE_DOUBLE
#undef UPDATE_STRING
}
