/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bodymetrics.h"
#include "core/sensorservice.h"
#include "core/datapipeline.h"
#include "core/database.h"

#include <QDebug>
#include <QDate>
#include <QDateTime>
#include <QtMath>
#include <algorithm>

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

BodyMetrics::BodyMetrics(QObject *parent)
    : QObject(parent)
    , m_sensorService(nullptr)
    , m_dataPipeline(nullptr)
    , m_database(nullptr)
    , m_monitoring(false)
    , m_spo2Measuring(false)
    , m_heartRate(0)
    , m_hrvRmssd(0.0)
    , m_spo2(0)
    , m_stressIndex(0)
    , m_temperature(0.0)
    , m_breathingRate(0)
    , m_restingHr(0)
    , m_dailySteps(0)
    , m_dailyCalories(0)
    , m_hrvTimer(new QTimer(this))
    , m_spo2Timer(new QTimer(this))
    , m_breathingTimer(new QTimer(this))
    , m_dailyPersistTimer(new QTimer(this))
    , m_hrvLastBpm(0)
    , m_hrvLastTimestamp(0)
    , m_hrvMeasuring(false)
{
    m_hrvTimer->setSingleShot(true);
    m_hrvTimer->setInterval(HRV_MEASUREMENT_MS);
    connect(m_hrvTimer, &QTimer::timeout, this, &BodyMetrics::onHrvTimer);

    m_spo2Timer->setSingleShot(true);
    m_spo2Timer->setInterval(SPO2_TIMEOUT_MS);
    connect(m_spo2Timer, &QTimer::timeout, this, &BodyMetrics::onSpo2Timeout);

    m_breathingTimer->setInterval(BREATHING_WINDOW_MS);
    m_breathingTimer->setSingleShot(false);
    connect(m_breathingTimer, &QTimer::timeout, this, &BodyMetrics::onBreathingRateTimer);

    m_dailyPersistTimer->setInterval(DAILY_PERSIST_MS);
    m_dailyPersistTimer->setSingleShot(false);
    connect(m_dailyPersistTimer, &QTimer::timeout, this, &BodyMetrics::onDailyPersistTimer);
}

BodyMetrics::~BodyMetrics()
{
    if (m_monitoring) stopMonitoring();
}

void BodyMetrics::setSensorService(SensorService *ss) { m_sensorService = ss; }
void BodyMetrics::setDataPipeline(DataPipeline *dp)   { m_dataPipeline = dp; }
void BodyMetrics::setDatabase(Database *db)            { m_database = db; }

// ═════════════════════════════════════════════════════════════════════════════
// Current metrics map
// ═════════════════════════════════════════════════════════════════════════════

QVariantMap BodyMetrics::current() const
{
    QVariantMap m;
    m[QStringLiteral("heart_rate")]    = m_heartRate;
    m[QStringLiteral("hrv_rmssd")]     = m_hrvRmssd;
    m[QStringLiteral("spo2")]          = m_spo2;
    m[QStringLiteral("stress_index")]  = m_stressIndex;
    m[QStringLiteral("temperature")]   = m_temperature;
    m[QStringLiteral("breathing_rate")]= m_breathingRate;
    m[QStringLiteral("resting_hr")]    = m_restingHr;
    m[QStringLiteral("daily_steps")]   = m_dailySteps;
    m[QStringLiteral("daily_calories")]= m_dailyCalories;
    return m;
}

// ═════════════════════════════════════════════════════════════════════════════
// Monitoring lifecycle
// ═════════════════════════════════════════════════════════════════════════════

