/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sleeptracker.h"
#include "core/database.h"
#include "core/sensorservice.h"
#include "core/datapipeline.h"

#include <QDebug>
#include <QtMath>
#include <algorithm>

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

SleepTracker::SleepTracker(QObject *parent)
    : QObject(parent)
    , m_database(nullptr)
    , m_sensorService(nullptr)
    , m_dataPipeline(nullptr)
    , m_tracking(false)
    , m_trackingStartMs(0)
    , m_epochTimer(new QTimer(this))
    , m_flushTimer(new QTimer(this))
    , m_epochMovementCount(0)
    , m_lastHrBpm(0)
    , m_lastHrTimestampMs(0)
    , m_overnightHrvRmssd(0.0)
    , m_overnightRestingHr(0)
    , m_sleepDebtMinutes(0)
{
    m_epochTimer->setInterval(EPOCH_DURATION_MS);
    m_epochTimer->setSingleShot(false);
    connect(m_epochTimer, &QTimer::timeout, this, &SleepTracker::onEpochTimer);

    m_flushTimer->setInterval(FLUSH_INTERVAL_MS);
    m_flushTimer->setSingleShot(false);
    connect(m_flushTimer, &QTimer::timeout, this, &SleepTracker::onFlushTimer);
}

SleepTracker::~SleepTracker()
{
    if (m_tracking)
        stopTracking();
}

void SleepTracker::setDatabase(Database *db)
{
    m_database = db;
}

void SleepTracker::setSensorService(SensorService *ss)
{
    m_sensorService = ss;
}

void SleepTracker::setDataPipeline(DataPipeline *dp)
{
    m_dataPipeline = dp;
}

// ═════════════════════════════════════════════════════════════════════════════
// Tracking lifecycle
// ═════════════════════════════════════════════════════════════════════════════

void SleepTracker::startTracking()
{
    if (m_tracking) return;
    if (!m_sensorService) {
        qWarning() << "SleepTracker: cannot start without SensorService";
        return;
    }

    m_tracking = true;
    m_trackingStartMs = QDateTime::currentMSecsSinceEpoch();

    // Reset accumulators
    m_epochs.clear();
    m_overnightRRIntervals.clear();
    m_overnightHrSamples.clear();
    m_epochMovementCount = 0;
    m_epochHrSamples.clear();
    m_epochAccelMag.clear();
    m_stageTimeline.clear();
    m_lastHrBpm = 0;
    m_lastHrTimestampMs = 0;

    // Subscribe sensors at low-power intervals for overnight
    // HR: every 10 seconds (6 bpm in a minute gives useful data + low power)
    m_sensorService->subscribe(SensorService::HeartRate, 10000);
    // Accelerometer: every 1 second (movement detection, very low cost)
    m_sensorService->subscribe(SensorService::Accelerometer, 1000);

    // Connect sensor signals
    connect(m_sensorService, &SensorService::heartRateChanged,
            this, &SleepTracker::onHrData);
    // For accelerometer we need the raw sample signal
    connect(m_sensorService, &SensorService::sensorData,
            this, [this](const SensorService::SensorSample &s) {
                if (s.type == SensorService::Accelerometer)
                    onAccelData(s.values[0], s.values[1], s.values[2]);
            });

    m_epochTimer->start();
    m_flushTimer->start();

    emit trackingChanged();
    qInfo() << "SleepTracker: tracking started";
}

void SleepTracker::stopTracking()
{
    if (!m_tracking) return;

    m_tracking = false;
    m_epochTimer->stop();
    m_flushTimer->stop();

    // Flush the last partial epoch
    onEpochTimer();

    // Unsubscribe sensors
    if (m_sensorService) {
        m_sensorService->unsubscribe(SensorService::HeartRate);
        m_sensorService->unsubscribe(SensorService::Accelerometer);
        disconnect(m_sensorService, nullptr, this, nullptr);
    }

    // Run the morning analysis
    processLastNight();

    emit trackingChanged();
    qInfo() << "SleepTracker: tracking stopped, epochs:" << m_epochs.size();
}

