/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * BolideMetrics QML Plugin — lightweight D-Bus consumer.
 *
 * Any QML application can:
 *   import BolideMetrics 1.0
 *   MetricsService { id: metrics }
 *   Text { text: metrics.heartRate + " bpm" }
 *
 * This plugin connects to org.bolide.fitness.Metrics over D-Bus and
 * mirrors all properties locally, subscribing to PropertiesChanged.
 * It does NOT link to bolide-fitness — purely a D-Bus client.
 */

#ifndef METRICSCLIENT_H
#define METRICSCLIENT_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QVariantMap>
#include <QTimer>

/**
 * @brief QML-friendly D-Bus client for the metrics broadcaster.
 *
 * Automatically connects when the service appears and reconnects
 * if it restarts. All properties are kept in sync via
 * org.freedesktop.DBus.Properties.PropertiesChanged signals.
 */
class MetricsClient : public QObject
{
    Q_OBJECT

    // ── Body ────────────────────────────────────────────────────────────
    Q_PROPERTY(int heartRate READ heartRate NOTIFY heartRateChanged)
    Q_PROPERTY(int spo2 READ spo2 NOTIFY spo2Changed)
    Q_PROPERTY(int stressIndex READ stressIndex NOTIFY stressIndexChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(int breathingRate READ breathingRate NOTIFY breathingRateChanged)

    // ── Activity ────────────────────────────────────────────────────────
    Q_PROPERTY(int dailySteps READ dailySteps NOTIFY dailyStepsChanged)
    Q_PROPERTY(int dailyCalories READ dailyCalories NOTIFY dailyCaloriesChanged)
    Q_PROPERTY(int dailyFloorsClimbed READ dailyFloorsClimbed NOTIFY dailyFloorsClimbedChanged)
    Q_PROPERTY(double dailyDistance READ dailyDistance NOTIFY dailyDistanceChanged)
    Q_PROPERTY(int activeMinutes READ activeMinutes NOTIFY activeMinutesChanged)

    // ── Recovery ────────────────────────────────────────────────────────
    Q_PROPERTY(int restingHr READ restingHr NOTIFY restingHrChanged)
    Q_PROPERTY(double hrvRmssd READ hrvRmssd NOTIFY hrvRmssdChanged)
    Q_PROPERTY(int readinessScore READ readinessScore NOTIFY readinessScoreChanged)
    Q_PROPERTY(int recoveryTime READ recoveryTime NOTIFY recoveryTimeChanged)

    // ── Sleep ───────────────────────────────────────────────────────────
    Q_PROPERTY(int sleepQuality READ sleepQuality NOTIFY sleepChanged)
    Q_PROPERTY(int sleepDuration READ sleepDuration NOTIFY sleepChanged)
    Q_PROPERTY(int sleepDeep READ sleepDeep NOTIFY sleepChanged)
    Q_PROPERTY(int sleepRem READ sleepRem NOTIFY sleepChanged)
    Q_PROPERTY(int sleepLight READ sleepLight NOTIFY sleepChanged)

    // ── Performance ─────────────────────────────────────────────────────
    Q_PROPERTY(double vo2max READ vo2max NOTIFY performanceChanged)
    Q_PROPERTY(double trainingLoad READ trainingLoad NOTIFY performanceChanged)
    Q_PROPERTY(QString trainingStatus READ trainingStatus NOTIFY performanceChanged)

    // ── Goals ───────────────────────────────────────────────────────────
    Q_PROPERTY(double stepsGoalProgress READ stepsGoalProgress NOTIFY goalsChanged)
    Q_PROPERTY(double caloriesGoalProgress READ caloriesGoalProgress NOTIFY goalsChanged)
    Q_PROPERTY(double activeMinutesGoalProgress READ activeMinutesGoalProgress NOTIFY goalsChanged)

    // ── Connection state ────────────────────────────────────────────────
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
    explicit MetricsClient(QObject *parent = nullptr);
    ~MetricsClient() override;

    // Property getters
    int heartRate() const { return m_heartRate; }
    int spo2() const { return m_spo2; }
    int stressIndex() const { return m_stressIndex; }
    double temperature() const { return m_temperature; }
    int breathingRate() const { return m_breathingRate; }

    int dailySteps() const { return m_dailySteps; }
    int dailyCalories() const { return m_dailyCalories; }
    int dailyFloorsClimbed() const { return m_dailyFloorsClimbed; }
    double dailyDistance() const { return m_dailyDistance; }
    int activeMinutes() const { return m_activeMinutes; }

    int restingHr() const { return m_restingHr; }
    double hrvRmssd() const { return m_hrvRmssd; }
    int readinessScore() const { return m_readinessScore; }
    int recoveryTime() const { return m_recoveryTime; }

    int sleepQuality() const { return m_sleepQuality; }
    int sleepDuration() const { return m_sleepDuration; }
    int sleepDeep() const { return m_sleepDeep; }
    int sleepRem() const { return m_sleepRem; }
    int sleepLight() const { return m_sleepLight; }

    double vo2max() const { return m_vo2max; }
    double trainingLoad() const { return m_trainingLoad; }
    QString trainingStatus() const { return m_trainingStatus; }

    double stepsGoalProgress() const { return m_stepsGoalProgress; }
    double caloriesGoalProgress() const { return m_caloriesGoalProgress; }
    double activeMinutesGoalProgress() const { return m_activeMinutesGoalProgress; }

    bool connected() const { return m_connected; }

signals:
    void heartRateChanged();
    void spo2Changed();
    void stressIndexChanged();
    void temperatureChanged();
    void breathingRateChanged();

    void dailyStepsChanged();
    void dailyCaloriesChanged();
    void dailyFloorsClimbedChanged();
    void dailyDistanceChanged();
    void activeMinutesChanged();

    void restingHrChanged();
    void hrvRmssdChanged();
    void readinessScoreChanged();
    void recoveryTimeChanged();

    void sleepChanged();
    void performanceChanged();
    void goalsChanged();

    void connectedChanged();
    void metricsUpdated(const QString &metricName);

private slots:
    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);
    void onPropertiesChanged(const QString &interface,
                             const QVariantMap &changed,
                             const QStringList &invalidated);

private:
    void connectToService();
    void fetchAllMetrics();
    void applyMetrics(const QVariantMap &metrics);

    QDBusServiceWatcher *m_watcher;
    QDBusInterface      *m_iface;
    bool                 m_connected;

    // Cached values
    int    m_heartRate;
    int    m_spo2;
    int    m_stressIndex;
    double m_temperature;
    int    m_breathingRate;

    int    m_dailySteps;
    int    m_dailyCalories;
    int    m_dailyFloorsClimbed;
    double m_dailyDistance;
    int    m_activeMinutes;

    int    m_restingHr;
    double m_hrvRmssd;
    int    m_readinessScore;
    int    m_recoveryTime;

    int    m_sleepQuality;
    int    m_sleepDuration;
    int    m_sleepDeep;
    int    m_sleepRem;
    int    m_sleepLight;

    double m_vo2max;
    double m_trainingLoad;
    QString m_trainingStatus;

    double m_stepsGoalProgress;
    double m_caloriesGoalProgress;
    double m_activeMinutesGoalProgress;
};

#endif // METRICSCLIENT_H
