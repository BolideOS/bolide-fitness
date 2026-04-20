/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "builtinmodels.h"

#include <QFile>
#include <QtMath>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════════
// ActivityRecognitionModel
// ═══════════════════════════════════════════════════════════════════════════

bool ActivityRecognitionModel::loadModel(const QString &modelPath)
{
    // Check if TFLite model file exists
    if (QFile::exists(modelPath)) {
        // TODO: Load actual TFLite model via TFLite C API
        // For now, mark as loaded with heuristic fallback
        m_loaded = true;
        return true;
    }

    // Even without TFLite, provide heuristic-based classification
    m_loaded = true;
    return true;
}

void ActivityRecognitionModel::unloadModel()
{
    m_loaded = false;
}

QVariantList ActivityRecognitionModel::inputSpec() const
{
    QVariantMap spec;
    spec[QStringLiteral("name")]  = QStringLiteral("accel_magnitude");
    spec[QStringLiteral("shape")] = QVariantList{128};  // 128 samples window
    spec[QStringLiteral("dtype")] = QStringLiteral("float32");
    return {spec};
}

QVariantList ActivityRecognitionModel::outputSpec() const
{
    QVariantMap spec;
    spec[QStringLiteral("name")]   = QStringLiteral("activity");
    spec[QStringLiteral("shape")]  = QVariantList{6};
    spec[QStringLiteral("dtype")]  = QStringLiteral("float32");
    spec[QStringLiteral("labels")] = QVariantList{
        QStringLiteral("idle"),
        QStringLiteral("walking"),
        QStringLiteral("running"),
        QStringLiteral("cycling"),
        QStringLiteral("swimming"),
        QStringLiteral("unknown")
    };
    return {spec};
}

QVariantMap ActivityRecognitionModel::predict(const QVariantMap &inputs)
{
    // Extract accelerometer magnitudes
    QVariantList rawData = inputs.value(QStringLiteral("input")).toList();
    if (rawData.isEmpty())
        rawData = inputs.value(QStringLiteral("accel_magnitude")).toList();

    QVector<float> magnitudes;
    magnitudes.reserve(rawData.size());
    for (const QVariant &v : rawData)
        magnitudes.append(v.toFloat());

    return heuristicPredict(magnitudes);
}

