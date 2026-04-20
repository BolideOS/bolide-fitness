/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DATAPIPELINE_H
#define DATAPIPELINE_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include "ringbuffer.h"
#include "sensorservice.h"

class Database;

/**
 * @brief Sensor data buffering, sampling, and aggregation pipeline.
 *
 * Sits between SensorService (raw data producer) and consumers (UI, metrics,
 * storage). Maintains per-sensor ring buffers, computes rolling statistics,
 * and periodically flushes aggregated data to the database.
 *
 * Performance design:
 * - Pre-allocated ring buffers (no malloc in hot path)
 * - Batched database writes (configurable flush interval)
 * - Separate buffers for workout data vs background monitoring
 */
class DataPipeline : public QObject
{
    Q_OBJECT

public:
    explicit DataPipeline(SensorService *sensorService, QObject *parent = nullptr);
    ~DataPipeline() override;

    // ── Configuration ───────────────────────────────────────────────────

    /** Set ring buffer capacity per sensor type (call before startRecording). */
    void setBufferCapacity(SensorService::SensorType type, int samples);

    /** Set database flush interval in seconds (default: 30). */
    void setFlushInterval(int seconds);

    // ── Recording lifecycle ─────────────────────────────────────────────

    /** Start recording sensor data for a workout. */
    void startRecording(qint64 workoutId);

    /** Stop recording, flush remaining data. */
    void stopRecording();

    bool isRecording() const { return m_recording; }
    qint64 currentWorkoutId() const { return m_workoutId; }

    // ── Data access (for UI / metrics) ──────────────────────────────────

    /** Get read-only reference to a sensor's ring buffer. */
    const RingBuffer<float> &hrBuffer()        const { return m_hrBuffer; }
    const RingBuffer<float> &elevationBuffer() const { return m_elevationBuffer; }
    const RingBuffer<float> &paceBuffer()      const { return m_paceBuffer; }
    const RingBuffer<float> &cadenceBuffer()   const { return m_cadenceBuffer; }

    /** Rolling statistics over last N samples. */
    float hrMean(int n = -1)   const { return m_hrBuffer.mean(n); }
    float hrMax(int n = -1)    const { return m_hrBuffer.max(n); }
    float hrMin(int n = -1)    const { return m_hrBuffer.min(n); }

    /** Total accumulated distance (meters). */
    double totalDistance() const { return m_totalDistance; }

    /** Total elevation gain (meters). */
    double totalElevationGain() const { return m_totalElevationGain; }

    /** Total step count during this recording. */
    int totalSteps() const { return m_totalSteps; }

    /** Current pace (seconds per km). Returns 0 if no data. */
    float currentPace() const;

    /** Current speed (m/s). */
    float currentSpeed() const { return m_currentSpeed; }

    /** Smoothed barometric altitude (meters). */
    float currentAltitude() const { return m_currentAltitude; }

    // ── GPS track access ────────────────────────────────────────────────

    struct GpsTrackPoint {
        qint64 timestampUs;
        double latitude;
        double longitude;
        double altitude;
        float  speed;
    };

    const QVector<GpsTrackPoint> &gpsTrack() const { return m_gpsTrack; }

    // ── Elevation series (for Hill Score) ───────────────────────────────

    /** Get smoothed elevation series (meters). */
    QVector<float> elevationSeries() const;

    /** True if elevation data comes from barometer (vs GPS only). */
    bool hasBarometerElevation() const { return m_hasBaroElev; }

    // ── Database binding ────────────────────────────────────────────────

    void setDatabase(Database *db);

signals:
    void hrUpdated(int bpm);
    void distanceUpdated(double meters);
    void elevationUpdated(double gainMeters, float currentAltitude);
    void paceUpdated(float secPerKm);
    void stepsUpdated(int totalSteps);

private slots:
    void onSensorData(const SensorService::SensorSample &sample);
    void onGpsPosition(double lat, double lon, double alt, float speed, float bearing);
    void onFlushTimer();

private:
    void processHeartRate(const SensorService::SensorSample &sample);
    void processBarometer(const SensorService::SensorSample &sample);
    void processAccelerometer(const SensorService::SensorSample &sample);
    void processStepCount(const SensorService::SensorSample &sample);
    void processGps(double lat, double lon, double alt, float speed, float bearing);

    double haversineDistance(double lat1, double lon1, double lat2, double lon2) const;
    float baroAltitude(float pressureHPa) const;

    SensorService *m_sensorService;
    Database *m_database;
    QTimer *m_flushTimer;

    bool m_recording;
    qint64 m_workoutId;

    // Per-sensor ring buffers (pre-allocated)
    RingBuffer<float> m_hrBuffer;           // BPM values
    RingBuffer<float> m_elevationBuffer;    // altitude in meters
    RingBuffer<float> m_paceBuffer;         // sec/km
    RingBuffer<float> m_cadenceBuffer;      // steps/min

    // GPS track
    QVector<GpsTrackPoint> m_gpsTrack;
    double m_lastLat;
    double m_lastLon;
    double m_lastAlt;

    // Accumulators
    double m_totalDistance;          // meters
    double m_totalElevationGain;    // meters
    int    m_totalSteps;
    int    m_startSteps;            // step count at start of recording
    float  m_currentSpeed;          // m/s
    float  m_currentAltitude;       // meters (baro-corrected or GPS)
    float  m_lastBaroAltitude;      // for elevation gain tracking
    bool   m_hasBaroElev;

    // Elevation smoothing window
    RingBuffer<float> m_rawElevWindow;  // raw baro readings for smoothing

    // Batched DB writes
    struct PendingSample {
        QString sensor;
        qint64 timestamp;
        float value;
    };
    QVector<PendingSample> m_pendingSamples;
    QVector<GpsTrackPoint> m_pendingGps;
    static const int MAX_PENDING = 500;
};

#endif // DATAPIPELINE_H
