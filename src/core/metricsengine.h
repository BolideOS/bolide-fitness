/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef METRICSENGINE_H
#define METRICSENGINE_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QVariantMap>
#include "pluginregistry.h"

class MetricPlugin;

/**
 * @brief Dispatches metric computations to registered plugins.
 *
 * Metrics are pure-function plugins registered by ID. The engine:
 * - Resolves dependencies between metrics
 * - Caches results within a computation cycle
 * - Supports batch computation (compute all applicable metrics at once)
 *
 * Adding a new metric:
 * 1. Subclass MetricPlugin
 * 2. Register with MetricsEngine::registerMetric()
 * 3. Done — the engine auto-discovers inputs and computes when data is available
 */
class MetricsEngine : public QObject
{
    Q_OBJECT

public:
    explicit MetricsEngine(QObject *parent = nullptr);
    ~MetricsEngine() override;

    // ── Metric registration ─────────────────────────────────────────────

    /** Register a metric plugin. Engine takes ownership. */
    void registerMetric(MetricPlugin *plugin);

    /** Unregister and delete a metric by ID. */
    void unregisterMetric(const QString &id);

    /** Get a registered metric plugin by ID. */
    MetricPlugin *metric(const QString &id) const;

    /** List all registered metric IDs. */
    QStringList registeredMetrics() const;

    // ── Computation ─────────────────────────────────────────────────────

    /**
     * Compute a single metric given inputs.
     * Returns QVariant() if metric not found or inputs insufficient.
     */
    QVariant compute(const QString &metricId, const QVariantMap &inputs);

    /**
     * Compute all metrics that have sufficient inputs.
     * Returns map of metricId -> computed value.
     */
    QVariantMap computeAll(const QVariantMap &inputs);

    /**
     * Compute a set of specific metrics.
     * Returns map of metricId -> computed value (only those that succeeded).
     */
    QVariantMap computeSet(const QStringList &metricIds, const QVariantMap &inputs);

signals:
    void metricComputed(const QString &metricId, const QVariant &value);
    void computationError(const QString &metricId, const QString &error);

private:
    bool hasRequiredInputs(MetricPlugin *plugin, const QVariantMap &inputs) const;

    QMap<QString, MetricPlugin*> m_metrics;
};

#endif // METRICSENGINE_H
