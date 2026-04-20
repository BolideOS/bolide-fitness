/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef HRVMETRICS_H
#define HRVMETRICS_H

#include "metricplugin.h"

/**
 * @brief RMSSD (Root Mean Square of Successive Differences) HRV metric.
 *
 * Standard time-domain HRV measure computed from R-R intervals.
 * Higher values indicate better parasympathetic (recovery) tone.
 *
 * Input: rr_intervals_ms (QVariantList of double, in milliseconds)
 * Output: RMSSD in milliseconds
 */
class HRVRmssdPlugin : public MetricPlugin
{
public:
    QString id() const override { return QStringLiteral("hrv_rmssd"); }
    QString name() const override { return QStringLiteral("HRV RMSSD"); }
    QString unit() const override { return QStringLiteral("ms"); }
    QString category() const override { return QStringLiteral("recovery"); }

    QStringList requiredInputs() const override {
        return QStringList() << QStringLiteral("rr_intervals_ms");
    }

    QVariant compute(const QVariantMap &inputs) override;

    double minValue() const override { return 0.0; }
    double maxValue() const override { return 200.0; }
};

/**
 * @brief SDNN (Standard Deviation of NN Intervals) HRV metric.
 *
 * Overall HRV marker reflecting both sympathetic and parasympathetic
 * contributions. Computed from R-R intervals.
 *
 * Input: rr_intervals_ms (QVariantList of double)
 * Output: SDNN in milliseconds
 */
class HRVSdnnPlugin : public MetricPlugin
{
public:
    QString id() const override { return QStringLiteral("hrv_sdnn"); }
    QString name() const override { return QStringLiteral("HRV SDNN"); }
    QString unit() const override { return QStringLiteral("ms"); }
    QString category() const override { return QStringLiteral("recovery"); }

    QStringList requiredInputs() const override {
        return QStringList() << QStringLiteral("rr_intervals_ms");
    }

    QVariant compute(const QVariantMap &inputs) override;

    double minValue() const override { return 0.0; }
    double maxValue() const override { return 300.0; }
};

#endif // HRVMETRICS_H
