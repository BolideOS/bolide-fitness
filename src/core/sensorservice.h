/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SENSORSERVICE_H
#define SENSORSERVICE_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QFile>

/**
 * @brief Unified sensor data service with pluggable backends.
 *
 * Coordinates with bolide-powerd (which controls sensor power modes) and
 * reads actual sensor data through the best available backend:
 *   1. SensorFW D-Bus (managed sensors, most AsteroidOS watches)
 *   2. IIO sysfs / character devices (direct, low-latency)
 *   3. RTOS co-processor (TicWatch Pro 3 Ultra dual-display chip)
 *   4. Simulated backend (emulator / testing)
 *
 * Usage:
 *   SensorService *ss = new SensorService(this);
 *   connect(ss, &SensorService::heartRateChanged, this, &MyClass::onHR);
 *   ss->subscribe(SensorType::HeartRate, 1000);  // 1 Hz
 */
class SensorService : public QObject
{
    Q_OBJECT

public:
    enum SensorType {
        Accelerometer = 0,
        Gyroscope,
        HeartRate,
        PPGRaw,
        Barometer,
        SpO2,
        Temperature,
        Compass,
        AmbientLight,
        GPS,
        StepCount,
        SensorTypeCount  // sentinel
    };
    Q_ENUM(SensorType)

    struct SensorSample {
        SensorType type;
        qint64 timestampUs;     // microseconds since epoch
        float values[4];        // up to 4 channels (x,y,z,w or single value)
        int channelCount;
    };

    struct GpsPosition {
        qint64 timestampUs;
        double latitude;
        double longitude;
        double altitude;
        float  speed;           // m/s
        float  bearing;         // degrees
        float  accuracy;        // meters
    };

    // ── Backend interface ───────────────────────────────────────────────

    class Backend {
    public:
        virtual ~Backend() = default;
        virtual QString name() const = 0;
        virtual bool initialize() = 0;
        virtual bool startSensor(SensorType type, int intervalMs) = 0;
        virtual bool stopSensor(SensorType type) = 0;
        virtual bool isAvailable(SensorType type) const = 0;
        virtual QList<SensorType> availableSensors() const = 0;
    };

    // ── Construction ────────────────────────────────────────────────────

    explicit SensorService(QObject *parent = nullptr);
    ~SensorService() override;

    // Register a backend (SensorService takes ownership)
    void addBackend(Backend *backend);

    // Probe all backends and powerd for available sensors
    void initialize();

    // ── Subscription API ────────────────────────────────────────────────

    /** Subscribe to sensor data at minimum interval. Multiple subscribers merge. */
    bool subscribe(SensorType type, int intervalMs);
    bool unsubscribe(SensorType type);
    bool isSubscribed(SensorType type) const;

    /** Check sensor availability across all backends. */
    bool isAvailable(SensorType type) const;
    QList<SensorType> availableSensors() const;

    // ── Latest readings (for QML property binding) ──────────────────────

    int   heartRate()   const { return m_heartRate; }
    float baroPressure() const { return m_baroPressure; }
    float temperature() const { return m_temperature; }
    int   spo2()        const { return m_spo2; }
    int   stepCount()   const { return m_stepCount; }

    GpsPosition lastGpsPosition() const { return m_lastGps; }

    /** True when running in emulator with simulated data. */
    bool isSimulated() const { return m_simulated; }

signals:
    void sensorData(const SensorSample &sample);
    void heartRateChanged(int bpm);
    void gpsPositionChanged(double lat, double lon, double alt, float speed, float bearing);
    void barometerChanged(float pressureHPa);
    void spo2Changed(int percent);
    void stepCountChanged(int steps);
    void temperatureChanged(float celsius);
    void sensorError(SensorType type, const QString &error);

public slots:
    /** Called by backends when new data arrives. */
    void onBackendData(const SensorSample &sample);
    void onBackendGps(const GpsPosition &pos);

private:
    // Powerd D-Bus coordination
    bool requestSensorAccess(SensorType type, int intervalMs);
    bool releaseSensorAccess(SensorType type);
    void detectEmulator();
    Backend *bestBackendFor(SensorType type) const;

    static QString sensorTypeName(SensorType type);

    QVector<Backend*> m_backends;
    QMap<SensorType, int> m_subscriptions;     // type -> intervalMs
    QMap<SensorType, Backend*> m_activeBackend; // type -> backend serving it

    // Powerd D-Bus
    QDBusInterface *m_powerd;
    QDBusConnection m_systemBus;

    // Cached latest values
    int   m_heartRate;
    float m_baroPressure;
    float m_temperature;
    int   m_spo2;
    int   m_stepCount;
    GpsPosition m_lastGps;

    bool m_simulated;
    bool m_initialized;
};

// ── Built-in backends ───────────────────────────────────────────────────────

/**
 * @brief SensorFW D-Bus backend for AsteroidOS watches.
 */
class SensorFWDataBackend : public SensorService::Backend {
public:
    explicit SensorFWDataBackend(SensorService *service);
    ~SensorFWDataBackend() override;

    QString name() const override { return QStringLiteral("sensorfwd"); }
    bool initialize() override;
    bool startSensor(SensorService::SensorType type, int intervalMs) override;
    bool stopSensor(SensorService::SensorType type) override;
    bool isAvailable(SensorService::SensorType type) const override;
    QList<SensorService::SensorType> availableSensors() const override;

private:
    SensorService *m_service;
    QSet<SensorService::SensorType> m_available;
    bool m_initialized;
};

/**
 * @brief Direct IIO/sysfs backend for raw sensor access.
 */
class IIODataBackend : public SensorService::Backend {
public:
    explicit IIODataBackend(SensorService *service);
    ~IIODataBackend() override;

    QString name() const override { return QStringLiteral("iio"); }
    bool initialize() override;
    bool startSensor(SensorService::SensorType type, int intervalMs) override;
    bool stopSensor(SensorService::SensorType type) override;
    bool isAvailable(SensorService::SensorType type) const override;
    QList<SensorService::SensorType> availableSensors() const override;

private:
    struct IIODevice {
        QString path;
        QString name;
        SensorService::SensorType type;
    };
    void discoverDevices();

    SensorService *m_service;
    QMap<SensorService::SensorType, IIODevice> m_devices;
    QMap<SensorService::SensorType, QTimer*> m_pollTimers;
};

/**
 * @brief Simulated sensor backend for emulator testing.
 */
class SimulatedBackend : public SensorService::Backend {
public:
    explicit SimulatedBackend(SensorService *service);
    ~SimulatedBackend() override;

    QString name() const override { return QStringLiteral("simulated"); }
    bool initialize() override;
    bool startSensor(SensorService::SensorType type, int intervalMs) override;
    bool stopSensor(SensorService::SensorType type) override;
    bool isAvailable(SensorService::SensorType type) const override;
    QList<SensorService::SensorType> availableSensors() const override;

    void setWorkoutSimulation(const QString &workoutType);
    void clearWorkoutSimulation();

private:
    SensorService *m_service;
    QMap<SensorService::SensorType, QTimer*> m_timers;
    QString m_workoutType;
    int m_simulatedSteps;
    qint64 m_startTimeUs;
};

#endif // SENSORSERVICE_H
