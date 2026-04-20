/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fitexporter.h"
#include <QDateTime>
#include <QVariantList>
#include <QtEndian>

/*
 * FIT file format reference:
 * - 14-byte file header
 * - Data records (definition + data messages)
 * - 2-byte CRC
 *
 * We emit a simplified but valid FIT file:
 *   FileId(0) → Activity(34) → Session(18) → Record(20)... → Lap(19)
 *
 * FIT epoch: 1989-12-31T00:00:00 UTC  (631065600 seconds before Unix epoch)
 */

static const quint32 FIT_EPOCH_OFFSET = 631065600;

// FIT global message numbers
static const quint16 MESG_FILE_ID   = 0;
static const quint16 MESG_SESSION   = 18;
static const quint16 MESG_LAP       = 19;
static const quint16 MESG_RECORD    = 20;
static const quint16 MESG_ACTIVITY  = 34;

// FIT base types
static const quint8 FIT_UINT8   = 0x00;
static const quint8 FIT_SINT32  = 0x85;
static const quint8 FIT_UINT16  = 0x84;
static const quint8 FIT_UINT32  = 0x86;
static const quint8 FIT_ENUM    = 0x00;
static const quint8 FIT_STRING  = 0x07;

// CRC lookup table
static const quint16 crc_table[16] = {
    0x0000, 0xCC01, 0xD801, 0x1400,
    0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401,
    0x5000, 0x9C01, 0x8801, 0x4400
};

quint16 FitExporter::crc16(const QByteArray &data, quint16 crc)
{
    for (int i = 0; i < data.size(); ++i) {
        quint8 byte = static_cast<quint8>(data[i]);
        quint16 tmp = crc_table[crc & 0xF];
        crc = (crc >> 4) & 0x0FFF;
        crc = crc ^ tmp ^ crc_table[byte & 0xF];
        tmp = crc_table[crc & 0xF];
        crc = (crc >> 4) & 0x0FFF;
        crc = crc ^ tmp ^ crc_table[(byte >> 4) & 0xF];
    }
    return crc;
}

quint32 FitExporter::toFitTimestamp(qint64 epochSecs)
{
    return static_cast<quint32>(epochSecs - FIT_EPOCH_OFFSET);
}

qint32 FitExporter::toSemicircles(double degrees)
{
    return static_cast<qint32>(degrees * (2147483648.0 / 180.0));
}

quint16 FitExporter::toFitAltitude(double meters)
{
    // FIT altitude: (value + 500) * 5, stored as uint16
    return static_cast<quint16>((meters + 500.0) * 5.0);
}

quint16 FitExporter::toFitSpeed(double metersPerSec)
{
    // FIT speed: value * 1000, stored as uint16
    return static_cast<quint16>(metersPerSec * 1000.0);
}

void FitExporter::writeByte(QByteArray &buf, quint8 val)
{
    buf.append(static_cast<char>(val));
}

void FitExporter::writeUint16LE(QByteArray &buf, quint16 val)
{
    buf.append(static_cast<char>(val & 0xFF));
    buf.append(static_cast<char>((val >> 8) & 0xFF));
}

void FitExporter::writeUint32LE(QByteArray &buf, quint32 val)
{
    buf.append(static_cast<char>(val & 0xFF));
    buf.append(static_cast<char>((val >> 8) & 0xFF));
    buf.append(static_cast<char>((val >> 16) & 0xFF));
    buf.append(static_cast<char>((val >> 24) & 0xFF));
}

void FitExporter::writeSint32LE(QByteArray &buf, qint32 val)
{
    writeUint32LE(buf, static_cast<quint32>(val));
}

QByteArray FitExporter::buildFileHeader(quint32 dataSize)
{
    QByteArray hdr;
    hdr.reserve(14);
    writeByte(hdr, 14);                 // Header size
    writeByte(hdr, 0x20);              // Protocol version 2.0
    writeUint16LE(hdr, 0x0814);        // Profile version 20.84
    writeUint32LE(hdr, dataSize);      // Data size (filled later)
    hdr.append(".FIT", 4);             // Data type signature
    writeUint16LE(hdr, crc16(hdr));    // Header CRC
    return hdr;
}

