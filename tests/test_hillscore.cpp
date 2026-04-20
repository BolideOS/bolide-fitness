/*
 * test_hillscore.cpp – Unit tests for HillScorePlugin
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QtTest>
#include <QVariantMap>
#include "metrics/hillscore.h"

class TestHillScore : public QObject
{
    Q_OBJECT

private slots:
    void testMetadata()
    {
        HillScorePlugin hs;
        QCOMPARE(hs.id(), QStringLiteral("hill_score"));
        QCOMPARE(hs.unit(), QStringLiteral(""));
        QVERIFY(hs.requiredInputs().contains("elevation_series"));
        QVERIFY(hs.requiredInputs().contains("time_series"));
    }

    void testMissingInputs()
    {
        HillScorePlugin hs;
        QVariantMap inputs;
        QVariant result = hs.compute(inputs);
        // Should return invalid or 0 for missing data
        QVERIFY(!result.isValid() || result.toDouble() == 0.0);
    }

    void testFlatTerrain()
    {
        HillScorePlugin hs;
        QVariantMap inputs;

        // 10 samples over 600 seconds, flat elevation
        QVariantList elev, times;
        for (int i = 0; i < 10; ++i) {
            elev.append(100.0);  // constant altitude
            times.append(static_cast<double>(i * 60));
        }

        inputs["elevation_series"] = elev;
        inputs["time_series"] = times;
        inputs["duration_seconds"] = 600.0;

        QVariant result = hs.compute(inputs);
        double score = result.toDouble();
        // Flat terrain → very low or zero hill score
        QVERIFY(score >= 0.0);
        QVERIFY(score < 10.0);
    }

    void testModeratClimb()
    {
        HillScorePlugin hs;
        QVariantMap inputs;

        // 10 samples over 600 seconds, gaining 300m (steady climb)
        QVariantList elev, times;
        for (int i = 0; i < 10; ++i) {
            elev.append(100.0 + i * 30.0);  // +30m per minute
            times.append(static_cast<double>(i * 60));
        }

        inputs["elevation_series"] = elev;
        inputs["time_series"] = times;
        inputs["duration_seconds"] = 600.0;

        QVariant result = hs.compute(inputs);
        double score = result.toDouble();
        // Moderate climb should yield a decent score
        QVERIFY(score > 10.0);
        QVERIFY(score <= 100.0);
    }

    void testSteepClimbWithHighHR()
    {
        HillScorePlugin hs;
        QVariantMap inputs;

        // Aggressive climb: +50m per minute for 10 minutes
        QVariantList elev, times;
        for (int i = 0; i < 10; ++i) {
            elev.append(200.0 + i * 50.0);
            times.append(static_cast<double>(i * 60));
        }

        inputs["elevation_series"] = elev;
        inputs["time_series"] = times;
        inputs["duration_seconds"] = 600.0;
        inputs["avg_hr"] = 175.0;        // high HR → correction factor
        inputs["max_hr"] = 190.0;

        QVariant result = hs.compute(inputs);
        double score = result.toDouble();
        // High climb rate but HR penalty should still give a meaningful score
        QVERIFY(score > 0.0);
        QVERIFY(score <= 100.0);
    }

    void testVO2maxFitnessScaling()
    {
        HillScorePlugin hs;

        // Same climb, different VO2max
        QVariantList elev, times;
        for (int i = 0; i < 10; ++i) {
            elev.append(100.0 + i * 25.0);
            times.append(static_cast<double>(i * 60));
        }

        QVariantMap inputsLow;
        inputsLow["elevation_series"] = elev;
        inputsLow["time_series"] = times;
        inputsLow["duration_seconds"] = 600.0;
        inputsLow["vo2max"] = 30.0;

        QVariantMap inputsHigh;
        inputsHigh["elevation_series"] = elev;
        inputsHigh["time_series"] = times;
        inputsHigh["duration_seconds"] = 600.0;
        inputsHigh["vo2max"] = 60.0;

        double scoreLow = hs.compute(inputsLow).toDouble();
        double scoreHigh = hs.compute(inputsHigh).toDouble();

        // Higher VO2max should yield a higher hill score
        QVERIFY(scoreHigh > scoreLow);
    }

    void testAltitudePenalty()
    {
        HillScorePlugin hs;

        // Same climb at low vs high altitude
        auto makeInputs = [](double baseAlt) {
            QVariantMap inputs;
            QVariantList elev, times;
            for (int i = 0; i < 10; ++i) {
                elev.append(baseAlt + i * 20.0);
                times.append(static_cast<double>(i * 60));
            }
            inputs["elevation_series"] = elev;
            inputs["time_series"] = times;
            inputs["duration_seconds"] = 600.0;
            return inputs;
        };

        double scoreLow  = hs.compute(makeInputs(500.0)).toDouble();
        double scoreHigh = hs.compute(makeInputs(3000.0)).toDouble();

        // High altitude should penalize the score
        QVERIFY(scoreHigh < scoreLow);
    }

    void testScoreBounds()
    {
        HillScorePlugin hs;

        // Extreme climb: unrealistically fast
        QVariantList elev, times;
        for (int i = 0; i < 10; ++i) {
            elev.append(i * 500.0);  // 500m per minute - impossible
            times.append(static_cast<double>(i * 60));
        }

        QVariantMap inputs;
        inputs["elevation_series"] = elev;
        inputs["time_series"] = times;
        inputs["duration_seconds"] = 600.0;

        double score = hs.compute(inputs).toDouble();
        // Score must be clamped to [0, 100]
        QVERIFY(score >= 0.0);
        QVERIFY(score <= 100.0);
    }
};

QTEST_MAIN(TestHillScore)
#include "test_hillscore.moc"
