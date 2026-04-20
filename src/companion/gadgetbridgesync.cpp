/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "gadgetbridgesync.h"
#include "core/database.h"
#include "export/exportmanager.h"

#include <QDBusReply>
#include <QDateTime>
#include <QDate>
#include <QDataStream>
#include <QSettings>

// AsteroidOS BLE D-Bus service names
static const char *BLE_SERVICE      = "org.asteroidos.ble";
static const char *BLE_SERVICE_PATH = "/org/asteroidos/ble";
static const char *BLE_IFACE        = "org.asteroidos.ble.Connection";

// Gadgetbridge custom UUIDs for data transfer
static const QString DATA_TX_UUID = QStringLiteral("00000001-0000-1000-8000-00805f9b34fb");

// Settings keys
static const char *SETTINGS_GROUP     = "gadgetbridge";
static const char *SETTINGS_LAST_SYNC = "last_sync_epoch";
static const char *SETTINGS_AUTO_SYNC = "auto_sync";
static const char *SETTINGS_INTERVAL  = "sync_interval_min";

GadgetbridgeSync::GadgetbridgeSync(QObject *parent)
    : QObject(parent)
    , m_database(nullptr)
    , m_exportManager(nullptr)
    , m_sessionBus(QDBusConnection::sessionBus())
    , m_bleService(nullptr)
    , m_connected(false)
    , m_syncing(false)
    , m_autoSync(true)
    , m_pendingItems(0)
    , m_lastSyncEpoch(0)
    , m_autoSyncTimer(new QTimer(this))
    , m_syncIntervalMin(30)
{
    // Load saved settings
    QSettings settings;
    settings.beginGroup(QLatin1String(SETTINGS_GROUP));
    m_lastSyncEpoch  = settings.value(QLatin1String(SETTINGS_LAST_SYNC), 0).toLongLong();
    m_autoSync        = settings.value(QLatin1String(SETTINGS_AUTO_SYNC), true).toBool();
    m_syncIntervalMin = settings.value(QLatin1String(SETTINGS_INTERVAL), 30).toInt();
    settings.endGroup();

    if (m_lastSyncEpoch > 0) {
        m_lastSyncTime = QDateTime::fromSecsSinceEpoch(m_lastSyncEpoch)
                         .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    }

    m_autoSyncTimer->setInterval(m_syncIntervalMin * 60 * 1000);
    connect(m_autoSyncTimer, &QTimer::timeout, this, &GadgetbridgeSync::onAutoSyncTimer);
}

GadgetbridgeSync::~GadgetbridgeSync()
{
    delete m_bleService;
}

void GadgetbridgeSync::setDatabase(Database *db)
{
    m_database = db;
    countPendingItems();
}

void GadgetbridgeSync::setExportManager(ExportManager *em)
{
    m_exportManager = em;
}

bool GadgetbridgeSync::initialize()
{
    m_bleService = new QDBusInterface(
        QLatin1String(BLE_SERVICE),
        QLatin1String(BLE_SERVICE_PATH),
        QLatin1String(BLE_IFACE),
        m_sessionBus);

    if (!m_bleService->isValid()) {
        qWarning("GadgetbridgeSync: AsteroidOS BLE service not available");
        delete m_bleService;
        m_bleService = nullptr;
        return false;
    }

    // Connect to BLE connection state changes
    m_sessionBus.connect(
        QLatin1String(BLE_SERVICE),
        QLatin1String(BLE_SERVICE_PATH),
        QLatin1String(BLE_IFACE),
        QStringLiteral("ConnectionChanged"),
        this, SLOT(onBleConnectionChanged(bool)));

    // Connect to incoming data
    m_sessionBus.connect(
        QLatin1String(BLE_SERVICE),
        QLatin1String(BLE_SERVICE_PATH),
        QLatin1String(BLE_IFACE),
        QStringLiteral("DataReceived"),
        this, SLOT(onBleDataReceived(QByteArray)));

    if (m_autoSync)
        m_autoSyncTimer->start();

    return true;
}

// ── Manual sync ─────────────────────────────────────────────────────────────