// ═════════════════════════════════════════════════════════════════════════════
// Sensor data callbacks
// ═════════════════════════════════════════════════════════════════════════════

void SleepTracker::onHrData(int bpm)
{
    if (!m_tracking || bpm <= 0) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Accumulate for epoch
    m_epochHrSamples.append(bpm);
    m_overnightHrSamples.append(bpm);

    // Estimate R-R interval from successive HR readings
    if (m_lastHrBpm > 0 && m_lastHrTimestampMs > 0) {
        // R-R from BPM: rr_ms = 60000 / bpm
        float rr = 60000.0f / static_cast<float>(bpm);
        if (rr > 300.0f && rr < 2000.0f) {
            m_overnightRRIntervals.append(rr);
        }
    }

    m_lastHrBpm = bpm;
    m_lastHrTimestampMs = now;
}

void SleepTracker::onAccelData(float x, float y, float z)
{
    if (!m_tracking) return;

    float mag = qSqrt(x * x + y * y + z * z);
    m_epochAccelMag.append(mag);

    // Count movement: significant deviation from 1g (resting)
    if (qAbs(mag - 1.0f) > MOVEMENT_THRESHOLD)
        m_epochMovementCount++;
}

// ═════════════════════════════════════════════════════════════════════════════
// Epoch processing (every 30 seconds)
// ═════════════════════════════════════════════════════════════════════════════

void SleepTracker::onEpochTimer()
{
    if (!m_tracking && m_epochHrSamples.isEmpty()) return;

    Epoch epoch;
    epoch.timestampMs = QDateTime::currentMSecsSinceEpoch();
    epoch.movementCount = m_epochMovementCount;

    // Average HR for this epoch
    int hrSum = 0;
    for (int hr : m_epochHrSamples) hrSum += hr;
    epoch.avgHr = m_epochHrSamples.isEmpty() ? 0 :
                  hrSum / m_epochHrSamples.size();

    // Local HR variability (crude RMSSD from BPM within epoch)
    epoch.hrVariability = 0.0f;
    if (m_epochHrSamples.size() >= 2) {
        float sumSqDiff = 0.0f;
        for (int i = 1; i < m_epochHrSamples.size(); ++i) {
            float diff = static_cast<float>(m_epochHrSamples[i] - m_epochHrSamples[i - 1]);
            sumSqDiff += diff * diff;
        }
        epoch.hrVariability = qSqrt(sumSqDiff / (m_epochHrSamples.size() - 1));
    }

    // Running average HR from all epochs
    int recentAvgHr = epoch.avgHr;
    if (m_epochs.size() >= 10) {
        int sum = 0;
        for (int i = m_epochs.size() - 10; i < m_epochs.size(); ++i)
            sum += m_epochs[i].avgHr;
        recentAvgHr = sum / 10;
    }

    // Classify
    epoch.stage = classifyEpoch(epoch.movementCount, epoch.avgHr,
                                 epoch.hrVariability, recentAvgHr);
    m_epochs.append(epoch);

    // Reset epoch accumulators
    m_epochMovementCount = 0;
    m_epochHrSamples.clear();
    m_epochAccelMag.clear();
}

void SleepTracker::onFlushTimer()
{
    // Periodically store overnight sensor batches to DB
    // This ensures data survives a crash
    if (!m_database || m_overnightHrSamples.isEmpty()) return;

    QVector<QPair<qint64, float>> batch;
    batch.reserve(m_overnightHrSamples.size());
    qint64 baseTime = m_trackingStartMs / 1000; // epoch seconds
    for (int i = 0; i < m_overnightHrSamples.size(); ++i) {
        batch.append(qMakePair(baseTime + i * 10, // approximate 10s intervals
                                static_cast<float>(m_overnightHrSamples[i])));
    }

    // Store under a pseudo workout ID = -1 (sleep session)
    m_database->storeSensorBatch(-1, QStringLiteral("sleep_hr"), batch);
}

// ═════════════════════════════════════════════════════════════════════════════
// Sleep stage classification
// ═════════════════════════════════════════════════════════════════════════════

