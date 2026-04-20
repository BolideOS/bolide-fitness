/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sensorslabmanager.h"
#include "core/sensorservice.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>

SensorsLabManager::SensorsLabManager(QObject *parent)
    : QObject(parent)
    , m_sensorService(nullptr)
    , m_recording(false)
    , m_totalSamples(0)
    , m_durationNotifier(new QTimer(this))
{
    m_durationNotifier->setInterval(500);
    connect(m_durationNotifier, &QTimer::timeout, this, &SensorsLabManager::durationChanged);
}

SensorsLabManager::~SensorsLabManager()
{
    stopAllStreams();
}

void SensorsLabManager::setSensorService(SensorService *ss)
{
    if (m_sensorService) {
        disconnect(m_sensorService, nullptr, this, nullptr);
    }

    m_sensorService = ss;

    if (m_sensorService) {
        // Connect to the generic sensor data signal
        connect(m_sensorService, &SensorService::sensorData, this,
                [this](const SensorService::SensorSample &sample) {
            int type = static_cast<int>(sample.type);
            if (!m_streams.contains(type))
                return;

            onSensorData(type, sample.timestampUs,
                         sample.values[0], sample.values[1],
                         sample.values[2], sample.values[3],
                         sample.channelCount);
        });
    }
}

// ── Streaming control ───────────────────────────────────────────────────────

bool SensorsLabManager::startStream(int sensorType, int intervalMs)
{
    if (!m_sensorService)
        return false;

    auto st = static_cast<SensorService::SensorType>(sensorType);

    if (!m_sensorService->isAvailable(st))
        return false;

    if (m_streams.contains(sensorType)) {
        // Already streaming — update interval
        m_sensorService->unsubscribe(st);
    }

    if (!m_sensorService->subscribe(st, intervalMs))
        return false;

    SensorStream stream;
    stream.intervalMs = intervalMs;
    stream.latest = {};
    m_streams.insert(sensorType, stream);

    emit activeSensorsChanged();
    return true;
}

bool SensorsLabManager::stopStream(int sensorType)
{
    if (!m_streams.contains(sensorType))
        return false;

    if (m_sensorService) {
        auto st = static_cast<SensorService::SensorType>(sensorType);
        m_sensorService->unsubscribe(st);
    }

    m_streams.remove(sensorType);
    emit activeSensorsChanged();
    return true;
}

void SensorsLabManager::stopAllStreams()
{
    QList<int> types = m_streams.keys();
    for (int type : types)
        stopStream(type);
}

// ── Recording ───────────────────────────────────────────────────────────────

void SensorsLabManager::startRecording()
{
    if (m_recording)
        return;

    // Clear previous recording buffers
    for (auto it = m_streams.begin(); it != m_streams.end(); ++it)
        it->recordBuffer.clear();

    m_totalSamples = 0;
    m_recording = true;
    m_recordingTimer.start();
    m_durationNotifier->start();

    emit recordingChanged();
    emit sampleCountChanged();
}

void SensorsLabManager::stopRecording()
{
    if (!m_recording)
        return;

    m_recording = false;
    m_durationNotifier->stop();
    emit recordingChanged();
}

double SensorsLabManager::recordingDuration() const
{
    if (!m_recording && m_totalSamples == 0)
        return 0.0;
    return m_recordingTimer.elapsed() / 1000.0;
}

// ── Data access ─────────────────────────────────────────────────────────────

QVariantList SensorsLabManager::recentSamples(int sensorType, int maxSamples)
{
    QVariantList result;

    if (!m_streams.contains(sensorType))
        return result;

    const SensorStream &stream = m_streams[sensorType];
    const QVector<Sample> &buf = stream.liveBuffer;

    int start = qMax(0, buf.size() - maxSamples);
    result.reserve(buf.size() - start);

    qint64 baseTime = buf.isEmpty() ? 0 : buf.first().timestampUs;

    for (int i = start; i < buf.size(); ++i) {
        QVariantMap pt;
        pt[QStringLiteral("t")] = (buf[i].timestampUs - baseTime) / 1000.0;  // ms
        pt[QStringLiteral("v")] = static_cast<double>(buf[i].values[0]);
        if (buf[i].channels > 1)
            pt[QStringLiteral("v2")] = static_cast<double>(buf[i].values[1]);
        if (buf[i].channels > 2)
            pt[QStringLiteral("v3")] = static_cast<double>(buf[i].values[2]);
        if (buf[i].channels > 3)
            pt[QStringLiteral("v4")] = static_cast<double>(buf[i].values[3]);
        result.append(pt);
    }

    return result;
}

