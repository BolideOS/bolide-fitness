/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GPX (GPS Exchange Format) exporter.
 */

#ifndef GPXEXPORTER_H
#define GPXEXPORTER_H

#include "exportplugin.h"

/**
 * @brief Exports workouts to GPX (GPS Exchange Format) 1.1.
 *
 * GPX is a universal GPS data format supported by virtually every
 * mapping and fitness application. Outputs a track with segments
 * containing time-stamped track points with position, elevation,
 * and heart rate (via Garmin TrackPointExtension).
 */
class GpxExporter : public ExportPlugin
{
public:
    GpxExporter() = default;
    ~GpxExporter() override = default;

    QString formatId() const override { return QStringLiteral("gpx"); }
    QString formatName() const override { return QStringLiteral("GPX"); }
    QString fileExtension() const override { return QStringLiteral(".gpx"); }
    QString mimeType() const override { return QStringLiteral("application/gpx+xml"); }

    QByteArray exportWorkout(const QVariantMap &workoutData,
                             const QVariantList &gpsTrack,
                             const QVariantMap &metrics) override;

private:
    static QString isoTimestamp(qint64 epochSecs);
};

#endif // GPXEXPORTER_H
