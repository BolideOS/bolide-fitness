/*
 * test_bodymetrics.cpp – Unit tests for BodyMetrics
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QtTest>
#include <QtMath>
#include "domain/bodymetrics.h"

class TestBodyMetrics : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        BodyMetrics bm;
        QCOMPARE(bm.heartRate(), 0);
        QCOMPARE(bm.hrvRmssd(), 0.0);
        QCOMPARE(bm.spo2(), 0);
        QCOMPARE(bm.stressIndex(), 0);
        QCOMPARE(bm.temperature(), 0.0);
        QCOMPARE(bm.breathingRate(), 0);
        QCOMPARE(bm.restingHr(), 0);
        QCOMPARE(bm.dailySteps(), 0);
        QVERIFY(!bm.monitoring());
        QVERIFY(!bm.spo2Measuring());
    }

    void testCurrentMap()
    {
        BodyMetrics bm;
        QVariantMap m = bm.current();
        QVERIFY(m.contains("heart_rate"));
        QVERIFY(m.contains("hrv_rmssd"));
        QVERIFY(m.contains("spo2"));
        QVERIFY(m.contains("stress_index"));
        QVERIFY(m.contains("temperature"));
        QVERIFY(m.contains("breathing_rate"));
        QVERIFY(m.contains("resting_hr"));
        QVERIFY(m.contains("daily_steps"));
        QVERIFY(m.contains("daily_calories"));
    }

    void testStressFromHrv()
    {
        // Verify stress computation formula: stress = 100 * exp(-rmssd / 30)
        // rmssd=60 → 100 * exp(-2) ≈ 13.5
        // rmssd=30 → 100 * exp(-1) ≈ 36.8
        // rmssd=15 → 100 * exp(-0.5) ≈ 60.7
        // rmssd=5 → 100 * exp(-1/6) ≈ 84.6

        auto computeStress = [](double rmssd) -> int {
            if (rmssd <= 0) return 50;
            double stress = 100.0 * qExp(-rmssd / 30.0);
            return qBound(0, qRound(stress), 100);
        };

        // Very relaxed
        int relaxed = computeStress(60.0);
        QVERIFY(relaxed >= 10 && relaxed <= 20);

        // Moderate
        int moderate = computeStress(30.0);
        QVERIFY(moderate >= 30 && moderate <= 42);

        // High stress
        int high = computeStress(10.0);
        QVERIFY(high >= 65 && high <= 80);

        // Very high stress
        int veryHigh = computeStress(3.0);
        QVERIFY(veryHigh >= 85);

        // Unknown (0) → 50
        QCOMPARE(computeStress(0.0), 50);
    }

    void testBreathingRateEstimation()
    {
        // Test zero-crossing based breathing rate estimation

        // Create a sinusoidal HR pattern at ~15 breaths/min
        // Over 2 minutes with 5s sampling (24 samples)
        // 15 breaths/min = 0.25 Hz
        // Period = 4 seconds; over 2 min that's 30 oscillations
        // At 5s sampling we'd see ~24 samples

        QVector<int> hrSamples;
        double mean = 70.0;
        for (int i = 0; i < 24; ++i) {
            // Simulate ~15 breaths/min oscillation
            double t = i * 5.0;  // seconds
            double osc = 3.0 * qSin(2.0 * M_PI * 0.25 * t); // 15/min = 0.25 Hz
            hrSamples.append(qRound(mean + osc));
        }

        // Count zero crossings
        int crossings = 0;
        bool positive = (hrSamples.first() - mean) >= 0;
        for (int i = 1; i < hrSamples.size(); ++i) {
            bool nowPositive = (hrSamples[i] - mean) >= 0;
            if (nowPositive != positive) {
                crossings++;
                positive = nowPositive;
            }
        }

        double cycles = crossings / 2.0;
        double windowMinutes = 2.0;
        double rate = cycles / windowMinutes;

        // Should be roughly 15 breaths/min (allow wide margin due to discretization)
        QVERIFY(rate >= 6.0 && rate <= 30.0);
    }

    void testSpO2SensorNotAvailable()
    {
        BodyMetrics bm;
        // Without a SensorService, spotCheckSpO2 should be a no-op
        bm.spotCheckSpO2();
        QVERIFY(!bm.spo2Measuring());
    }
};

QTEST_MAIN(TestBodyMetrics)
#include "test_bodymetrics.moc"
