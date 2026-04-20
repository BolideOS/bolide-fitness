/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "bletransport.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDBusReply>
#include <QDBusObjectPath>
#include <QTemporaryFile>
#include <QVariantMap>

// ═══════════════════════════════════════════════════════════════════════════
// FileTransport
// ═══════════════════════════════════════════════════════════════════════════

FileTransport::FileTransport()
{
    m_exportDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                  + QStringLiteral("/exports");
}

bool FileTransport::send(const QByteArray &data,
                          const QString &filename,
                          const QString &mimeType)
{
    Q_UNUSED(mimeType);

    QDir().mkpath(m_exportDir);

    QString path = m_exportDir + QStringLiteral("/") + filename;

    // Avoid overwriting: append counter if file exists
    if (QFile::exists(path)) {
        QFileInfo fi(path);
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix();
        int counter = 1;
        while (QFile::exists(path)) {
            path = m_exportDir + QStringLiteral("/") + base
                   + QStringLiteral("_") + QString::number(counter++)
                   + QStringLiteral(".") + ext;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    qint64 written = file.write(data);
    file.close();

    if (written != data.size())
        return false;

    m_lastPath = path;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// BleTransport
// ═══════════════════════════════════════════════════════════════════════════

BleTransport::BleTransport()
    : m_obexClient(nullptr)
    , m_sessionBus(QDBusConnection::sessionBus())
{
    // Connect to BlueZ OBEX client
    m_obexClient = new QDBusInterface(
        QStringLiteral("org.bluez.obex"),
        QStringLiteral("/org/bluez/obex"),
        QStringLiteral("org.bluez.obex.Client1"),
        m_sessionBus);
}

BleTransport::~BleTransport()
{
    destroySession();
    delete m_obexClient;
}

bool BleTransport::isAvailable() const
{
    if (!m_obexClient || !m_obexClient->isValid())
        return false;

    // Check if we have a target device
    return !m_targetDevice.isEmpty();
}

bool BleTransport::send(const QByteArray &data,
                         const QString &filename,
                         const QString &mimeType)
{
    Q_UNUSED(mimeType);

    if (!isAvailable())
        return false;

    // Write data to a temporary file (OBEX needs a file path)
    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false); // We'll clean up after transfer
    if (!tmpFile.open())
        return false;

    tmpFile.write(data);
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    // Create OBEX session to target device
    if (!createSession()) {
        QFile::remove(tmpPath);
        return false;
    }

    // Push file via OBEX Object Push
    QDBusInterface push(
        QStringLiteral("org.bluez.obex"),
        m_sessionPath,
        QStringLiteral("org.bluez.obex.ObjectPush1"),
        m_sessionBus);

    if (!push.isValid()) {
        QFile::remove(tmpPath);
        destroySession();
        return false;
    }

    QDBusReply<void> reply = push.call(
        QStringLiteral("SendFile"), tmpPath);

    bool ok = reply.isValid();

    // Cleanup
    QFile::remove(tmpPath);
    destroySession();

    return ok;
}

QStringList BleTransport::pairedDevices() const
{
    QStringList devices;

    // Query BlueZ for paired devices
    QDBusInterface manager(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QDBusConnection::systemBus());

    if (!manager.isValid())
        return devices;

    QDBusReply<QVariantMap> reply = manager.call(QStringLiteral("GetManagedObjects"));
    if (!reply.isValid())
        return devices;

    // Parse managed objects for devices with Paired=true
    QVariantMap objects = reply.value();
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        QVariantMap interfaces = it.value().toMap();
        QVariantMap dev = interfaces.value(QStringLiteral("org.bluez.Device1")).toMap();
        if (!dev.isEmpty() && dev.value(QStringLiteral("Paired")).toBool()) {
            QString addr = dev.value(QStringLiteral("Address")).toString();
            QString name = dev.value(QStringLiteral("Name")).toString();
            devices.append(addr + QStringLiteral(" (") + name + QStringLiteral(")"));
        }
    }

    return devices;
}

bool BleTransport::createSession()
{
    if (!m_obexClient || !m_obexClient->isValid())
        return false;

    QVariantMap args;
    args[QStringLiteral("Target")] = QStringLiteral("opp"); // Object Push Profile

    QDBusReply<QDBusObjectPath> reply = m_obexClient->call(
        QStringLiteral("CreateSession"), m_targetDevice, args);

    if (!reply.isValid())
        return false;

    m_sessionPath = reply.value().path();
    return true;
}

void BleTransport::destroySession()
{
    if (m_sessionPath.isEmpty() || !m_obexClient)
        return;

    m_obexClient->call(QStringLiteral("RemoveSession"),
                       QDBusObjectPath(m_sessionPath));
    m_sessionPath.clear();
}
