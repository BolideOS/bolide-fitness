/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "workoutmanager.h"
#include "../core/sensorservice.h"
#include "../core/datapipeline.h"
#include "../core/database.h"
#include "../core/metricsengine.h"
#include "../core/pluginregistry.h"

#include <QDebug>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <mgconfitem.h>

#define GLANCE_PREFIX "/org/bolideos/glance/bolide-fitness"
#define POWERD_SERVICE    "org.bolideos.powerd"
#define POWERD_PATH       "/org/bolideos/powerd"
#define POWERD_INTERFACE  "org.bolideos.powerd.ProfileManager"

WorkoutManager *WorkoutManager::s_instance = nullptr;

QObject *WorkoutManager::qmlInstance(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return instance();
}

WorkoutManager *WorkoutManager::instance()
{
    if (!s_instance) {
        s_instance = new WorkoutManager();
    }
    return s_instance;
}

WorkoutManager::WorkoutManager(QObject *parent)
    : QObject(parent)
    , m_sensorService(nullptr)
    , m_dataPipeline(nullptr)
    , m_database(nullptr)
    , m_metricsEngine(nullptr)
    , m_pluginRegistry(nullptr)
    , m_powerd(nullptr)
    , m_systemBus(QDBusConnection::systemBus())
    , m_active(false)
    , m_paused(false)
    , m_workoutDbId(-1)
    , m_startTimeEpoch(0)
    , m_elapsedSeconds(0)
    , m_pausedSeconds(0)
    , m_heartRate(0)
    , m_distance(0.0)
    , m_elevationGain(0.0)
    , m_currentAltitude(0.0f)
    , m_calories(0)
    , m_steps(0)
    , m_latitude(0.0)
    , m_longitude(0.0)
    , m_currentPace(0.0f)
    , m_currentSpeed(0.0f)
{
    // Connect to powerd
    if (m_systemBus.isConnected()) {
        m_powerd = new QDBusInterface(
            POWERD_SERVICE, POWERD_PATH, POWERD_INTERFACE,
            m_systemBus, this);

        if (!m_powerd->isValid()) {
            qWarning() << "WorkoutManager: powerd not available:"
                       << m_powerd->lastError().message();
            delete m_powerd;
            m_powerd = nullptr;
        } else {
            m_systemBus.connect(
                POWERD_SERVICE, POWERD_PATH, POWERD_INTERFACE,
                QStringLiteral("ActiveProfileChanged"), this,
                SLOT(onPowerdActiveProfileChanged(QString,QString)));

            QVariant reply = callPowerdSync(QStringLiteral("GetActiveProfile"));
            if (reply.isValid())
                m_activeProfileId = reply.toString();
        }
    }

    // Elapsed timer
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, &WorkoutManager::onElapsedTick);

    // Glance dconf items
    m_glanceTitle     = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/title"), this);
    m_glanceValue     = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/value"), this);
    m_glanceSubtitle  = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/subtitle"), this);
    m_glanceGraph     = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/graph"), this);
    m_glanceColors    = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/colors"), this);
    m_glanceTimestamp = new MGConfItem(QStringLiteral(GLANCE_PREFIX "/timestamp"), this);

    publishGlance();
}

WorkoutManager::~WorkoutManager()
{
    if (m_active) stopWorkout();
}

void WorkoutManager::setSensorService(SensorService *ss)
{
    m_sensorService = ss;
    connect(ss, &SensorService::heartRateChanged, this, &WorkoutManager::onHeartRateChanged);
    connect(ss, &SensorService::gpsPositionChanged, this, &WorkoutManager::onGpsPosition);
}

void WorkoutManager::setDataPipeline(DataPipeline *dp)
{
    m_dataPipeline = dp;
    connect(dp, &DataPipeline::distanceUpdated, this, &WorkoutManager::onDistanceUpdated);
    connect(dp, &DataPipeline::elevationUpdated, this, &WorkoutManager::onElevationUpdated);
    connect(dp, &DataPipeline::stepsUpdated, this, &WorkoutManager::onStepsUpdated);
    connect(dp, &DataPipeline::paceUpdated, this, &WorkoutManager::onPaceUpdated);
}

void WorkoutManager::setDatabase(Database *db)
{
    m_database = db;
}

void WorkoutManager::setMetricsEngine(MetricsEngine *me)
{
    m_metricsEngine = me;
}