QVariantMap SensorsLabManager::latestValue(int sensorType)
{
    QVariantMap result;

    if (!m_streams.contains(sensorType))
        return result;

    const Sample &s = m_streams[sensorType].latest;
    result[QStringLiteral("value")]     = static_cast<double>(s.values[0]);
    result[QStringLiteral("timestamp")] = s.timestampUs / 1000;  // ms

    QVariantList vals;
    for (int i = 0; i < s.channels; ++i)
        vals.append(static_cast<double>(s.values[i]));
    result[QStringLiteral("values")] = vals;

    return result;
}

// ── Export ───────────────────────────────────────────────────────────────────

QString SensorsLabManager::exportCSV(const QString &filePath)
{
    QString path = filePath;
    if (path.isEmpty()) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                      + QStringLiteral("/exports");
        QDir().mkpath(dir);
        path = dir + QStringLiteral("/sensors_")
               + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss"))
               + QStringLiteral(".csv");
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();

    QTextStream out(&file);

    // Header
    out << "sensor_type,sensor_name,timestamp_us,ch0,ch1,ch2,ch3\n";

    // Write all recorded data sorted by timestamp
    struct TaggedSample {
        int type;
        Sample sample;
    };

    QVector<TaggedSample> allSamples;
    for (auto it = m_streams.constBegin(); it != m_streams.constEnd(); ++it) {
        for (const Sample &s : it->recordBuffer) {
            allSamples.append({it.key(), s});
        }
    }

    // Sort by timestamp
    std::sort(allSamples.begin(), allSamples.end(),
              [](const TaggedSample &a, const TaggedSample &b) {
        return a.sample.timestampUs < b.sample.timestampUs;
    });

    for (const TaggedSample &ts : allSamples) {
        out << ts.type << ','
            << sensorName(ts.type) << ','
            << ts.sample.timestampUs << ','
            << ts.sample.values[0] << ','
            << ts.sample.values[1] << ','
            << ts.sample.values[2] << ','
            << ts.sample.values[3] << '\n';
    }

    file.close();
    return path;
}

QString SensorsLabManager::exportSensorCSV(int sensorType, const QString &filePath)
{
    if (!m_streams.contains(sensorType))
        return QString();

    QString path = filePath;
    if (path.isEmpty()) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                      + QStringLiteral("/exports");
        QDir().mkpath(dir);
        path = dir + QStringLiteral("/") + sensorName(sensorType).toLower().replace(' ', '_')
               + QStringLiteral("_")
               + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss"))
               + QStringLiteral(".csv");
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();

    QTextStream out(&file);

    int ch = sensorChannels(sensorType);
    out << "timestamp_us";
    for (int i = 0; i < ch; ++i)
        out << ",ch" << i;
    out << '\n';

    const QVector<Sample> &buf = m_streams[sensorType].recordBuffer;
    for (const Sample &s : buf) {
        out << s.timestampUs;
        for (int i = 0; i < ch; ++i)
            out << ',' << s.values[i];
        out << '\n';
    }

    file.close();
    return path;
}

// ── Info ────────────────────────────────────────────────────────────────────

QVariantList SensorsLabManager::activeSensors() const
{
    QVariantList list;
    for (auto it = m_streams.constBegin(); it != m_streams.constEnd(); ++it) {
        QVariantMap s;
        s[QStringLiteral("type")]     = it.key();
        s[QStringLiteral("name")]     = sensorName(it.key());
        s[QStringLiteral("interval")] = it->intervalMs;
        s[QStringLiteral("samples")]  = it->liveBuffer.size();
        list.append(s);
    }
    return list;
}

