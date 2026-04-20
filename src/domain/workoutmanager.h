/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WORKOUTMANAGER_H
#define WORKOUTMANAGER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QQmlEngine>
#include <QJSEngine>

class MGConfItem;
class SensorService;
class DataPipeline;
class Database;
class MetricsEngine;
class PluginRegistry;

/**
 * @brief Workout lifecycle manager — the main QML-facing controller.
 *
 * Orchestrates:
 * - Workout start/stop with powerd profile switching
 * - Sensor subscriptions for active workout
 * - Data pipeline recording
 * - Post-workout metric computation
 * - Glance card publishing
 * - Recent workout history
 *
 * Exposed to QML as a singleton via org.bolide.fitness 1.0 / WorkoutManager.
 */
class WorkoutManager : public QObject
{
    Q_OBJECT

    // ── Active workout properties (bindable from QML) ───────────────────
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(QString workoutType READ workoutType NOTIFY workoutTypeChanged)
    Q_PROPERTY(QString workoutName READ workoutName NOTIFY workoutTypeChanged)
    Q_PROPERTY(QString workoutIcon READ workoutIcon NOTIFY workoutTypeChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY elapsedSecondsChanged)
    Q_PROPERTY(int heartRate READ heartRate NOTIFY heartRateChanged)
    Q_PROPERTY(double distance READ distance NOTIFY distanceChanged)
    Q_PROPERTY(double elevationGain READ elevationGain NOTIFY elevationChanged)
    Q_PROPERTY(float currentAltitude READ currentAltitude NOTIFY elevationChanged)
    Q_PROPERTY(int calories READ calories NOTIFY caloriesChanged)
    Q_PROPERTY(int steps READ steps NOTIFY stepsChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY positionChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY positionChanged)
    Q_PROPERTY(float currentPace READ currentPace NOTIFY paceChanged)
    Q_PROPERTY(float currentSpeed READ currentSpeed NOTIFY paceChanged)
    Q_PROPERTY(QVariantList hrHistory READ hrHistory NOTIFY hrHistoryChanged)
    Q_PROPERTY(QString activeProfileId READ activeProfileId NOTIFY activeProfileIdChanged)
    Q_PROPERTY(QVariantMap lastMetrics READ lastMetrics NOTIFY metricsChanged)

    // ── Workout catalog ─────────────────────────────────────────────────
    Q_PROPERTY(QVariantList workoutTypes READ workoutTypes NOTIFY workoutTypesChanged)
    Q_PROPERTY(QVariantList recentWorkouts READ recentWorkouts NOTIFY recentWorkoutsChanged)

public:
    static QObject *qmlInstance(QQmlEngine *engine, QJSEngine *scriptEngine);
    static WorkoutManager *instance();

    explicit WorkoutManager(QObject *parent = nullptr);
    ~WorkoutManager() override;

    // ── Dependency injection ────────────────────────────────────────────
    void setSensorService(SensorService *ss);
    void setDataPipeline(DataPipeline *dp);
    void setDatabase(Database *db);
    void setMetricsEngine(MetricsEngine *me);
    void setPluginRegistry(PluginRegistry *pr);

