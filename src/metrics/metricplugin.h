/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef METRICPLUGIN_H
#define METRICPLUGIN_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

/**
 * @brief Abstract interface for metric computation plugins.
 *
 * Each metric is a pure function: given a set of named inputs, produce
 * a numeric result plus optional metadata.
 *
 * Adding a new metric:
 * 1. Subclass MetricPlugin
 * 2. Implement id(), name(), unit(), requiredInputs(), compute()
 * 3. Register with MetricsEngine::registerMetric()
 *
 * Inputs are passed as QVariantMap with standardized keys:
 * - "distance_m"          (double)  Total distance in meters
 * - "time_s"              (double)  Duration in seconds
 * - "hr_avg"              (double)  Average heart rate
 * - "hr_max"              (int)     Max heart rate
 * - "hr_rest"             (int)     Resting heart rate
 * - "elev_start"          (double)  Starting elevation (m)
 * - "elev_end"            (double)  Ending elevation (m)
 * - "elev_gain_m"         (double)  Total elevation gain
 * - "elev_series"         (QVariantList of float)  Smoothed elevation series
 * - "has_barometer"       (bool)    Whether baro data is available
 * - "vo2max_estimate"     (double)  Current VO2max estimate
 * - "temp_c"              (double)  Ambient temperature
 * - "altitude_m"          (double)  Current altitude for altitude adjustment
 * - "age"                 (int)     User age
 * - "weight_kg"           (double)  User weight
 * - "gender"              (QString) "male" or "female"
 * - "segment_data"        (QVariantList)  Per-segment input maps
 * - "hr_series"           (QVariantList of int)  HR time series
 * - "rr_intervals_ms"     (QVariantList of double) R-R intervals for HRV
 * - "steps"               (int)     Step count
 * - "cadence_avg"         (double)  Average cadence
 */
class MetricPlugin
{
public:
    virtual ~MetricPlugin() = default;

    /** Unique metric identifier (e.g., "hill_score", "vo2max_estimate"). */
    virtual QString id() const = 0;

    /** Human-readable name for UI display. */
    virtual QString name() const = 0;

    /** Display unit (e.g., "bpm", "ml/kg/min", "score"). */
    virtual QString unit() const = 0;

    /** Category for grouping (e.g., "performance", "health", "recovery"). */
    virtual QString category() const { return QStringLiteral("general"); }

    /** Inputs that MUST be present for compute() to succeed. */
    virtual QStringList requiredInputs() const = 0;

    /** Inputs that are used if present but not mandatory. */
    virtual QStringList optionalInputs() const { return QStringList(); }

    /**
     * Compute the metric value from given inputs.
     * Returns the computed value, or QVariant() on failure.
     */
    virtual QVariant compute(const QVariantMap &inputs) = 0;

    /**
     * Compute with extended result (value + metadata).
     * Default implementation calls compute() and wraps in a map.
     */
    virtual QVariantMap computeDetailed(const QVariantMap &inputs)
    {
        QVariant result = compute(inputs);
        QVariantMap detailed;
        detailed[QStringLiteral("value")] = result;
        detailed[QStringLiteral("metric_id")] = id();
        detailed[QStringLiteral("unit")] = unit();
        return detailed;
    }

    /** Minimum value for normalization (optional, for 0-100 scores). */
    virtual double minValue() const { return 0.0; }

    /** Maximum value for normalization. */
    virtual double maxValue() const { return 100.0; }
};

#endif // METRICPLUGIN_H
