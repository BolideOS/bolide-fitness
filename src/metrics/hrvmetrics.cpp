/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "hrvmetrics.h"
#include <cmath>
#include <QVariantList>

// ─── RMSSD ──────────────────────────────────────────────────────────────────

QVariant HRVRmssdPlugin::compute(const QVariantMap &inputs)
{
    QVariantList rrList = inputs[QStringLiteral("rr_intervals_ms")].toList();
    if (rrList.size() < 3) return QVariant(); // Need at least 3 intervals

    // Filter out physiologically implausible R-R intervals
    QVector<double> rr;
    rr.reserve(rrList.size());
    for (const QVariant &v : rrList) {
        double val = v.toDouble();
        // Valid R-R: 300ms (200bpm) to 2000ms (30bpm)
        if (val >= 300.0 && val <= 2000.0) {
            rr.append(val);
        }
    }

    if (rr.size() < 3) return QVariant();

    // RMSSD = sqrt(mean(successive_differences^2))
    double sumSquaredDiffs = 0.0;
    int count = 0;

    for (int i = 1; i < rr.size(); ++i) {
        double diff = rr[i] - rr[i - 1];
        sumSquaredDiffs += diff * diff;
        ++count;
    }

    if (count == 0) return QVariant();

    double rmssd = sqrt(sumSquaredDiffs / static_cast<double>(count));
    return rmssd;
}

// ─── SDNN ───────────────────────────────────────────────────────────────────

QVariant HRVSdnnPlugin::compute(const QVariantMap &inputs)
{
    QVariantList rrList = inputs[QStringLiteral("rr_intervals_ms")].toList();
    if (rrList.size() < 3) return QVariant();

    // Filter implausible values
    QVector<double> rr;
    rr.reserve(rrList.size());
    for (const QVariant &v : rrList) {
        double val = v.toDouble();
        if (val >= 300.0 && val <= 2000.0) {
            rr.append(val);
        }
    }

    if (rr.size() < 3) return QVariant();

    // Compute mean
    double sum = 0.0;
    for (double val : rr) sum += val;
    double mean = sum / static_cast<double>(rr.size());

    // Compute standard deviation
    double sumSqDev = 0.0;
    for (double val : rr) {
        double dev = val - mean;
        sumSqDev += dev * dev;
    }

    double sdnn = sqrt(sumSqDev / static_cast<double>(rr.size()));
    return sdnn;
}
