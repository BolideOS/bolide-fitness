/*
 * Copyright (C) 2026 BolideOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * QML plugin registration for BolideMetrics 1.0
 */

#include <QQmlExtensionPlugin>
#include <QtQml>

#include "metricsclient.h"

class BolideMetricsPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("BolideMetrics"));

        // MetricsService: instantiate in QML for D-Bus subscriptions
        qmlRegisterType<MetricsClient>(uri, 1, 0, "MetricsService");
    }
};

#include "plugin.moc"
