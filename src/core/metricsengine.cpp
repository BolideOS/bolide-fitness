/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "metricsengine.h"
#include "../metrics/metricplugin.h"
#include <QDebug>

MetricsEngine::MetricsEngine(QObject *parent)
    : QObject(parent)
{
}

MetricsEngine::~MetricsEngine()
{
    qDeleteAll(m_metrics);
}

void MetricsEngine::registerMetric(MetricPlugin *plugin)
{
    if (!plugin) return;
    const QString id = plugin->id();
    if (m_metrics.contains(id)) {
        qWarning() << "MetricsEngine: replacing existing metric:" << id;
        delete m_metrics[id];
    }
    m_metrics[id] = plugin;
    qDebug() << "MetricsEngine: registered metric" << id;
}

void MetricsEngine::unregisterMetric(const QString &id)
{
    if (m_metrics.contains(id)) {
        delete m_metrics.take(id);
    }
}

MetricPlugin *MetricsEngine::metric(const QString &id) const
{
    return m_metrics.value(id, nullptr);
}

QStringList MetricsEngine::registeredMetrics() const
{
    return m_metrics.keys();
}

QVariant MetricsEngine::compute(const QString &metricId, const QVariantMap &inputs)
{
    MetricPlugin *plugin = m_metrics.value(metricId);
    if (!plugin) {
        emit computationError(metricId, QStringLiteral("Metric not registered"));
        return QVariant();
    }

    if (!hasRequiredInputs(plugin, inputs)) {
        emit computationError(metricId, QStringLiteral("Missing required inputs"));
        return QVariant();
    }

    QVariant result = plugin->compute(inputs);
    if (result.isValid()) {
        emit metricComputed(metricId, result);
    }
    return result;
}

QVariantMap MetricsEngine::computeAll(const QVariantMap &inputs)
{
    QVariantMap results;
    for (auto it = m_metrics.constBegin(); it != m_metrics.constEnd(); ++it) {
        if (hasRequiredInputs(it.value(), inputs)) {
            QVariant result = it.value()->compute(inputs);
            if (result.isValid()) {
                results[it.key()] = result;
                emit metricComputed(it.key(), result);
            }
        }
    }
    return results;
}

QVariantMap MetricsEngine::computeSet(const QStringList &metricIds,
                                       const QVariantMap &inputs)
{
    QVariantMap results;
    for (const QString &id : metricIds) {
        MetricPlugin *plugin = m_metrics.value(id);
        if (!plugin) continue;
        if (!hasRequiredInputs(plugin, inputs)) continue;

        QVariant result = plugin->compute(inputs);
        if (result.isValid()) {
            results[id] = result;
            emit metricComputed(id, result);
        }
    }
    return results;
}

bool MetricsEngine::hasRequiredInputs(MetricPlugin *plugin,
                                       const QVariantMap &inputs) const
{
    for (const QString &req : plugin->requiredInputs()) {
        if (!inputs.contains(req)) return false;
    }
    return true;
}
