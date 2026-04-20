/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "mlregistry.h"
#include "mlplugin.h"

#include <QDir>
#include <QFileInfo>

MLRegistry::MLRegistry(QObject *parent)
    : QObject(parent)
{
}

MLRegistry::~MLRegistry()
{
    unloadAll();
    qDeleteAll(m_plugins);
}

// ── Registration ────────────────────────────────────────────────────────────

void MLRegistry::registerPlugin(MLPlugin *plugin)
{
    if (!plugin) return;

    const QString id = plugin->modelId();

    // Replace existing
    if (m_plugins.contains(id)) {
        m_plugins[id]->unloadModel();
        delete m_plugins.take(id);
    }

    m_plugins.insert(id, plugin);
    emit modelsChanged();
}

void MLRegistry::unregisterPlugin(const QString &modelId)
{
    if (!m_plugins.contains(modelId))
        return;

    MLPlugin *p = m_plugins.take(modelId);
    p->unloadModel();
    delete p;
    emit modelsChanged();
}

// ── Discovery ───────────────────────────────────────────────────────────────

int MLRegistry::discoverModels(const QString &modelDir)
{
    QDir dir(modelDir);
    if (!dir.exists())
        return 0;

    int loaded = 0;
    QStringList filters;
    filters << QStringLiteral("*.tflite") << QStringLiteral("*.onnx");

    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fi : files) {
        // For each model file, try to load with registered plugins
        // that aren't yet loaded
        for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
            MLPlugin *p = it.value();
            if (p->isLoaded()) continue;

            // Try loading if the filename matches the model ID
            QString baseName = fi.completeBaseName();
            if (baseName == p->modelId() || baseName.contains(p->modelId())) {
                if (p->loadModel(fi.absoluteFilePath())) {
                    ++loaded;
                    emit modelLoaded(p->modelId());
                } else {
                    emit modelError(p->modelId(),
                                    QStringLiteral("Failed to load: ") + fi.fileName());
                }
            }
        }
    }

    if (loaded > 0)
        emit modelsChanged();

    return loaded;
}

// ── Queries ─────────────────────────────────────────────────────────────────

QVariantList MLRegistry::modelList() const
{
    QVariantList list;
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        MLPlugin *p = it.value();
        QVariantMap info;
        info[QStringLiteral("id")]          = p->modelId();
        info[QStringLiteral("name")]        = p->modelName();
        info[QStringLiteral("version")]     = p->version();
        info[QStringLiteral("category")]    = p->category();
        info[QStringLiteral("description")] = p->description();
        info[QStringLiteral("loaded")]      = p->isLoaded();
        info[QStringLiteral("size")]        = p->modelSize();
        info[QStringLiteral("inferenceMs")] = p->estimatedInferenceMs();

        QVariantList sensors;
        for (const QString &s : p->requiredSensors())
            sensors.append(s);
        info[QStringLiteral("requiredSensors")] = sensors;

        list.append(info);
    }
    return list;
}

MLPlugin *MLRegistry::plugin(const QString &modelId) const
{
    return m_plugins.value(modelId, nullptr);
}

bool MLRegistry::hasModel(const QString &modelId) const
{
    return m_plugins.contains(modelId) && m_plugins[modelId]->isLoaded();
}

bool MLRegistry::isAvailable() const
{
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        if (it.value()->isLoaded())
            return true;
    }
    return false;
}

QVariantList MLRegistry::modelsByCategory(const QString &category) const
{
    QVariantList result;
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        if (it.value()->category() == category) {
            QVariantMap info;
            info[QStringLiteral("id")]      = it.value()->modelId();
            info[QStringLiteral("name")]    = it.value()->modelName();
            info[QStringLiteral("loaded")]  = it.value()->isLoaded();
            result.append(info);
        }
    }
    return result;
}

// ── Inference dispatch ──────────────────────────────────────────────────────

QVariantMap MLRegistry::predict(const QString &modelId, const QVariantMap &inputs)
{
    MLPlugin *p = m_plugins.value(modelId, nullptr);
    if (!p || !p->isLoaded()) {
        emit modelError(modelId, QStringLiteral("Model not loaded"));
        return QVariantMap();
    }

    QVariantMap result = p->predict(inputs);
    emit predictionComplete(modelId, result);
    return result;
}

QVariantMap MLRegistry::predictWindow(const QString &modelId,
                                       const QVector<float> &data,
                                       int windowSize)
{
    MLPlugin *p = m_plugins.value(modelId, nullptr);
    if (!p || !p->isLoaded()) {
        emit modelError(modelId, QStringLiteral("Model not loaded"));
        return QVariantMap();
    }

    QVariantMap result = p->predictWindow(data, windowSize);
    emit predictionComplete(modelId, result);
    return result;
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

int MLRegistry::loadAll(const QString &modelDir)
{
    return discoverModels(modelDir);
}

void MLRegistry::unloadAll()
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value()->isLoaded())
            it.value()->unloadModel();
    }
    emit modelsChanged();
}
