/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BLE and file export transports.
 */

#ifndef BLETRANSPORT_H
#define BLETRANSPORT_H

#include "exportplugin.h"
#include <QDBusInterface>
#include <QDBusConnection>

/**
 * @brief File-based export transport.
 *
 * Writes exported data to the local filesystem.
 * Default export directory: ~/bolide-fitness/exports/
 */
class FileTransport : public ExportTransport
{
public:
    FileTransport();
    ~FileTransport() override = default;

    QString transportId() const override { return QStringLiteral("file"); }
    QString transportName() const override { return QStringLiteral("Save to File"); }
    bool isAvailable() const override { return true; }

    bool send(const QByteArray &data,
              const QString &filename,
              const QString &mimeType) override;

    /** Get the last written file path. */
    QString lastFilePath() const { return m_lastPath; }

    /** Set the export directory. */
    void setExportDir(const QString &dir) { m_exportDir = dir; }

private:
    QString m_exportDir;
    QString m_lastPath;
};

/**
 * @brief BLE OBEX Object Push transport.
 *
 * Uses BlueZ OBEX D-Bus API to push files to paired devices.
 * Requires a paired phone/computer with OBEX Object Push support.
 *
 * D-Bus path: org.bluez.obex → /org/bluez/obex
 * Interface: org.bluez.obex.Client1
 */
class BleTransport : public ExportTransport
{
public:
    BleTransport();
    ~BleTransport() override;

    QString transportId() const override { return QStringLiteral("ble"); }
    QString transportName() const override { return QStringLiteral("Bluetooth"); }
    bool isAvailable() const override;

    bool send(const QByteArray &data,
              const QString &filename,
              const QString &mimeType) override;

    /** Set the target Bluetooth device address. */
    void setTargetDevice(const QString &address) { m_targetDevice = address; }

    /** Get list of paired devices. */
    QStringList pairedDevices() const;

private:
    bool createSession();
    void destroySession();

    QDBusInterface *m_obexClient;
    QDBusConnection m_sessionBus;
    QString m_targetDevice;
    QString m_sessionPath;
};

#endif // BLETRANSPORT_H