QVariantMap ActivityRecognitionModel::heuristicPredict(const QVector<float> &accelMagnitudes)
{
    QVariantMap result;

    if (accelMagnitudes.isEmpty()) {
        result[QStringLiteral("label")]      = QStringLiteral("unknown");
        result[QStringLiteral("confidence")] = 0.0;
        return result;
    }

    // Compute statistics over the window
    double sum = 0.0, sumSq = 0.0;
    float maxVal = accelMagnitudes.first();
    float minVal = accelMagnitudes.first();

    for (float v : accelMagnitudes) {
        sum += v;
        sumSq += v * v;
        maxVal = qMax(maxVal, v);
        minVal = qMin(minVal, v);
    }

    int n = accelMagnitudes.size();
    double mean = sum / n;
    double variance = (sumSq / n) - (mean * mean);
    double stdDev = qSqrt(qMax(0.0, variance));
    double range = static_cast<double>(maxVal - minVal);

    // Zero-crossing rate (proxy for periodicity)
    int crossings = 0;
    for (int i = 1; i < n; ++i) {
        if ((accelMagnitudes[i] - static_cast<float>(mean)) *
            (accelMagnitudes[i - 1] - static_cast<float>(mean)) < 0)
            ++crossings;
    }
    double zcr = static_cast<double>(crossings) / n;

    // Heuristic classification based on accelerometer statistics
    QString label;
    double confidence;

    if (stdDev < 0.05 && range < 0.2) {
        // Very low movement → idle
        label = QStringLiteral("idle");
        confidence = 0.85;
    } else if (stdDev < 0.3 && zcr > 0.15 && zcr < 0.45) {
        // Moderate rhythmic movement → walking
        label = QStringLiteral("walking");
        confidence = 0.70;
    } else if (stdDev > 0.5 && zcr > 0.25 && range > 1.5) {
        // High intensity rhythmic → running
        label = QStringLiteral("running");
        confidence = 0.65;
    } else if (stdDev > 0.2 && stdDev < 0.6 && zcr > 0.3) {
        // Moderate with high frequency → cycling
        label = QStringLiteral("cycling");
        confidence = 0.55;
    } else if (mean < 0.5 && stdDev > 0.1 && stdDev < 0.4) {
        // Low mean (buoyancy) + some movement → swimming
        label = QStringLiteral("swimming");
        confidence = 0.45;
    } else {
        label = QStringLiteral("unknown");
        confidence = 0.30;
    }

    result[QStringLiteral("label")]      = label;
    result[QStringLiteral("confidence")] = confidence;

    // Softmax-style probability distribution
    QVariantMap probs;
    QStringList labels = {QStringLiteral("idle"), QStringLiteral("walking"),
                          QStringLiteral("running"), QStringLiteral("cycling"),
                          QStringLiteral("swimming"), QStringLiteral("unknown")};
    for (const QString &l : labels) {
        probs[l] = (l == label) ? confidence : (1.0 - confidence) / 5.0;
    }
    result[QStringLiteral("probabilities")] = probs;

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// SleepStagingModel
// ═══════════════════════════════════════════════════════════════════════════

bool SleepStagingModel::loadModel(const QString &modelPath)
{
    if (QFile::exists(modelPath)) {
        // TODO: Load TFLite model
        m_loaded = true;
        return true;
    }

    // Heuristic fallback always available
    m_loaded = true;
    return true;
}

void SleepStagingModel::unloadModel()
{
    m_loaded = false;
}

QVariantList SleepStagingModel::inputSpec() const
{
    QVariantMap accelSpec;
    accelSpec[QStringLiteral("name")]  = QStringLiteral("movement");
    accelSpec[QStringLiteral("shape")] = QVariantList{30};  // 30 values per epoch
    accelSpec[QStringLiteral("dtype")] = QStringLiteral("float32");

    QVariantMap hrSpec;
    hrSpec[QStringLiteral("name")]  = QStringLiteral("heart_rate");
    hrSpec[QStringLiteral("shape")] = QVariantList{30};
    hrSpec[QStringLiteral("dtype")] = QStringLiteral("float32");

    return {accelSpec, hrSpec};
}

QVariantList SleepStagingModel::outputSpec() const
{
    QVariantMap spec;
    spec[QStringLiteral("name")]   = QStringLiteral("stage");
    spec[QStringLiteral("shape")]  = QVariantList{4};
    spec[QStringLiteral("dtype")]  = QStringLiteral("float32");
    spec[QStringLiteral("labels")] = QVariantList{
        QStringLiteral("awake"),
        QStringLiteral("light"),
        QStringLiteral("deep"),
        QStringLiteral("rem")
    };
    return {spec};
}

QVariantMap SleepStagingModel::predict(const QVariantMap &inputs)
{
    QVariantMap result;

    // Extract movement and HR data
    QVariantList movementRaw = inputs.value(QStringLiteral("movement")).toList();
    QVariantList hrRaw = inputs.value(QStringLiteral("heart_rate")).toList();

    if (movementRaw.isEmpty() || hrRaw.isEmpty()) {
        result[QStringLiteral("label")]      = QStringLiteral("awake");
        result[QStringLiteral("confidence")] = 0.0;
        return result;
    }

    // Compute movement stats
    double movSum = 0.0;
    for (const QVariant &v : movementRaw)
        movSum += v.toDouble();
    double movMean = movSum / movementRaw.size();

    // Compute HR stats
    double hrSum = 0.0;
    for (const QVariant &v : hrRaw)
        hrSum += v.toDouble();
    double hrMean = hrSum / hrRaw.size();

    // HR variability (simple std dev)
    double hrSumSq = 0.0;
    for (const QVariant &v : hrRaw) {
        double diff = v.toDouble() - hrMean;
        hrSumSq += diff * diff;
    }
    double hrStdDev = qSqrt(hrSumSq / hrRaw.size());

    // Context from previous epoch (if provided)
    double avgHR = inputs.value(QStringLiteral("night_avg_hr"), hrMean).toDouble();

    // Heuristic sleep staging (mirrors SleepTracker logic)
    QString stage;
    double confidence;

    if (movMean > 0.15) {
        stage = QStringLiteral("awake");
        confidence = 0.80;
    } else if (movMean < 0.02 && hrMean < avgHR - 8.0) {
        stage = QStringLiteral("deep");
        confidence = 0.75;
    } else if (movMean < 0.05 && hrStdDev > 3.0 && qAbs(hrMean - avgHR) < 5.0) {
        stage = QStringLiteral("rem");
        confidence = 0.60;
    } else {
        stage = QStringLiteral("light");
        confidence = 0.70;
    }

    result[QStringLiteral("label")]      = stage;
    result[QStringLiteral("confidence")] = confidence;

    // Probability distribution
    QVariantMap probs;
    QStringList labels = {QStringLiteral("awake"), QStringLiteral("light"),
                          QStringLiteral("deep"), QStringLiteral("rem")};
    for (const QString &l : labels) {
        probs[l] = (l == stage) ? confidence : (1.0 - confidence) / 3.0;
    }
    result[QStringLiteral("probabilities")] = probs;

    return result;
}
