/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "datapipeline.h"
#include "database.h"
#include <QDebug>
#include <QDateTime>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Sea-level standard pressure (hPa) for barometric altitude
static const float SEA_LEVEL_PRESSURE = 1013.25f;

DataPipeline::DataPipeline(SensorService *sensorService, QObject *parent)
    : QObject(parent)
    , m_sensorService(sensorService)
    , m_database(nullptr)
    , m_flushTimer(new QTimer(this))
    , m_recording(false)
    , m_workoutId(-1)
    , m_hrBuffer(360)           // 6 minutes at 1Hz
    , m_elevationBuffer(600)    // 10 minutes at 1Hz
    , m_paceBuffer(120)         // 2 minutes at 1Hz
    , m_cadenceBuffer(120)
    , m_lastLat(0.0)
    , m_lastLon(0.0)
    , m_lastAlt(0.0)
    , m_totalDistance(0.0)
    , m_totalElevationGain(0.0)
    , m_totalSteps(0)
    , m_startSteps(0)
    , m_currentSpeed(0.0f)
    , m_currentAltitude(0.0f)
    , m_lastBaroAltitude(0.0f)
    , m_hasBaroElev(false)
    , m_rawElevWindow(10)       // 10-sample smoothing window
{
    connect(m_sensorService, &SensorService::sensorData,
            this, &DataPipeline::onSensorData);
    connect(m_sensorService, &SensorService::gpsPositionChanged,
            this, &DataPipeline::onGpsPosition);

    m_flushTimer->setInterval(30000); // 30 seconds default
    connect(m_flushTimer, &QTimer::timeout, this, &DataPipeline::onFlushTimer);

    m_pendingSamples.reserve(MAX_PENDING);
    m_pendingGps.reserve(200);
}

DataPipeline::~DataPipeline()
{
    if (m_recording)
        stopRecording();
}

void DataPipeline::setBufferCapacity(SensorService::SensorType type, int samples)
{
    switch (type) {
    case SensorService::HeartRate:      m_hrBuffer.reserve(samples); break;
    case SensorService::Barometer:      m_elevationBuffer.reserve(samples); break;
    default: break;
    }
}

void DataPipeline::setFlushInterval(int seconds)
{
    m_flushTimer->setInterval(seconds * 1000);
}

void DataPipeline::startRecording(qint64 workoutId)
{
    m_recording = true;
    m_workoutId = workoutId;

    // Reset accumulators
    m_hrBuffer.clear();
    m_elevationBuffer.clear();
    m_paceBuffer.clear();
    m_cadenceBuffer.clear();
    m_gpsTrack.clear();
    m_rawElevWindow.clear();

    m_lastLat = 0.0;
    m_lastLon = 0.0;
    m_lastAlt = 0.0;
    m_totalDistance = 0.0;
    m_totalElevationGain = 0.0;
    m_totalSteps = 0;
    m_startSteps = m_sensorService->stepCount();
    m_currentSpeed = 0.0f;
    m_currentAltitude = 0.0f;
    m_lastBaroAltitude = 0.0f;
    m_hasBaroElev = false;

    m_pendingSamples.clear();
    m_pendingGps.clear();

    m_flushTimer->start();
    qDebug() << "DataPipeline: recording started for workout" << workoutId;
}

void DataPipeline::stopRecording()
{
    m_flushTimer->stop();
    m_recording = false;

    // Flush any remaining data
    onFlushTimer();

    qDebug() << "DataPipeline: recording stopped. Distance:"
             << m_totalDistance << "m, Elevation:" << m_totalElevationGain << "m";
}

float DataPipeline::currentPace() const
{
    if (m_currentSpeed <= 0.01f) return 0.0f;
    return 1000.0f / m_currentSpeed; // sec/km
}

QVector<float> DataPipeline::elevationSeries() const
{
    QVector<float> series(m_elevationBuffer.count());
    m_elevationBuffer.copyAll(series.data());
    return series;
}

void DataPipeline::setDatabase(Database *db)
{
    m_database = db;
}

void DataPipeline::onSensorData(const SensorService::SensorSample &sample)
{
    if (!m_recording) return;

    switch (sample.type) {
    case SensorService::HeartRate:     processHeartRate(sample); break;
    case SensorService::Barometer:     processBarometer(sample); break;
    case SensorService::Accelerometer: processAccelerometer(sample); break;
    case SensorService::StepCount:     processStepCount(sample); break;
    default: break;
    }
}

void DataPipeline::onGpsPosition(double lat, double lon, double alt,
                                  float speed, float bearing)
{
    if (!m_recording) return;
    processGps(lat, lon, alt, speed, bearing);
}

void DataPipeline::processHeartRate(const SensorService::SensorSample &sample)
{
    float bpm = sample.values[0];
    if (bpm < 30.0f || bpm > 220.0f) return; // reject noise

    m_hrBuffer.push(bpm);
    emit hrUpdated(static_cast<int>(bpm));

    // Queue for DB
    if (m_database) {
        PendingSample ps;
        ps.sensor = QStringLiteral("hr");
        ps.timestamp = sample.timestampUs / 1000; // to ms
        ps.value = bpm;
        m_pendingSamples.append(ps);
    }
}

