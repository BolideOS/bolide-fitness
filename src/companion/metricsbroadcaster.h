/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MetricsBroadcaster — D-Bus service that publishes live body/fitness
 * metrics so external consumers (watchfaces, launcher, settings) can
 * subscribe without linking to bolide-fitness internals.
 *
 * Service:   org.bolide.fitness.Metrics
 * Path:      /org/bolide/fitness/Metrics
 * Interface: org.bolide.fitness.Metrics
 *
 * Design goals:
 * - One authoritative source of truth (bolide-fitness owns data)
 * - Low overhead: only sends signals when values actually change
 * - Companion-sync-ready: metrics can also be updated via PushMetrics
 *   on CompanionService for phone → watch sync
 * - Modular: consumer plugin (BolideMetrics QML) is a separate library
 *   so watchfaces don't need bolide-fitness to be running at compile time
 */

#ifndef METRICSBROADCASTER_H
#define METRICSBROADCASTER_H

#include <QObject>
#include <QDBusConnection>
#include <QVariantMap>
#include <QTimer>

class BodyMetrics;
class SleepTracker;
class ReadinessScore;
class TrendsManager;
class Database;

/**
 * @brief Publishes fitness metrics on D-Bus for system-wide consumption.
 *
 * Properties are exposed as D-Bus properties with PropertiesChanged signals.
 * Consumers use the standard org.freedesktop.DBus.Properties interface.
 *
 * Metric categories:
 *  - Body (HR, SpO2, stress, temperature, breathing rate)
 *  - Activity (steps, calories, floors climbed, distance)
 *  - Recovery (resting HR, HRV, readiness score, recovery time)
 *  - Sleep (last night quality, duration, deep/REM/light minutes)
 *  - Performance (VO2max, training load, training status)
 */
