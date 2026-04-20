/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QDate>
#include <QVector>

struct sqlite3;
struct sqlite3_stmt;

/**
 * @brief SQLite database optimized for wearable sensor data.
 *
 * Design choices:
 * - WAL journal mode for concurrent reads during writes
 * - NORMAL synchronous for balance of safety + speed
 * - Memory-mapped I/O for fast reads
 * - Prepared statements cached for hot paths
 * - Batched inserts via transactions
 *
 * Schema covers:
 * - Workout records with summary metrics
 * - Sensor sample batches (binary ring buffer overflow)
 * - Per-workout computed metrics
 * - Daily aggregated metrics (for trends)
 * - GPS track points
 * - Sleep sessions (Phase 2)
 * - Body metric snapshots (Phase 2)
 */
class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    // ── Lifecycle ───────────────────────────────────────────────────────

    bool open(const QString &path);
    void close();
    bool isOpen() const;

    // ── Workout CRUD ────────────────────────────────────────────────────

    /** Create a new workout record. Returns workout ID or -1 on error. */
    qint64 createWorkout(const QString &type, qint64 startTimeEpoch,
                          const QString &profileId = QString());

    /** Update workout with final stats. */
    bool finishWorkout(qint64 workoutId, qint64 endTimeEpoch,
                       int durationSeconds, double distanceM,
                       double elevationGainM, int calories,
                       int avgHR, int maxHR, double avgPace);

    /** Get workout by ID. */
    QVariantMap getWorkout(qint64 workoutId);

    /** Get recent workouts. */
    QVariantList recentWorkouts(int limit = 20);

    /** Delete a workout and all associated data. */
    bool deleteWorkout(qint64 workoutId);

    // ── Sensor batch storage ────────────────────────────────────────────

    /** Store a batch of sensor samples in a single transaction. */
    bool storeSensorBatch(qint64 workoutId, const QString &sensor,
                          const QVector<QPair<qint64, float>> &samples);

    // ── GPS track storage ───────────────────────────────────────────────

    struct GpsPoint {
        qint64 timestamp;
        double latitude;
        double longitude;
        double altitude;
        float  speed;
        float  bearing;
        float  accuracy;
    };

    bool storeGpsTrack(qint64 workoutId, const QVector<GpsPoint> &points);
    QVector<GpsPoint> getGpsTrack(qint64 workoutId);

    // ── Metric storage ──────────────────────────────────────────────────

    bool storeWorkoutMetric(qint64 workoutId, const QString &metricId,
                            double value, const QVariantMap &metadata = QVariantMap());

    QVariantList getWorkoutMetrics(qint64 workoutId);
    QVariant getWorkoutMetric(qint64 workoutId, const QString &metricId);

    // ── Daily aggregates ────────────────────────────────────────────────

    bool storeDailyMetric(const QDate &date, const QString &metricId,
                          double value, const QVariantMap &metadata = QVariantMap());

    QVariantList getDailyMetrics(const QString &metricId,
                                 const QDate &from, const QDate &to);

    double getDailyMetricValue(const QDate &date, const QString &metricId);

    // ── Sleep sessions (Phase 2 stub) ───────────────────────────────────

    qint64 createSleepSession(qint64 startTime, qint64 endTime);
    bool updateSleepSession(qint64 sessionId, const QVariantMap &data);
    QVariantMap getLatestSleep();
    QVariantList getSleepHistory(int days);

    // ── User profile (for metric calculations) ─────────────────────────

    bool setUserProfile(const QVariantMap &profile);
    QVariantMap getUserProfile();

    // ── Maintenance ─────────────────────────────────────────────────────

    /** Delete data older than given number of days. */
    bool pruneOldData(int keepDays = 365);

    /** Get database size in bytes. */
    qint64 databaseSize() const;

signals:
    void databaseError(const QString &error);

private:
    bool createTables();
    bool execSQL(const char *sql);
    sqlite3_stmt *prepare(const char *sql);
    void finalize(sqlite3_stmt *stmt);
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    QString m_path;
    sqlite3 *m_db;

    // Cached prepared statements for hot paths
    sqlite3_stmt *m_stmtInsertSample;
    sqlite3_stmt *m_stmtInsertGps;
    sqlite3_stmt *m_stmtInsertMetric;
    sqlite3_stmt *m_stmtInsertDailyMetric;
};

#endif // DATABASE_H