void GadgetbridgeSync::syncNow()
{
    if (m_syncing)
        return;

    m_syncing = true;
    emit syncingChanged();

    int total = 0;

    // Sync steps
    {
        QDate from = (m_lastSyncEpoch > 0)
            ? QDateTime::fromSecsSinceEpoch(m_lastSyncEpoch).date()
            : QDate::currentDate().addDays(-7);
        QDate to = QDate::currentDate();

        QByteArray stepData = serializeStepData(from, to);
        if (!stepData.isEmpty() && sendViaBle(stepData, DATA_TX_UUID))
            total += stepData.size();
    }

    // Sync HR
    {
        QDate from = (m_lastSyncEpoch > 0)
            ? QDateTime::fromSecsSinceEpoch(m_lastSyncEpoch).date()
            : QDate::currentDate().addDays(-7);
        QDate to = QDate::currentDate();

        QByteArray hrData = serializeHeartRateData(from, to);
        if (!hrData.isEmpty() && sendViaBle(hrData, DATA_TX_UUID))
            total += hrData.size();
    }

    // Sync sleep
    {
        QDate from = (m_lastSyncEpoch > 0)
            ? QDateTime::fromSecsSinceEpoch(m_lastSyncEpoch).date()
            : QDate::currentDate().addDays(-7);
        QDate to = QDate::currentDate();

        QByteArray sleepData = serializeSleepData(from, to);
        if (!sleepData.isEmpty() && sendViaBle(sleepData, DATA_TX_UUID))
            total += sleepData.size();
    }

    // Update last sync time
    m_lastSyncEpoch = QDateTime::currentSecsSinceEpoch();
    m_lastSyncTime = QDateTime::fromSecsSinceEpoch(m_lastSyncEpoch)
                     .toString(QStringLiteral("yyyy-MM-dd HH:mm"));

    QSettings settings;
    settings.beginGroup(QLatin1String(SETTINGS_GROUP));
    settings.setValue(QLatin1String(SETTINGS_LAST_SYNC), m_lastSyncEpoch);
    settings.endGroup();

    m_syncing = false;
    m_pendingItems = 0;

    emit syncingChanged();
    emit lastSyncTimeChanged();
    emit pendingItemsChanged();
    emit syncComplete(total);
}

void GadgetbridgeSync::syncWorkouts()
{
    if (!m_database || !m_exportManager)
        return;

    // Get workouts since last sync
    QVariantList workouts = m_database->recentWorkouts(50);
    for (const QVariant &v : workouts) {
        QVariantMap w = v.toMap();
        qint64 startTime = w.value(QStringLiteral("startTime")).toLongLong();
        if (startTime <= m_lastSyncEpoch)
            continue;

        // Export as FIT and send
        qint64 id = w.value(QStringLiteral("id")).toLongLong();
        QByteArray fitData = m_exportManager->exportBytes(id, QStringLiteral("fit"));
        if (!fitData.isEmpty())
            sendViaBle(fitData, DATA_TX_UUID);
    }
}

void GadgetbridgeSync::syncActivityData()
{
    QDate from = (m_lastSyncEpoch > 0)
        ? QDateTime::fromSecsSinceEpoch(m_lastSyncEpoch).date()
        : QDate::currentDate().addDays(-7);
    QDate to = QDate::currentDate();

    QByteArray data;
    data.append(serializeStepData(from, to));
    data.append(serializeHeartRateData(from, to));

    if (!data.isEmpty())
        sendViaBle(data, DATA_TX_UUID);
}

int GadgetbridgeSync::countPendingItems()
{
    if (!m_database) {
        m_pendingItems = 0;
        return 0;
    }

    // Count daily metrics entries since last sync
    QDate from = (m_lastSyncEpoch > 0)
        ? QDateTime::fromSecsSinceEpoch(m_lastSyncEpoch).date()
        : QDate::currentDate().addDays(-30);
    QDate to = QDate::currentDate();

    QVariantList steps = m_database->getDailyMetrics(QStringLiteral("daily_steps"), from, to);
    m_pendingItems = steps.size();
    emit pendingItemsChanged();
    return m_pendingItems;
}

// ── Configuration ───────────────────────────────────────────────────────────

void GadgetbridgeSync::setAutoSync(bool enabled)
{
    m_autoSync = enabled;

    QSettings settings;
    settings.beginGroup(QLatin1String(SETTINGS_GROUP));
    settings.setValue(QLatin1String(SETTINGS_AUTO_SYNC), enabled);
    settings.endGroup();

    if (enabled && m_connected)
        m_autoSyncTimer->start();
    else
        m_autoSyncTimer->stop();
}

void GadgetbridgeSync::setSyncInterval(int minutes)
{
    m_syncIntervalMin = qMax(5, minutes);
    m_autoSyncTimer->setInterval(m_syncIntervalMin * 60 * 1000);

    QSettings settings;
    settings.beginGroup(QLatin1String(SETTINGS_GROUP));
    settings.setValue(QLatin1String(SETTINGS_INTERVAL), m_syncIntervalMin);
    settings.endGroup();
}

// ── Private slots ───────────────────────────────────────────────────────────

void GadgetbridgeSync::onBleConnectionChanged(bool connected)
{
    m_connected = connected;
    emit connectionChanged();

    if (connected && m_autoSync) {
        // Auto-sync on connection
        QTimer::singleShot(2000, this, &GadgetbridgeSync::syncNow);
        m_autoSyncTimer->start();
    } else {
        m_autoSyncTimer->stop();
    }
}

