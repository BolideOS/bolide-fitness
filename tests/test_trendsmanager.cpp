/*
 * Copyright (C) 2026 BolideOS Contributors
 * License: GPLv3
 *
 * Unit tests for TrendsManager.
 */

#include <QtTest/QtTest>
#include "domain/trendsmanager.h"

class TestTrendsManager : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        TrendsManager tm;
        // Without database, baselines should be empty
        QVERIFY(tm.baselines().isEmpty());
    }

    void testAvailableMetrics()
    {
        TrendsManager tm;
        QVariantList metrics = tm.availableMetrics();
        QVERIFY(metrics.size() >= 10);

        // Check required metrics exist
        bool hasRHR = false, hasHRV = false, hasSteps = false;
        for (const QVariant &v : metrics) {
            QVariantMap m = v.toMap();
            QString id = m.value("id").toString();
            if (id == "resting_hr") hasRHR = true;
            if (id == "overnight_hrv") hasHRV = true;
            if (id == "daily_steps") hasSteps = true;
        }
        QVERIFY(hasRHR);
        QVERIFY(hasHRV);
        QVERIFY(hasSteps);
    }

    void testTrendDataWithoutDb()
    {
        TrendsManager tm;
        QVariantList data = tm.trendData("resting_hr", 30);
        QVERIFY(data.isEmpty());  // No database, no data
    }

    void testTrendDirectionWithoutDb()
    {
        TrendsManager tm;
        QCOMPARE(tm.trendDirection("resting_hr"), 0);  // Stable when no data
        QCOMPARE(tm.trendChangePercent("resting_hr"), 0.0);
    }

    void testWeeklyAggregatesWithoutDb()
    {
        TrendsManager tm;
        QVariantList agg = tm.weeklyAggregates("daily_steps", 4);
        QVERIFY(agg.isEmpty());
    }

    void testMonthlyAggregatesWithoutDb()
    {
        TrendsManager tm;
        QVariantList agg = tm.monthlyAggregates("sleep_quality", 3);
        QVERIFY(agg.isEmpty());
    }

    void testMetricCategories()
    {
        TrendsManager tm;
        QVariantList metrics = tm.availableMetrics();

        QSet<QString> categories;
        for (const QVariant &v : metrics)
            categories.insert(v.toMap().value("category").toString());

        QVERIFY(categories.contains("heart"));
        QVERIFY(categories.contains("sleep"));
        QVERIFY(categories.contains("activity"));
        QVERIFY(categories.contains("fitness"));
    }
};

QTEST_MAIN(TestTrendsManager)
#include "test_trendsmanager.moc"