void WorkoutManager::setPluginRegistry(PluginRegistry *pr)
{
    m_pluginRegistry = pr;
}

// ─── Property getters ───────────────────────────────────────────────────────

QString WorkoutManager::workoutName() const
{
    if (!m_pluginRegistry) return m_workoutType;
    QVariantMap wt = m_pluginRegistry->workoutType(m_workoutType);
    return wt.value(QStringLiteral("name"), m_workoutType).toString();
}

QString WorkoutManager::workoutIcon() const
{
    if (!m_pluginRegistry) return QStringLiteral("ios-fitness-outline");
    QVariantMap wt = m_pluginRegistry->workoutType(m_workoutType);
    return wt.value(QStringLiteral("icon"), QStringLiteral("ios-fitness-outline")).toString();
}

QVariantList WorkoutManager::workoutTypes() const
{
    if (m_pluginRegistry) return m_pluginRegistry->workoutTypes();
    return QVariantList();
}

QVariantList WorkoutManager::recentWorkouts() const
{
    if (m_database) return m_database->recentWorkouts(20);
    return QVariantList();
}

// ─── Workout lifecycle ──────────────────────────────────────────────────────

bool WorkoutManager::startWorkout(const QString &type)
{
    if (m_active) {
        emit workoutError(QStringLiteral("Workout already active"));
        return false;
    }

    if (m_pluginRegistry && !m_pluginRegistry->hasWorkoutType(type)) {
        emit workoutError(QStringLiteral("Unknown workout type: ") + type);
        return false;
    }

    // Save profile and switch to workout profile
    m_previousProfileId = m_activeProfileId;
    callPowerd(QStringLiteral("StartWorkout"), QVariantList() << type);

    // Create DB record
    m_startTimeEpoch = QDateTime::currentSecsSinceEpoch();
    if (m_database) {
        m_workoutDbId = m_database->createWorkout(type, m_startTimeEpoch, m_activeProfileId);
    }

    // Reset state
    m_active = true;
    m_paused = false;
    m_workoutType = type;
    m_elapsedSeconds = 0;
    m_pausedSeconds = 0;
    m_heartRate = 0;
    m_distance = 0.0;
    m_elevationGain = 0.0;
    m_currentAltitude = 0.0f;
    m_calories = 0;
    m_steps = 0;
    m_latitude = 0.0;
    m_longitude = 0.0;
    m_currentPace = 0.0f;
    m_currentSpeed = 0.0f;
    m_hrHistory.clear();
    m_lastMetrics.clear();

    emit activeChanged();
    emit workoutTypeChanged();
    emit elapsedSecondsChanged();
    emit heartRateChanged();
    emit distanceChanged();
    emit elevationChanged();
    emit caloriesChanged();
    emit stepsChanged();
    emit hrHistoryChanged();
    emit metricsChanged();

    // Subscribe sensors
    subscribeSensors();

    // Start data pipeline recording
    if (m_dataPipeline && m_workoutDbId >= 0) {
        m_dataPipeline->startRecording(m_workoutDbId);
    }

    // Start elapsed timer
    m_elapsedTimer->start();

    publishGlance();
    emit workoutStarted(type);

    qDebug() << "Workout started:" << type << "db_id:" << m_workoutDbId;
    return true;
}

bool WorkoutManager::stopWorkout()
{
    if (!m_active) return false;

    // Stop timer
    m_elapsedTimer->stop();

    // Stop data pipeline recording
    if (m_dataPipeline) {
        m_dataPipeline->stopRecording();
    }

    // Unsubscribe sensors
    unsubscribeSensors();

    // Compute post-workout metrics
    computePostWorkoutMetrics();

    // Finalize DB record
    if (m_database && m_workoutDbId >= 0) {
        qint64 endTime = QDateTime::currentSecsSinceEpoch();
        int avgHR = 0;
        int maxHR = 0;
        if (m_dataPipeline) {
            avgHR = static_cast<int>(m_dataPipeline->hrMean());
            maxHR = static_cast<int>(m_dataPipeline->hrMax());
        }
        m_database->finishWorkout(m_workoutDbId, endTime,
                                   m_elapsedSeconds, m_distance,
                                   m_elevationGain, m_calories,
                                   avgHR, maxHR,
                                   static_cast<double>(m_currentPace));
    }

    // Restore previous profile
    callPowerd(QStringLiteral("StopWorkout"));

    qint64 finishedId = m_workoutDbId;

    // Reset state
    m_active = false;
    m_paused = false;
    m_workoutType.clear();
    m_workoutDbId = -1;

    emit activeChanged();
    emit pausedChanged();
    emit workoutTypeChanged();
    emit recentWorkoutsChanged();

    publishGlance();
    emit workoutStopped(finishedId);

    qDebug() << "Workout stopped. ID:" << finishedId;
    return true;
}

