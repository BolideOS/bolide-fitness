/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sensorservice.h"
#include <QDebug>
#include <QDir>
#include <QDBusReply>
#include <QDateTime>
#include <QProcess>
#include <cmath>
#include <cstdlib>

#define POWERD_SERVICE    "org.bolideos.powerd"
#define POWERD_PATH       "/org/bolideos/powerd"
#define POWERD_INTERFACE  "org.bolideos.powerd.ProfileManager"

// ═══════════════════════════════════════════════════════════════════════════
// SensorService
// ═══════════════════════════════════════════════════════════════════════════

SensorService::SensorService(QObject *parent)
    : QObject(parent)
    , m_powerd(nullptr)
    , m_systemBus(QDBusConnection::systemBus())
    , m_heartRate(0)
    , m_baroPressure(0.0f)
    , m_temperature(0.0f)
    , m_spo2(0)
    , m_stepCount(0)
    , m_simulated(false)
    , m_initialized(false)
{
    m_lastGps = {};
}

SensorService::~SensorService()
{
    // Stop all active sensors
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        if (m_activeBackend.contains(it.key())) {
            m_activeBackend[it.key()]->stopSensor(it.key());
        }
        releaseSensorAccess(it.key());
    }
    qDeleteAll(m_backends);
}

void SensorService::addBackend(Backend *backend)
{
    m_backends.append(backend);
}

void SensorService::initialize()
{
    if (m_initialized) return;

    // Connect to powerd
    if (m_systemBus.isConnected()) {
        m_powerd = new QDBusInterface(
            POWERD_SERVICE, POWERD_PATH, POWERD_INTERFACE,
            m_systemBus, this);
        if (!m_powerd->isValid()) {
            qWarning() << "SensorService: powerd not available:" << m_powerd->lastError().message();
            delete m_powerd;
            m_powerd = nullptr;
        }
    }

    // Detect emulator
    detectEmulator();

    // Initialize all backends
    for (Backend *b : m_backends) {
        if (b->initialize()) {
            qDebug() << "SensorService: backend" << b->name() << "initialized";
        } else {
            qWarning() << "SensorService: backend" << b->name() << "failed to initialize";
        }
    }

    // If no real backends available, auto-add simulated
    if (m_simulated) {
        bool hasSimulated = false;
        for (Backend *b : m_backends) {
            if (b->name() == QLatin1String("simulated")) {
                hasSimulated = true;
                break;
            }
        }
        if (!hasSimulated) {
            SimulatedBackend *sim = new SimulatedBackend(this);
            sim->initialize();
            m_backends.append(sim);
            qDebug() << "SensorService: auto-added simulated backend (emulator detected)";
        }
    }

    m_initialized = true;
}

bool SensorService::subscribe(SensorType type, int intervalMs)
{
    if (!m_initialized) initialize();

    Backend *backend = bestBackendFor(type);
    if (!backend) {
        qWarning() << "SensorService: no backend for" << sensorTypeName(type);
        emit sensorError(type, QStringLiteral("No backend available"));
        return false;
    }

    // Tell powerd we need this sensor
    requestSensorAccess(type, intervalMs);

    // Start reading via backend
    if (!backend->startSensor(type, intervalMs)) {
        qWarning() << "SensorService: failed to start" << sensorTypeName(type)
                    << "on" << backend->name();
        releaseSensorAccess(type);
        return false;
    }

    m_subscriptions[type] = intervalMs;
    m_activeBackend[type] = backend;
    qDebug() << "SensorService: subscribed to" << sensorTypeName(type)
             << "at" << intervalMs << "ms via" << backend->name();
    return true;
}

bool SensorService::unsubscribe(SensorType type)
{
    if (!m_subscriptions.contains(type)) return false;

    if (m_activeBackend.contains(type)) {
        m_activeBackend[type]->stopSensor(type);
        m_activeBackend.remove(type);
    }

    releaseSensorAccess(type);
    m_subscriptions.remove(type);
    return true;
}