void GadgetbridgeSync::onBleDataReceived(const QByteArray &data)
{
    handleGadgetbridgeCommand(data);
}

void GadgetbridgeSync::onAutoSyncTimer()
{
    if (m_connected && !m_syncing)
        syncNow();
}

// ── Data serialization ──────────────────────────────────────────────────────

QByteArray GadgetbridgeSync::serializeStepData(const QDate &from, const QDate &to)
{
    if (!m_database)
        return QByteArray();

    QVariantList steps = m_database->getDailyMetrics(QStringLiteral("daily_steps"), from, to);
    if (steps.isEmpty())
        return QByteArray();

    // Simple binary format: [type:1][count:2][entries...]
    // Each entry: [timestamp:4][steps:4]
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << quint8(0x01);  // Type: steps
    stream << quint16(steps.size());

    for (const QVariant &v : steps) {
        QVariantMap row = v.toMap();
        QDate d = QDate::fromString(row[QStringLiteral("date")].toString(), Qt::ISODate);
        qint64 epoch = QDateTime(d, QTime(12, 0), Qt::UTC).toSecsSinceEpoch();
        quint32 value = static_cast<quint32>(row[QStringLiteral("value")].toDouble());

        stream << static_cast<quint32>(epoch);
        stream << value;
    }

    return data;
}

QByteArray GadgetbridgeSync::serializeHeartRateData(const QDate &from, const QDate &to)
{
    if (!m_database)
        return QByteArray();

    QVariantList hrs = m_database->getDailyMetrics(QStringLiteral("resting_hr"), from, to);
    if (hrs.isEmpty())
        return QByteArray();

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << quint8(0x02);  // Type: heart rate
    stream << quint16(hrs.size());

    for (const QVariant &v : hrs) {
        QVariantMap row = v.toMap();
        QDate d = QDate::fromString(row[QStringLiteral("date")].toString(), Qt::ISODate);
        qint64 epoch = QDateTime(d, QTime(12, 0), Qt::UTC).toSecsSinceEpoch();
        quint8 hr = static_cast<quint8>(row[QStringLiteral("value")].toDouble());

        stream << static_cast<quint32>(epoch);
        stream << hr;
    }

    return data;
}

QByteArray GadgetbridgeSync::serializeSleepData(const QDate &from, const QDate &to)
{
    if (!m_database)
        return QByteArray();

    int days = from.daysTo(to);
    QVariantList sleepHistory = m_database->getSleepHistory(days);
    if (sleepHistory.isEmpty())
        return QByteArray();

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << quint8(0x03);  // Type: sleep
    stream << quint16(sleepHistory.size());

    for (const QVariant &v : sleepHistory) {
        QVariantMap row = v.toMap();
        quint32 start   = static_cast<quint32>(row[QStringLiteral("start_time")].toLongLong());
        quint32 end     = static_cast<quint32>(row[QStringLiteral("end_time")].toLongLong());
        quint8 quality  = static_cast<quint8>(row[QStringLiteral("quality_score")].toInt());
        quint16 deepMin = static_cast<quint16>(row[QStringLiteral("deep_minutes")].toInt());
        quint16 remMin  = static_cast<quint16>(row[QStringLiteral("rem_minutes")].toInt());

        stream << start << end << quality << deepMin << remMin;
    }

    return data;
}

bool GadgetbridgeSync::sendViaBle(const QByteArray &data, const QString &characteristicUuid)
{
    if (!m_bleService || !m_connected)
        return false;

    // Send via AsteroidOS BLE characteristic write
    QDBusReply<bool> reply = m_bleService->call(
        QStringLiteral("WriteCharacteristic"),
        characteristicUuid,
        data);

    return reply.isValid() && reply.value();
}

void GadgetbridgeSync::handleGadgetbridgeCommand(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    quint8 cmdType = static_cast<quint8>(data[0]);

    switch (cmdType) {
    case 0x10:  // Sync request
        syncNow();
        break;

    case 0x11:  // Time sync
        // Gadgetbridge sends current phone time — we can use this
        // to validate/correct watch time
        break;

    case 0x12:  // Request workout list
        syncWorkouts();
        break;

    case 0x20:  // Sync acknowledgment
        // Mark items as synced
        break;

    default:
        qDebug("GadgetbridgeSync: Unknown command 0x%02x", cmdType);
        break;
    }
}

QDBusInterface *GadgetbridgeSync::bleInterface(const QString &iface)
{
    return new QDBusInterface(
        QLatin1String(BLE_SERVICE),
        QLatin1String(BLE_SERVICE_PATH),
        iface,
        m_sessionBus,
        this);
}
