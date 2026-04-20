/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Sensors & Lab manager — raw sensor streaming, live graphing, CSV export.
 */

#ifndef SENSORSLABMANAGER_H
#define SENSORSLABMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>

class SensorService;

/**
 * @brief Raw sensor data streaming and export for the Sensors & Lab page.
 *
 * Provides:
 * - Real-time streaming of any sensor at configurable rates
 * - Rolling buffer of last N samples per sensor for live graphing
 * - CSV export of recorded sensor data
 * - Sensor availability reporting
 *
 * This is a "lab mode" feature for advanced users who want to see
 * raw sensor data, validate hardware, or export data for analysis.
 */
class SensorsLabManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY sampleCountChanged)
    Q_PROPERTY(double recordingDuration READ recordingDuration NOTIFY durationChanged)
    Q_PROPERTY(QVariantList activeSensors READ activeSensors NOTIFY activeSensorsChanged)
    Q_PROPERTY(QVariantList availableSensorList READ availableSensorList CONSTANT)

public:
    explicit SensorsLabManager(QObject *parent = nullptr);
    ~SensorsLabManager() override;

    void setSensorService(SensorService *ss);

    // ── Streaming control ───────────────────────────────────────────────

    /**
     * Start streaming a sensor at given interval.
     * @param sensorType  SensorService::SensorType enum value
     * @param intervalMs  Sampling interval in milliseconds
     */
    Q_INVOKABLE bool startStream(int sensorType, int intervalMs = 100);

    /** Stop streaming a specific sensor. */
    Q_INVOKABLE bool stopStream(int sensorType);

    /** Stop all active streams. */
    Q_INVOKABLE void stopAllStreams();

    // ── Recording ───────────────────────────────────────────────────────

    /** Start recording all active streams to an in-memory buffer. */
    Q_INVOKABLE void startRecording();

    /** Stop recording. */
    Q_INVOKABLE void stopRecording();

    bool isRecording() const { return m_recording; }
    int sampleCount() const { return m_totalSamples; }
    double recordingDuration() const;

    // ── Data access ─────────────────────────────────────────────────────

    /**
     * Get recent samples for a sensor (for live graph).
     * Returns list of { "t": relativeMs, "v": value, "v2": ..., "v3": ... }
     * Buffer holds last 200 samples.
     */
    Q_INVOKABLE QVariantList recentSamples(int sensorType, int maxSamples = 200);

    /**
     * Get the latest value for a sensor.
     * Returns { "value": primary, "values": [ch0, ch1, ch2, ch3], "timestamp": ms }
     */
    Q_INVOKABLE QVariantMap latestValue(int sensorType);

    // ── Export ──────────────────────────────────────────────────────────

    /**
     * Export recorded data to CSV file.
     * @param filePath  Output path. If empty, auto-generates in data dir.
     * @return The file path written, or empty on failure.
     */
    Q_INVOKABLE QString exportCSV(const QString &filePath = QString());

    /**
     * Export only one sensor's data to CSV.
     */
    Q_INVOKABLE QString exportSensorCSV(int sensorType, const QString &filePath = QString());

    // ── Info ────────────────────────────────────────────────────────────

    QVariantList activeSensors() const;
    QVariantList availableSensorList() const;

    /** Human-readable name for a sensor type. */
    Q_INVOKABLE static QString sensorName(int sensorType);

    /** Number of data channels for a sensor type. */
    Q_INVOKABLE static int sensorChannels(int sensorType);

    /** Unit string for a sensor type. */
    Q_INVOKABLE static QString sensorUnit(int sensorType);

signals:
    void recordingChanged();
    void sampleCountChanged();
    void durationChanged();
    void activeSensorsChanged();
    void sampleReceived(int sensorType, double value);

private slots:
    void onSensorData(int type, qint64 timestampUs, float v0, float v1, float v2, float v3, int channels);

private:
    struct Sample {
        qint64 timestampUs;
        float values[4];
        int channels;
    };

    struct SensorStream {
        int intervalMs;
        QVector<Sample> liveBuffer;     // rolling buffer (max 200)
        QVector<Sample> recordBuffer;   // recording buffer (unbounded during recording)
        Sample latest;
        static const int LIVE_BUFFER_SIZE = 200;
    };

    SensorService *m_sensorService;
    QMap<int, SensorStream> m_streams;

    bool m_recording;
    int m_totalSamples;
    QElapsedTimer m_recordingTimer;
    QTimer *m_durationNotifier;
};

#endif // SENSORSLABMANAGER_H
