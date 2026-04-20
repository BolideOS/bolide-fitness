/*
 * Copyright (C) 2026 BolideOS Contributors
 * License: GPLv3
 *
 * Unit tests for SensorsLabManager.
 */

#include <QtTest/QtTest>
#include "domain/sensorslabmanager.h"

class TestSensorsLab : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        SensorsLabManager mgr;
        QVERIFY(!mgr.isRecording());
        QCOMPARE(mgr.sampleCount(), 0);
        QCOMPARE(mgr.recordingDuration(), 0.0);
        QVERIFY(mgr.activeSensors().isEmpty());
    }

    void testSensorNames()
    {
        // Test all sensor type names
        QCOMPARE(SensorsLabManager::sensorName(0), QString("Accelerometer"));
        QCOMPARE(SensorsLabManager::sensorName(1), QString("Gyroscope"));
        QCOMPARE(SensorsLabManager::sensorName(2), QString("Heart Rate"));
        QCOMPARE(SensorsLabManager::sensorName(3), QString("PPG Raw"));
        QCOMPARE(SensorsLabManager::sensorName(4), QString("Barometer"));
        QCOMPARE(SensorsLabManager::sensorName(5), QString("SpO2"));
        QCOMPARE(SensorsLabManager::sensorName(6), QString("Temperature"));
        QCOMPARE(SensorsLabManager::sensorName(7), QString("Compass"));
        QCOMPARE(SensorsLabManager::sensorName(8), QString("Ambient Light"));
        QCOMPARE(SensorsLabManager::sensorName(9), QString("GPS"));
        QCOMPARE(SensorsLabManager::sensorName(10), QString("Step Counter"));
    }

    void testSensorChannels()
    {
        // Accelerometer: 3 channels (x, y, z)
        QCOMPARE(SensorsLabManager::sensorChannels(0), 3);
        // Heart Rate: 1 channel
        QCOMPARE(SensorsLabManager::sensorChannels(2), 1);
        // GPS: 4 channels (lat, lon, alt, speed)
        QCOMPARE(SensorsLabManager::sensorChannels(9), 4);
    }

    void testSensorUnits()
    {
        QCOMPARE(SensorsLabManager::sensorUnit(0), QString("m/s²"));
        QCOMPARE(SensorsLabManager::sensorUnit(2), QString("bpm"));
        QCOMPARE(SensorsLabManager::sensorUnit(4), QString("hPa"));
        QCOMPARE(SensorsLabManager::sensorUnit(5), QString("%"));
        QCOMPARE(SensorsLabManager::sensorUnit(6), QString("°C"));
    }

    void testStartStreamWithoutSensor()
    {
        SensorsLabManager mgr;
        // Without sensor service, startStream should fail
        QVERIFY(!mgr.startStream(2, 100));
    }

    void testRecordingToggle()
    {
        SensorsLabManager mgr;

        mgr.startRecording();
        QVERIFY(mgr.isRecording());

        mgr.stopRecording();
        QVERIFY(!mgr.isRecording());
    }

    void testRecentSamplesEmpty()
    {
        SensorsLabManager mgr;
        QVariantList samples = mgr.recentSamples(2, 100);
        QVERIFY(samples.isEmpty());
    }

    void testLatestValueEmpty()
    {
        SensorsLabManager mgr;
        QVariantMap val = mgr.latestValue(2);
        QVERIFY(val.isEmpty());
    }
};

QTEST_MAIN(TestSensorsLab)
#include "test_sensorslab.moc"
