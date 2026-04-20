/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef CALORIEESTIMATE_H
#define CALORIEESTIMATE_H

#include "metricplugin.h"

/**
 * @brief Calorie expenditure estimator using HR-based energy model.
 *
 * Uses the Keytel et al. (2005) equation when HR data is available,
 * falls back to MET-based estimate from workout type and duration.
 *
 * Required: time_s, workout_type
 * Optional: hr_avg, weight_kg, age, gender
 */
class CalorieEstimatePlugin : public MetricPlugin
{
public:
    QString id() const override { return QStringLiteral("calories"); }
    QString name() const override { return QStringLiteral("Calories"); }
    QString unit() const override { return QStringLiteral("kcal"); }
    QString category() const override { return QStringLiteral("general"); }

    QStringList requiredInputs() const override {
        return QStringList()
            << QStringLiteral("time_s");
    }

    QStringList optionalInputs() const override {
        return QStringList()
            << QStringLiteral("hr_avg")
            << QStringLiteral("weight_kg")
            << QStringLiteral("age")
            << QStringLiteral("gender")
            << QStringLiteral("workout_type");
    }

    QVariant compute(const QVariantMap &inputs) override;

private:
    double metForWorkoutType(const QString &type) const;
};

#endif // CALORIEESTIMATE_H
