/*
 * Copyright (C) 2026 BolideOS Contributors
 * License: GPLv3
 *
 * Unit tests for ML plugin infrastructure.
 */

#include <QtTest/QtTest>
#include "ml/mlregistry.h"
#include "ml/builtinmodels.h"

class TestMLRegistry : public QObject
{
    Q_OBJECT

private slots:
    void testRegistryInitialState()
    {
        MLRegistry registry;
        QVERIFY(!registry.isAvailable());
        QVERIFY(registry.modelList().isEmpty());
        QVERIFY(!registry.hasModel("activity_recognition"));
    }

    void testRegisterPlugin()
    {
        MLRegistry registry;
        registry.registerPlugin(new ActivityRecognitionModel());

        QVariantList models = registry.modelList();
        QCOMPARE(models.size(), 1);
        QCOMPARE(models.first().toMap().value("id").toString(),
                 QString("activity_recognition"));
    }

    void testActivityRecognitionHeuristic()
    {
        ActivityRecognitionModel model;

        // Load with empty path (heuristic mode)
        QVERIFY(model.loadModel(QString()));

        // Test idle detection: very low movement
        QVariantList idleData;
        for (int i = 0; i < 128; ++i)
            idleData.append(1.0);  // gravity, no movement

        QVariantMap inputs;
        inputs["input"] = idleData;

        QVariantMap resultMap = model.predict(inputs);
        QCOMPARE(resultMap.value("label").toString(), QString("idle"));
        QVERIFY(resultMap.value("confidence").toDouble() > 0.5);
    }

    void testSleepStagingHeuristic()
    {
        SleepStagingModel model;
        QVERIFY(model.loadModel(QString()));

        // Test awake detection: high movement
        QVariantList highMovement, normalHR;
        for (int i = 0; i < 30; ++i) {
            highMovement.append(0.5);  // lots of movement
            normalHR.append(75.0);
        }

        QVariantMap inputs;
        inputs["movement"]   = highMovement;
        inputs["heart_rate"] = normalHR;

        QVariantMap resultMap = model.predict(inputs);
        QCOMPARE(resultMap.value("label").toString(), QString("awake"));
    }

    void testSleepStagingDeepSleep()
    {
        SleepStagingModel model;
        model.loadModel(QString());

        QVariantList lowMovement, lowHR;
        for (int i = 0; i < 30; ++i) {
            lowMovement.append(0.005);  // very still
            lowHR.append(52.0);         // low HR
        }

        QVariantMap inputs;
        inputs["movement"]     = lowMovement;
        inputs["heart_rate"]   = lowHR;
        inputs["night_avg_hr"] = 65.0;  // 52 is 13 below avg

        QVariantMap resultMap = model.predict(inputs);
        QCOMPARE(resultMap.value("label").toString(), QString("deep"));
    }

    void testModelsByCategory()
    {
        MLRegistry registry;
        registry.registerPlugin(new ActivityRecognitionModel());
        registry.registerPlugin(new SleepStagingModel());

        QVariantList activityModels = registry.modelsByCategory("activity");
        QCOMPARE(activityModels.size(), 1);

        QVariantList sleepModels = registry.modelsByCategory("sleep");
        QCOMPARE(sleepModels.size(), 1);
    }

    void testUnregisterPlugin()
    {
        MLRegistry registry;
        registry.registerPlugin(new ActivityRecognitionModel());
        QCOMPARE(registry.modelList().size(), 1);

        registry.unregisterPlugin("activity_recognition");
        QVERIFY(registry.modelList().isEmpty());
    }

    void testPredictWithoutModel()
    {
        MLRegistry registry;
        QVariantMap result = registry.predict("nonexistent", QVariantMap());
        QVERIFY(result.isEmpty());
    }
};

QTEST_MAIN(TestMLRegistry)
#include "test_mlregistry.moc"