void WorkoutManager::pauseWorkout()
{
    if (!m_active || m_paused) return;
    m_paused = true;
    m_elapsedTimer->stop();
    emit pausedChanged();
}

void WorkoutManager::resumeWorkout()
{
    if (!m_active || !m_paused) return;
    m_paused = false;
    m_elapsedTimer->start();
    emit pausedChanged();
}

// ─── Profile management ─────────────────────────────────────────────────────

QString WorkoutManager::profileForWorkout(const QString &workoutType)
{
    QVariant reply = callPowerdSync(QStringLiteral("GetWorkoutProfiles"));
    if (!reply.isValid()) return QString();

    QJsonDocument doc = QJsonDocument::fromJson(reply.toString().toUtf8());
    if (!doc.isObject()) return QString();

    return doc.object().value(workoutType).toString();
}

bool WorkoutManager::setProfileForWorkout(const QString &workoutType,
                                           const QString &profileId)
{
    return callPowerd(QStringLiteral("SetWorkoutProfile"),
                       QVariantList() << workoutType << profileId);
}

QVariantList WorkoutManager::availableProfiles()
{
    QVariant reply = callPowerdSync(QStringLiteral("GetProfiles"));
    if (!reply.isValid()) return QVariantList();

    QJsonDocument doc = QJsonDocument::fromJson(reply.toString().toUtf8());
    if (!doc.isArray()) return QVariantList();

    QVariantList result;
    for (const QJsonValue &val : doc.array()) {
        if (!val.isObject()) continue;
        QJsonObject p = val.toObject();
        QVariantMap map;
        map[QStringLiteral("id")] = p[QStringLiteral("id")].toString();
        map[QStringLiteral("name")] = p[QStringLiteral("name")].toString();
        map[QStringLiteral("icon")] = p[QStringLiteral("icon")].toString();
        map[QStringLiteral("color")] = p[QStringLiteral("color")].toString();
        result.append(map);
    }
    return result;
}

QStringList WorkoutManager::dataScreensForWorkout(const QString &workoutType)
{
    if (!m_pluginRegistry)
        return QStringList() << QStringLiteral("default_timer")
                              << QStringLiteral("default_hr");
    return m_pluginRegistry->defaultScreensForWorkout(workoutType);
}

QVariantMap WorkoutManager::userProfile()
{
    if (m_database) return m_database->getUserProfile();
    return QVariantMap();
}

bool WorkoutManager::setUserProfile(const QVariantMap &profile)
{
    if (m_database) return m_database->setUserProfile(profile);
    return false;
}

// ─── Slots ──────────────────────────────────────────────────────────────────

void WorkoutManager::onElapsedTick()
{
    if (!m_active || m_paused) return;

    m_elapsedSeconds++;
    emit elapsedSecondsChanged();

    // Update calories estimate periodically
    if (m_metricsEngine && m_elapsedSeconds % 10 == 0) {
        QVariantMap inputs;
        inputs[QStringLiteral("time_s")] = static_cast<double>(m_elapsedSeconds);
        inputs[QStringLiteral("hr_avg")] = (m_dataPipeline ? m_dataPipeline->hrMean() : 0.0);
        inputs[QStringLiteral("workout_type")] = m_workoutType;

        QVariantMap profile = userProfile();
        if (profile.contains(QStringLiteral("weight_kg")))
            inputs[QStringLiteral("weight_kg")] = profile[QStringLiteral("weight_kg")].toDouble();
        if (profile.contains(QStringLiteral("age")))
            inputs[QStringLiteral("age")] = profile[QStringLiteral("age")].toInt();
        if (profile.contains(QStringLiteral("gender")))
            inputs[QStringLiteral("gender")] = profile[QStringLiteral("gender")];

        QVariant cal = m_metricsEngine->compute(QStringLiteral("calories"), inputs);
        if (cal.isValid()) {
            m_calories = cal.toInt();
            emit caloriesChanged();
        }
    }

    // Publish glance every 5 seconds
    if (m_elapsedSeconds % 5 == 0)
        publishGlance();
}