    // ── Property getters ────────────────────────────────────────────────
    bool active() const { return m_active; }
    bool paused() const { return m_paused; }
    QString workoutType() const { return m_workoutType; }
    QString workoutName() const;
    QString workoutIcon() const;
    int elapsedSeconds() const { return m_elapsedSeconds; }
    int heartRate() const { return m_heartRate; }
    double distance() const { return m_distance; }
    double elevationGain() const { return m_elevationGain; }
    float currentAltitude() const { return m_currentAltitude; }
    int calories() const { return m_calories; }
    int steps() const { return m_steps; }
    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }
    float currentPace() const { return m_currentPace; }
    float currentSpeed() const { return m_currentSpeed; }
    QVariantList hrHistory() const { return m_hrHistory; }
    QString activeProfileId() const { return m_activeProfileId; }
    QVariantMap lastMetrics() const { return m_lastMetrics; }
    QVariantList workoutTypes() const;
    QVariantList recentWorkouts() const;

    // ── QML-invokable methods ───────────────────────────────────────────

    Q_INVOKABLE bool startWorkout(const QString &type);
    Q_INVOKABLE bool stopWorkout();
    Q_INVOKABLE void pauseWorkout();
    Q_INVOKABLE void resumeWorkout();

    Q_INVOKABLE QString profileForWorkout(const QString &workoutType);
    Q_INVOKABLE bool setProfileForWorkout(const QString &workoutType,
                                           const QString &profileId);
    Q_INVOKABLE QVariantList availableProfiles();
    Q_INVOKABLE QStringList dataScreensForWorkout(const QString &workoutType);

    /** Get user profile for metric computation. */
    Q_INVOKABLE QVariantMap userProfile();
    Q_INVOKABLE bool setUserProfile(const QVariantMap &profile);

signals:
    void activeChanged();
    void pausedChanged();
    void workoutTypeChanged();
    void elapsedSecondsChanged();
    void heartRateChanged();
    void distanceChanged();
    void elevationChanged();
    void caloriesChanged();
    void stepsChanged();
    void positionChanged();
    void paceChanged();
    void hrHistoryChanged();
    void activeProfileIdChanged();
    void metricsChanged();
    void workoutTypesChanged();
    void recentWorkoutsChanged();

    void workoutStarted(const QString &type);
    void workoutStopped(qint64 workoutId);
    void workoutError(const QString &error);

private slots:
    void onElapsedTick();
    void onHeartRateChanged(int bpm);
    void onDistanceUpdated(double meters);
    void onElevationUpdated(double gainMeters, float currentAltitude);
    void onStepsUpdated(int totalSteps);
    void onPaceUpdated(float secPerKm);
    void onGpsPosition(double lat, double lon, double alt, float speed, float bearing);
    void onPowerdActiveProfileChanged(const QString &id, const QString &name);

private:
    bool callPowerd(const QString &method, const QVariantList &args = QVariantList());
    QVariant callPowerdSync(const QString &method, const QVariantList &args = QVariantList());
    void subscribeSensors();
    void unsubscribeSensors();
    void computePostWorkoutMetrics();
    void publishGlance();
    void clearGlance();

    static WorkoutManager *s_instance;

    // Dependencies
    SensorService *m_sensorService;
    DataPipeline *m_dataPipeline;
    Database *m_database;
    MetricsEngine *m_metricsEngine;
    PluginRegistry *m_pluginRegistry;

    // D-Bus
    QDBusInterface *m_powerd;
    QDBusConnection m_systemBus;

    // Timers
    QTimer *m_elapsedTimer;

    // Workout state
    bool m_active;
    bool m_paused;
    QString m_workoutType;
    qint64 m_workoutDbId;
    qint64 m_startTimeEpoch;
    int m_elapsedSeconds;
    int m_pausedSeconds;

    // Live data
    int    m_heartRate;
    double m_distance;
    double m_elevationGain;
    float  m_currentAltitude;
    int    m_calories;
    int    m_steps;
    double m_latitude;
    double m_longitude;
    float  m_currentPace;
    float  m_currentSpeed;

    // HR history for graph (last 60 readings)
    QVariantList m_hrHistory;

    // Profile state
    QString m_activeProfileId;
    QString m_previousProfileId;

    // Post-workout metrics
    QVariantMap m_lastMetrics;

    // Glance items
    MGConfItem *m_glanceTitle;
    MGConfItem *m_glanceValue;
    MGConfItem *m_glanceSubtitle;
    MGConfItem *m_glanceGraph;
    MGConfItem *m_glanceColors;
    MGConfItem *m_glanceTimestamp;
};

#endif // WORKOUTMANAGER_H
