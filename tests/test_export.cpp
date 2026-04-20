/*
 * Copyright (C) 2026 BolideOS Contributors
 * License: GPLv3
 *
 * Unit tests for export format plugins (FIT, TCX, GPX).
 */

#include <QtTest/QtTest>
#include "export/fitexporter.h"
#include "export/tcxexporter.h"
#include "export/gpxexporter.h"
#include "export/exportmanager.h"

class TestExport : public QObject
{
    Q_OBJECT

private:
    QVariantMap sampleWorkout()
    {
        QVariantMap w;
        w["type"]      = "outdoor_run";
        w["startTime"] = 1714000000LL;  // 2024-04-25 approx
        w["duration"]  = 1800;          // 30 minutes
        w["distance"]  = 5000.0;        // 5 km
        w["avgHR"]     = 145;
        w["maxHR"]     = 172;
        w["calories"]  = 320;
        return w;
    }

    QVariantList sampleTrack()
    {
        QVariantList track;
        for (int i = 0; i < 5; ++i) {
            QVariantMap pt;
            pt["timestamp"] = 1714000000LL + i * 60;
            pt["latitude"]  = 48.8566 + i * 0.001;
            pt["longitude"] = 2.3522 + i * 0.001;
            pt["altitude"]  = 35.0 + i * 2.0;
            pt["heartRate"] = 140 + i * 3;
            pt["speed"]     = 3.0 + i * 0.1;
            track.append(pt);
        }
        return track;
    }

private slots:
    // ── FIT format tests ────────────────────────────────────────────
    void testFitFormatId()
    {
        FitExporter fit;
        QCOMPARE(fit.formatId(), QString("fit"));
        QCOMPARE(fit.fileExtension(), QString(".fit"));
        QVERIFY(!fit.mimeType().isEmpty());
    }

    void testFitExportProducesData()
    {
        FitExporter fit;
        QByteArray data = fit.exportWorkout(sampleWorkout(), sampleTrack(), QVariantMap());
        QVERIFY(!data.isEmpty());

        // FIT file must start with 14-byte header
        QVERIFY(data.size() >= 14);

        // Check header size byte
        QCOMPARE(static_cast<quint8>(data[0]), quint8(14));

        // Check ".FIT" signature at offset 8
        QCOMPARE(data.mid(8, 4), QByteArray(".FIT"));
    }

    void testFitEmptyWorkout()
    {
        FitExporter fit;
        QByteArray data = fit.exportWorkout(QVariantMap(), QVariantList(), QVariantMap());
        QVERIFY(data.isEmpty());
    }

    // ── TCX format tests ────────────────────────────────────────────
    void testTcxFormatId()
    {
        TcxExporter tcx;
        QCOMPARE(tcx.formatId(), QString("tcx"));
        QCOMPARE(tcx.fileExtension(), QString(".tcx"));
    }

    void testTcxExportProducesXml()
    {
        TcxExporter tcx;
        QByteArray data = tcx.exportWorkout(sampleWorkout(), sampleTrack(), QVariantMap());
        QVERIFY(!data.isEmpty());

        // Must be valid XML
        QString xml = QString::fromUtf8(data);
        QVERIFY(xml.contains("TrainingCenterDatabase"));
        QVERIFY(xml.contains("Activity"));
        QVERIFY(xml.contains("Trackpoint"));
        QVERIFY(xml.contains("HeartRateBpm"));
    }

    void testTcxSportMapping()
    {
        TcxExporter tcx;

        // Running workout should map to Running sport
        QVariantMap runWorkout = sampleWorkout();
        runWorkout["type"] = "outdoor_run";
        QByteArray data = tcx.exportWorkout(runWorkout, QVariantList(), QVariantMap());
        QString xml = QString::fromUtf8(data);
        QVERIFY(xml.contains("Sport=\"Running\""));
    }

    // ── GPX format tests ────────────────────────────────────────────
    void testGpxFormatId()
    {
        GpxExporter gpx;
        QCOMPARE(gpx.formatId(), QString("gpx"));
        QCOMPARE(gpx.fileExtension(), QString(".gpx"));
    }

    void testGpxExportProducesXml()
    {
        GpxExporter gpx;
        QByteArray data = gpx.exportWorkout(sampleWorkout(), sampleTrack(), QVariantMap());
        QVERIFY(!data.isEmpty());

        QString xml = QString::fromUtf8(data);
        QVERIFY(xml.contains("<gpx"));
        QVERIFY(xml.contains("<trk>"));
        QVERIFY(xml.contains("<trkpt"));
        QVERIFY(xml.contains("<ele>"));
        QVERIFY(xml.contains("gpxtpx:hr"));
    }

    void testGpxCoordinates()
    {
        GpxExporter gpx;
        QByteArray data = gpx.exportWorkout(sampleWorkout(), sampleTrack(), QVariantMap());
        QString xml = QString::fromUtf8(data);

        // Check that coordinates are present
        QVERIFY(xml.contains("lat=\"48.856"));
        QVERIFY(xml.contains("lon=\"2.352"));
    }

    // ── ExportManager tests ─────────────────────────────────────────
    void testExportManagerBuiltins()
    {
        ExportManager mgr;
        mgr.registerBuiltins();

        QVariantList formats = mgr.availableFormats();
        QVERIFY(formats.size() >= 3);

        // Check all three formats registered
        QSet<QString> ids;
        for (const QVariant &v : formats)
            ids.insert(v.toMap().value("id").toString());

        QVERIFY(ids.contains("fit"));
        QVERIFY(ids.contains("tcx"));
        QVERIFY(ids.contains("gpx"));
    }

    void testExportManagerTransports()
    {
        ExportManager mgr;
        mgr.registerBuiltins();

        QVariantList transports = mgr.availableTransports();
        QVERIFY(transports.size() >= 2);

        QSet<QString> ids;
        for (const QVariant &v : transports)
            ids.insert(v.toMap().value("id").toString());

        QVERIFY(ids.contains("file"));
        QVERIFY(ids.contains("ble"));
    }
};

QTEST_MAIN(TestExport)
#include "test_export.moc"