void WorkoutManager::onHeartRateChanged(int bpm)
{
    if (!m_active) return;

    m_heartRate = bpm;
    emit heartRateChanged();

    // Add to history ring (keep last 60)
    QVariantMap reading;
    reading[QStringLiteral("timestamp")] = QDateTime::currentSecsSinceEpoch();
    reading[QStringLiteral("bpm")] = bpm;
    m_hrHistory.append(reading);
    while (m_hrHistory.size() > 60) m_hrHistory.removeFirst();
    emit hrHistoryChanged();
}

void WorkoutManager::onDistanceUpdated(double meters)
{
    m_distance = meters;
    emit distanceChanged();
}

void WorkoutManager::onElevationUpdated(double gainMeters, float currentAlt)
{
    m_elevationGain = gainMeters;
    m_currentAltitude = currentAlt;
    emit elevationChanged();
}

void WorkoutManager::onStepsUpdated(int totalSteps)
{
    m_steps = totalSteps;
    emit stepsChanged();
}

void WorkoutManager::onPaceUpdated(float secPerKm)
{
    m_currentPace = secPerKm;
    if (secPerKm > 0.0f) m_currentSpeed = 1000.0f / secPerKm;
    emit paceChanged();
}

void WorkoutManager::onGpsPosition(double lat, double lon, double alt,
                                    float speed, float bearing)
{
    Q_UNUSED(alt);
    Q_UNUSED(bearing);
    m_latitude = lat;
    m_longitude = lon;
    m_currentSpeed = speed;
    emit positionChanged();
}

void WorkoutManager::onPowerdActiveProfileChanged(const QString &id,
                                                    const QString &name)
{
    Q_UNUSED(name);
    m_activeProfileId = id;
    emit activeProfileIdChanged();
}

// ─── Private helpers ────────────────────────────────────────────────────────

void WorkoutManager::subscribeSensors()
{
    if (!m_sensorService) return;

    // Always subscribe: HR
    m_sensorService->subscribe(SensorService::HeartRate, 1000);
    m_sensorService->subscribe(SensorService::StepCount, 1000);

    // Check workout config for GPS/baro requirements
    if (m_pluginRegistry) {
        QVariantMap wt = m_pluginRegistry->workoutType(m_workoutType);
        bool gps = wt.value(QStringLiteral("gps_default"), true).toBool();
        bool baro = wt.value(QStringLiteral("barometer_default"), true).toBool();

        if (gps) m_sensorService->subscribe(SensorService::GPS, 1000);
        if (baro) m_sensorService->subscribe(SensorService::Barometer, 1000);
    } else {
        // Default: subscribe to GPS and baro
        m_sensorService->subscribe(SensorService::GPS, 1000);
        m_sensorService->subscribe(SensorService::Barometer, 1000);
    }

    // Tell simulated backend what workout we're doing
    if (m_sensorService->isSimulated()) {
        for (SensorService::Backend *b : QVector<SensorService::Backend*>()) {
            // Access through the service — handled internally by SimulatedBackend
        }
    }
}

void WorkoutManager::unsubscribeSensors()
{
    if (!m_sensorService) return;

    m_sensorService->unsubscribe(SensorService::HeartRate);
    m_sensorService->unsubscribe(SensorService::StepCount);
    m_sensorService->unsubscribe(SensorService::GPS);
    m_sensorService->unsubscribe(SensorService::Barometer);
}

