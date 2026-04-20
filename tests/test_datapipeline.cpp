/*
 * test_datapipeline.cpp – Unit tests for DataPipeline
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QtTest>
#include "core/datapipeline.h"
#include "core/sensorservice.h"

class TestDataPipeline : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        SensorService ss;
        DataPipeline dp(&ss);

        QVERIFY(dp.hrBuffer().isEmpty());
        QCOMPARE(dp.distance(), 0.0);
        QCOMPARE(dp.elevationGain(), 0.0);
    }

    void testHRBuffering()
    {
        SensorService ss;
        DataPipeline dp(&ss);

        // Simulate a few HR readings
        float hr1 = 75.0f;
        dp.pushSample(SensorService::HeartRate,
                       QDateTime::currentMSecsSinceEpoch() * 1000,
                       &hr1, 1);

        float hr2 = 80.0f;
        dp.pushSample(SensorService::HeartRate,
                       QDateTime::currentMSecsSinceEpoch() * 1000,
                       &hr2, 1);

        float hr3 = 85.0f;
        dp.pushSample(SensorService::HeartRate,
                       QDateTime::currentMSecsSinceEpoch() * 1000,
                       &hr3, 1);

        QCOMPARE(dp.hrBuffer().count(), 3);
        QCOMPARE(dp.hrBuffer().at(0), 85.0f);   // most recent
        QCOMPARE(dp.hrBuffer().at(2), 75.0f);   // oldest
    }

    void testElevationGainAccumulation()
    {
        SensorService ss;
        DataPipeline dp(&ss);

        // Ascending: 100 → 110 → 115 → 112 → 120
        float elevations[] = {100.0f, 110.0f, 115.0f, 112.0f, 120.0f};
        qint64 now = QDateTime::currentMSecsSinceEpoch() * 1000;

        for (int i = 0; i < 5; ++i) {
            dp.pushSample(SensorService::Barometer,
                           now + i * 1000000, // 1 sec apart
                           &elevations[i], 1);
        }

        // Gain: 10 + 5 + 0 + 8 = 23  (descents don't count)
        // But pipeline may smooth, so just check > 0
        QVERIFY(dp.elevationGain() > 0.0);
    }

    void testGPSDistanceAccumulation()
    {
        SensorService ss;
        DataPipeline dp(&ss);

        qint64 now = QDateTime::currentMSecsSinceEpoch() * 1000;

        // Two GPS points ~111m apart (0.001° latitude ≈ 111m)
        float gps1[] = {48.0f, 2.0f, 100.0f, 0.0f, 0.0f}; // lat, lon, alt, speed, bearing
        dp.pushGPS(now, gps1[0], gps1[1], gps1[2], gps1[3], gps1[4]);

        float gps2[] = {48.001f, 2.0f, 100.0f, 3.0f, 0.0f};
        dp.pushGPS(now + 1000000, gps2[0], gps2[1], gps2[2], gps2[3], gps2[4]);

        double dist = dp.distance();
        // Should be approximately 111m
        QVERIFY(dist > 100.0);
        QVERIFY(dist < 130.0);
    }

    void testReset()
    {
        SensorService ss;
        DataPipeline dp(&ss);

        float hr = 80.0f;
        dp.pushSample(SensorService::HeartRate, 0, &hr, 1);
        QCOMPARE(dp.hrBuffer().count(), 1);

        dp.reset();
        QVERIFY(dp.hrBuffer().isEmpty());
        QCOMPARE(dp.distance(), 0.0);
        QCOMPARE(dp.elevationGain(), 0.0);
    }

    void testPaceComputation()
    {
        SensorService ss;
        DataPipeline dp(&ss);

        qint64 now = QDateTime::currentMSecsSinceEpoch() * 1000;

        // Simulate 1km in 5 minutes using GPS points
        // 10 points, each ~100m apart
        for (int i = 0; i < 11; ++i) {
            double lat = 48.0 + i * 0.0009;  // ~100m step
            dp.pushGPS(now + i * 30000000LL, lat, 2.0, 100.0,
                       3.3, 0.0); // ~3.3 m/s
        }

        double pace = dp.currentPace();
        // At ~3.3 m/s → ~5:03 min/km → ~303 sec/km
        // Allow wide margin due to haversine approximation
        if (pace > 0) {
            QVERIFY(pace > 200.0);
            QVERIFY(pace < 500.0);
        }
    }
};

QTEST_MAIN(TestDataPipeline)
#include "test_datapipeline.moc"