QVariantList SensorsLabManager::availableSensorList() const
{
    QVariantList list;
    // List all possible sensor types
    for (int i = 0; i < static_cast<int>(SensorService::SensorTypeCount); ++i) {
        bool avail = m_sensorService ? m_sensorService->isAvailable(
            static_cast<SensorService::SensorType>(i)) : false;

        QVariantMap s;
        s[QStringLiteral("type")]      = i;
        s[QStringLiteral("name")]      = sensorName(i);
        s[QStringLiteral("unit")]      = sensorUnit(i);
        s[QStringLiteral("channels")]  = sensorChannels(i);
        s[QStringLiteral("available")] = avail;
        list.append(s);
    }
    return list;
}

QString SensorsLabManager::sensorName(int sensorType)
{
    switch (static_cast<SensorService::SensorType>(sensorType)) {
    case SensorService::Accelerometer:  return QStringLiteral("Accelerometer");
    case SensorService::Gyroscope:      return QStringLiteral("Gyroscope");
    case SensorService::HeartRate:      return QStringLiteral("Heart Rate");
    case SensorService::PPGRaw:         return QStringLiteral("PPG Raw");
    case SensorService::Barometer:      return QStringLiteral("Barometer");
    case SensorService::SpO2:           return QStringLiteral("SpO2");
    case SensorService::Temperature:    return QStringLiteral("Temperature");
    case SensorService::Compass:        return QStringLiteral("Compass");
    case SensorService::AmbientLight:   return QStringLiteral("Ambient Light");
    case SensorService::GPS:            return QStringLiteral("GPS");
    case SensorService::StepCount:      return QStringLiteral("Step Counter");
    default:                            return QStringLiteral("Unknown");
    }
}

int SensorsLabManager::sensorChannels(int sensorType)
{
    switch (static_cast<SensorService::SensorType>(sensorType)) {
    case SensorService::Accelerometer:  return 3;   // x, y, z
    case SensorService::Gyroscope:      return 3;
    case SensorService::Compass:        return 3;
    case SensorService::GPS:            return 4;   // lat, lon, alt, speed
    default:                            return 1;
    }
}

QString SensorsLabManager::sensorUnit(int sensorType)
{
    switch (static_cast<SensorService::SensorType>(sensorType)) {
    case SensorService::Accelerometer:  return QStringLiteral("m/s²");
    case SensorService::Gyroscope:      return QStringLiteral("°/s");
    case SensorService::HeartRate:      return QStringLiteral("bpm");
    case SensorService::PPGRaw:         return QStringLiteral("raw");
    case SensorService::Barometer:      return QStringLiteral("hPa");
    case SensorService::SpO2:           return QStringLiteral("%");
    case SensorService::Temperature:    return QStringLiteral("°C");
    case SensorService::Compass:        return QStringLiteral("µT");
    case SensorService::AmbientLight:   return QStringLiteral("lux");
    case SensorService::GPS:            return QStringLiteral("deg");
    case SensorService::StepCount:      return QStringLiteral("steps");
    default:                            return QStringLiteral("");
    }
}

// ── Private slots ───────────────────────────────────────────────────────────

void SensorsLabManager::onSensorData(int type, qint64 timestampUs,
                                      float v0, float v1, float v2, float v3,
                                      int channels)
{
    if (!m_streams.contains(type))
        return;

    SensorStream &stream = m_streams[type];

    Sample s;
    s.timestampUs = timestampUs;
    s.values[0] = v0;
    s.values[1] = v1;
    s.values[2] = v2;
    s.values[3] = v3;
    s.channels = channels;

    stream.latest = s;

    // Add to live rolling buffer
    if (stream.liveBuffer.size() >= SensorStream::LIVE_BUFFER_SIZE)
        stream.liveBuffer.removeFirst();
    stream.liveBuffer.append(s);

    // Add to recording buffer if recording
    if (m_recording) {
        stream.recordBuffer.append(s);
        m_totalSamples++;
        emit sampleCountChanged();
    }

    emit sampleReceived(type, static_cast<double>(v0));
}
