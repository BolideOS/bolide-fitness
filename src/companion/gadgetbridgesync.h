/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gadgetbridge synchronization bridge.
 * Translates between Bolide Fitness data model and Gadgetbridge
 * BLE protocol for seamless Android companion integration.
 */

#ifndef GADGETBRIDGESYNC_H
#define GADGETBRIDGESYNC_H

#include <QObject>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>
#include <QByteArray>

class Database;
class ExportManager;

/**
 * @brief Gadgetbridge-compatible BLE synchronization manager.
 *
 * Implements a subset of the Gadgetbridge device protocol to sync
 * fitness data to the Android Gadgetbridge companion app. Uses the
 * AsteroidOS BLE connectivity via D-Bus.
 *
 * Sync protocol:
 * 1. Watch advertises via AsteroidOS BLE service
 * 2. Gadgetbridge connects and sends sync request
 * 3. This service responds with workout/health data in Gadgetbridge format
 * 4. Bidirectional: receives time sync, weather, notifications
 *
 * D-Bus integration:
 * - Listens on org.asteroidos.* namespace for BLE events
 * - Pushes data via AsteroidOS BLE characteristics
 *
 * Data mapping:
 * - Steps → ActivitySample
 * - HR    → HeartRateSample
 * - Sleep → SleepActivitySample
 * - SpO2  → Spo2Sample (Gadgetbridge 0.71+)
 */
class GadgetbridgeSync : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool syncing READ isSyncing NOTIFY syncingChanged)
    Q_PROPERTY(QString lastSyncTime READ lastSyncTime NOTIFY lastSyncTimeChanged)
    Q_PROPERTY(int pendingItems READ pendingItems NOTIFY pendingItemsChanged)

public:
    explicit GadgetbridgeSync(QObject *parent = nullptr);
    ~GadgetbridgeSync() override;

    void setDatabase(Database *db);
    void setExportManager(ExportManager *em);

    /** Initialize D-Bus connections to AsteroidOS BLE service. */
    bool initialize();

    // ── Connection state ────────────────────────────────────────────────

    bool isConnected() const { return m_connected; }
    bool isSyncing() const { return m_syncing; }
    QString lastSyncTime() const { return m_lastSyncTime; }
    int pendingItems() const { return m_pendingItems; }

    // ── Manual sync ─────────────────────────────────────────────────────

    /** Trigger a manual sync of all unsent data. */
    Q_INVOKABLE void syncNow();

    /** Sync only workouts since last sync. */
    Q_INVOKABLE void syncWorkouts();

    /** Sync step/HR/sleep activity data. */
    Q_INVOKABLE void syncActivityData();

    /** Count pending (unsent) items. */
    Q_INVOKABLE int countPendingItems();

    // ── Configuration ───────────────────────────────────────────────────

    /** Enable/disable auto-sync on BLE connection. */
    Q_INVOKABLE void setAutoSync(bool enabled);

    /** Set sync interval in minutes (default: 30). */
    Q_INVOKABLE void setSyncInterval(int minutes);

signals:
    void connectionChanged();
    void syncingChanged();
    void lastSyncTimeChanged();
    void pendingItemsChanged();
    void syncComplete(int itemsSynced);
    void syncError(const QString &error);

private slots:
    void onBleConnectionChanged(bool connected);
    void onBleDataReceived(const QByteArray &data);
    void onAutoSyncTimer();

private:
    // ── Data serialization ──────────────────────────────────────────────

    /** Serialize step data for Gadgetbridge format. */
    QByteArray serializeStepData(const QDate &from, const QDate &to);

    /** Serialize HR data. */
    QByteArray serializeHeartRateData(const QDate &from, const QDate &to);

    /** Serialize sleep data. */
    QByteArray serializeSleepData(const QDate &from, const QDate &to);

    /** Send data via AsteroidOS BLE characteristic. */
    bool sendViaBle(const QByteArray &data, const QString &characteristicUuid);

    /** Parse incoming Gadgetbridge commands. */
    void handleGadgetbridgeCommand(const QByteArray &data);

    // ── D-Bus helpers ───────────────────────────────────────────────────

    QDBusInterface *bleInterface(const QString &iface);

    // ── State ───────────────────────────────────────────────────────────

    Database *m_database;
    ExportManager *m_exportManager;

    QDBusConnection m_sessionBus;
    QDBusInterface *m_bleService;

    bool m_connected;
    bool m_syncing;
    bool m_autoSync;
    QString m_lastSyncTime;
    int m_pendingItems;
    qint64 m_lastSyncEpoch;   // epoch seconds of last successful sync

    QTimer *m_autoSyncTimer;
    int m_syncIntervalMin;
};

#endif // GADGETBRIDGESYNC_H