void BodyMetrics::startMonitoring()
{
    if (m_monitoring || !m_sensorService) return;

    m_monitoring = true;

    // Subscribe to sensors at moderate background rate
    m_sensorService->subscribe(SensorService::HeartRate, 5000);    // 5s
    m_sensorService->subscribe(SensorService::StepCount, 10000);   // 10s

    // Optional sensors – subscribe if available
    if (m_sensorService->isAvailable(SensorService::Temperature))
        m_sensorService->subscribe(SensorService::Temperature, 30000);

    // Connect signals
    connect(m_sensorService, &SensorService::heartRateChanged,
            this, &BodyMetrics::onHrData);
    connect(m_sensorService, &SensorService::spo2Changed,
            this, &BodyMetrics::onSpo2Data);
    connect(m_sensorService, &SensorService::temperatureChanged,
            this, &BodyMetrics::onTemperatureData);
    connect(m_sensorService, &SensorService::stepCountChanged,
            this, &BodyMetrics::onStepCountData);

    m_breathingTimer->start();
    m_dailyPersistTimer->start();

    emit monitoringChanged();
    qInfo() << "BodyMetrics: monitoring started";
}

void BodyMetrics::stopMonitoring()
{
    if (!m_monitoring) return;
    m_monitoring = false;

    m_breathingTimer->stop();
    m_dailyPersistTimer->stop();

    if (m_sensorService) {
        m_sensorService->unsubscribe(SensorService::HeartRate);
        m_sensorService->unsubscribe(SensorService::StepCount);
        m_sensorService->unsubscribe(SensorService::Temperature);
        disconnect(m_sensorService, nullptr, this, nullptr);
    }

    // Final persist
    onDailyPersistTimer();

    emit monitoringChanged();
    qInfo() << "BodyMetrics: monitoring stopped";
}

// ═════════════════════════════════════════════════════════════════════════════
// SpO2 spot-check
// ═════════════════════════════════════════════════════════════════════════════

void BodyMetrics::spotCheckSpO2()
{
    if (!m_sensorService || m_spo2Measuring) return;

    m_spo2Measuring = true;
    emit spo2MeasuringChanged();

    // Subscribe SpO2 sensor
    m_sensorService->subscribe(SensorService::SpO2, 1000);
    m_spo2Timer->start();

    qInfo() << "BodyMetrics: SpO2 spot-check started";
}

void BodyMetrics::onSpo2Timeout()
{
    if (!m_spo2Measuring) return;

    m_spo2Measuring = false;
    if (m_sensorService)
        m_sensorService->unsubscribe(SensorService::SpO2);

    emit spo2MeasuringChanged();
    qInfo() << "BodyMetrics: SpO2 measurement timeout, last reading:" << m_spo2;
}

// ═════════════════════════════════════════════════════════════════════════════
// HRV spot-check
// ═════════════════════════════════════════════════════════════════════════════

void BodyMetrics::spotCheckHRV()
{
    if (!m_sensorService || m_hrvMeasuring) return;

    m_hrvMeasuring = true;
    m_hrvRRBuffer.clear();
    m_hrvLastBpm = 0;
    m_hrvLastTimestamp = 0;

    // Ensure HR is subscribed at higher rate for RR capture
    m_sensorService->subscribe(SensorService::HeartRate, 1000);
    m_hrvTimer->start();

    qInfo() << "BodyMetrics: HRV spot-check started (60s window)";
}

void BodyMetrics::onHrvTimer()
{
    m_hrvMeasuring = false;

    if (m_hrvRRBuffer.size() < 5) {
        qWarning() << "BodyMetrics: insufficient RR data for HRV:"
                    << m_hrvRRBuffer.size();
        return;
    }

    // Compute RMSSD from collected R-R intervals
    double sumSqDiff = 0.0;
    int validPairs = 0;
    for (int i = 1; i < m_hrvRRBuffer.size(); ++i) {
        float rr1 = m_hrvRRBuffer[i - 1];
        float rr2 = m_hrvRRBuffer[i];
        // Filter implausible
        if (rr1 >= 300 && rr1 <= 2000 && rr2 >= 300 && rr2 <= 2000) {
            double diff = static_cast<double>(rr2 - rr1);
            sumSqDiff += diff * diff;
            validPairs++;
        }
    }

    if (validPairs > 0) {
        m_hrvRmssd = qSqrt(sumSqDiff / validPairs);
        m_stressIndex = computeStressFromHrv(m_hrvRmssd);
        emit metricsChanged();
        emit hrvMeasurementComplete(m_hrvRmssd);
        qInfo() << "BodyMetrics: HRV RMSSD:" << m_hrvRmssd
                << "ms, stress:" << m_stressIndex;
    }

    // Return HR to monitoring rate if still monitoring
    if (m_monitoring && m_sensorService)
        m_sensorService->subscribe(SensorService::HeartRate, 5000);
}

