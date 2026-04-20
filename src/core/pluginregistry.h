/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PLUGINREGISTRY_H
#define PLUGINREGISTRY_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QVariantList>

class MetricPlugin;
class MetricsEngine;

/**
 * @brief Central registry for all pluggable components.
 *
 * Manages:
 * - Metric plugins (computational algorithms)
 * - Workout type definitions (loaded from JSON)
 * - Data screen layouts (loaded from JSON)
 * - Export format plugins (Phase 3)
 *
 * Plugin discovery:
 * - Built-in plugins are registered at startup
 * - JSON-defined plugins are loaded from /usr/share/bolide-fitness/configs/
 * - User overrides from ~/.config/bolide-fitness/configs/ (future)
 */
class PluginRegistry : public QObject
{
    Q_OBJECT

public:
    explicit PluginRegistry(QObject *parent = nullptr);
    ~PluginRegistry() override;

    // ── Initialization ──────────────────────────────────────────────────

    /** Load JSON configs from a directory. */
    bool loadConfigs(const QString &configDir);

    /** Register all built-in metric plugins with the given engine. */
    void registerBuiltinMetrics(MetricsEngine *engine);

    // ── Workout type queries ────────────────────────────────────────────

    /** Get all loaded workout type definitions. */
    QVariantList workoutTypes() const;

    /** Get a specific workout type definition. */
    QVariantMap workoutType(const QString &id) const;

    /** Check if a workout type ID is valid. */
    bool hasWorkoutType(const QString &id) const;

    // ── Screen layout queries ───────────────────────────────────────────

    /** Get screen layout definition by name. */
    QVariantMap screenLayout(const QString &name) const;

    /** Get all available screen layout names. */
    QStringList screenLayouts() const;

    /** Get default screens for a workout type. */
    QStringList defaultScreensForWorkout(const QString &workoutTypeId) const;

    // ── Metric plugin metadata ──────────────────────────────────────────

    /** Get metadata for all registered metrics. */
    QVariantList metricDefinitions() const;

signals:
    void configsLoaded();
    void configError(const QString &error);

private:
    bool loadWorkoutTypes(const QString &dir);
    bool loadScreenLayouts(const QString &dir);

    QMap<QString, QVariantMap> m_workoutTypes;
    QMap<QString, QVariantMap> m_screenLayouts;
};

#endif // PLUGINREGISTRY_H
