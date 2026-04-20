/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Body Metrics dashboard module (Phase 2).
 *
 * Real-time and on-demand body metrics:
 *   - Heart rate (continuous or spot-check)
 *   - HRV (RMSSD from 60-second R-R capture)
 *   - SpO2 (on-demand spot-check via PPG/SpO2 sensor)
 *   - Stress index (derived from HRV: low HRV → high stress)
 *   - Body temperature (if sensor available)
 *   - Breathing rate (estimated from accelerometer chest movement
 *     or PPG respiratory modulation)
 *   - Resting heart rate (10th percentile from daily data)
 *   - Daily step count
 *   - Daily calories
 *
 * SpO2 modes:
 *   - On-demand: user taps "Measure SpO2", 30-second measurement
 *   - Profile-driven: periodic spot-checks (configurable frequency)
 */

#ifndef BODYMETRICS_H
#define BODYMETRICS_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>
#include <QTimer>

class SensorService;
class DataPipeline;
class Database;

class BodyMetrics : public QObject
{
    Q_OBJECT

    // ── Current readings ────────────────────────────────────────────────
    Q_PROPERTY(int heartRate READ heartRate NOTIFY metricsChanged)
    Q_PROPERTY(double hrvRmssd READ hrvRmssd NOTIFY metricsChanged)
    Q_PROPERTY(int spo2 READ spo2 NOTIFY spo2Changed)
    Q_PROPERTY(int stressIndex READ stressIndex NOTIFY metricsChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY metricsChanged)
    Q_PROPERTY(int breathingRate READ breathingRate NOTIFY metricsChanged)
    Q_PROPERTY(int restingHr READ restingHr NOTIFY metricsChanged)
    Q_PROPERTY(int dailySteps READ dailySteps NOTIFY metricsChanged)
    Q_PROPERTY(int dailyCalories READ dailyCalories NOTIFY metricsChanged)
    Q_PROPERTY(bool monitoring READ monitoring NOTIFY monitoringChanged)
    Q_PROPERTY(bool spo2Measuring READ spo2Measuring NOTIFY spo2MeasuringChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY metricsChanged)

public:
    explicit BodyMetrics(QObject *parent = nullptr);
    ~BodyMetrics() override;

    void setSensorService(SensorService *ss);
    void setDataPipeline(DataPipeline *dp);
    void setDatabase(Database *db);

    // ── Property getters ────────────────────────────────────────────────

    int heartRate() const { return m_heartRate; }
    double hrvRmssd() const { return m_hrvRmssd; }
    int spo2() const { return m_spo2; }
    int stressIndex() const { return m_stressIndex; }
    double temperature() const { return m_temperature; }
    int breathingRate() const { return m_breathingRate; }
    int restingHr() const { return m_restingHr; }
    int dailySteps() const { return m_dailySteps; }
    int dailyCalories() const { return m_dailyCalories; }
    bool monitoring() const { return m_monitoring; }
    bool spo2Measuring() const { return m_spo2Measuring; }

    QVariantMap current() const;

    // ── Actions ─────────────────────────────────────────────────────────

    Q_INVOKABLE void startMonitoring();
    Q_INVOKABLE void stopMonitoring();
    Q_INVOKABLE void spotCheckSpO2();
    Q_INVOKABLE void spotCheckHRV();

    /** Load today's stored metrics from DB. */
    void loadDailyMetrics();

signals:
    void metricsChanged();
    void monitoringChanged();
    void spo2Changed(int percent);
    void spo2MeasuringChanged();
    void hrvMeasurementComplete(double rmssd);
    void stressUpdated(int stressIndex);

private slots:
    void onHrData(int bpm);
    void onSpo2Data(int percent);
    void onTemperatureData(float celsius);
    void onStepCountData(int steps);
    void onHrvTimer();
    void onSpo2Timeout();
    void onBreathingRateTimer();
    void onDailyPersistTimer();

private:
    // ── Stress computation ──────────────────────────────────────────────
    int computeStressFromHrv(double rmssd) const;
    double computeBreathingRateFromHR(const QVector<int> &hrSamples) const;

    // ── Members ─────────────────────────────────────────────────────────
    SensorService *m_sensorService;
    DataPipeline  *m_dataPipeline;
    Database      *m_database;

    bool m_monitoring;
    bool m_spo2Measuring;

    // Current readings
    int    m_heartRate;
    double m_hrvRmssd;
    int    m_spo2;
    int    m_stressIndex;
    double m_temperature;
    int    m_breathingRate;
    int    m_restingHr;
    int    m_dailySteps;
    int    m_dailyCalories;

    // HRV spot-check
    QTimer *m_hrvTimer;            // 60-second measurement window
    QVector<float> m_hrvRRBuffer;  // R-R intervals during measurement
    int m_hrvLastBpm;
    qint64 m_hrvLastTimestamp;
    bool m_hrvMeasuring;

    // SpO2 measurement
    QTimer *m_spo2Timer;           // 30-second timeout

    // Breathing rate
    QTimer *m_breathingTimer;
    QVector<int> m_breathingHrWindow; // HR samples for respiratory estimation

    // Daily persistence
    QTimer *m_dailyPersistTimer;

    // HR history for resting HR calculation
    QVector<int> m_dayHrSamples;

    static const int HRV_MEASUREMENT_MS = 60000;   // 60 seconds
    static const int SPO2_TIMEOUT_MS = 35000;       // 35 seconds
    static const int BREATHING_WINDOW_MS = 120000;  // 2 minutes
    static const int DAILY_PERSIST_MS = 600000;     // 10 minutes
};

#endif // BODYMETRICS_H
