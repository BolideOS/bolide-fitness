/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "glanceprovider.h"

#include <QDBusConnection>
#include <QDBusError>

GlanceProvider *GlanceProvider::s_instance = nullptr;

GlanceProvider *GlanceProvider::instance()
{
    if (!s_instance)
        s_instance = new GlanceProvider();
    return s_instance;
}

GlanceProvider::GlanceProvider(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

GlanceProvider::~GlanceProvider()
{
    if (s_instance == this)
        s_instance = nullptr;
}

bool GlanceProvider::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    if (!bus.registerObject(QStringLiteral("/org/bolide/fitness/Glances"),
                            this,
                            QDBusConnection::ExportAllSlots
                            | QDBusConnection::ExportAllSignals
                            | QDBusConnection::ExportAllProperties)) {
        qWarning("GlanceProvider: failed to register D-Bus object: %s",
                 qPrintable(bus.lastError().message()));
        return false;
    }

    /* Service name is shared with CompanionService (org.bolide.fitness.*) */
    if (!bus.registerService(QStringLiteral("org.bolide.fitness.Glances"))) {
        qWarning("GlanceProvider: failed to register D-Bus service: %s",
                 qPrintable(bus.lastError().message()));
        return false;
    }

    return true;
}

QVariantList GlanceProvider::allGlances() const
{
    QVariantList list;
    for (auto it = m_glances.constBegin(); it != m_glances.constEnd(); ++it)
        list.append(it.value());
    return list;
}

QVariantMap GlanceProvider::glance(const QString &screenId) const
{
    return m_glances.value(screenId);
}

void GlanceProvider::updateGlance(const QString &screenId,
                                  const QVariantMap &data)
{
    QVariantMap g = data;
    g[QStringLiteral("id")] = screenId;
    m_glances[screenId] = g;
    emit glanceUpdated(screenId, g);
    emit glancesChanged();
}

void GlanceProvider::removeGlance(const QString &screenId)
{
    if (m_glances.remove(screenId) > 0) {
        emit glancesChanged();
    }
}