QByteArray FitExporter::buildFileId(quint32 timestamp)
{
    QByteArray msg;

    // Definition message for local message 0 (FileId)
    writeByte(msg, 0x40);               // Definition message, local 0
    writeByte(msg, 0);                  // Reserved
    writeByte(msg, 0);                  // Architecture: little-endian
    writeUint16LE(msg, MESG_FILE_ID);   // Global message number
    writeByte(msg, 4);                  // Number of fields

    // Field 0: type (enum, 1 byte) = 4 (activity)
    writeByte(msg, 0);   writeByte(msg, 1);   writeByte(msg, FIT_ENUM);
    // Field 1: manufacturer (uint16) = 255 (development)
    writeByte(msg, 1);   writeByte(msg, 2);   writeByte(msg, FIT_UINT16);
    // Field 3: serial_number (uint32)
    writeByte(msg, 3);   writeByte(msg, 4);   writeByte(msg, FIT_UINT32);
    // Field 4: time_created (uint32)
    writeByte(msg, 4);   writeByte(msg, 4);   writeByte(msg, FIT_UINT32);

    // Data message for local 0
    writeByte(msg, 0x00);               // Data message, local 0
    writeByte(msg, 4);                  // type = activity
    writeUint16LE(msg, 255);            // manufacturer = development
    writeUint32LE(msg, 0xBEEF);         // serial number
    writeUint32LE(msg, timestamp);

    return msg;
}

QByteArray FitExporter::buildActivity(quint32 timestamp, quint32 totalTimerTime, int numSessions)
{
    QByteArray msg;

    // Definition message for local 1 (Activity)
    writeByte(msg, 0x41);               // Definition, local 1
    writeByte(msg, 0);
    writeByte(msg, 0);
    writeUint16LE(msg, MESG_ACTIVITY);
    writeByte(msg, 4);

    // timestamp, total_timer_time, num_sessions, type
    writeByte(msg, 253); writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // timestamp
    writeByte(msg, 0);   writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // total_timer_time
    writeByte(msg, 1);   writeByte(msg, 2); writeByte(msg, FIT_UINT16);  // num_sessions
    writeByte(msg, 2);   writeByte(msg, 1); writeByte(msg, FIT_ENUM);    // type

    // Data message local 1
    writeByte(msg, 0x01);
    writeUint32LE(msg, timestamp);
    writeUint32LE(msg, totalTimerTime * 1000); // milliseconds
    writeUint16LE(msg, static_cast<quint16>(numSessions));
    writeByte(msg, 0); // manual

    return msg;
}

QByteArray FitExporter::buildSession(const QVariantMap &workout, quint32 startTime)
{
    QByteArray msg;

    // Definition message for local 2 (Session)
    writeByte(msg, 0x42);
    writeByte(msg, 0);
    writeByte(msg, 0);
    writeUint16LE(msg, MESG_SESSION);
    writeByte(msg, 7);

    writeByte(msg, 253); writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // timestamp
    writeByte(msg, 2);   writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // start_time
    writeByte(msg, 7);   writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // total_elapsed_time
    writeByte(msg, 9);   writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // total_distance (cm)
    writeByte(msg, 16);  writeByte(msg, 2); writeByte(msg, FIT_UINT16);  // avg_heart_rate
    writeByte(msg, 17);  writeByte(msg, 2); writeByte(msg, FIT_UINT16);  // max_heart_rate
    writeByte(msg, 11);  writeByte(msg, 2); writeByte(msg, FIT_UINT16);  // total_calories

    int durationS = workout.value(QStringLiteral("duration")).toInt();
    double distM  = workout.value(QStringLiteral("distance")).toDouble();
    int avgHR     = workout.value(QStringLiteral("avgHR")).toInt();
    int maxHR     = workout.value(QStringLiteral("maxHR")).toInt();
    int calories  = workout.value(QStringLiteral("calories")).toInt();

    quint32 endTime = startTime + static_cast<quint32>(durationS);

    // Data message local 2
    writeByte(msg, 0x02);
    writeUint32LE(msg, endTime);
    writeUint32LE(msg, startTime);
    writeUint32LE(msg, static_cast<quint32>(durationS) * 1000);  // ms
    writeUint32LE(msg, static_cast<quint32>(distM * 100.0));     // cm
    writeUint16LE(msg, static_cast<quint16>(avgHR));
    writeUint16LE(msg, static_cast<quint16>(maxHR));
    writeUint16LE(msg, static_cast<quint16>(calories));

    return msg;
}