// ═════════════════════════════════════════════════════════════════════════════
// Sensor callbacks
// ═════════════════════════════════════════════════════════════════════════════

void BodyMetrics::onHrData(int bpm)
{
    if (bpm <= 0) return;

    m_heartRate = bpm;
    m_dayHrSamples.append(bpm);
    m_breathingHrWindow.append(bpm);

    // HRV R-R capture during measurement
    if (m_hrvMeasuring) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        float rr = 60000.0f / static_cast<float>(bpm);
        if (rr >= 300.0f && rr <= 2000.0f) {
            m_hrvRRBuffer.append(rr);
        }
        m_hrvLastBpm = bpm;
        m_hrvLastTimestamp = now;
    }

    // Update resting HR (10th percentile of day's readings)
    if (m_dayHrSamples.size() >= 10) {
        QVector<int> sorted = m_dayHrSamples;
        std::sort(sorted.begin(), sorted.end());
        int idx = qMax(0, static_cast<int>(sorted.size() * 0.10));
        m_restingHr = sorted[idx];
    }

    emit metricsChanged();
}

void BodyMetrics::onSpo2Data(int percent)
{
    if (percent <= 0 || percent > 100) return;

    m_spo2 = percent;
    emit spo2Changed(percent);

    if (m_spo2Measuring) {
        // Got a valid reading → done
        m_spo2Measuring = false;
        m_spo2Timer->stop();
        if (m_sensorService)
            m_sensorService->unsubscribe(SensorService::SpO2);
        emit spo2MeasuringChanged();

        // Persist
        if (m_database) {
            m_database->storeDailyMetric(QDate::currentDate(),
                                          QStringLiteral("spo2"), percent);
        }

        qInfo() << "BodyMetrics: SpO2 reading:" << percent << "%";
    }
}

void BodyMetrics::onTemperatureData(float celsius)
{
    m_temperature = static_cast<double>(celsius);
    emit metricsChanged();
}

void BodyMetrics::onStepCountData(int steps)
{
    m_dailySteps = steps;
    emit metricsChanged();
}

// ═════════════════════════════════════════════════════════════════════════════
// Breathing rate estimation
// ═════════════════════════════════════════════════════════════════════════════

void BodyMetrics::onBreathingRateTimer()
{
    if (m_breathingHrWindow.size() < 20) {
        m_breathingHrWindow.clear();
        return;
    }

    double br = computeBreathingRateFromHR(m_breathingHrWindow);
    if (br > 0) {
        m_breathingRate = qRound(br);
        emit metricsChanged();
    }

    m_breathingHrWindow.clear();
}

