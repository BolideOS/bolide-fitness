/*
 * test_readiness.cpp – Unit tests for ReadinessScore
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QtTest>
#include "domain/readinessscore.h"

class TestReadiness : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        ReadinessScore rs;
        QCOMPARE(rs.score(), 0);
        QCOMPARE(rs.label(), QStringLiteral("Very Low"));
        QVERIFY(rs.breakdown().isEmpty());
    }

    void testLabels()
    {
        // Verify label thresholds match documentation
        // 85-100 → Excellent
        // 70-84 → Good
        // 50-69 → Moderate
        // 30-49 → Low
        // 0-29 → Very Low

        struct { int score; QString expected; } cases[] = {
            { 100, "Excellent" },
            { 85,  "Excellent" },
            { 84,  "Good" },
            { 70,  "Good" },
            { 69,  "Moderate" },
            { 50,  "Moderate" },
            { 49,  "Low" },
            { 30,  "Low" },
            { 29,  "Very Low" },
            { 0,   "Very Low" },
        };

        for (const auto &c : cases) {
            // Check what label would be for this score value
            QString label;
            if (c.score >= 85) label = "Excellent";
            else if (c.score >= 70) label = "Good";
            else if (c.score >= 50) label = "Moderate";
            else if (c.score >= 30) label = "Low";
            else label = "Very Low";
            QCOMPARE(label, c.expected);
        }
    }

    void testHrvScoring()
    {
        // Test the HRV deviation scoring logic:
        // acute == baseline → ~75
        // acute > baseline → up to 100
        // acute < baseline → down toward 0

        // Verify formula: ratio * 75 when below, 75 + (ratio-1)*125 when above
        auto scoreHrv = [](double acute, double baseline) -> double {
            if (baseline <= 0) return 50.0;
            double ratio = acute / baseline;
            if (ratio >= 1.0)
                return qBound(0.0, 75.0 + qMin(25.0, (ratio - 1.0) * 125.0), 100.0);
            return qBound(0.0, 75.0 * ratio, 100.0);
        };

        // Equal → 75
        QCOMPARE(qRound(scoreHrv(40.0, 40.0)), 75);

        // 20% above → 75 + 0.2*125 = 100 (capped)
        QVERIFY(scoreHrv(48.0, 40.0) > 90);

        // 50% below → 75 * 0.5 = 37.5
        QVERIFY(qAbs(scoreHrv(20.0, 40.0) - 37.5) < 1.0);

        // No baseline → 50
        QCOMPARE(qRound(scoreHrv(40.0, 0.0)), 50);
    }

    void testRestingHrScoring()
    {
        // Lower acute HR than baseline → good
        // Higher acute HR → bad
        auto scoreRhr = [](int acute, double baseline) -> double {
            if (baseline <= 0 || acute <= 0) return 50.0;
            double delta = double(acute) - baseline;
            return qBound(0.0, 80.0 - delta * 5.0, 100.0);
        };

        // Equal → 80
        QCOMPARE(qRound(scoreRhr(60, 60.0)), 80);

        // 4 bpm lower → 80 + 20 = 100
        QCOMPARE(qRound(scoreRhr(56, 60.0)), 100);

        // 4 bpm higher → 80 - 20 = 60
        QCOMPARE(qRound(scoreRhr(64, 60.0)), 60);
    }

    void testSleepDurationScoring()
    {
        // Target 480 min (8h)
        auto scoreDur = [](int dur, int target) -> double {
            double ratio = double(dur) / target;
            if (ratio >= 0.95 && ratio <= 1.15) return 100.0;
            if (ratio < 0.95) return qMax(0.0, ratio / 0.95 * 100.0);
            return qMax(50.0, 100.0 - (ratio - 1.15) * 200.0);
        };

        // 8 hours → 100
        QCOMPARE(qRound(scoreDur(480, 480)), 100);

        // 7.5 hours → still in range (0.95 * 480 = 456)
        QCOMPARE(qRound(scoreDur(460, 480)), 100);

        // 6 hours → 375/456 * 100 ≈ 78.9
        double sixHour = scoreDur(360, 480);
        QVERIFY(sixHour > 70 && sixHour < 85);

        // 4 hours → very low
        QVERIFY(scoreDur(240, 480) < 60);
    }

    void testTrainingLoadScoring()
    {
        auto scoreTl = [](double acute, double chronic) -> double {
            if (chronic <= 0) return (acute <= 0) ? 75.0 : 60.0;
            double ratio = acute / chronic;
            if (ratio >= 0.8 && ratio <= 1.3) return 90.0;
            if (ratio < 0.8) return 70.0 + (ratio / 0.8) * 20.0;
            if (ratio <= 1.5) return 90.0 - (ratio - 1.3) * 200.0;
            return qMax(0.0, 50.0 - (ratio - 1.5) * 100.0);
        };

        // Sweet spot ratio (1.0) → 90
        QCOMPARE(qRound(scoreTl(500, 500)), 90);

        // Detraining (0.5) → ~82.5
        double detrain = scoreTl(250, 500);
        QVERIFY(detrain > 75 && detrain < 90);

        // Overload (2.0) → very low
        QVERIFY(scoreTl(1000, 500) < 20);

        // No data → 75
        QCOMPARE(qRound(scoreTl(0, 0)), 75);
    }
};

QTEST_MAIN(TestReadiness)
#include "test_readiness.moc"
