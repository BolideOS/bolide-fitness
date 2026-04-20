/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "vo2maxestimate.h"
#include <cmath>
#include <algorithm>

QVariant VO2MaxEstimatePlugin::compute(const QVariantMap &inputs)
{
    double distanceM = inputs[QStringLiteral("distance_m")].toDouble();
    double timeS     = inputs[QStringLiteral("time_s")].toDouble();
    double hrAvg     = inputs[QStringLiteral("hr_avg")].toDouble();

    if (distanceM <= 0.0 || timeS <= 0.0 || hrAvg <= 0.0) return QVariant();

    // Need at least 6 minutes of data for a meaningful estimate
    if (timeS < 360.0) return QVariant();

    // Speed in m/min
    double speedMPerMin = distanceM / (timeS / 60.0);

    // Estimate HR max (Tanaka formula) if not provided
    double hrMax = inputs.value(QStringLiteral("hr_max"), 0.0).toDouble();
    int age = inputs.value(QStringLiteral("age"), 0).toInt();
    if (hrMax <= 0.0) {
        if (age > 0) {
            hrMax = 208.0 - 0.7 * age;  // Tanaka et al. (2001)
        } else {
            hrMax = 190.0;  // Default assumption
        }
    }

    double hrRest = inputs.value(QStringLiteral("hr_rest"), 60.0).toDouble();

    // ── ACSM Running VO2 equation ───────────────────────────────────────
    // VO2 (ml/kg/min) = 0.2 * speed (m/min) + 0.9 * speed * grade + 3.5
    // For flat running (grade ≈ 0): VO2 ≈ 0.2 * speed + 3.5
    double vo2Running = 0.2 * speedMPerMin + 3.5;

    // ── Heart rate reserve correction ───────────────────────────────────
    // %HRR correlates with %VO2R (Swain & Leutholtz)
    double hrReserve = hrMax - hrRest;
    double percentHRR = 0.0;
    if (hrReserve > 0.0) {
        percentHRR = (hrAvg - hrRest) / hrReserve;
    }

    // Estimated VO2max from %HRR and running VO2
    double vo2max;
    if (percentHRR > 0.1 && percentHRR < 1.0) {
        // VO2max ≈ VO2_at_pace / %VO2R
        // %VO2R ≈ %HRR (Swain relationship)
        vo2max = vo2Running / percentHRR;
    } else {
        // Fallback: simplified Uth-Sørensen-Overgaard-Pedersen (2004)
        // VO2max = 15.3 * (HRmax / HRrest)
        vo2max = 15.3 * (hrMax / hrRest);
    }

    // Clamp to physiological range
    vo2max = std::max(15.0, std::min(90.0, vo2max));

    return vo2max;
}