SleepTracker::SleepStage SleepTracker::classifyEpoch(
    int movementCount, int avgHr, float hrVariability, int recentAvgHr) const
{
    /*
     * Hybrid accelerometry + HR staging algorithm:
     *
     * 1. High movement (>15/epoch) → Awake
     * 2. Very low movement + HR well below running avg → Deep
     * 3. Low movement (1-3) + HR at/above running avg + high HRV → REM
     *    (REM has characteristic HR variability with micro-arousals)
     * 4. Otherwise → Light
     */

    if (movementCount >= MOVEMENT_AWAKE_THRESH)
        return Awake;

    if (avgHr > 0 && recentAvgHr > 0) {
        int hrDelta = avgHr - recentAvgHr;

        // Deep sleep: very still + HR drops below running average
        if (movementCount <= 1 && hrDelta <= HR_DEEP_DELTA)
            return Deep;

        // REM: relatively still but HR is variable and at/above avg
        if (movementCount <= MOVEMENT_REM_THRESH &&
            hrVariability > 3.0f && hrDelta >= -2)
            return REM;
    } else if (movementCount <= 1) {
        // No HR data but very still → assume deep
        return Deep;
    }

    return Light;
}

// ═════════════════════════════════════════════════════════════════════════════
// Morning analysis
// ═════════════════════════════════════════════════════════════════════════════

void SleepTracker::processLastNight()
{
    if (m_epochs.isEmpty()) {
        qWarning() << "SleepTracker: no epoch data to process";
        return;
    }

    qint64 endMs = QDateTime::currentMSecsSinceEpoch();
    int totalEpochs = m_epochs.size();
    int totalMinutes = totalEpochs * (EPOCH_DURATION_MS / 1000) / 60;

    // Count stages
    int deepEpochs = 0, lightEpochs = 0, remEpochs = 0, awakeEpochs = 0;
    for (const Epoch &e : m_epochs) {
        switch (e.stage) {
        case Deep:  deepEpochs++;  break;
        case Light: lightEpochs++; break;
        case REM:   remEpochs++;   break;
        case Awake: awakeEpochs++; break;
        }
    }

    int epochMinutes = EPOCH_DURATION_MS / 60000;
    int deepMin  = deepEpochs  * epochMinutes;
    int lightMin = lightEpochs * epochMinutes;
    int remMin   = remEpochs   * epochMinutes;
    int awakeMin = awakeEpochs * epochMinutes;

    // Sleep onset latency: number of initial awake epochs
    int latencyEpochs = 0;
    for (const Epoch &e : m_epochs) {
        if (e.stage == Awake) latencyEpochs++;
        else break;
    }
    int latencyMin = latencyEpochs * epochMinutes;

    // Quality score
    double quality = computeQualityScore(totalMinutes, deepMin, remMin,
                                          awakeMin, latencyMin);

    // Overnight HRV RMSSD
    m_overnightHrvRmssd = computeOvernightRmssd(m_overnightRRIntervals);

    // Overnight resting HR (10th percentile of HR readings)
    if (!m_overnightHrSamples.isEmpty()) {
        QVector<int> sorted = m_overnightHrSamples;
        std::sort(sorted.begin(), sorted.end());
        int idx = qMax(0, static_cast<int>(sorted.size() * 0.10));
        m_overnightRestingHr = sorted[idx];
    }

    // Build stage timeline for UI
    m_stageTimeline.clear();
    for (const Epoch &e : m_epochs) {
        QVariantMap entry;
        entry[QStringLiteral("timestamp")] = e.timestampMs;
        entry[QStringLiteral("stage")] = static_cast<int>(e.stage);
        entry[QStringLiteral("hr")] = e.avgHr;
        entry[QStringLiteral("movement")] = e.movementCount;
        m_stageTimeline.append(entry);
    }

    // Build result map
    m_lastNight.clear();
    m_lastNight[QStringLiteral("start_time")] = m_trackingStartMs / 1000;
    m_lastNight[QStringLiteral("end_time")] = endMs / 1000;
    m_lastNight[QStringLiteral("duration_minutes")] = totalMinutes;
    m_lastNight[QStringLiteral("deep_minutes")] = deepMin;
    m_lastNight[QStringLiteral("light_minutes")] = lightMin;
    m_lastNight[QStringLiteral("rem_minutes")] = remMin;
    m_lastNight[QStringLiteral("awake_minutes")] = awakeMin;
    m_lastNight[QStringLiteral("quality_score")] = quality;
    m_lastNight[QStringLiteral("latency_minutes")] = latencyMin;
    m_lastNight[QStringLiteral("overnight_hrv_rmssd")] = m_overnightHrvRmssd;
    m_lastNight[QStringLiteral("overnight_resting_hr")] = m_overnightRestingHr;
    m_lastNight[QStringLiteral("total_epochs")] = totalEpochs;

    // Persist to database
    if (m_database) {
        qint64 sessionId = m_database->createSleepSession(
            m_trackingStartMs / 1000, endMs / 1000);
        if (sessionId >= 0) {
            m_database->updateSleepSession(sessionId, m_lastNight);
        }

        // Store daily metrics for readiness and trends
        QDate today = QDate::currentDate();
        m_database->storeDailyMetric(today, QStringLiteral("sleep_duration"), totalMinutes);
        m_database->storeDailyMetric(today, QStringLiteral("sleep_quality"), quality);
        m_database->storeDailyMetric(today, QStringLiteral("sleep_deep"), deepMin);
        m_database->storeDailyMetric(today, QStringLiteral("sleep_rem"), remMin);
        m_database->storeDailyMetric(today, QStringLiteral("overnight_hrv"), m_overnightHrvRmssd);
        m_database->storeDailyMetric(today, QStringLiteral("resting_hr"), m_overnightRestingHr);
    }

    // Recompute sleep debt
    m_sleepDebtMinutes = computeSleepDebt();

    emit lastNightChanged();
    emit sleepDebtChanged();
    emit morningAnalysisComplete(m_lastNight);

    qInfo() << "SleepTracker: analysis complete – quality:" << quality
            << "total:" << totalMinutes << "min"
            << "deep:" << deepMin << "rem:" << remMin
            << "resting HR:" << m_overnightRestingHr
            << "HRV RMSSD:" << m_overnightHrvRmssd;
}

