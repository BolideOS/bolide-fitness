/*
 * bolide-fitness – Modular fitness & health application for BolideOS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QGuiApplication>
#include <QQuickView>
#include <QScreen>
#include <QtQml>

#include <asteroidapp.h>

#include "core/sensorservice.h"
#include "core/datapipeline.h"
#include "core/database.h"
#include "core/metricsengine.h"
#include "core/pluginregistry.h"
#include "core/screenregistry.h"
#include "core/glanceprovider.h"
#include "domain/workoutmanager.h"
#include "domain/sleeptracker.h"
#include "domain/readinessscore.h"
#include "domain/bodymetrics.h"
#include "domain/trendsmanager.h"
#include "domain/sensorslabmanager.h"
#include "export/exportmanager.h"
#include "ml/mlregistry.h"
#include "ml/builtinmodels.h"
#include "companion/companionservice.h"
#include "companion/gadgetbridgesync.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(AsteroidApp::application(argc, argv));
    QScopedPointer<QQuickView> view(AsteroidApp::createView());

    /* ── Determine config & data paths ─────────────────────────────── */
    const QString configDir = QStringLiteral("/usr/share/bolide-fitness/configs");
    const QString modelDir  = QStringLiteral("/usr/share/bolide-fitness/models");
    const QString dataDir   = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + QStringLiteral("/fitness.db");

    /* ── Instantiate services ──────────────────────────────────────── */
    SensorService   sensorService;
    DataPipeline    dataPipeline(&sensorService);
    Database        database;
    MetricsEngine   metricsEngine;
    PluginRegistry  registry;

    /* Open database (creates schema on first run) */
    if (!database.open(dbPath))
        qWarning("bolide-fitness: failed to open database at %s",
                 qPrintable(dbPath));

    /* Load workout-type & screen configs from JSON */
    registry.loadConfigs(configDir);

    /* Register built-in metric plugins */
    registry.registerBuiltinMetrics(&metricsEngine);

    /* ── Load navigation layout ────────────────────────────────────── */
    ScreenRegistry *screenRegistry = ScreenRegistry::instance();
    screenRegistry->loadLayout(configDir, dataDir);

    /* ── Create glance provider (D-Bus) ────────────────────────────── */
    GlanceProvider *glanceProvider = GlanceProvider::instance();
    glanceProvider->registerService();

    /* ── Create WorkoutManager singleton ───────────────────────────── */
    WorkoutManager *wm = WorkoutManager::instance();
    wm->setSensorService(&sensorService);
    wm->setDataPipeline(&dataPipeline);
    wm->setDatabase(&database);
    wm->setMetricsEngine(&metricsEngine);
    wm->setPluginRegistry(&registry);

    /* ── Create Phase 2 singletons ─────────────────────────────────── */
    static SleepTracker sleepTracker;
    sleepTracker.setDatabase(&database);
    sleepTracker.setSensorService(&sensorService);
    sleepTracker.setDataPipeline(&dataPipeline);
    sleepTracker.loadLastNight();

    static ReadinessScore readinessScore;
    readinessScore.setDatabase(&database);
    readinessScore.setSleepTracker(&sleepTracker);
    readinessScore.loadToday();

    static BodyMetrics bodyMetrics;
    bodyMetrics.setSensorService(&sensorService);
    bodyMetrics.setDataPipeline(&dataPipeline);
    bodyMetrics.setDatabase(&database);
    bodyMetrics.loadDailyMetrics();

    /* Auto-recompute readiness after sleep analysis */
    QObject::connect(&sleepTracker, &SleepTracker::morningAnalysisComplete,
                     &readinessScore, &ReadinessScore::recompute);

    /* ── Create Phase 3 singletons ─────────────────────────────────── */
    static TrendsManager trendsManager;
    trendsManager.setDatabase(&database);

    static ExportManager exportManager;
    exportManager.setDatabase(&database);
    exportManager.registerBuiltins();

    static SensorsLabManager sensorsLab;
    sensorsLab.setSensorService(&sensorService);

    /* ── Create Phase 4 singletons ─────────────────────────────────── */
    static MLRegistry mlRegistry;
    mlRegistry.registerPlugin(new ActivityRecognitionModel());
    mlRegistry.registerPlugin(new SleepStagingModel());
    mlRegistry.discoverModels(modelDir);

    static CompanionService companionService;
    companionService.setDatabase(&database);
    companionService.setExportManager(&exportManager);
    companionService.setTrendsManager(&trendsManager);
    companionService.registerService();

    static GadgetbridgeSync gadgetbridgeSync;
    gadgetbridgeSync.setDatabase(&database);
    gadgetbridgeSync.setExportManager(&exportManager);
    gadgetbridgeSync.initialize();

    /* Wire workout completion → companion notification */
    QObject::connect(wm, &WorkoutManager::workoutStopped,
                     [&](qint64 workoutId) {
        QVariantMap summary = database.getWorkout(workoutId);
        companionService.WorkoutCompleted(workoutId, summary);
    });

    /* Wire sleep analysis → companion notification */
    QObject::connect(&sleepTracker, &SleepTracker::morningAnalysisComplete,
                     [&]() {
        QVariantMap sleepData = database.getLatestSleep();
        companionService.SleepAnalysisComplete(sleepData);
    });

    /* ── Register QML types ────────────────────────────────────────── */
    qmlRegisterSingletonType<WorkoutManager>(
        "org.bolide.fitness", 1, 0, "WorkoutManager",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(WorkoutManager::instance(),
                                           QQmlEngine::CppOwnership);
            return WorkoutManager::instance();
        });

    qmlRegisterSingletonType<SleepTracker>(
        "org.bolide.fitness", 1, 0, "SleepTracker",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(&sleepTracker,
                                           QQmlEngine::CppOwnership);
            return &sleepTracker;
        });

    qmlRegisterSingletonType<ReadinessScore>(
        "org.bolide.fitness", 1, 0, "ReadinessScore",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(&readinessScore,
                                           QQmlEngine::CppOwnership);
            return &readinessScore;
        });

    qmlRegisterSingletonType<BodyMetrics>(
        "org.bolide.fitness", 1, 0, "BodyMetrics",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(&bodyMetrics,
                                           QQmlEngine::CppOwnership);
            return &bodyMetrics;
        });

    qmlRegisterSingletonType<TrendsManager>(
        "org.bolide.fitness", 1, 0, "TrendsManager",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(&trendsManager,
                                           QQmlEngine::CppOwnership);
            return &trendsManager;
        });

    qmlRegisterSingletonType<ExportManager>(
        "org.bolide.fitness", 1, 0, "ExportManager",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(&exportManager,
                                           QQmlEngine::CppOwnership);
            return &exportManager;
        });

    qmlRegisterSingletonType<SensorsLabManager>(
        "org.bolide.fitness", 1, 0, "SensorsLab",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(&sensorsLab,
                                           QQmlEngine::CppOwnership);
            return &sensorsLab;
        });

    qmlRegisterSingletonType<MLRegistry>(
        "org.bolide.fitness", 1, 0, "MLRegistry",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(&mlRegistry,
                                           QQmlEngine::CppOwnership);
            return &mlRegistry;
        });

    qmlRegisterSingletonType<GadgetbridgeSync>(
        "org.bolide.fitness", 1, 0, "GadgetbridgeSync",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(&gadgetbridgeSync,
                                           QQmlEngine::CppOwnership);
            return &gadgetbridgeSync;
        });

    qmlRegisterSingletonType<ScreenRegistry>(
        "org.bolide.fitness", 1, 0, "ScreenRegistry",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(ScreenRegistry::instance(),
                                           QQmlEngine::CppOwnership);
            return ScreenRegistry::instance();
        });

    qmlRegisterSingletonType<GlanceProvider>(
        "org.bolide.fitness", 1, 0, "GlanceProvider",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            QQmlEngine::setObjectOwnership(GlanceProvider::instance(),
                                           QQmlEngine::CppOwnership);
            return GlanceProvider::instance();
        });

    /* ── Show view ─────────────────────────────────────────────────── */
    view->setSource(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    view->resize(app->primaryScreen()->size());
    view->show();

    return app->exec();
}
