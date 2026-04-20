/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ML model registry — manages model discovery, loading, and inference dispatch.
 */

#ifndef MLREGISTRY_H
#define MLREGISTRY_H

#include <QObject>
#include <QMap>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class MLPlugin;

/**
 * @brief Central registry for ML inference models.
 *
 * Discovers .tflite models in the model directory, manages plugin
 * lifecycle, and dispatches inference requests.
 *
 * Model directory structure:
 *   /usr/share/bolide-fitness/models/
 *   ├── activity_recognition.tflite
 *   ├── sleep_staging.tflite
 *   └── hr_anomaly.tflite
 *
 * Each model has a corresponding MLPlugin subclass that knows how
 * to interpret the model's inputs and outputs.
 */
class MLRegistry : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList models READ modelList NOTIFY modelsChanged)
    Q_PROPERTY(bool available READ isAvailable CONSTANT)

public:
    explicit MLRegistry(QObject *parent = nullptr);
    ~MLRegistry() override;

    // ── Registration ────────────────────────────────────────────────────

    /** Register an ML plugin. Registry takes ownership. */
    void registerPlugin(MLPlugin *plugin);

    /** Unregister and delete a plugin by model ID. */
    void unregisterPlugin(const QString &modelId);

    // ── Discovery ───────────────────────────────────────────────────────

    /**
     * Scan model directory and load all discoverable models.
     * @param modelDir  Path to model directory
     * @return Number of models successfully loaded
     */
    Q_INVOKABLE int discoverModels(const QString &modelDir);

    // ── Queries ─────────────────────────────────────────────────────────

    /** Get all registered model metadata. */
    QVariantList modelList() const;

    /** Get a specific plugin by model ID. */
    MLPlugin *plugin(const QString &modelId) const;

    /** Check if a model is registered and loaded. */
    Q_INVOKABLE bool hasModel(const QString &modelId) const;

    /** Check if ML inference is available (any model loaded). */
    bool isAvailable() const;

    /** Get models by category. */
    Q_INVOKABLE QVariantList modelsByCategory(const QString &category) const;

    // ── Inference dispatch ──────────────────────────────────────────────

    /**
     * Run inference on a specific model.
     * @param modelId  Model identifier
     * @param inputs   Input tensor data
     * @return Prediction results, or empty map on failure
     */
    Q_INVOKABLE QVariantMap predict(const QString &modelId, const QVariantMap &inputs);

    /**
     * Run sliding-window inference.
     */
    Q_INVOKABLE QVariantMap predictWindow(const QString &modelId,
                                           const QVector<float> &data,
                                           int windowSize);

    // ── Lifecycle ───────────────────────────────────────────────────────

    /** Load all registered models from their paths. */
    Q_INVOKABLE int loadAll(const QString &modelDir);

    /** Unload all models to free memory. */
    Q_INVOKABLE void unloadAll();

signals:
    void modelsChanged();
    void modelLoaded(const QString &modelId);
    void modelError(const QString &modelId, const QString &error);
    void predictionComplete(const QString &modelId, const QVariantMap &result);

private:
    QMap<QString, MLPlugin*> m_plugins;
};

#endif // MLREGISTRY_H
