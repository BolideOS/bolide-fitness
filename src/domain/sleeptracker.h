/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Sleep tracking module (Phase 2).
 *
 * Overnight tracking workflow:
 *   1. User taps "Start Sleep" before bed (or auto-detect via accelerometer calm)
 *   2. Sensor subscriptions: HR at ultra-low power (10-30 s), accelerometer batch
 *   3. Raw data stored in memory ring buffers + periodic DB batch flush
 *   4. On wakeup (user tap or motion spike), run morning analysis:
 *      - Epoch-based staging: Awake / Light / Deep / REM
 *      - Quality score (0-100) based on efficiency, latency, deep%
 *      - HRV overnight baseline (RMSSD) for readiness input
 *
 * Algorithm (accelerometry + HR hybrid):
 *   - 30-second epochs scored by movement count + HR variability
 *   - High movement → Awake
 *   - Low movement + elevated HR → REM
 *   - Low movement + low/stable HR → Deep
 *   - Otherwise → Light
 *
 * Power budget: ~2-5 mA sustained overnight (profile-dependent).
 */

#ifndef SLEEPTRACKER_H
#define SLEEPTRACKER_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>
#include <QTimer>
#include <QDateTime>

class Database;
class SensorService;
class DataPipeline;

class SleepTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool tracking READ tracking NOTIFY trackingChanged)
    Q_PROPERTY(QVariantMap lastNight READ lastNight NOTIFY lastNightChanged)
    Q_PROPERTY(int sleepDebtMinutes READ sleepDebtMinutes NOTIFY sleepDebtChanged)
    Q_PROPERTY(int totalMinutes READ totalMinutes NOTIFY lastNightChanged)
    Q_PROPERTY(int deepMinutes READ deepMinutes NOTIFY lastNightChanged)
    Q_PROPERTY(int lightMinutes READ lightMinutes NOTIFY lastNightChanged)
    Q_PROPERTY(int remMinutes READ remMinutes NOTIFY lastNightChanged)
    Q_PROPERTY(int awakeMinutes READ awakeMinutes NOTIFY lastNightChanged)
    Q_PROPERTY(double qualityScore READ qualityScore NOTIFY lastNightChanged)
    Q_PROPERTY(double overnightHrvRmssd READ overnightHrvRmssd NOTIFY lastNightChanged)
    Q_PROPERTY(int overnightRestingHr READ overnightRestingHr NOTIFY lastNightChanged)
    Q_PROPERTY(QVariantList stageTimeline READ stageTimeline NOTIFY lastNightChanged)

public:
    explicit SleepTracker(QObject *parent = nullptr);
    ~SleepTracker() override;

    void setDatabase(Database *db);
    void setSensorService(SensorService *ss);
    void setDataPipeline(DataPipeline *dp);

    // ── Properties ──────────────────────────────────────────────────────

    bool tracking() const { return m_tracking; }

    QVariantMap lastNight() const { return m_lastNight; }
    int sleepDebtMinutes() const { return m_sleepDebtMinutes; }

    int totalMinutes() const { return m_lastNight.value(QStringLiteral("duration_minutes"), 0).toInt(); }
    int deepMinutes() const { return m_lastNight.value(QStringLiteral("deep_minutes"), 0).toInt(); }
    int lightMinutes() const { return m_lastNight.value(QStringLiteral("light_minutes"), 0).toInt(); }
    int remMinutes() const { return m_lastNight.value(QStringLiteral("rem_minutes"), 0).toInt(); }
    int awakeMinutes() const { return m_lastNight.value(QStringLiteral("awake_minutes"), 0).toInt(); }
    double qualityScore() const { return m_lastNight.value(QStringLiteral("quality_score"), 0.0).toDouble(); }
    double overnightHrvRmssd() const { return m_overnightHrvRmssd; }
    int overnightRestingHr() const { return m_overnightRestingHr; }

    QVariantList stageTimeline() const { return m_stageTimeline; }
    QVariantList weeklyHistory() const;
    QVariantList monthlyTrends() const;

    // ── Actions ─────────────────────────────────────────────────────────

    Q_INVOKABLE void startTracking();
    Q_INVOKABLE void stopTracking();
    Q_INVOKABLE void processLastNight();

    /** Load last night from DB (called on app start). */
    void loadLastNight();

    /** Compute sleep debt from last 7 nights vs target (default 480 min). */
    int computeSleepDebt(int targetMinutes = 480);

signals:
    void trackingChanged();
    void lastNightChanged();
    void sleepDebtChanged();
    void morningAnalysisComplete(const QVariantMap &result);

private slots:
    void onHrData(int bpm);
    void onAccelData(float x, float y, float z);
    void onEpochTimer();
    void onFlushTimer();

private:
    // ── Sleep staging ───────────────────────────────────────────────────

    enum SleepStage {
        Awake = 0,
        Light = 1,
        Deep  = 2,
        REM   = 3
    };

    struct Epoch {
        qint64 timestampMs;
        int movementCount;      // accelerometer threshold crossings
        int avgHr;
        float hrVariability;    // local RMSSD-like measure
        SleepStage stage;
    };

    SleepStage classifyEpoch(int movementCount, int avgHr,
                              float hrVariability, int recentAvgHr) const;

    double computeQualityScore(int totalMin, int deepMin, int remMin,
                                int awakeMin, int latencyMin) const;

    double computeOvernightRmssd(const QVector<float> &rrIntervals) const;

    // ── Member data ─────────────────────────────────────────────────────

    Database *m_database;
    SensorService *m_sensorService;
    DataPipeline *m_dataPipeline;

    bool m_tracking;
    qint64 m_trackingStartMs;

    // Epoch accumulator (30-second window)
    QTimer *m_epochTimer;
    QTimer *m_flushTimer;
    static const int EPOCH_DURATION_MS = 30000;  // 30 seconds
    static const int FLUSH_INTERVAL_MS = 300000; // 5 minutes

    // Within-epoch accumulators
    int m_epochMovementCount;
    QVector<int> m_epochHrSamples;
    QVector<float> m_epochAccelMag;  // magnitude samples within epoch

    // Full-night data
    QVector<Epoch> m_epochs;
    QVector<float> m_overnightRRIntervals;
    QVector<int> m_overnightHrSamples;
    int m_lastHrBpm;
    qint64 m_lastHrTimestampMs;

    // Results
    QVariantMap m_lastNight;
    QVariantList m_stageTimeline;
    double m_overnightHrvRmssd;
    int m_overnightRestingHr;
    int m_sleepDebtMinutes;

    // Movement threshold (in g, for accelerometer magnitude change)
    static constexpr float MOVEMENT_THRESHOLD = 0.15f;
    static constexpr int HR_DEEP_DELTA = -8;   // HR below running avg for deep
    static constexpr int MOVEMENT_AWAKE_THRESH = 15; // movements per epoch
    static constexpr int MOVEMENT_REM_THRESH = 3;    // low but non-zero
};

#endif // SLEEPTRACKER_H