class MetricsBroadcaster : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bolide.fitness.Metrics")

    // ── Body metrics (real-time) ────────────────────────────────────────
    Q_PROPERTY(int heartRate READ heartRate NOTIFY heartRateChanged)
    Q_PROPERTY(int spo2 READ spo2 NOTIFY spo2Changed)
    Q_PROPERTY(int stressIndex READ stressIndex NOTIFY stressIndexChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(int breathingRate READ breathingRate NOTIFY breathingRateChanged)

    // ── Activity metrics (daily) ────────────────────────────────────────
    Q_PROPERTY(int dailySteps READ dailySteps NOTIFY dailyStepsChanged)
    Q_PROPERTY(int dailyCalories READ dailyCalories NOTIFY dailyCaloriesChanged)
    Q_PROPERTY(int dailyFloorsClimbed READ dailyFloorsClimbed NOTIFY dailyFloorsClimbedChanged)
    Q_PROPERTY(double dailyDistance READ dailyDistance NOTIFY dailyDistanceChanged)
    Q_PROPERTY(int activeMinutes READ activeMinutes NOTIFY activeMinutesChanged)

    // ── Recovery metrics ────────────────────────────────────────────────
    Q_PROPERTY(int restingHr READ restingHr NOTIFY restingHrChanged)
    Q_PROPERTY(double hrvRmssd READ hrvRmssd NOTIFY hrvRmssdChanged)
    Q_PROPERTY(int readinessScore READ readinessScore NOTIFY readinessScoreChanged)
    Q_PROPERTY(int recoveryTime READ recoveryTime NOTIFY recoveryTimeChanged)

    // ── Sleep (last night) ──────────────────────────────────────────────
    Q_PROPERTY(int sleepQuality READ sleepQuality NOTIFY sleepChanged)
    Q_PROPERTY(int sleepDuration READ sleepDuration NOTIFY sleepChanged)
    Q_PROPERTY(int sleepDeep READ sleepDeep NOTIFY sleepChanged)
    Q_PROPERTY(int sleepRem READ sleepRem NOTIFY sleepChanged)
    Q_PROPERTY(int sleepLight READ sleepLight NOTIFY sleepChanged)

    // ── Performance metrics ─────────────────────────────────────────────
    Q_PROPERTY(double vo2max READ vo2max NOTIFY performanceChanged)
    Q_PROPERTY(double trainingLoad READ trainingLoad NOTIFY performanceChanged)
    Q_PROPERTY(QString trainingStatus READ trainingStatus NOTIFY performanceChanged)

    // ── Goal progress (0.0 - 1.0+) ─────────────────────────────────────
    Q_PROPERTY(double stepsGoalProgress READ stepsGoalProgress NOTIFY goalsChanged)
    Q_PROPERTY(double caloriesGoalProgress READ caloriesGoalProgress NOTIFY goalsChanged)
    Q_PROPERTY(double activeMinutesGoalProgress READ activeMinutesGoalProgress NOTIFY goalsChanged)

    // ── Timestamp of last update ────────────────────────────────────────
    Q_PROPERTY(qlonglong lastUpdateTimestamp READ lastUpdateTimestamp NOTIFY anyMetricChanged)

public:
    explicit MetricsBroadcaster(QObject *parent = nullptr);
    ~MetricsBroadcaster() override;

    void setBodyMetrics(BodyMetrics *bm);
    void setSleepTracker(SleepTracker *st);
    void setReadinessScore(ReadinessScore *rs);
    void setTrendsManager(TrendsManager *tm);
    void setDatabase(Database *db);

    /** Register on the session D-Bus. Returns true on success. */
    bool registerService();

    // ── Property getters ────────────────────────────────────────────────

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

    qlonglong lastUpdateTimestamp() const { return m_lastUpdate; }

    // ── D-Bus methods ───────────────────────────────────────────────────

    /** Get all current metrics as a map — useful for initial fetch. */
    Q_INVOKABLE QVariantMap GetAllMetrics() const;

    /** Get a single metric by key name. */
    Q_INVOKABLE QVariant GetMetric(const QString &key) const;

    /**
     * Push metrics from an external source (companion app sync).
     * Keys match property names. Only changed values are applied.
     * Returns number of metrics actually updated.
     */
    Q_INVOKABLE int PushMetrics(const QVariantMap &metrics);

signals:
    // Per-category change signals
    void heartRateChanged(int bpm);
    void spo2Changed(int percent);
    void stressIndexChanged(int index);
    void temperatureChanged(double celsius);
    void breathingRateChanged(int brpm);

    void dailyStepsChanged(int steps);
    void dailyCaloriesChanged(int calories);
    void dailyFloorsClimbedChanged(int floors);
    void dailyDistanceChanged(double meters);
    void activeMinutesChanged(int minutes);

    void restingHrChanged(int bpm);
    void hrvRmssdChanged(double ms);
    void readinessScoreChanged(int score);
    void recoveryTimeChanged(int hours);

    void sleepChanged();
    void performanceChanged();
    void goalsChanged();

    /** Emitted whenever any metric changes. Carries metric name. */
    void anyMetricChanged(const QString &metricName);

private slots:
    void onBodyMetricsChanged();
    void onSpo2Changed(int percent);
    void onReadinessRecomputed();
    void onSleepAnalysisComplete();
    void onPerformanceUpdated();

private:
    void emitDBusPropertiesChanged(const QStringList &properties);
    void loadGoals();

    // Sources
    BodyMetrics    *m_bodyMetrics;
    SleepTracker   *m_sleepTracker;
    ReadinessScore *m_readiness;
    TrendsManager  *m_trendsManager;
    Database       *m_database;

    bool m_registered;

    // ── Cached values ───────────────────────────────────────────────────
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

    // Goals
    int    m_stepsGoal;
    int    m_caloriesGoal;
    int    m_activeMinutesGoal;
    double m_stepsGoalProgress;
    double m_caloriesGoalProgress;
    double m_activeMinutesGoalProgress;

    qlonglong m_lastUpdate;
};

#endif // METRICSBROADCASTER_H
