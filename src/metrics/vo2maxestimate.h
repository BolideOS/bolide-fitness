/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef VO2MAXESTIMATE_H
#define VO2MAXESTIMATE_H

#include "metricplugin.h"

/**
 * @brief VO2max estimation from running data using Firstbeat-style model.
 *
 * Uses the relationship between running speed, heart rate, and cardiac drift
 * to estimate maximal oxygen uptake.
 *
 * Required: distance_m, time_s, hr_avg
 * Optional: hr_max, hr_rest, age, gender, weight_kg
 */
class VO2MaxEstimatePlugin : public MetricPlugin
{
public:
    QString id() const override { return QStringLiteral("vo2max_estimate"); }
    QString name() const override { return QStringLiteral("VO2max Estimate"); }
    QString unit() const override { return QStringLiteral("ml/kg/min"); }
    QString category() const override { return QStringLiteral("performance"); }

    QStringList requiredInputs() const override {
        return QStringList()
            << QStringLiteral("distance_m")
            << QStringLiteral("time_s")
            << QStringLiteral("hr_avg");
    }

    QStringList optionalInputs() const override {
        return QStringList()
            << QStringLiteral("hr_max")
            << QStringLiteral("hr_rest")
            << QStringLiteral("age")
            << QStringLiteral("gender")
            << QStringLiteral("weight_kg");
    }

    QVariant compute(const QVariantMap &inputs) override;

    double minValue() const override { return 15.0; }
    double maxValue() const override { return 90.0; }
};

#endif // VO2MAXESTIMATE_H