bool SensorService::isSubscribed(SensorType type) const
{
    return m_subscriptions.contains(type);
}

bool SensorService::isAvailable(SensorType type) const
{
    for (Backend *b : m_backends) {
        if (b->isAvailable(type)) return true;
    }
    return false;
}

QList<SensorService::SensorType> SensorService::availableSensors() const
{
    QSet<SensorType> result;
    for (Backend *b : m_backends) {
        for (SensorType t : b->availableSensors())
            result.insert(t);
    }
    return result.toList();
}

void SensorService::onBackendData(const SensorSample &sample)
{
    // Update cached values based on type
    switch (sample.type) {
    case HeartRate:
        if (sample.channelCount >= 1) {
            int bpm = static_cast<int>(sample.values[0]);
            if (bpm != m_heartRate) {
                m_heartRate = bpm;
                emit heartRateChanged(bpm);
            }
        }
        break;
    case Barometer:
        if (sample.channelCount >= 1) {
            m_baroPressure = sample.values[0];
            emit barometerChanged(m_baroPressure);
        }
        break;
    case SpO2:
        if (sample.channelCount >= 1) {
            int pct = static_cast<int>(sample.values[0]);
            if (pct != m_spo2) {
                m_spo2 = pct;
                emit spo2Changed(pct);
            }
        }
        break;
    case Temperature:
        if (sample.channelCount >= 1) {
            m_temperature = sample.values[0];
            emit temperatureChanged(m_temperature);
        }
        break;
    case StepCount:
        if (sample.channelCount >= 1) {
            int steps = static_cast<int>(sample.values[0]);
            if (steps != m_stepCount) {
                m_stepCount = steps;
                emit stepCountChanged(steps);
            }
        }
        break;
    default:
        break;
    }

    emit sensorData(sample);
}

void SensorService::onBackendGps(const GpsPosition &pos)
{
    m_lastGps = pos;
    emit gpsPositionChanged(pos.latitude, pos.longitude, pos.altitude,
                            pos.speed, pos.bearing);
}

bool SensorService::requestSensorAccess(SensorType type, int intervalMs)
{
    if (!m_powerd) return false;

    QDBusMessage reply = m_powerd->call(
        QStringLiteral("RequestSensorAccess"),
        sensorTypeName(type), intervalMs);

    if (reply.type() == QDBusMessage::ErrorMessage) {
        // Not fatal: powerd may not support this method yet
        qDebug() << "SensorService: RequestSensorAccess not available (ok)";
        return false;
    }
    return true;
}

bool SensorService::releaseSensorAccess(SensorType type)
{
    if (!m_powerd) return false;

    QDBusMessage reply = m_powerd->call(
        QStringLiteral("ReleaseSensorAccess"),
        sensorTypeName(type));
    return reply.type() != QDBusMessage::ErrorMessage;
}

void SensorService::detectEmulator()
{
    // Check common emulator indicators
    QFile cpuinfo(QStringLiteral("/proc/cpuinfo"));
    if (cpuinfo.open(QIODevice::ReadOnly)) {
        QByteArray data = cpuinfo.readAll();
        if (data.contains("QEMU") || data.contains("GenuineIntel")) {
            m_simulated = true;
            return;
        }
    }

    // Check for emulator env var
    if (qEnvironmentVariableIsSet("ASTEROID_EMULATOR")) {
        m_simulated = true;
        return;
    }

    // Check for /dev/iio (if absent, likely emulator)
    QDir iioDir(QStringLiteral("/sys/bus/iio/devices"));
    if (!iioDir.exists() || iioDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
        m_simulated = true;
    }
}