void DataPipeline::processBarometer(const SensorService::SensorSample &sample)
{
    float pressureHPa = sample.values[0];
    float altitude = baroAltitude(pressureHPa);

    // Smoothing via rolling window
    m_rawElevWindow.push(altitude);
    float smoothed = m_rawElevWindow.mean();

    m_currentAltitude = smoothed;
    m_elevationBuffer.push(smoothed);
    m_hasBaroElev = true;

    // Track elevation gain (only count positive changes > 1m threshold)
    if (m_lastBaroAltitude > 0.0f) {
        float delta = smoothed - m_lastBaroAltitude;
        if (delta > 1.0f) {
            m_totalElevationGain += delta;
        }
    }
    m_lastBaroAltitude = smoothed;

    emit elevationUpdated(m_totalElevationGain, m_currentAltitude);

    if (m_database) {
        PendingSample ps;
        ps.sensor = QStringLiteral("elevation");
        ps.timestamp = sample.timestampUs / 1000;
        ps.value = smoothed;
        m_pendingSamples.append(ps);
    }
}

void DataPipeline::processAccelerometer(const SensorService::SensorSample &sample)
{
    Q_UNUSED(sample);
    // Phase 2: use for cadence detection, auto-pause, activity recognition
}

void DataPipeline::processStepCount(const SensorService::SensorSample &sample)
{
    int currentSteps = static_cast<int>(sample.values[0]);
    m_totalSteps = currentSteps - m_startSteps;
    if (m_totalSteps < 0) m_totalSteps = 0; // handle counter reset
    emit stepsUpdated(m_totalSteps);
}

void DataPipeline::processGps(double lat, double lon, double alt,
                               float speed, float bearing)
{
    Q_UNUSED(bearing);

    // If we have a previous position, accumulate distance
    if (m_lastLat != 0.0 && m_lastLon != 0.0) {
        double dist = haversineDistance(m_lastLat, m_lastLon, lat, lon);
        // Filter GPS noise: ignore jumps < 1m or > 100m
        if (dist > 1.0 && dist < 100.0) {
            m_totalDistance += dist;
        }
    }

    // Use GPS altitude if no baro
    if (!m_hasBaroElev) {
        m_currentAltitude = static_cast<float>(alt);
        m_elevationBuffer.push(m_currentAltitude);

        if (m_lastAlt > 0.0) {
            double delta = alt - m_lastAlt;
            if (delta > 1.0) {
                m_totalElevationGain += delta;
            }
        }
        m_lastAlt = alt;
        emit elevationUpdated(m_totalElevationGain, m_currentAltitude);
    }

    m_lastLat = lat;
    m_lastLon = lon;
    m_currentSpeed = speed;

    // Calculate pace (sec/km)
    float pace = (speed > 0.1f) ? (1000.0f / speed) : 0.0f;
    m_paceBuffer.push(pace);

    emit distanceUpdated(m_totalDistance);
    emit paceUpdated(pace);

    // Store GPS track point
    GpsTrackPoint tp;
    tp.timestampUs = QDateTime::currentMSecsSinceEpoch() * 1000;
    tp.latitude = lat;
    tp.longitude = lon;
    tp.altitude = alt;
    tp.speed = speed;
    m_gpsTrack.append(tp);

    if (m_database) {
        m_pendingGps.append(tp);
    }
}

float DataPipeline::baroAltitude(float pressureHPa) const
{
    // International barometric formula
    // altitude = 44330 * (1 - (P/P0)^(1/5.255))
    if (pressureHPa <= 0.0f) return 0.0f;
    return 44330.0f * (1.0f - powf(pressureHPa / SEA_LEVEL_PRESSURE, 0.19029f));
}

double DataPipeline::haversineDistance(double lat1, double lon1,
                                       double lat2, double lon2) const
{
    const double R = 6371000.0; // Earth radius in meters
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dLon / 2.0) * sin(dLon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return R * c;
}

void DataPipeline::onFlushTimer()
{
    if (!m_database || !m_database->isOpen()) return;
    if (m_workoutId < 0) return;

    // Flush sensor samples
    if (!m_pendingSamples.isEmpty()) {
        // Group by sensor type for batched inserts
        QMap<QString, QVector<QPair<qint64, float>>> grouped;
        for (const PendingSample &ps : m_pendingSamples) {
            grouped[ps.sensor].append(qMakePair(ps.timestamp, ps.value));
        }
        for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
            m_database->storeSensorBatch(m_workoutId, it.key(), it.value());
        }
        m_pendingSamples.clear();
    }

    // Flush GPS track
    if (!m_pendingGps.isEmpty()) {
        QVector<Database::GpsPoint> dbPoints;
        dbPoints.reserve(m_pendingGps.size());
        for (const GpsTrackPoint &tp : m_pendingGps) {
            Database::GpsPoint gp;
            gp.timestamp = tp.timestampUs / 1000;
            gp.latitude = tp.latitude;
            gp.longitude = tp.longitude;
            gp.altitude = tp.altitude;
            gp.speed = tp.speed;
            gp.bearing = 0.0f;
            gp.accuracy = 0.0f;
            dbPoints.append(gp);
        }
        m_database->storeGpsTrack(m_workoutId, dbPoints);
        m_pendingGps.clear();
    }
}
