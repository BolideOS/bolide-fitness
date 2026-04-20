/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "exportmanager.h"
#include "exportplugin.h"
#include "fitexporter.h"
#include "tcxexporter.h"
#include "gpxexporter.h"
#include "bletransport.h"
#include "core/database.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>

ExportManager::ExportManager(QObject *parent)
    : QObject(parent)
    , m_database(nullptr)
    , m_exporting(false)
    , m_progress(0.0)
{
}

ExportManager::~ExportManager()
{
    qDeleteAll(m_formats);
    qDeleteAll(m_transports);
}

void ExportManager::setDatabase(Database *db)
{
    m_database = db;
}

// ── Plugin registration ─────────────────────────────────────────────────────

void ExportManager::registerFormat(ExportPlugin *plugin)
{
    if (!plugin) return;

    // Delete old one if same ID
    if (m_formats.contains(plugin->formatId()))
        delete m_formats.take(plugin->formatId());

    m_formats.insert(plugin->formatId(), plugin);
}

void ExportManager::registerTransport(ExportTransport *transport)
{
    if (!transport) return;

    if (m_transports.contains(transport->transportId()))
        delete m_transports.take(transport->transportId());

    m_transports.insert(transport->transportId(), transport);
    emit transportsChanged();
}

void ExportManager::registerBuiltins()
{
    registerFormat(new FitExporter());
    registerFormat(new TcxExporter());
    registerFormat(new GpxExporter());
    registerTransport(new FileTransport());
    registerTransport(new BleTransport());
}

// ── QML API ─────────────────────────────────────────────────────────────────

QVariantList ExportManager::availableFormats() const
{
    QVariantList list;
    for (auto it = m_formats.constBegin(); it != m_formats.constEnd(); ++it) {
        QVariantMap fmt;
        fmt[QStringLiteral("id")]        = it.value()->formatId();
        fmt[QStringLiteral("name")]      = it.value()->formatName();
        fmt[QStringLiteral("extension")] = it.value()->fileExtension();
        fmt[QStringLiteral("mimeType")]  = it.value()->mimeType();
        list.append(fmt);
    }
    return list;
}

QVariantList ExportManager::availableTransports() const
{
    QVariantList list;
    for (auto it = m_transports.constBegin(); it != m_transports.constEnd(); ++it) {
        QVariantMap t;
        t[QStringLiteral("id")]        = it.value()->transportId();
        t[QStringLiteral("name")]      = it.value()->transportName();
        t[QStringLiteral("available")] = it.value()->isAvailable();
        list.append(t);
    }
    return list;
}

QByteArray ExportManager::generateExportData(qint64 workoutId, const QString &formatId)
{
    if (!m_database) {
        m_lastError = QStringLiteral("No database");
        emit lastErrorChanged();
        return QByteArray();
    }

    ExportPlugin *plugin = m_formats.value(formatId);
    if (!plugin) {
        m_lastError = QStringLiteral("Unknown format: ") + formatId;
        emit lastErrorChanged();
        return QByteArray();
    }

    // Gather workout data from database
    QVariantMap workoutData = m_database->getWorkout(workoutId);
    if (workoutData.isEmpty()) {
        m_lastError = QStringLiteral("Workout not found: ") + QString::number(workoutId);
        emit lastErrorChanged();
        return QByteArray();
    }

    // Get GPS track
    QVector<Database::GpsPoint> gpsPoints = m_database->getGpsTrack(workoutId);
    QVariantList gpsTrack;
    gpsTrack.reserve(gpsPoints.size());
    for (const auto &pt : gpsPoints) {
        QVariantMap p;
        p[QStringLiteral("timestamp")] = pt.timestamp;
        p[QStringLiteral("latitude")]  = pt.latitude;
        p[QStringLiteral("longitude")] = pt.longitude;
        p[QStringLiteral("altitude")]  = pt.altitude;
        p[QStringLiteral("speed")]     = pt.speed;
        gpsTrack.append(p);
    }

    // Get metrics
    QVariantList metricsList = m_database->getWorkoutMetrics(workoutId);
    QVariantMap metrics;
    for (const QVariant &v : metricsList) {
        QVariantMap m = v.toMap();
        metrics[m.value(QStringLiteral("metric_id")).toString()] = m.value(QStringLiteral("value"));
    }

    return plugin->exportWorkout(workoutData, gpsTrack, metrics);
}

bool ExportManager::exportWorkout(qint64 workoutId, const QString &formatId,
                                   const QString &transportId)
{
    if (m_exporting)
        return false;

    ExportTransport *transport = m_transports.value(transportId);
    if (!transport) {
        m_lastError = QStringLiteral("Unknown transport: ") + transportId;
        emit lastErrorChanged();
        return false;
    }

    if (!transport->isAvailable()) {
        m_lastError = QStringLiteral("Transport not available: ") + transportId;
        emit lastErrorChanged();
        return false;
    }

    m_exporting = true;
    m_progress = 0.0;
    emit exportingChanged();
    emit progressChanged();

    QByteArray data = generateExportData(workoutId, formatId);
    if (data.isEmpty()) {
        m_exporting = false;
        emit exportingChanged();
        emit exportFailed(m_lastError);
        return false;
    }

    m_progress = 0.5;
    emit progressChanged();

    ExportPlugin *plugin = m_formats.value(formatId);
    QString filename = suggestedFilename(workoutId, formatId);

    bool ok = transport->send(data, filename, plugin->mimeType());

    m_exporting = false;
    m_progress = ok ? 1.0 : 0.0;
    emit exportingChanged();
    emit progressChanged();

    if (ok) {
        emit exportComplete(formatId, transportId);
    } else {
        m_lastError = QStringLiteral("Transport send failed");
        emit lastErrorChanged();
        emit exportFailed(m_lastError);
    }

    return ok;
}

bool ExportManager::exportToFile(qint64 workoutId, const QString &formatId,
                                  const QString &filePath)
{
    QByteArray data = generateExportData(workoutId, formatId);
    if (data.isEmpty())
        return false;

    // Ensure directory exists
    QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QStringLiteral("Cannot write to: ") + filePath;
        emit lastErrorChanged();
        return false;
    }

    file.write(data);
    file.close();
    return true;
}

QByteArray ExportManager::exportBytes(qint64 workoutId, const QString &formatId)
{
    return generateExportData(workoutId, formatId);
}

QString ExportManager::suggestedFilename(qint64 workoutId, const QString &formatId)
{
    ExportPlugin *plugin = m_formats.value(formatId);
    if (!plugin)
        return QString();

    QVariantMap workout;
    if (m_database)
        workout = m_database->getWorkout(workoutId);

    QString type = workout.value(QStringLiteral("type"), QStringLiteral("workout")).toString();
    qint64 ts = workout.value(QStringLiteral("startTime"), QDateTime::currentSecsSinceEpoch()).toLongLong();
    QString dateStr = QDateTime::fromSecsSinceEpoch(ts, Qt::UTC).toString(QStringLiteral("yyyy-MM-dd_HHmmss"));

    return QStringLiteral("bolide_%1_%2%3").arg(type, dateStr, plugin->fileExtension());
}
