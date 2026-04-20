/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef GLANCEPROVIDER_H
#define GLANCEPROVIDER_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>

class QDBusConnection;

/**
 * @brief D-Bus service exposing per-screen glance data to the launcher.
 *
 * Each screen in the fitness app can register glance data: a compact
 * representation suitable for display in the bolide-launcher glance list
 * (similar to Garmin Fenix 8 glances).
 *
 * Glance format per screen:
 *   {
 *     "id":        "sleep-overview",
 *     "title":     "Sleep",
 *     "value":     "7h 23m",
 *     "unit":      "",
 *     "label":     "Quality: 85",
 *     "progress":  0.85,        // 0.0–1.0 for progress bar (optional)
 *     "sparkline": [65,70,72,68,75,80,77],  // mini chart data (optional)
 *     "icon":      "ios-moon",
 *     "color":     "#9C27B0"
 *   }
 *
 * D-Bus service: org.bolide.fitness.Glances
 * D-Bus path:    /org/bolide/fitness/Glances
 *
 * Methods:
 *   QVariantList allGlances()
 *   QVariantMap  glance(screenId)
 *
 * Signal:
 *   glanceUpdated(screenId, glanceData)
 */
class GlanceProvider : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bolide.fitness.Glances")

    /** All current glance data as a list of maps. */
    Q_PROPERTY(QVariantList glances READ allGlances NOTIFY glancesChanged)

public:
    static GlanceProvider *instance();

    explicit GlanceProvider(QObject *parent = nullptr);
    ~GlanceProvider() override;

    /** Register this object on the session D-Bus. */
    bool registerService();

    /** Get all glance data. */
    Q_INVOKABLE QVariantList allGlances() const;

    /** Get glance data for a specific screen. */
    Q_INVOKABLE QVariantMap glance(const QString &screenId) const;

    // ── Called by domain singletons to update glance data ───────────────

    /** Update or create glance data for a screen. */
    Q_INVOKABLE void updateGlance(const QString &screenId,
                                  const QVariantMap &data);

    /** Remove glance data for a screen. */
    Q_INVOKABLE void removeGlance(const QString &screenId);

signals:
    void glancesChanged();

    /** Emitted on D-Bus when a specific glance changes. */
    void glanceUpdated(const QString &screenId, const QVariantMap &data);

private:
    static GlanceProvider *s_instance;
    QMap<QString, QVariantMap> m_glances;
};

#endif // GLANCEPROVIDER_H
