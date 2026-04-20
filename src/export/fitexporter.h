/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FIT format exporter.
 * Produces Garmin FIT binary files from workout data.
 * Implements the ANT+ FIT SDK protocol structures.
 */

#ifndef FITEXPORTER_H
#define FITEXPORTER_H

#include "exportplugin.h"

/**
 * @brief Exports workouts to Garmin FIT binary format.
 *
 * FIT (Flexible and Interoperable Data Transfer) is the de facto
 * standard for fitness device data exchange. This exporter produces
 * minimal valid FIT files containing:
 * - File ID message
 * - Activity message
 * - Session message (summary stats)
 * - Record messages (per-second data points with HR, position, altitude)
 * - Lap message
 *
 * The output is compatible with Garmin Connect, Strava, TrainingPeaks.
 */
class FitExporter : public ExportPlugin
{
public:
    FitExporter() = default;
    ~FitExporter() override = default;

    QString formatId() const override { return QStringLiteral("fit"); }
    QString formatName() const override { return QStringLiteral("Garmin FIT"); }
    QString fileExtension() const override { return QStringLiteral(".fit"); }
    QString mimeType() const override { return QStringLiteral("application/vnd.ant.fit"); }

    QByteArray exportWorkout(const QVariantMap &workoutData,
                             const QVariantList &gpsTrack,
                             const QVariantMap &metrics) override;

private:
    // FIT protocol helpers
    QByteArray buildFileHeader(quint32 dataSize);
    QByteArray buildFileId(quint32 timestamp);
    QByteArray buildActivity(quint32 timestamp, quint32 totalTimerTime, int numSessions);
    QByteArray buildSession(const QVariantMap &workout, quint32 startTime);
    QByteArray buildLap(const QVariantMap &workout, quint32 startTime);
    QByteArray buildRecord(quint32 timestamp, qint32 lat, qint32 lon,
                           quint16 altitude, quint8 heartRate, quint16 speed);

    static quint16 crc16(const QByteArray &data, quint16 crc = 0);
    static quint32 toFitTimestamp(qint64 epochSecs);
    static qint32 toSemicircles(double degrees);
    static quint16 toFitAltitude(double meters);
    static quint16 toFitSpeed(double metersPerSec);

    void writeByte(QByteArray &buf, quint8 val);
    void writeUint16LE(QByteArray &buf, quint16 val);
    void writeUint32LE(QByteArray &buf, quint32 val);
    void writeSint32LE(QByteArray &buf, qint32 val);
};

#endif // FITEXPORTER_H
