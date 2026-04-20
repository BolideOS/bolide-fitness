/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "hillscore.h"
#include <cmath>
#include <algorithm>
#include <QVariantList>

static inline double clamp(double v, double lo, double hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

QVariant HillScorePlugin::compute(const QVariantMap &inputs)
{
    // Handle segment data: average of per-segment scores
    if (inputs.contains(QStringLiteral("segment_data"))) {
        QVariantList segments = inputs[QStringLiteral("segment_data")].toList();
        if (!segments.isEmpty()) {
            double sum = 0.0;
            int count = 0;
            for (const QVariant &seg : segments) {
                QVariantMap segInputs = seg.toMap();
                double segScore = computeRaw(segInputs);
                if (segScore >= 0.0) {
                    sum += segScore;
                    ++count;
                }
            }
            if (count > 0) {
                return clamp(sum / count, 0.0, 100.0);
            }
        }
    }

    double score = computeRaw(inputs);
    return (score >= 0.0) ? QVariant(score) : QVariant();
}

QVariantMap HillScorePlugin::computeDetailed(const QVariantMap &inputs)
{
    QVariantMap result;
    result[QStringLiteral("metric_id")] = id();
    result[QStringLiteral("unit")] = unit();

    QVariant value = compute(inputs);
    result[QStringLiteral("value")] = value;

    // Add breakdown metadata
    double distanceM = inputs[QStringLiteral("distance_m")].toDouble();
    double timeS = inputs[QStringLiteral("time_s")].toDouble();
    double hrAvg = inputs[QStringLiteral("hr_avg")].toDouble();
    bool hasBaro = inputs.value(QStringLiteral("has_barometer"), false).toBool();

    result[QStringLiteral("distance_m")] = distanceM;
    result[QStringLiteral("time_s")] = timeS;
    result[QStringLiteral("hr_avg")] = hrAvg;
    result[QStringLiteral("has_barometer")] = hasBaro;

    if (distanceM > 0 && timeS > 0) {
        // Compute VAM for display
        double elevGain = 0.0;
        if (hasBaro && inputs.contains(QStringLiteral("elev_series"))) {
            QVariantList series = inputs[QStringLiteral("elev_series")].toList();
            if (series.size() >= 2) {
                QVector<float> smoothed = smoothSeries(series, 5);
                elevGain = qMax(0.0, static_cast<double>(smoothed.last() - smoothed.first()));
            }
        } else {
            double elevEnd = inputs.value(QStringLiteral("elev_end"), 0.0).toDouble();
            double elevStart = inputs.value(QStringLiteral("elev_start"), 0.0).toDouble();
            elevGain = qMax(0.0, elevEnd - elevStart);
        }
        double vam = (elevGain / timeS) * 3600.0;
        result[QStringLiteral("vam")] = vam;
        result[QStringLiteral("elevation_gain_m")] = elevGain;
        result[QStringLiteral("grade_percent")] = (distanceM > 0) ? (elevGain / distanceM * 100.0) : 0.0;
    }

    return result;
}

double HillScorePlugin::computeRaw(const QVariantMap &inputs) const
{
    double distanceM = inputs[QStringLiteral("distance_m")].toDouble();
    double timeS     = inputs[QStringLiteral("time_s")].toDouble();
    double hrAvg     = inputs[QStringLiteral("hr_avg")].toDouble();

    // Validate core inputs
    if (distanceM <= 0.0 || timeS <= 0.0 || hrAvg <= 0.0) {
        return -1.0;
    }

    // Fitness scaling
    double vo2max = inputs.value(QStringLiteral("vo2max_estimate"), 50.0).toDouble();
    double fitnessScale = vo2max / 50.0;

    bool hasBaro = inputs.value(QStringLiteral("has_barometer"), false).toBool();
    double elevGainM = 0.0;
    double hillScore = 0.0;

    if (hasBaro && inputs.contains(QStringLiteral("elev_series"))) {
        // ── Barometer path: use smoothed elevation series ────────────
        QVariantList series = inputs[QStringLiteral("elev_series")].toList();
        QVector<float> smoothed = smoothSeries(series, 5);

        if (smoothed.size() >= 2) {
            elevGainM = qMax(0.0, static_cast<double>(smoothed.last() - smoothed.first()));
        }

        double vam = (elevGainM / timeS) * 3600.0;
        double efficiency = vam / hrAvg;
        hillScore = 0.7 * efficiency + 0.3 * fitnessScale;
    } else {
        // ── Fallback: raw elevation from GPS ─────────────────────────
        double elevStart = inputs.value(QStringLiteral("elev_start"), 0.0).toDouble();
        double elevEnd   = inputs.value(QStringLiteral("elev_end"), 0.0).toDouble();
        double elevGainRaw = elevEnd - elevStart;

        if (elevGainRaw < 0.0) elevGainRaw = 0.0;

        // Sanity cap: elevation gain can't exceed 50% of horizontal distance
        double maxGain = distanceM * 0.5;
        if (elevGainRaw > maxGain) elevGainRaw = maxGain;

        elevGainM = elevGainRaw;
        double vam = (elevGainM / timeS) * 3600.0;
        double efficiency = vam / hrAvg;
        hillScore = 0.8 * efficiency + 0.2 * fitnessScale;
    }

    // ── Grade bonus ─────────────────────────────────────────────────────
    if (distanceM > 0.0) {
        double grade = elevGainM / distanceM;
        hillScore *= (1.0 + grade * 0.5);
    }

    // ── HR intensity correction ─────────────────────────────────────────
    if (inputs.contains(QStringLiteral("hr_max"))) {
        double hrMax = inputs[QStringLiteral("hr_max")].toDouble();
        if (hrMax > 0.0) {
            double hrRatio = hrAvg / hrMax;
            if (hrRatio > 0.90) {
                hillScore *= 0.85;
            } else if (hrRatio > 0.80) {
                hillScore *= 0.93;
            }
        }
    }

    // ── Altitude correction ─────────────────────────────────────────────
    if (inputs.contains(QStringLiteral("altitude_m"))) {
        double altitude = inputs[QStringLiteral("altitude_m")].toDouble();
        if (altitude > 1500.0) {
            double penalty = (altitude - 1500.0) / 300.0 * 0.01;
            hillScore *= (1.0 - penalty);
        }
    }

    // ── Temperature correction ──────────────────────────────────────────
    if (inputs.contains(QStringLiteral("temp_c"))) {
        double tempC = inputs[QStringLiteral("temp_c")].toDouble();
        if (tempC > 25.0) {
            hillScore *= (1.0 - (tempC - 25.0) * 0.005);
        }
        if (tempC < 0.0) {
            hillScore *= (1.0 - fabs(tempC) * 0.003);
        }
    }

    // ── Normalize to 0–100 ──────────────────────────────────────────────
    const double referenceValue = 0.05;
    double normalized = clamp((hillScore / referenceValue) * 100.0, 0.0, 100.0);

    return normalized;
}

QVector<float> HillScorePlugin::smoothSeries(const QVariantList &series, int window) const
{
    QVector<float> raw;
    raw.reserve(series.size());
    for (const QVariant &v : series) {
        raw.append(v.toFloat());
    }

    if (raw.size() <= window) return raw;

    // Simple moving average
    QVector<float> smoothed;
    smoothed.reserve(raw.size());
    int halfW = window / 2;

    for (int i = 0; i < raw.size(); ++i) {
        float sum = 0.0f;
        int count = 0;
        for (int j = i - halfW; j <= i + halfW; ++j) {
            if (j >= 0 && j < raw.size()) {
                sum += raw[j];
                ++count;
            }
        }
        smoothed.append(sum / static_cast<float>(count));
    }

    return smoothed;
}
