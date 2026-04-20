/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * TCX (Training Center XML) format exporter.
 */

#ifndef TCXEXPORTER_H
#define TCXEXPORTER_H

#include "exportplugin.h"

/**
 * @brief Exports workouts to Garmin TCX (Training Center XML) format.
 *
 * TCX is a widely supported XML format used by Garmin Connect, Strava,
 * and other fitness platforms. Supports activities with laps, track
 * points (position, HR, altitude, time), and summary statistics.
 */
class TcxExporter : public ExportPlugin
{
public:
    TcxExporter() = default;
    ~TcxExporter() override = default;

    QString formatId() const override { return QStringLiteral("tcx"); }
    QString formatName() const override { return QStringLiteral("Garmin TCX"); }
    QString fileExtension() const override { return QStringLiteral(".tcx"); }
    QString mimeType() const override { return QStringLiteral("application/vnd.garmin.tcx+xml"); }

    QByteArray exportWorkout(const QVariantMap &workoutData,
                             const QVariantList &gpsTrack,
                             const QVariantMap &metrics) override;

private:
    static QString mapWorkoutType(const QString &type);
    static QString isoTimestamp(qint64 epochSecs);
};

#endif // TCXEXPORTER_H
