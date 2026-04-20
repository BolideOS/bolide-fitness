/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "gpxexporter.h"
#include <QXmlStreamWriter>
#include <QDateTime>
#include <QBuffer>

QString GpxExporter::isoTimestamp(qint64 epochSecs)
{
    return QDateTime::fromSecsSinceEpoch(epochSecs, Qt::UTC)
           .toString(Qt::ISODate);
}

QByteArray GpxExporter::exportWorkout(const QVariantMap &workoutData,
                                       const QVariantList &gpsTrack,
                                       const QVariantMap &metrics)
{
    Q_UNUSED(metrics);

    if (workoutData.isEmpty())
        return QByteArray();

    qint64 startEpoch = workoutData.value(QStringLiteral("startTime")).toLongLong();
    QString type      = workoutData.value(QStringLiteral("type")).toString();

    QByteArray output;
    QBuffer buffer(&output);
    buffer.open(QIODevice::WriteOnly);

    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument(QStringLiteral("1.0"), true);

    // Root element: <gpx>
    xml.writeStartElement(QStringLiteral("gpx"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.1"));
    xml.writeAttribute(QStringLiteral("creator"), QStringLiteral("Bolide Fitness"));
    xml.writeDefaultNamespace(QStringLiteral("http://www.topografix.com/GPX/1/1"));
    xml.writeNamespace(QStringLiteral("http://www.garmin.com/xmlschemas/TrackPointExtension/v1"),
                       QStringLiteral("gpxtpx"));
    xml.writeNamespace(QStringLiteral("http://www.w3.org/2001/XMLSchema-instance"),
                       QStringLiteral("xsi"));

    // Metadata
    xml.writeStartElement(QStringLiteral("metadata"));
    xml.writeTextElement(QStringLiteral("name"),
                         type + QStringLiteral(" - ") + isoTimestamp(startEpoch));
    xml.writeTextElement(QStringLiteral("time"), isoTimestamp(startEpoch));
    xml.writeEndElement(); // metadata

    // Track
    xml.writeStartElement(QStringLiteral("trk"));
    xml.writeTextElement(QStringLiteral("name"), type);
    xml.writeTextElement(QStringLiteral("type"), type);

    xml.writeStartElement(QStringLiteral("trkseg"));

    for (const QVariant &pt : gpsTrack) {
        QVariantMap p = pt.toMap();
        double lat   = p.value(QStringLiteral("latitude")).toDouble();
        double lon   = p.value(QStringLiteral("longitude")).toDouble();
        double alt   = p.value(QStringLiteral("altitude")).toDouble();
        qint64 ts    = p.value(QStringLiteral("timestamp")).toLongLong();
        int hr       = p.value(QStringLiteral("heartRate"), 0).toInt();

        xml.writeStartElement(QStringLiteral("trkpt"));
        xml.writeAttribute(QStringLiteral("lat"), QString::number(lat, 'f', 7));
        xml.writeAttribute(QStringLiteral("lon"), QString::number(lon, 'f', 7));

        xml.writeTextElement(QStringLiteral("ele"), QString::number(alt, 'f', 1));
        xml.writeTextElement(QStringLiteral("time"), isoTimestamp(ts));

        // Heart rate via Garmin extension
        if (hr > 0) {
            xml.writeStartElement(QStringLiteral("extensions"));
            xml.writeStartElement(QStringLiteral("gpxtpx:TrackPointExtension"));
            xml.writeTextElement(QStringLiteral("gpxtpx:hr"), QString::number(hr));
            xml.writeEndElement(); // TrackPointExtension
            xml.writeEndElement(); // extensions
        }

        xml.writeEndElement(); // trkpt
    }

    xml.writeEndElement(); // trkseg
    xml.writeEndElement(); // trk
    xml.writeEndElement(); // gpx
    xml.writeEndDocument();

    buffer.close();
    return output;
}
