/*
 * Copyright (C) 2026 BolideOS Contributors
 * License: GPLv3
 *
 * Phase 3 stub: Export plugin interface.
 * Pluggable exporters for FIT, TCX, GPX formats.
 * Export transport: file-based, D-Bus API, BLE transfer.
 */

#ifndef EXPORTPLUGIN_H
#define EXPORTPLUGIN_H

#include <QString>
#include <QVariantMap>
#include <QByteArray>

class Database;

/**
 * @brief Abstract export format plugin.
 *
 * Implement for each format (FIT, TCX, GPX).
 * The export pipeline: DB workout -> ExportPlugin::exportWorkout() -> bytes -> transport
 */
class ExportPlugin
{
public:
    virtual ~ExportPlugin() = default;

    /** Format identifier (e.g., "fit", "tcx", "gpx"). */
    virtual QString formatId() const = 0;

    /** Human-readable format name. */
    virtual QString formatName() const = 0;

    /** File extension including dot (e.g., ".fit"). */
    virtual QString fileExtension() const = 0;

    /** MIME type for the export format. */
    virtual QString mimeType() const = 0;

    /**
     * Export a workout to the format's byte representation.
     * @param workoutData  Workout summary from Database::getWorkout()
     * @param gpsTrack     GPS track points
     * @param metrics      Computed metrics
     * @return Serialized bytes, or empty on failure
     */
    virtual QByteArray exportWorkout(const QVariantMap &workoutData,
                                     const QVariantList &gpsTrack,
                                     const QVariantMap &metrics) = 0;
};

/**
 * @brief Abstract export transport.
 *
 * Implement for each transport method (file, D-Bus, BLE).
 */
class ExportTransport
{
public:
    virtual ~ExportTransport() = default;

    virtual QString transportId() const = 0;
    virtual QString transportName() const = 0;
    virtual bool isAvailable() const = 0;

    /**
     * Send exported data via this transport.
     * @param data      Exported bytes
     * @param filename  Suggested filename
     * @param mimeType  MIME type of the data
     * @return true on success
     */
    virtual bool send(const QByteArray &data,
                      const QString &filename,
                      const QString &mimeType) = 0;
};

#endif // EXPORTPLUGIN_H
