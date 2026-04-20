/*
 * Copyright (C) 2026 BolideOS Contributors
 * License: GPLv3
 *
 * Unit tests for CompanionService.
 */

#include <QtTest/QtTest>
#include "companion/companionservice.h"

class TestCompanion : public QObject
{
    Q_OBJECT

private slots:
    void testGetInfo()
    {
        CompanionService svc;
        QVariantMap info = svc.GetInfo();

        QCOMPARE(info.value("name").toString(), QString("Bolide Fitness"));
        QVERIFY(!info.value("version").toString().isEmpty());
        QVERIFY(info.value("capabilities").toList().size() >= 5);
    }

    void testPing()
    {
        CompanionService svc;
        QCOMPARE(svc.Ping(), QString("pong"));
    }

    void testGetRecentWorkoutsWithoutDb()
    {
        CompanionService svc;
        QVariantList workouts = svc.GetRecentWorkouts(10);
        QVERIFY(workouts.isEmpty());
    }

    void testGetTodaySummaryWithoutDb()
    {
        CompanionService svc;
        QVariantMap summary = svc.GetTodaySummary();
        QVERIFY(summary.isEmpty());
    }

    void testGetBaselinesWithoutTrends()
    {
        CompanionService svc;
        QVariantMap baselines = svc.GetBaselines();
        QVERIFY(baselines.isEmpty());
    }

    void testExportWithoutManager()
    {
        CompanionService svc;
        QByteArray data = svc.ExportWorkout(1, "gpx");
        QVERIFY(data.isEmpty());
    }
};

QTEST_MAIN(TestCompanion)
#include "test_companion.moc"
