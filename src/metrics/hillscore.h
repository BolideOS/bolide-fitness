/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef HILLSCORE_H
#define HILLSCORE_H

#include "metricplugin.h"

/**
 * @brief Hill Score metric (0–100) measuring uphill climbing efficiency.
 *
 * Combines VAM (Vertical Ascent Rate), cardiac efficiency, fitness scaling,
 * and environmental corrections (altitude, temperature, HR intensity).
 *
 * When barometer data is available, uses smoothed elevation series for
 * accurate gain calculation. Falls back to raw GPS elevation with sanity
 * clamping when barometer is absent.
 *
 * Supports per-segment computation for long hikes/runs with segment_data.
 *
 * Inputs:
 *   Required: distance_m, time_s, hr_avg
 *   Optional: elev_start, elev_end, elev_series[], has_barometer,
 *             vo2max_estimate, temp_c, altitude_m, hr_max, segment_data[]
 */
class HillScorePlugin : public MetricPlugin
{
public:
    QString id() const override { return QStringLiteral("hill_score"); }
    QString name() const override { return QStringLiteral("Hill Score"); }
    QString unit() const override { return QStringLiteral("score"); }
    QString category() const override { return QStringLiteral("performance"); }

    QStringList requiredInputs() const override {
        return QStringList()
            << QStringLiteral("distance_m")
            << QStringLiteral("time_s")
            << QStringLiteral("hr_avg");
    }

    QStringList optionalInputs() const override {
        return QStringList()
            << QStringLiteral("elev_start")
            << QStringLiteral("elev_end")
            << QStringLiteral("elev_series")
            << QStringLiteral("has_barometer")
            << QStringLiteral("vo2max_estimate")
            << QStringLiteral("temp_c")
            << QStringLiteral("altitude_m")
            << QStringLiteral("hr_max")
            << QStringLiteral("segment_data");
    }

    QVariant compute(const QVariantMap &inputs) override;

    QVariantMap computeDetailed(const QVariantMap &inputs) override;

private:
    double computeRaw(const QVariantMap &inputs) const;
    QVector<float> smoothSeries(const QVariantList &series, int window) const;
};

#endif // HILLSCORE_H
