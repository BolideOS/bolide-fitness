/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ML (Machine Learning) plugin interface.
 * Pluggable inference models for on-device predictions.
 */

#ifndef MLPLUGIN_H
#define MLPLUGIN_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

/**
 * @brief Abstract interface for on-device ML inference plugins.
 *
 * ML plugins perform lightweight inference using pre-trained models.
 * Use cases:
 * - Activity recognition (auto-detect workout type from sensor patterns)
 * - Sleep stage classification (improve over rule-based heuristics)
 * - HR anomaly detection (detect arrhythmias, bradycardia, tachycardia)
 * - VO2max prediction refinement
 * - Calorie estimation improvement
 *
 * Models are TFLite (TensorFlow Lite) files stored in:
 *   /usr/share/bolide-fitness/models/<model_id>.tflite
 *
 * Plugin lifecycle:
 * 1. Register with MLRegistry
 * 2. loadModel() called once to initialize
 * 3. predict() called with input tensor data
 * 4. unloadModel() on shutdown
 *
 * Performance constraints (wearable):
 * - Max model size: 2MB
 * - Max inference time: 50ms
 * - Float16/int8 quantized preferred
 */
class MLPlugin
{
public:
    virtual ~MLPlugin() = default;

    /** Unique model identifier. */
    virtual QString modelId() const = 0;

    /** Human-readable name. */
    virtual QString modelName() const = 0;

    /** Model version string. */
    virtual QString version() const = 0;

    /** Model category: "activity", "sleep", "health", "performance". */
    virtual QString category() const = 0;

    /** Description of what this model does. */
    virtual QString description() const = 0;

    // ── Model lifecycle ─────────────────────────────────────────────────

    /**
     * Load the model from the given path.
     * @param modelPath  Path to .tflite or other model file
     * @return true if loaded successfully
     */
    virtual bool loadModel(const QString &modelPath) = 0;

    /** Unload model and free resources. */
    virtual void unloadModel() = 0;

    /** Check if model is loaded and ready for inference. */
    virtual bool isLoaded() const = 0;

    // ── Inference ───────────────────────────────────────────────────────

    /**
     * Input tensor specification.
     * Returns list of { "name": str, "shape": [int], "dtype": str }
     */
    virtual QVariantList inputSpec() const = 0;

    /**
     * Output tensor specification.
     * Returns list of { "name": str, "shape": [int], "dtype": str, "labels": [str] }
     */
    virtual QVariantList outputSpec() const = 0;

    /**
     * Run inference on input data.
     * @param inputs  Map of input tensor name → QVector<float> data
     * @return Map of output tensor name → result value or vector
     */
    virtual QVariantMap predict(const QVariantMap &inputs) = 0;

    /**
     * Run inference on a sliding window of sensor data.
     * Convenience wrapper for time-series models.
     * @param sensorData  Vector of float values (window)
     * @param windowSize  Expected window size
     * @return Prediction map
     */
    virtual QVariantMap predictWindow(const QVector<float> &sensorData, int windowSize)
    {
        QVariantMap inputs;
        QVariantList data;
        data.reserve(sensorData.size());
        for (float v : sensorData)
            data.append(static_cast<double>(v));
        inputs[QStringLiteral("input")] = data;
        inputs[QStringLiteral("window_size")] = windowSize;
        return predict(inputs);
    }

    // ── Model metadata ──────────────────────────────────────────────────

    /** Estimated inference time in milliseconds. */
    virtual int estimatedInferenceMs() const { return 10; }

    /** Model file size in bytes. */
    virtual qint64 modelSize() const { return 0; }

    /** Required input sensors for this model. */
    virtual QStringList requiredSensors() const = 0;
};

#endif // MLPLUGIN_H
