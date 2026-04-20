/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "tcxexporter.h"
#include <QXmlStreamWriter>
#include <QDateTime>
#include <QBuffer>

QString TcxExporter::mapWorkoutType(const QString &type)
{
    // TCX sport types: Running, Biking, Other
    if (type.contains(QStringLiteral("run"), Qt::CaseInsensitive))
        return QStringLiteral("Running");
    if (type.contains(QStringLiteral("cycl"), Qt::CaseInsensitive) ||
        type.contains(QStringLiteral("bik"), Qt::CaseInsensitive))
        return QStringLiteral("Biking");
    return QStringLiteral("Other");
}

QString TcxExporter::isoTimestamp(qint64 epochSecs)
{
    return QDateTime::fromSecsSinceEpoch(epochSecs, Qt::UTC)
           .toString(Qt::ISODate);
}

QByteArray TcxExporter::exportWorkout(const QVariantMap &workoutData,
                                       const QVariantList &gpsTrack,
                                       const QVariantMap &metrics)
{
    Q_UNUSED(metrics);

    if (workoutData.isEmpty())
        return QByteArray();

    qint64 startEpoch = workoutData.value(QStringLiteral("startTime")).toLongLong();
    int durationS     = workoutData.value(QStringLiteral("duration")).toInt();
    double distM      = workoutData.value(QStringLiteral("distance")).toDouble();
    int avgHR         = workoutData.value(QStringLiteral("avgHR")).toInt();
    int maxHR         = workoutData.value(QStringLiteral("maxHR")).toInt();
    int calories      = workoutData.value(QStringLiteral("calories")).toInt();
    QString type      = workoutData.value(QStringLiteral("type")).toString();
    QString sport     = mapWorkoutType(type);

    QByteArray output;
    QBuffer buffer(&output);
    buffer.open(QIODevice::WriteOnly);

    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument(QStringLiteral("1.0"), true);

    // Root element
    xml.writeStartElement(QStringLiteral("TrainingCenterDatabase"));
    xml.writeDefaultNamespace(QStringLiteral("http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2"));
    xml.writeNamespace(QStringLiteral("http://www.w3.org/2001/XMLSchema-instance"), QStringLiteral("xsi"));

    xml.writeStartElement(QStringLiteral("Activities"));
    xml.writeStartElement(QStringLiteral("Activity"));
    xml.writeAttribute(QStringLiteral("Sport"), sport);

    xml.writeTextElement(QStringLiteral("Id"), isoTimestamp(startEpoch));

    // Single Lap
    xml.writeStartElement(QStringLiteral("Lap"));
    xml.writeAttribute(QStringLiteral("StartTime"), isoTimestamp(startEpoch));

    xml.writeTextElement(QStringLiteral("TotalTimeSeconds"), QString::number(durationS));
    xml.writeTextElement(QStringLiteral("DistanceMeters"), QString::number(distM, 'f', 1));
    xml.writeTextElement(QStringLiteral("Calories"), QString::number(calories));

    // Average HR
    if (avgHR > 0) {
        xml.writeStartElement(QStringLiteral("AverageHeartRateBpm"));
        xml.writeTextElement(QStringLiteral("Value"), QString::number(avgHR));
        xml.writeEndElement();
    }

    // Max HR
    if (maxHR > 0) {
        xml.writeStartElement(QStringLiteral("MaximumHeartRateBpm"));
        xml.writeTextElement(QStringLiteral("Value"), QString::number(maxHR));
        xml.writeEndElement();
    }

    xml.writeTextElement(QStringLiteral("Intensity"), QStringLiteral("Active"));
    xml.writeTextElement(QStringLiteral("TriggerMethod"), QStringLiteral("Manual"));

    // Track
    xml.writeStartElement(QStringLiteral("Track"));

    for (const QVariant &pt : gpsTrack) {
        QVariantMap p = pt.toMap();
        qint64 ts    = p.value(QStringLiteral("timestamp")).toLongLong();
        double lat   = p.value(QStringLiteral("latitude")).toDouble();
        double lon   = p.value(QStringLiteral("longitude")).toDouble();
        double alt   = p.value(QStringLiteral("altitude")).toDouble();
        int hr       = p.value(QStringLiteral("heartRate"), 0).toInt();
        double dist  = p.value(QStringLiteral("cumulativeDistance"), 0.0).toDouble();

        xml.writeStartElement(QStringLiteral("Trackpoint"));
        xml.writeTextElement(QStringLiteral("Time"), isoTimestamp(ts));

        if (lat != 0.0 || lon != 0.0) {
            xml.writeStartElement(QStringLiteral("Position"));
            xml.writeTextElement(QStringLiteral("LatitudeDegrees"), QString::number(lat, 'f', 7));
            xml.writeTextElement(QStringLiteral("LongitudeDegrees"), QString::number(lon, 'f', 7));
            xml.writeEndElement(); // Position
        }

        xml.writeTextElement(QStringLiteral("AltitudeMeters"), QString::number(alt, 'f', 1));
        xml.writeTextElement(QStringLiteral("DistanceMeters"), QString::number(dist, 'f', 1));

        if (hr > 0) {
            xml.writeStartElement(QStringLiteral("HeartRateBpm"));
            xml.writeTextElement(QStringLiteral("Value"), QString::number(hr));
            xml.writeEndElement();
        }

        xml.writeEndElement(); // Trackpoint
    }

    xml.writeEndElement(); // Track
    xml.writeEndElement(); // Lap
    xml.writeEndElement(); // Activity
    xml.writeEndElement(); // Activities

    // Creator
    xml.writeStartElement(QStringLiteral("Author"));
    xml.writeAttribute(QStringLiteral("xsi:type"), QStringLiteral("Application_t"));
    xml.writeTextElement(QStringLiteral("Name"), QStringLiteral("Bolide Fitness"));
    xml.writeEndElement(); // Author

    xml.writeEndElement(); // TrainingCenterDatabase
    xml.writeEndDocument();

    buffer.close();
    return output;
}