SensorService::Backend *SensorService::bestBackendFor(SensorType type) const
{
    // Priority: SensorFW > IIO > RTOS > Simulated
    for (Backend *b : m_backends) {
        if (b->name() != QLatin1String("simulated") && b->isAvailable(type))
            return b;
    }
    // Fallback to simulated
    for (Backend *b : m_backends) {
        if (b->isAvailable(type))
            return b;
    }
    return nullptr;
}

QString SensorService::sensorTypeName(SensorType type)
{
    switch (type) {
    case Accelerometer: return QStringLiteral("accelerometer");
    case Gyroscope:     return QStringLiteral("gyroscope");
    case HeartRate:     return QStringLiteral("heart_rate");
    case PPGRaw:        return QStringLiteral("ppg_raw");
    case Barometer:     return QStringLiteral("barometer");
    case SpO2:          return QStringLiteral("spo2");
    case Temperature:   return QStringLiteral("temperature");
    case Compass:       return QStringLiteral("compass");
    case AmbientLight:  return QStringLiteral("ambient_light");
    case GPS:           return QStringLiteral("gps");
    case StepCount:     return QStringLiteral("step_count");
    default:            return QStringLiteral("unknown");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// SensorFWDataBackend
// ═══════════════════════════════════════════════════════════════════════════

SensorFWDataBackend::SensorFWDataBackend(SensorService *service)
    : m_service(service)
    , m_initialized(false)
{
}

SensorFWDataBackend::~SensorFWDataBackend() = default;

bool SensorFWDataBackend::initialize()
{
    // Check if sensorfwd is running
    QDBusConnection bus = QDBusConnection::systemBus();
    QDBusInterface iface(
        QStringLiteral("com.nokia.SensorService"),
        QStringLiteral("/SensorManager"),
        QStringLiteral("local.SensorManager"),
        bus);

    if (!iface.isValid()) {
        qDebug() << "SensorFWDataBackend: SensorFW not available";
        return false;
    }

    // Probe available sensors
    // Common SensorFW sensor IDs:
    const QStringList probeIds = {
        QStringLiteral("accelerometersensor"),
        QStringLiteral("gyroscopesensor"),
        QStringLiteral("heartratemonitor"),
        QStringLiteral("pressuresensor"),
        QStringLiteral("compasssensor"),
        QStringLiteral("alssensor"),
    };

    static const SensorService::SensorType probeTypes[] = {
        SensorService::Accelerometer,
        SensorService::Gyroscope,
        SensorService::HeartRate,
        SensorService::Barometer,
        SensorService::Compass,
        SensorService::AmbientLight,
    };

    for (int i = 0; i < probeIds.size(); ++i) {
        QDBusMessage msg = iface.call(QStringLiteral("loadPlugin"), probeIds[i]);
        if (msg.type() != QDBusMessage::ErrorMessage) {
            m_available.insert(probeTypes[i]);
        }
    }

    m_initialized = !m_available.isEmpty();
    qDebug() << "SensorFWDataBackend:" << m_available.size() << "sensors available";
    return m_initialized;
}

bool SensorFWDataBackend::startSensor(SensorService::SensorType type, int intervalMs)
{
    Q_UNUSED(type);
    Q_UNUSED(intervalMs);
    // TODO: Open SensorFW session and connect to data signals
    // For now, this is a placeholder — real implementation connects
    // to SensorFW D-Bus data signals per sensor type
    return m_available.contains(type);
}

bool SensorFWDataBackend::stopSensor(SensorService::SensorType type)
{
    Q_UNUSED(type);
    return true;
}

bool SensorFWDataBackend::isAvailable(SensorService::SensorType type) const
{
    return m_available.contains(type);
}

QList<SensorService::SensorType> SensorFWDataBackend::availableSensors() const
{
    return m_available.toList();
}

// ═══════════════════════════════════════════════════════════════════════════
// IIODataBackend
// ═══════════════════════════════════════════════════════════════════════════

IIODataBackend::IIODataBackend(SensorService *service)
    : m_service(service)
{
}

IIODataBackend::~IIODataBackend()
{
    qDeleteAll(m_pollTimers);
}

bool IIODataBackend::initialize()
{
    discoverDevices();
    return !m_devices.isEmpty();
}

void IIODataBackend::discoverDevices()
{
    QDir iioDir(QStringLiteral("/sys/bus/iio/devices"));
    if (!iioDir.exists()) return;

    for (const QString &entry : iioDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString devPath = iioDir.absoluteFilePath(entry);
        QString namePath = devPath + QStringLiteral("/name");

        QFile nameFile(namePath);
        if (!nameFile.open(QIODevice::ReadOnly)) continue;

        QString name = QString::fromLatin1(nameFile.readAll().trimmed());
        nameFile.close();

        // Map IIO device names to sensor types
        // These vary by watch model; common patterns:
        IIODevice dev;
        dev.path = devPath;
        dev.name = name;

        if (name.contains(QLatin1String("accel"))) {
            dev.type = SensorService::Accelerometer;
            m_devices[SensorService::Accelerometer] = dev;
        } else if (name.contains(QLatin1String("gyro"))) {
            dev.type = SensorService::Gyroscope;
            m_devices[SensorService::Gyroscope] = dev;
        } else if (name.contains(QLatin1String("heart")) || name.contains(QLatin1String("ppg"))) {
            dev.type = SensorService::HeartRate;
            m_devices[SensorService::HeartRate] = dev;
        } else if (name.contains(QLatin1String("pressure")) || name.contains(QLatin1String("baro"))) {
            dev.type = SensorService::Barometer;
            m_devices[SensorService::Barometer] = dev;
        } else if (name.contains(QLatin1String("magn")) || name.contains(QLatin1String("compass"))) {
            dev.type = SensorService::Compass;
            m_devices[SensorService::Compass] = dev;
        } else if (name.contains(QLatin1String("illuminance")) || name.contains(QLatin1String("als"))) {
            dev.type = SensorService::AmbientLight;
            m_devices[SensorService::AmbientLight] = dev;
        } else if (name.contains(QLatin1String("temp"))) {
            dev.type = SensorService::Temperature;
            m_devices[SensorService::Temperature] = dev;
        }
    }

    qDebug() << "IIODataBackend: discovered" << m_devices.size() << "IIO sensors";
}

bool IIODataBackend::startSensor(SensorService::SensorType type, int intervalMs)
{
    if (!m_devices.contains(type)) return false;

    // Stop existing timer if any
    if (m_pollTimers.contains(type)) {
        m_pollTimers[type]->stop();
        delete m_pollTimers[type];
    }

    const IIODevice &dev = m_devices[type];
    QTimer *timer = new QTimer();
    timer->setInterval(intervalMs);

    QObject::connect(timer, &QTimer::timeout, [this, type, dev]() {
        SensorService::SensorSample sample;
        sample.type = type;
        sample.timestampUs = QDateTime::currentMSecsSinceEpoch() * 1000;
        sample.channelCount = 0;

        // Read from sysfs based on sensor type
        auto readFloat = [&](const QString &attr) -> float {
            QFile f(dev.path + QStringLiteral("/") + attr);
            if (f.open(QIODevice::ReadOnly)) {
                bool ok;
                float val = QString::fromLatin1(f.readAll().trimmed()).toFloat(&ok);
                return ok ? val : 0.0f;
            }
            return 0.0f;
        };

        switch (type) {
        case SensorService::Accelerometer:
            sample.values[0] = readFloat(QStringLiteral("in_accel_x_raw"));
            sample.values[1] = readFloat(QStringLiteral("in_accel_y_raw"));
            sample.values[2] = readFloat(QStringLiteral("in_accel_z_raw"));
            sample.channelCount = 3;
            break;
        case SensorService::Gyroscope:
            sample.values[0] = readFloat(QStringLiteral("in_anglvel_x_raw"));
            sample.values[1] = readFloat(QStringLiteral("in_anglvel_y_raw"));
            sample.values[2] = readFloat(QStringLiteral("in_anglvel_z_raw"));
            sample.channelCount = 3;
            break;
        case SensorService::HeartRate:
            sample.values[0] = readFloat(QStringLiteral("in_heart_rate_raw"));
            if (sample.values[0] <= 0)
                sample.values[0] = readFloat(QStringLiteral("data"));
            sample.channelCount = 1;
            break;
        case SensorService::Barometer:
            sample.values[0] = readFloat(QStringLiteral("in_pressure_input"));
            // Convert kPa to hPa if needed
            if (sample.values[0] > 0 && sample.values[0] < 200)
                sample.values[0] *= 10.0f;
            sample.channelCount = 1;
            break;
        case SensorService::Temperature:
            sample.values[0] = readFloat(QStringLiteral("in_temp_input"));
            // Some sensors report in millidegrees
            if (sample.values[0] > 1000)
                sample.values[0] /= 1000.0f;
            sample.channelCount = 1;
            break;
        default:
            break;
        }

        if (sample.channelCount > 0) {
            m_service->onBackendData(sample);
        }
    });

    timer->start();
    m_pollTimers[type] = timer;
    return true;
}

bool IIODataBackend::stopSensor(SensorService::SensorType type)
{
    if (m_pollTimers.contains(type)) {
        m_pollTimers[type]->stop();
        delete m_pollTimers[type];
        m_pollTimers.remove(type);
    }
    return true;
}

bool IIODataBackend::isAvailable(SensorService::SensorType type) const
{
    return m_devices.contains(type);
}

QList<SensorService::SensorType> IIODataBackend::availableSensors() const
{
    return m_devices.keys();
}

// ═══════════════════════════════════════════════════════════════════════════
// SimulatedBackend
// ═══════════════════════════════════════════════════════════════════════════

SimulatedBackend::SimulatedBackend(SensorService *service)
    : m_service(service)
    , m_simulatedSteps(0)
    , m_startTimeUs(0)
{
}

SimulatedBackend::~SimulatedBackend()
{
    qDeleteAll(m_timers);
}

bool SimulatedBackend::initialize()
{
    m_startTimeUs = QDateTime::currentMSecsSinceEpoch() * 1000;
    return true;
}

bool SimulatedBackend::startSensor(SensorService::SensorType type, int intervalMs)
{
    if (m_timers.contains(type)) {
        m_timers[type]->stop();
        delete m_timers[type];
    }

    QTimer *timer = new QTimer();
    timer->setInterval(intervalMs);

    QObject::connect(timer, &QTimer::timeout, [this, type]() {
        SensorService::SensorSample sample;
        sample.type = type;
        sample.timestampUs = QDateTime::currentMSecsSinceEpoch() * 1000;
        sample.channelCount = 0;

        qint64 elapsedSec = (sample.timestampUs - m_startTimeUs) / 1000000;
        double phase = static_cast<double>(elapsedSec) / 60.0; // cycles per minute

        switch (type) {
        case SensorService::HeartRate: {
            int baseHR = 70;
            int offset = 40;
            if (m_workoutType == QLatin1String("outdoor_run") ||
                m_workoutType == QLatin1String("cycling")) {
                offset = 60;
            } else if (m_workoutType == QLatin1String("strength") ||
                       m_workoutType == QLatin1String("yoga")) {
                offset = 20;
            } else if (m_workoutType == QLatin1String("ultra_hike")) {
                offset = 50;
            }
            int variation = static_cast<int>(10.0 * sin(phase * 3.14159));
            int hr = m_workoutType.isEmpty() ? 70 : baseHR + offset + variation;
            hr = qBound(50, hr, 195);
            sample.values[0] = static_cast<float>(hr);
            sample.channelCount = 1;
            break;
        }
        case SensorService::Barometer: {
            // Simulate ~1013 hPa with slow drift (elevation changes)
            float pressure = 1013.25f - static_cast<float>(sin(phase * 0.1) * 5.0);
            sample.values[0] = pressure;
            sample.channelCount = 1;
            break;
        }
        case SensorService::Accelerometer: {
            float magnitude = m_workoutType.isEmpty() ? 0.2f : 1.5f;
            sample.values[0] = static_cast<float>(sin(phase * 2.0) * magnitude);
            sample.values[1] = static_cast<float>(cos(phase * 2.0) * magnitude);
            sample.values[2] = 9.81f + static_cast<float>(sin(phase * 4.0) * 0.3);
            sample.channelCount = 3;
            break;
        }
        case SensorService::Gyroscope: {
            float rate = m_workoutType.isEmpty() ? 0.05f : 0.5f;
            sample.values[0] = static_cast<float>(sin(phase * 3.0) * rate);
            sample.values[1] = static_cast<float>(cos(phase * 3.0) * rate);
            sample.values[2] = static_cast<float>(sin(phase * 1.5) * rate * 0.5);
            sample.channelCount = 3;
            break;
        }
        case SensorService::SpO2: {
            sample.values[0] = 96.0f + static_cast<float>(sin(phase * 0.2) * 2.0);
            sample.channelCount = 1;
            break;
        }
        case SensorService::Temperature: {
            sample.values[0] = 36.5f + static_cast<float>(sin(phase * 0.05) * 0.5);
            sample.channelCount = 1;
            break;
        }
        case SensorService::StepCount: {
            if (!m_workoutType.isEmpty()) {
                m_simulatedSteps += 2; // ~120 steps/min at 1Hz
            }
            sample.values[0] = static_cast<float>(m_simulatedSteps);
            sample.channelCount = 1;
            break;
        }
        case SensorService::Compass: {
            float heading = static_cast<float>(fmod(phase * 10.0, 360.0));
            sample.values[0] = heading;
            sample.channelCount = 1;
            break;
        }
        default:
            break;
        }

        if (sample.channelCount > 0) {
            m_service->onBackendData(sample);
        }

        // Simulate GPS separately
        if (type == SensorService::GPS) {
            SensorService::GpsPosition pos;
            pos.timestampUs = sample.timestampUs;
            pos.latitude = 48.8566 + sin(phase * 0.01) * 0.001;
            pos.longitude = 2.3522 + cos(phase * 0.01) * 0.001;
            pos.altitude = 35.0 + sin(phase * 0.05) * 20.0;
            pos.speed = m_workoutType.isEmpty() ? 0.0f : 2.5f + static_cast<float>(sin(phase) * 0.5);
            pos.bearing = static_cast<float>(fmod(phase * 5.0, 360.0));
            pos.accuracy = 5.0f;
            m_service->onBackendGps(pos);
        }
    });

    timer->start();
    m_timers[type] = timer;
    return true;
}

bool SimulatedBackend::stopSensor(SensorService::SensorType type)
{
    if (m_timers.contains(type)) {
        m_timers[type]->stop();
        delete m_timers[type];
        m_timers.remove(type);
    }
    return true;
}

bool SimulatedBackend::isAvailable(SensorService::SensorType type) const
{
    Q_UNUSED(type);
    return true; // Simulated backend supports everything
}

QList<SensorService::SensorType> SimulatedBackend::availableSensors() const
{
    QList<SensorService::SensorType> all;
    for (int i = 0; i < SensorService::SensorTypeCount; ++i)
        all.append(static_cast<SensorService::SensorType>(i));
    return all;
}

void SimulatedBackend::setWorkoutSimulation(const QString &workoutType)
{
    m_workoutType = workoutType;
    m_startTimeUs = QDateTime::currentMSecsSinceEpoch() * 1000;
    m_simulatedSteps = 0;
}

void SimulatedBackend::clearWorkoutSimulation()
{
    m_workoutType.clear();
}
