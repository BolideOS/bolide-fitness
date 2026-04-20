/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "calorieestimate.h"
#include <cmath>

QVariant CalorieEstimatePlugin::compute(const QVariantMap &inputs)
{
    double timeS = inputs[QStringLiteral("time_s")].toDouble();
    if (timeS <= 0.0) return QVariant();

    double timeMin = timeS / 60.0;
    double hrAvg = inputs.value(QStringLiteral("hr_avg"), 0.0).toDouble();
    double weightKg = inputs.value(QStringLiteral("weight_kg"), 70.0).toDouble();
    int age = inputs.value(QStringLiteral("age"), 30).toInt();
    QString gender = inputs.value(QStringLiteral("gender"), QStringLiteral("male")).toString();

    if (hrAvg > 40.0 && weightKg > 0.0 && age > 0) {
        // Keytel et al. (2005) HR-based calorie equation:
        //   Male:   kcal/min = (-55.0969 + 0.6309*HR + 0.1988*Wt + 0.2017*Age) / 4.184
        //   Female: kcal/min = (-20.4022 + 0.4472*HR - 0.1263*Wt + 0.074*Age) / 4.184
        double kcalPerMin;
        if (gender.toLower() == QLatin1String("female")) {
            kcalPerMin = (-20.4022 + 0.4472 * hrAvg - 0.1263 * weightKg + 0.074 * age) / 4.184;
        } else {
            kcalPerMin = (-55.0969 + 0.6309 * hrAvg + 0.1988 * weightKg + 0.2017 * age) / 4.184;
        }

        if (kcalPerMin < 1.0) kcalPerMin = 1.0; // Minimum 1 kcal/min during exercise
        return qRound(kcalPerMin * timeMin);
    }

    // Fallback: MET-based estimation
    QString workoutType = inputs.value(QStringLiteral("workout_type"), QStringLiteral("other")).toString();
    double met = metForWorkoutType(workoutType);

    // kcal = MET * weight_kg * time_hours
    double timeHours = timeS / 3600.0;
    double kcal = met * weightKg * timeHours;
    return qRound(kcal);
}

double CalorieEstimatePlugin::metForWorkoutType(const QString &type) const
{
    // MET values from Compendium of Physical Activities (Ainsworth et al.)
    if (type == QLatin1String("outdoor_run"))  return 9.8;
    if (type == QLatin1String("indoor_run"))   return 9.0;
    if (type == QLatin1String("treadmill"))    return 8.5;
    if (type == QLatin1String("outdoor_walk")) return 3.9;
    if (type == QLatin1String("indoor_walk"))  return 3.5;
    if (type == QLatin1String("hike"))         return 6.0;
    if (type == QLatin1String("ultra_hike"))   return 7.0;
    if (type == QLatin1String("cycling"))      return 7.5;
    if (type == QLatin1String("swimming"))     return 8.0;
    if (type == QLatin1String("strength"))     return 6.0;
    if (type == QLatin1String("yoga"))         return 3.0;
    if (type == QLatin1String("hiit"))         return 10.0;
    if (type == QLatin1String("rowing"))       return 7.0;
    if (type == QLatin1String("elliptical"))   return 5.0;
    if (type == QLatin1String("skiing"))       return 7.0;
    if (type == QLatin1String("climbing"))     return 8.0;
    return 5.0; // Default: moderate activity
}
