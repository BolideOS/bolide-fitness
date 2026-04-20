/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Built-in ML model stubs for activity recognition and sleep staging.
 * These provide the plugin interface and heuristic fallback when
 * TFLite models are not available on the device.
 */

#ifndef BUILTINMODELS_H
#define BUILTINMODELS_H

#include "mlplugin.h"

/**
 * @brief Activity recognition model plugin.
 *
 * Classifies the current physical activity from accelerometer data.
 * Labels: idle, walking, running, cycling, swimming, unknown
 *
 * When the TFLite model is loaded, uses neural network inference.
 * Falls back to simple threshold heuristics otherwise.
 */
class ActivityRecognitionModel : public MLPlugin
{
public:
    ActivityRecognitionModel() = default;
    ~ActivityRecognitionModel() override { unloadModel(); }

    QString modelId() const override { return QStringLiteral("activity_recognition"); }
    QString modelName() const override { return QStringLiteral("Activity Recognition"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    QString category() const override { return QStringLiteral("activity"); }
    QString description() const override {
        return QStringLiteral("Classifies physical activity from accelerometer data");
    }

    bool loadModel(const QString &modelPath) override;
    void unloadModel() override;
    bool isLoaded() const override { return m_loaded; }

    QVariantList inputSpec() const override;
    QVariantList outputSpec() const override;

    QVariantMap predict(const QVariantMap &inputs) override;

    int estimatedInferenceMs() const override { return 15; }
    QStringList requiredSensors() const override {
        return { QStringLiteral("accelerometer") };
    }

private:
    QVariantMap heuristicPredict(const QVector<float> &accelMagnitudes);
    bool m_loaded = false;
};

/**
 * @brief Sleep stage classification model plugin.
 *
 * Classifies sleep epochs into stages from accelerometer + HR data.
 * Labels: awake, light, deep, rem
 *
 * Improves on the rule-based heuristics in SleepTracker when available.
 */
class SleepStagingModel : public MLPlugin
{
public:
    SleepStagingModel() = default;
    ~SleepStagingModel() override { unloadModel(); }

    QString modelId() const override { return QStringLiteral("sleep_staging"); }
    QString modelName() const override { return QStringLiteral("Sleep Staging"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    QString category() const override { return QStringLiteral("sleep"); }
    QString description() const override {
        return QStringLiteral("Classifies sleep epochs into stages (awake/light/deep/REM)");
    }

    bool loadModel(const QString &modelPath) override;
    void unloadModel() override;
    bool isLoaded() const override { return m_loaded; }

    QVariantList inputSpec() const override;
    QVariantList outputSpec() const override;

    QVariantMap predict(const QVariantMap &inputs) override;

    int estimatedInferenceMs() const override { return 20; }
    QStringList requiredSensors() const override {
        return { QStringLiteral("accelerometer"), QStringLiteral("heart_rate") };
    }

private:
    bool m_loaded = false;
};

#endif // BUILTINMODELS_H