double BodyMetrics::computeBreathingRateFromHR(const QVector<int> &hrSamples) const
{
    /*
     * Breathing rate estimation from HR using Respiratory Sinus Arrhythmia:
     * During inhalation HR rises, during exhalation it falls.
     * We count the number of "oscillation peaks" in the HR signal
     * and normalize to breaths per minute.
     *
     * This is a simplified approach; real implementations use FFT
     * on the R-R interval series to find the respiratory peak
     * (typically 0.15-0.4 Hz → 9-24 breaths/min).
     *
     * Here we use zero-crossing counting on the detrended HR signal.
     */

    if (hrSamples.size() < 10) return 0.0;

    // Detrend: subtract running mean
    double mean = 0.0;
    for (int v : hrSamples) mean += v;
    mean /= hrSamples.size();

    // Count zero crossings
    int crossings = 0;
    bool positive = (hrSamples.first() - mean) >= 0;
    for (int i = 1; i < hrSamples.size(); ++i) {
        bool nowPositive = (hrSamples[i] - mean) >= 0;
        if (nowPositive != positive) {
            crossings++;
            positive = nowPositive;
        }
    }

    // Each full respiratory cycle has 2 zero crossings
    double cycles = crossings / 2.0;

    // Assume samples span ~ BREATHING_WINDOW_MS
    double windowMinutes = BREATHING_WINDOW_MS / 60000.0;
    double rate = cycles / windowMinutes;

    // Clamp to physiological range
    if (rate < 6.0 || rate > 40.0) return 0.0;
    return rate;
}

// ═════════════════════════════════════════════════════════════════════════════
// Stress index
// ═════════════════════════════════════════════════════════════════════════════

int BodyMetrics::computeStressFromHrv(double rmssd) const
{
    /*
     * Stress index (0-100) inversely proportional to HRV RMSSD.
     *
     * RMSSD interpretation (general population):
     *   >60 ms  → very relaxed (stress ~0-15)
     *   40-60   → relaxed (stress 15-35)
     *   20-40   → moderate stress (35-65)
     *   10-20   → high stress (65-85)
     *   <10     → very high stress (85-100)
     *
     * Formula: stress = 100 × exp(-rmssd / 30)
     * Roughly maps: rmssd=60→13, rmssd=30→37, rmssd=15→61, rmssd=5→85
     */

    if (rmssd <= 0.0) return 50; // unknown → moderate

    double stress = 100.0 * qExp(-rmssd / 30.0);
    return qBound(0, qRound(stress), 100);
}

// ═════════════════════════════════════════════════════════════════════════════
// Daily persistence
// ═════════════════════════════════════════════════════════════════════════════

void BodyMetrics::onDailyPersistTimer()
{
    if (!m_database) return;

    QDate today = QDate::currentDate();

    if (m_restingHr > 0)
        m_database->storeDailyMetric(today, QStringLiteral("resting_hr"), m_restingHr);
    if (m_dailySteps > 0)
        m_database->storeDailyMetric(today, QStringLiteral("daily_steps"), m_dailySteps);
    if (m_hrvRmssd > 0)
        m_database->storeDailyMetric(today, QStringLiteral("body_hrv"), m_hrvRmssd);
    if (m_stressIndex > 0)
        m_database->storeDailyMetric(today, QStringLiteral("stress_index"), m_stressIndex);
    if (m_breathingRate > 0)
        m_database->storeDailyMetric(today, QStringLiteral("breathing_rate"), m_breathingRate);
}

void BodyMetrics::loadDailyMetrics()
{
    if (!m_database) return;

    QDate today = QDate::currentDate();

    double val;
    val = m_database->getDailyMetricValue(today, QStringLiteral("resting_hr"));
    if (val > 0) m_restingHr = static_cast<int>(val);

    val = m_database->getDailyMetricValue(today, QStringLiteral("daily_steps"));
    if (val > 0) m_dailySteps = static_cast<int>(val);

    val = m_database->getDailyMetricValue(today, QStringLiteral("spo2"));
    if (val > 0) m_spo2 = static_cast<int>(val);

    val = m_database->getDailyMetricValue(today, QStringLiteral("stress_index"));
    if (val > 0) m_stressIndex = static_cast<int>(val);

    val = m_database->getDailyMetricValue(today, QStringLiteral("breathing_rate"));
    if (val > 0) m_breathingRate = static_cast<int>(val);

    val = m_database->getDailyMetricValue(today, QStringLiteral("body_hrv"));
    if (val > 0) m_hrvRmssd = val;

    emit metricsChanged();
}
