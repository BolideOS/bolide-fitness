/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Companion app D-Bus service.
 * Provides a well-defined API for phone companion apps to query
 * fitness data, trigger syncs, and receive live updates.
 */

#ifndef COMPANIONSERVICE_H
#define COMPANIONSERVICE_H

#include <QObject>
#include <QDBusConnection>
#include <QVariantMap>
#include <QVariantList>

class Database;
class ExportManager;
class TrendsManager;

/**
 * @brief D-Bus service for companion application communication.
 *
 * Registers on the session bus as:
 *   org.bolide.fitness.Companion
 *   /org/bolide/fitness/Companion
 *
 * Provides methods for companion apps to:
 * - Query workout history and details
 * - Get health metrics and trends
 * - Trigger data export/sync
 * - Get real-time updates via signals
 * - Manage user profile
 *
 * Protocol is designed for use with:
 * - Gadgetbridge (Android companion)
 * - Custom Qt companion apps
 * - CLI tools via dbus-send
 */
class CompanionService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bolide.fitness.Companion")

public:
    explicit CompanionService(QObject *parent = nullptr);
    ~CompanionService() override;

    void setDatabase(Database *db);
    void setExportManager(ExportManager *em);
    void setTrendsManager(TrendsManager *tm);

    /** Register on the session D-Bus. Returns true on success. */
    bool registerService();

    /** Unregister from D-Bus. */
    void unregisterService();

    bool isRegistered() const { return m_registered; }

    // ── D-Bus methods (exported) ────────────────────────────────────────

public slots:
    /** Get application version and capabilities. */
    QVariantMap GetInfo();

    /** Get recent workouts. */
    QVariantList GetRecentWorkouts(int limit);

    /** Get a specific workout by ID. */
    QVariantMap GetWorkout(qlonglong workoutId);

    /** Get GPS track for a workout. */
    QVariantList GetGpsTrack(qlonglong workoutId);

    /** Get workout metrics. */
    QVariantList GetWorkoutMetrics(qlonglong workoutId);

    /** Export a workout to bytes. */
    QByteArray ExportWorkout(qlonglong workoutId, const QString &format);

    /** Get today's health summary. */
    QVariantMap GetTodaySummary();

    /** Get trend data for a metric. */
    QVariantList GetTrendData(const QString &metricId, int days);

    /** Get current baselines. */
    QVariantMap GetBaselines();

    /** Get user profile. */
    QVariantMap GetUserProfile();

    /** Update user profile. */
    bool SetUserProfile(const QVariantMap &profile);

    /** Get sleep history. */
    QVariantList GetSleepHistory(int days);

    /** Get daily metric value. */
    double GetDailyMetric(const QString &date, const QString &metricId);

    /** Trigger a full sync (recompute baselines, etc). */
    void TriggerSync();

    /** Ping — returns "pong" for connectivity check. */
    QString Ping();

signals:
    // D-Bus signals (emitted for companion apps)

    /** Emitted when a workout starts. */
    void WorkoutStarted(const QString &type);

    /** Emitted when a workout completes. */
    void WorkoutCompleted(qlonglong workoutId, const QVariantMap &summary);

    /** Emitted when sleep analysis completes. */
    void SleepAnalysisComplete(const QVariantMap &sleepData);

    /** Emitted when health metrics are updated. */
    void MetricsUpdated(const QVariantMap &metrics);

    /** Emitted on sync completion. */
    void SyncComplete();

private:
    Database *m_database;
    ExportManager *m_exportManager;
    TrendsManager *m_trendsManager;
    bool m_registered;
};

#endif // COMPANIONSERVICE_H
