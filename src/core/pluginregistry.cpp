/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "pluginregistry.h"
#include "metricsengine.h"
#include "../metrics/metricplugin.h"
#include "../metrics/hillscore.h"
#include "../metrics/calorieestimate.h"
#include "../metrics/hrvmetrics.h"
#include "../metrics/vo2maxestimate.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

PluginRegistry::PluginRegistry(QObject *parent)
    : QObject(parent)
{
}

PluginRegistry::~PluginRegistry() = default;

bool PluginRegistry::loadConfigs(const QString &configDir)
{
    QDir dir(configDir);
    if (!dir.exists()) {
        qWarning() << "PluginRegistry: config dir not found:" << configDir;
        emit configError(QStringLiteral("Config directory not found: ") + configDir);
        return false;
    }

    bool ok = true;
    ok &= loadWorkoutTypes(dir.absoluteFilePath(QStringLiteral("workouts")));
    ok &= loadScreenLayouts(dir.absoluteFilePath(QStringLiteral("screens")));

    if (ok) emit configsLoaded();
    return ok;
}

void PluginRegistry::registerBuiltinMetrics(MetricsEngine *engine)
{
    engine->registerMetric(new HillScorePlugin());
    engine->registerMetric(new CalorieEstimatePlugin());
    engine->registerMetric(new HRVRmssdPlugin());
    engine->registerMetric(new HRVSdnnPlugin());
    engine->registerMetric(new VO2MaxEstimatePlugin());
    qDebug() << "PluginRegistry: registered" << engine->registeredMetrics().size()
             << "built-in metrics";
}

QVariantList PluginRegistry::workoutTypes() const
{
    QVariantList result;
    for (auto it = m_workoutTypes.constBegin(); it != m_workoutTypes.constEnd(); ++it) {
        result.append(it.value());
    }
    // Sort by sort_order
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()[QStringLiteral("sort_order")].toInt()
             < b.toMap()[QStringLiteral("sort_order")].toInt();
    });
    return result;
}

QVariantMap PluginRegistry::workoutType(const QString &id) const
{
    return m_workoutTypes.value(id);
}

bool PluginRegistry::hasWorkoutType(const QString &id) const
{
    return m_workoutTypes.contains(id);
}

QVariantMap PluginRegistry::screenLayout(const QString &name) const
{
    return m_screenLayouts.value(name);
}

QStringList PluginRegistry::screenLayouts() const
{
    return m_screenLayouts.keys();
}

QStringList PluginRegistry::defaultScreensForWorkout(const QString &workoutTypeId) const
{
    if (!m_workoutTypes.contains(workoutTypeId))
        return QStringList() << QStringLiteral("default_timer") << QStringLiteral("default_hr");

    QVariantMap wt = m_workoutTypes[workoutTypeId];
    QVariantList screens = wt[QStringLiteral("data_screens")].toList();
    QStringList result;
    for (const QVariant &s : screens) {
        result.append(s.toString());
    }
    return result.isEmpty()
        ? (QStringList() << QStringLiteral("default_timer") << QStringLiteral("default_hr"))
        : result;
}

QVariantList PluginRegistry::metricDefinitions() const
{
    // Returns metadata for all workout types' metrics
    QVariantList result;
    for (auto it = m_workoutTypes.constBegin(); it != m_workoutTypes.constEnd(); ++it) {
        QVariantList metrics = it.value()[QStringLiteral("metrics")].toList();
        for (const QVariant &m : metrics) {
            if (!result.contains(m))
                result.append(m);
        }
    }
    return result;
}

bool PluginRegistry::loadWorkoutTypes(const QString &dir)
{
    QDir d(dir);
    if (!d.exists()) {
        qWarning() << "PluginRegistry: workout configs dir not found:" << dir;
        return false;
    }

    QStringList files = d.entryList(QStringList() << QStringLiteral("*.json"),
                                     QDir::Files);
    if (files.isEmpty()) {
        qWarning() << "PluginRegistry: no workout config files in" << dir;
        return false;
    }

    for (const QString &filename : files) {
        QFile file(d.absoluteFilePath(filename));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "PluginRegistry: cannot open" << filename;
            continue;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        file.close();

        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "PluginRegistry: JSON error in" << filename
                       << ":" << parseError.errorString();
            continue;
        }

        if (!doc.isObject()) continue;
        QJsonObject obj = doc.object();

        QString id = obj[QStringLiteral("id")].toString();
        if (id.isEmpty()) {
            // Derive ID from filename
            id = filename;
            id.replace(QStringLiteral(".json"), QString());
        }

        QVariantMap wt = obj.toVariantMap();
        wt[QStringLiteral("id")] = id;
        m_workoutTypes[id] = wt;
    }

    qDebug() << "PluginRegistry: loaded" << m_workoutTypes.size() << "workout types";
    return !m_workoutTypes.isEmpty();
}

bool PluginRegistry::loadScreenLayouts(const QString &dir)
{
    QDir d(dir);
    if (!d.exists()) {
        qWarning() << "PluginRegistry: screen configs dir not found:" << dir;
        return false;
    }

    QStringList files = d.entryList(QStringList() << QStringLiteral("*.json"),
                                     QDir::Files);

    for (const QString &filename : files) {
        QFile file(d.absoluteFilePath(filename));
        if (!file.open(QIODevice::ReadOnly)) continue;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        file.close();

        if (parseError.error != QJsonParseError::NoError) continue;
        if (!doc.isObject()) continue;

        QString name = filename;
        name.replace(QStringLiteral(".json"), QString());

        m_screenLayouts[name] = doc.object().toVariantMap();
    }

    qDebug() << "PluginRegistry: loaded" << m_screenLayouts.size() << "screen layouts";
    return true;
}