// ═════════════════════════════════════════════════════════════════════════════
// Quality score computation
// ═════════════════════════════════════════════════════════════════════════════

double SleepTracker::computeQualityScore(int totalMin, int deepMin, int remMin,
                                          int awakeMin, int latencyMin) const
{
    /*
     * Quality score (0-100) considers:
     *   - Sleep efficiency: (total - awake) / total
     *   - Deep sleep ratio: 15-25% is optimal
     *   - REM ratio: 20-25% is optimal
     *   - Sleep onset latency: <15 min is good
     *   - Total duration: 7-9 hours is optimal
     *
     * Weighted composite:
     *   40% efficiency + 20% deep% + 20% REM% + 10% latency + 10% duration
     */

    if (totalMin <= 0) return 0.0;

    double score = 0.0;

    // 1. Efficiency (40%)
    double efficiency = static_cast<double>(totalMin - awakeMin) / totalMin;
    double effScore = qBound(0.0, efficiency * 100.0, 100.0);
    // Penalize below 85%
    if (efficiency < 0.85)
        effScore *= (efficiency / 0.85);
    score += effScore * 0.40;

    // 2. Deep sleep ratio (20%) – optimal ~20% of total
    double deepRatio = static_cast<double>(deepMin) / totalMin;
    double deepScore;
    if (deepRatio >= 0.15 && deepRatio <= 0.25)
        deepScore = 100.0;
    else if (deepRatio < 0.15)
        deepScore = (deepRatio / 0.15) * 100.0;
    else
        deepScore = qMax(0.0, 100.0 - (deepRatio - 0.25) * 400.0);
    score += deepScore * 0.20;

    // 3. REM ratio (20%) – optimal ~22% of total
    double remRatio = static_cast<double>(remMin) / totalMin;
    double remScore;
    if (remRatio >= 0.18 && remRatio <= 0.28)
        remScore = 100.0;
    else if (remRatio < 0.18)
        remScore = (remRatio / 0.18) * 100.0;
    else
        remScore = qMax(0.0, 100.0 - (remRatio - 0.28) * 400.0);
    score += remScore * 0.20;

    // 4. Latency (10%) – <15 min = perfect, >45 min = 0
    double latencyScore;
    if (latencyMin <= 15)
        latencyScore = 100.0;
    else if (latencyMin >= 45)
        latencyScore = 0.0;
    else
        latencyScore = 100.0 * (1.0 - (latencyMin - 15.0) / 30.0);
    score += latencyScore * 0.10;

    // 5. Duration (10%) – 7-9 hours optimal
    double durationScore;
    if (totalMin >= 420 && totalMin <= 540) // 7-9 hours
        durationScore = 100.0;
    else if (totalMin < 420)
        durationScore = (static_cast<double>(totalMin) / 420.0) * 100.0;
    else // >9 hours
        durationScore = qMax(0.0, 100.0 - (totalMin - 540.0) / 60.0 * 20.0);
    score += durationScore * 0.10;

    return qBound(0.0, score, 100.0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Overnight HRV
// ═════════════════════════════════════════════════════════════════════════════

double SleepTracker::computeOvernightRmssd(const QVector<float> &rrIntervals) const
{
    if (rrIntervals.size() < 2) return 0.0;

    // Filter implausible values
    QVector<float> valid;
    valid.reserve(rrIntervals.size());
    for (float rr : rrIntervals) {
        if (rr >= 300.0f && rr <= 2000.0f)
            valid.append(rr);
    }
    if (valid.size() < 2) return 0.0;

    // RMSSD
    double sumSqDiff = 0.0;
    for (int i = 1; i < valid.size(); ++i) {
        double diff = static_cast<double>(valid[i]) - static_cast<double>(valid[i - 1]);
        sumSqDiff += diff * diff;
    }

    return qSqrt(sumSqDiff / (valid.size() - 1));
}

// ═════════════════════════════════════════════════════════════════════════════
// Sleep debt
// ═════════════════════════════════════════════════════════════════════════════

int SleepTracker::computeSleepDebt(int targetMinutes)
{
    if (!m_database) return 0;

    QDate today = QDate::currentDate();
    QVariantList history = m_database->getDailyMetrics(
        QStringLiteral("sleep_duration"),
        today.addDays(-7), today);

    int totalActual = 0;
    int nightCount = 0;
    for (const QVariant &v : history) {
        QVariantMap row = v.toMap();
        totalActual += row.value(QStringLiteral("value"), 0).toInt();
        nightCount++;
    }

    if (nightCount == 0) return 0;

    int totalTarget = nightCount * targetMinutes;
    m_sleepDebtMinutes = qMax(0, totalTarget - totalActual);
    return m_sleepDebtMinutes;
}

// ═════════════════════════════════════════════════════════════════════════════
// History / load
// ═════════════════════════════════════════════════════════════════════════════

void SleepTracker::loadLastNight()
{
    if (!m_database) return;

    m_lastNight = m_database->getLatestSleep();
    if (!m_lastNight.isEmpty()) {
        m_overnightHrvRmssd = m_lastNight.value(
            QStringLiteral("overnight_hrv_rmssd"), 0.0).toDouble();
        m_overnightRestingHr = m_lastNight.value(
            QStringLiteral("overnight_resting_hr"), 0).toInt();
        emit lastNightChanged();
    }

    m_sleepDebtMinutes = computeSleepDebt();
    emit sleepDebtChanged();
}

QVariantList SleepTracker::weeklyHistory() const
{
    if (!m_database) return QVariantList();

    QDate today = QDate::currentDate();
    return m_database->getDailyMetrics(
        QStringLiteral("sleep_duration"),
        today.addDays(-7), today);
}

QVariantList SleepTracker::monthlyTrends() const
{
    if (!m_database) return QVariantList();

    QDate today = QDate::currentDate();
    return m_database->getDailyMetrics(
        QStringLiteral("sleep_duration"),
        today.addDays(-30), today);
}