void WorkoutManager::computePostWorkoutMetrics()
{
    if (!m_metricsEngine || !m_dataPipeline) return;

    QVariantMap inputs;
    inputs[QStringLiteral("distance_m")] = m_distance;
    inputs[QStringLiteral("time_s")] = static_cast<double>(m_elapsedSeconds);
    inputs[QStringLiteral("hr_avg")] = static_cast<double>(m_dataPipeline->hrMean());
    inputs[QStringLiteral("hr_max")] = static_cast<double>(m_dataPipeline->hrMax());
    inputs[QStringLiteral("workout_type")] = m_workoutType;
    inputs[QStringLiteral("has_barometer")] = m_dataPipeline->hasBarometerElevation();
    inputs[QStringLiteral("elev_gain_m")] = m_elevationGain;

    // Elevation series for Hill Score
    QVector<float> elevSeries = m_dataPipeline->elevationSeries();
    if (!elevSeries.isEmpty()) {
        QVariantList elevList;
        elevList.reserve(elevSeries.size());
        for (float v : elevSeries) elevList.append(v);
        inputs[QStringLiteral("elev_series")] = elevList;
        inputs[QStringLiteral("elev_start")] = static_cast<double>(elevSeries.first());
        inputs[QStringLiteral("elev_end")] = static_cast<double>(elevSeries.last());
    }

    // User profile data
    QVariantMap profile = userProfile();
    for (auto it = profile.constBegin(); it != profile.constEnd(); ++it) {
        inputs[it.key()] = it.value();
    }

    // Compute all applicable metrics
    m_lastMetrics = m_metricsEngine->computeAll(inputs);
    emit metricsChanged();

    // Store metrics in DB
    if (m_database && m_workoutDbId >= 0) {
        for (auto it = m_lastMetrics.constBegin(); it != m_lastMetrics.constEnd(); ++it) {
            m_database->storeWorkoutMetric(m_workoutDbId, it.key(),
                                            it.value().toDouble());
        }
    }

    qDebug() << "Post-workout metrics computed:" << m_lastMetrics.keys();
}

void WorkoutManager::publishGlance()
{
    qint64 now = QDateTime::currentSecsSinceEpoch();

    if (!m_active) {
        m_glanceTitle->set(QStringLiteral("Workout"));
        m_glanceValue->set(QStringLiteral("Ready"));
        m_glanceSubtitle->set(QStringLiteral("Tap to start"));
        m_glanceGraph->set(QString());
        m_glanceColors->set(QString());
        m_glanceTimestamp->set(now);
        return;
    }

    m_glanceTitle->set(workoutName());

    // Elapsed time
    int h = m_elapsedSeconds / 3600;
    int m = (m_elapsedSeconds % 3600) / 60;
    int s = m_elapsedSeconds % 60;
    QString elapsed = h > 0
        ? QString::asprintf("%d:%02d:%02d", h, m, s)
        : QString::asprintf("%d:%02d", m, s);
    m_glanceValue->set(elapsed);

    // Subtitle
    if (m_heartRate > 0) {
        m_glanceSubtitle->set(QString::number(m_heartRate) + QStringLiteral(" bpm"));
    } else {
        m_glanceSubtitle->set(QString::number(m_calories) + QStringLiteral(" cal"));
    }

    // HR graph
    if (!m_hrHistory.isEmpty()) {
        QStringList graphParts;
        int count = qMin(m_hrHistory.size(), 30);
        int start = m_hrHistory.size() - count;
        for (int i = start; i < m_hrHistory.size(); ++i) {
            int bpm = m_hrHistory[i].toMap()[QStringLiteral("bpm")].toInt();
            double normalized = qBound(0.0, (bpm - 60.0) / 140.0, 1.0);
            graphParts.append(QString::number(normalized, 'f', 2));
        }
        m_glanceGraph->set(graphParts.join(QStringLiteral(",")));
        m_glanceColors->set(QStringLiteral("#4CAF50,#FF9800,#F44336"));
    }

    m_glanceTimestamp->set(now);
}

void WorkoutManager::clearGlance()
{
    m_glanceTitle->unset();
    m_glanceValue->unset();
    m_glanceSubtitle->unset();
    m_glanceGraph->unset();
    m_glanceColors->unset();
    m_glanceTimestamp->unset();
}

bool WorkoutManager::callPowerd(const QString &method, const QVariantList &args)
{
    if (!m_powerd || !m_powerd->isValid()) return false;
    QDBusMessage reply = m_powerd->callWithArgumentList(QDBus::Block, method, args);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "D-Bus" << method << "failed:" << reply.errorMessage();
        return false;
    }
    if (!reply.arguments().isEmpty()) {
        QVariant first = reply.arguments().first();
        if (first.type() == QVariant::Bool) return first.toBool();
    }
    return true;
}

QVariant WorkoutManager::callPowerdSync(const QString &method, const QVariantList &args)
{
    if (!m_powerd || !m_powerd->isValid()) return QVariant();
    QDBusMessage reply = m_powerd->callWithArgumentList(QDBus::Block, method, args);
    if (reply.type() == QDBusMessage::ErrorMessage) return QVariant();
    return reply.arguments().isEmpty() ? QVariant() : reply.arguments().first();
}