QByteArray FitExporter::buildLap(const QVariantMap &workout, quint32 startTime)
{
    QByteArray msg;

    // Definition message for local 3 (Lap)
    writeByte(msg, 0x43);
    writeByte(msg, 0);
    writeByte(msg, 0);
    writeUint16LE(msg, MESG_LAP);
    writeByte(msg, 4);

    writeByte(msg, 253); writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // timestamp
    writeByte(msg, 2);   writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // start_time
    writeByte(msg, 7);   writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // total_elapsed_time
    writeByte(msg, 9);   writeByte(msg, 4); writeByte(msg, FIT_UINT32);  // total_distance

    int durationS = workout.value(QStringLiteral("duration")).toInt();
    double distM  = workout.value(QStringLiteral("distance")).toDouble();
    quint32 endTime = startTime + static_cast<quint32>(durationS);

    writeByte(msg, 0x03);
    writeUint32LE(msg, endTime);
    writeUint32LE(msg, startTime);
    writeUint32LE(msg, static_cast<quint32>(durationS) * 1000);
    writeUint32LE(msg, static_cast<quint32>(distM * 100.0));

    return msg;
}

QByteArray FitExporter::buildRecord(quint32 timestamp, qint32 lat, qint32 lon,
                                     quint16 altitude, quint8 heartRate, quint16 speed)
{
    QByteArray msg;

    // Use compressed timestamp header (local message 4 already defined)
    writeByte(msg, 0x04);
    writeUint32LE(msg, timestamp);
    writeSint32LE(msg, lat);
    writeSint32LE(msg, lon);
    writeUint16LE(msg, altitude);
    writeByte(msg, heartRate);
    writeUint16LE(msg, speed);

    return msg;
}

QByteArray FitExporter::exportWorkout(const QVariantMap &workoutData,
                                       const QVariantList &gpsTrack,
                                       const QVariantMap &metrics)
{
    Q_UNUSED(metrics);

    if (workoutData.isEmpty())
        return QByteArray();

    qint64 startEpoch = workoutData.value(QStringLiteral("startTime")).toLongLong();
    int durationS     = workoutData.value(QStringLiteral("duration")).toInt();
    quint32 fitStart  = toFitTimestamp(startEpoch);
    quint32 fitEnd    = fitStart + static_cast<quint32>(durationS);

    // Build data records
    QByteArray data;
    data.reserve(4096);

    data.append(buildFileId(fitStart));
    data.append(buildSession(workoutData, fitStart));

    // Define Record message once (local 4)
    {
        QByteArray defn;
        writeByte(defn, 0x44);   // Definition, local 4
        writeByte(defn, 0);
        writeByte(defn, 0);
        writeUint16LE(defn, MESG_RECORD);
        writeByte(defn, 6);

        writeByte(defn, 253); writeByte(defn, 4); writeByte(defn, FIT_UINT32);  // timestamp
        writeByte(defn, 0);   writeByte(defn, 4); writeByte(defn, FIT_SINT32);  // position_lat
        writeByte(defn, 1);   writeByte(defn, 4); writeByte(defn, FIT_SINT32);  // position_long
        writeByte(defn, 2);   writeByte(defn, 2); writeByte(defn, FIT_UINT16);  // altitude
        writeByte(defn, 3);   writeByte(defn, 1); writeByte(defn, FIT_UINT8);   // heart_rate
        writeByte(defn, 6);   writeByte(defn, 2); writeByte(defn, FIT_UINT16);  // speed

        data.append(defn);
    }

    // Emit Record messages from GPS track
    for (const QVariant &pt : gpsTrack) {
        QVariantMap p = pt.toMap();
        qint64 ts    = p.value(QStringLiteral("timestamp")).toLongLong();
        double lat   = p.value(QStringLiteral("latitude")).toDouble();
        double lon   = p.value(QStringLiteral("longitude")).toDouble();
        double alt   = p.value(QStringLiteral("altitude")).toDouble();
        int hr       = p.value(QStringLiteral("heartRate"), 0).toInt();
        double spd   = p.value(QStringLiteral("speed"), 0.0).toDouble();

        data.append(buildRecord(
            toFitTimestamp(ts),
            toSemicircles(lat),
            toSemicircles(lon),
            toFitAltitude(alt),
            static_cast<quint8>(hr),
            toFitSpeed(spd)
        ));
    }

    data.append(buildLap(workoutData, fitStart));
    data.append(buildActivity(fitEnd, static_cast<quint32>(durationS), 1));

    // Assemble final file: header + data + CRC
    QByteArray header = buildFileHeader(static_cast<quint32>(data.size()));

    QByteArray fit;
    fit.reserve(header.size() + data.size() + 2);
    fit.append(header);
    fit.append(data);

    quint16 fileCrc = crc16(data, crc16(header));
    writeByte(fit, static_cast<quint8>(fileCrc & 0xFF));
    writeByte(fit, static_cast<quint8>((fileCrc >> 8) & 0xFF));

    return fit;
}
