/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "database.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <sqlite3.h>

// ─── SQL Schema ─────────────────────────────────────────────────────────────

static const char *SCHEMA_SQL = R"SQL(
CREATE TABLE IF NOT EXISTS workouts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,
    start_time INTEGER NOT NULL,
    end_time INTEGER,
    duration_seconds INTEGER,
    distance_m REAL DEFAULT 0,
    elevation_gain_m REAL DEFAULT 0,
    calories INTEGER DEFAULT 0,
    avg_hr INTEGER DEFAULT 0,
    max_hr INTEGER DEFAULT 0,
    avg_pace REAL DEFAULT 0,
    profile_id TEXT,
    metadata TEXT
);

CREATE TABLE IF NOT EXISTS workout_samples (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    workout_id INTEGER NOT NULL,
    sensor TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    value REAL,
    FOREIGN KEY (workout_id) REFERENCES workouts(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS workout_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    workout_id INTEGER NOT NULL,
    metric_id TEXT NOT NULL,
    value REAL,
    metadata TEXT,
    FOREIGN KEY (workout_id) REFERENCES workouts(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS daily_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    date TEXT NOT NULL,
    metric_id TEXT NOT NULL,
    value REAL,
    metadata TEXT,
    UNIQUE(date, metric_id)
);

CREATE TABLE IF NOT EXISTS gps_tracks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    workout_id INTEGER NOT NULL,
    timestamp INTEGER NOT NULL,
    latitude REAL NOT NULL,
    longitude REAL NOT NULL,
    altitude REAL,
    speed REAL,
    bearing REAL,
    accuracy REAL,
    FOREIGN KEY (workout_id) REFERENCES workouts(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS sleep_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    start_time INTEGER NOT NULL,
    end_time INTEGER NOT NULL,
    duration_minutes INTEGER,
    quality_score REAL,
    deep_minutes INTEGER,
    light_minutes INTEGER,
    rem_minutes INTEGER,
    awake_minutes INTEGER,
    metadata TEXT
);

CREATE TABLE IF NOT EXISTS user_profile (
    key TEXT PRIMARY KEY,
    value TEXT
);

CREATE INDEX IF NOT EXISTS idx_workout_samples_workout
    ON workout_samples(workout_id, sensor);
CREATE INDEX IF NOT EXISTS idx_workout_samples_time
    ON workout_samples(timestamp);
CREATE INDEX IF NOT EXISTS idx_workout_metrics_workout
    ON workout_metrics(workout_id, metric_id);
CREATE INDEX IF NOT EXISTS idx_daily_metrics_date
    ON daily_metrics(date, metric_id);
CREATE INDEX IF NOT EXISTS idx_gps_tracks_workout
    ON gps_tracks(workout_id);
CREATE INDEX IF NOT EXISTS idx_sleep_sessions_time
    ON sleep_sessions(start_time);
)SQL";

static const char *PRAGMAS_SQL = R"SQL(
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA cache_size = -2000;
PRAGMA temp_store = MEMORY;
PRAGMA mmap_size = 8388608;
PRAGMA foreign_keys = ON;
)SQL";

// ─── Helper macros ──────────────────────────────────────────────────────────

#define CHECK_DB if (!m_db) { emit databaseError("Database not open"); return false; }
#define CHECK_DB_VAL(val) if (!m_db) { emit databaseError("Database not open"); return val; }

// ─── Database implementation ────────────────────────────────────────────────

Database::Database(QObject *parent)
    : QObject(parent)
    , m_db(nullptr)
    , m_stmtInsertSample(nullptr)
    , m_stmtInsertGps(nullptr)
    , m_stmtInsertMetric(nullptr)
    , m_stmtInsertDailyMetric(nullptr)
{
}

Database::~Database()
{
    close();
}

bool Database::open(const QString &path)
{
    if (m_db) close();

    // Ensure directory exists
    QFileInfo fi(path);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));

    m_path = path;
    int rc = sqlite3_open(path.toUtf8().constData(), &m_db);
    if (rc != SQLITE_OK) {
        emit databaseError(QString::fromUtf8(sqlite3_errmsg(m_db)));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Apply performance pragmas
    execSQL(PRAGMAS_SQL);

    // Create tables
    if (!createTables()) {
        close();
        return false;
    }

    // Prepare hot-path statements
    m_stmtInsertSample = prepare(
        "INSERT INTO workout_samples (workout_id, sensor, timestamp, value) "
        "VALUES (?, ?, ?, ?)");
    m_stmtInsertGps = prepare(
        "INSERT INTO gps_tracks (workout_id, timestamp, latitude, longitude, "
        "altitude, speed, bearing, accuracy) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    m_stmtInsertMetric = prepare(
        "INSERT INTO workout_metrics (workout_id, metric_id, value, metadata) "
        "VALUES (?, ?, ?, ?)");
    m_stmtInsertDailyMetric = prepare(
        "INSERT OR REPLACE INTO daily_metrics (date, metric_id, value, metadata) "
        "VALUES (?, ?, ?, ?)");

    qDebug() << "Database opened:" << path;
    return true;
}

void Database::close()
{
    finalize(m_stmtInsertSample);
    finalize(m_stmtInsertGps);
    finalize(m_stmtInsertMetric);
    finalize(m_stmtInsertDailyMetric);
    m_stmtInsertSample = nullptr;
    m_stmtInsertGps = nullptr;
    m_stmtInsertMetric = nullptr;
    m_stmtInsertDailyMetric = nullptr;

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::isOpen() const
{
    return m_db != nullptr;
}

// ─── Workout CRUD ───────────────────────────────────────────────────────────

qint64 Database::createWorkout(const QString &type, qint64 startTimeEpoch,
                                const QString &profileId)
{
    CHECK_DB_VAL(-1);

    sqlite3_stmt *stmt = prepare(
        "INSERT INTO workouts (type, start_time, profile_id) VALUES (?, ?, ?)");
    if (!stmt) return -1;

    sqlite3_bind_text(stmt, 1, type.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, startTimeEpoch);
    sqlite3_bind_text(stmt, 3, profileId.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    qint64 id = (rc == SQLITE_DONE) ? sqlite3_last_insert_rowid(m_db) : -1;
    finalize(stmt);

    if (id < 0) {
        emit databaseError(QStringLiteral("Failed to create workout"));
    }
    return id;
}

bool Database::finishWorkout(qint64 workoutId, qint64 endTimeEpoch,
                              int durationSeconds, double distanceM,
                              double elevationGainM, int calories,
                              int avgHR, int maxHR, double avgPace)
{
    CHECK_DB;

    sqlite3_stmt *stmt = prepare(
        "UPDATE workouts SET end_time=?, duration_seconds=?, distance_m=?, "
        "elevation_gain_m=?, calories=?, avg_hr=?, max_hr=?, avg_pace=? "
        "WHERE id=?");
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, endTimeEpoch);
    sqlite3_bind_int(stmt, 2, durationSeconds);
    sqlite3_bind_double(stmt, 3, distanceM);
    sqlite3_bind_double(stmt, 4, elevationGainM);
    sqlite3_bind_int(stmt, 5, calories);
    sqlite3_bind_int(stmt, 6, avgHR);
    sqlite3_bind_int(stmt, 7, maxHR);
    sqlite3_bind_double(stmt, 8, avgPace);
    sqlite3_bind_int64(stmt, 9, workoutId);

    int rc = sqlite3_step(stmt);
    finalize(stmt);
    return rc == SQLITE_DONE;
}

QVariantMap Database::getWorkout(qint64 workoutId)
{
    CHECK_DB_VAL(QVariantMap());

    sqlite3_stmt *stmt = prepare(
        "SELECT id, type, start_time, end_time, duration_seconds, distance_m, "
        "elevation_gain_m, calories, avg_hr, max_hr, avg_pace, profile_id, metadata "
        "FROM workouts WHERE id=?");
    if (!stmt) return QVariantMap();

    sqlite3_bind_int64(stmt, 1, workoutId);

    QVariantMap result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result[QStringLiteral("id")] = static_cast<qint64>(sqlite3_column_int64(stmt, 0));
        result[QStringLiteral("type")] = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        result[QStringLiteral("start_time")] = static_cast<qint64>(sqlite3_column_int64(stmt, 2));
        result[QStringLiteral("end_time")] = static_cast<qint64>(sqlite3_column_int64(stmt, 3));
        result[QStringLiteral("duration_seconds")] = sqlite3_column_int(stmt, 4);
        result[QStringLiteral("distance_m")] = sqlite3_column_double(stmt, 5);
        result[QStringLiteral("elevation_gain_m")] = sqlite3_column_double(stmt, 6);
        result[QStringLiteral("calories")] = sqlite3_column_int(stmt, 7);
        result[QStringLiteral("avg_hr")] = sqlite3_column_int(stmt, 8);
        result[QStringLiteral("max_hr")] = sqlite3_column_int(stmt, 9);
        result[QStringLiteral("avg_pace")] = sqlite3_column_double(stmt, 10);
        result[QStringLiteral("profile_id")] = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11)));
    }
    finalize(stmt);
    return result;
}

QVariantList Database::recentWorkouts(int limit)
{
    CHECK_DB_VAL(QVariantList());

    sqlite3_stmt *stmt = prepare(
        "SELECT id, type, start_time, end_time, duration_seconds, distance_m, "
        "elevation_gain_m, calories, avg_hr, max_hr, avg_pace, profile_id "
        "FROM workouts ORDER BY start_time DESC LIMIT ?");
    if (!stmt) return QVariantList();

    sqlite3_bind_int(stmt, 1, limit);

    QVariantList result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row[QStringLiteral("id")] = static_cast<qint64>(sqlite3_column_int64(stmt, 0));
        row[QStringLiteral("type")] = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        row[QStringLiteral("start_time")] = static_cast<qint64>(sqlite3_column_int64(stmt, 2));
        row[QStringLiteral("end_time")] = static_cast<qint64>(sqlite3_column_int64(stmt, 3));
        row[QStringLiteral("duration_seconds")] = sqlite3_column_int(stmt, 4);
        row[QStringLiteral("distance_m")] = sqlite3_column_double(stmt, 5);
        row[QStringLiteral("elevation_gain_m")] = sqlite3_column_double(stmt, 6);
        row[QStringLiteral("calories")] = sqlite3_column_int(stmt, 7);
        row[QStringLiteral("avg_hr")] = sqlite3_column_int(stmt, 8);
        row[QStringLiteral("max_hr")] = sqlite3_column_int(stmt, 9);
        row[QStringLiteral("avg_pace")] = sqlite3_column_double(stmt, 10);
        row[QStringLiteral("profile_id")] = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11)));
        result.append(row);
    }
    finalize(stmt);
    return result;
}

bool Database::deleteWorkout(qint64 workoutId)
{
    CHECK_DB;
    // CASCADE deletes samples, metrics, and gps tracks
    sqlite3_stmt *stmt = prepare("DELETE FROM workouts WHERE id=?");
    if (!stmt) return false;
    sqlite3_bind_int64(stmt, 1, workoutId);
    int rc = sqlite3_step(stmt);
    finalize(stmt);
    return rc == SQLITE_DONE;
}

// ─── Sensor batch storage ───────────────────────────────────────────────────

bool Database::storeSensorBatch(qint64 workoutId, const QString &sensor,
                                 const QVector<QPair<qint64, float>> &samples)
{
    CHECK_DB;
    if (!m_stmtInsertSample || samples.isEmpty()) return false;

    beginTransaction();

    QByteArray sensorUtf8 = sensor.toUtf8();
    for (const auto &pair : samples) {
        sqlite3_reset(m_stmtInsertSample);
        sqlite3_bind_int64(m_stmtInsertSample, 1, workoutId);
        sqlite3_bind_text(m_stmtInsertSample, 2, sensorUtf8.constData(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(m_stmtInsertSample, 3, pair.first);
        sqlite3_bind_double(m_stmtInsertSample, 4, pair.second);

        if (sqlite3_step(m_stmtInsertSample) != SQLITE_DONE) {
            rollbackTransaction();
            return false;
        }
    }

    return commitTransaction();
}

// ─── GPS track ──────────────────────────────────────────────────────────────

bool Database::storeGpsTrack(qint64 workoutId, const QVector<GpsPoint> &points)
{
    CHECK_DB;
    if (!m_stmtInsertGps || points.isEmpty()) return false;

    beginTransaction();

    for (const GpsPoint &p : points) {
        sqlite3_reset(m_stmtInsertGps);
        sqlite3_bind_int64(m_stmtInsertGps, 1, workoutId);
        sqlite3_bind_int64(m_stmtInsertGps, 2, p.timestamp);
        sqlite3_bind_double(m_stmtInsertGps, 3, p.latitude);
        sqlite3_bind_double(m_stmtInsertGps, 4, p.longitude);
        sqlite3_bind_double(m_stmtInsertGps, 5, p.altitude);
        sqlite3_bind_double(m_stmtInsertGps, 6, p.speed);
        sqlite3_bind_double(m_stmtInsertGps, 7, p.bearing);
        sqlite3_bind_double(m_stmtInsertGps, 8, p.accuracy);

        if (sqlite3_step(m_stmtInsertGps) != SQLITE_DONE) {
            rollbackTransaction();
            return false;
        }
    }

    return commitTransaction();
}

QVector<Database::GpsPoint> Database::getGpsTrack(qint64 workoutId)
{
    QVector<GpsPoint> result;
    CHECK_DB_VAL(result);

    sqlite3_stmt *stmt = prepare(
        "SELECT timestamp, latitude, longitude, altitude, speed, bearing, accuracy "
        "FROM gps_tracks WHERE workout_id=? ORDER BY timestamp");
    if (!stmt) return result;

    sqlite3_bind_int64(stmt, 1, workoutId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GpsPoint p;
        p.timestamp = sqlite3_column_int64(stmt, 0);
        p.latitude  = sqlite3_column_double(stmt, 1);
        p.longitude = sqlite3_column_double(stmt, 2);
        p.altitude  = sqlite3_column_double(stmt, 3);
        p.speed     = static_cast<float>(sqlite3_column_double(stmt, 4));
        p.bearing   = static_cast<float>(sqlite3_column_double(stmt, 5));
        p.accuracy  = static_cast<float>(sqlite3_column_double(stmt, 6));
        result.append(p);
    }
    finalize(stmt);
    return result;
}

// ─── Metrics ────────────────────────────────────────────────────────────────

bool Database::storeWorkoutMetric(qint64 workoutId, const QString &metricId,
                                   double value, const QVariantMap &metadata)
{
    CHECK_DB;
    if (!m_stmtInsertMetric) return false;

    sqlite3_reset(m_stmtInsertMetric);
    sqlite3_bind_int64(m_stmtInsertMetric, 1, workoutId);
    sqlite3_bind_text(m_stmtInsertMetric, 2, metricId.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(m_stmtInsertMetric, 3, value);

    if (!metadata.isEmpty()) {
        QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(metadata)).toJson(QJsonDocument::Compact);
        sqlite3_bind_text(m_stmtInsertMetric, 4, json.constData(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(m_stmtInsertMetric, 4);
    }

    return sqlite3_step(m_stmtInsertMetric) == SQLITE_DONE;
}

QVariantList Database::getWorkoutMetrics(qint64 workoutId)
{
    CHECK_DB_VAL(QVariantList());

    sqlite3_stmt *stmt = prepare(
        "SELECT metric_id, value, metadata FROM workout_metrics WHERE workout_id=?");
    if (!stmt) return QVariantList();

    sqlite3_bind_int64(stmt, 1, workoutId);

    QVariantList result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row[QStringLiteral("metric_id")] = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        row[QStringLiteral("value")] = sqlite3_column_double(stmt, 1);
        const char *meta = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (meta) {
            row[QStringLiteral("metadata")] = QJsonDocument::fromJson(
                QByteArray(meta)).object().toVariantMap();
        }
        result.append(row);
    }
    finalize(stmt);
    return result;
}

QVariant Database::getWorkoutMetric(qint64 workoutId, const QString &metricId)
{
    CHECK_DB_VAL(QVariant());

    sqlite3_stmt *stmt = prepare(
        "SELECT value FROM workout_metrics WHERE workout_id=? AND metric_id=?");
    if (!stmt) return QVariant();

    sqlite3_bind_int64(stmt, 1, workoutId);
    sqlite3_bind_text(stmt, 2, metricId.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    QVariant result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_double(stmt, 0);
    }
    finalize(stmt);
    return result;
}

// ─── Daily metrics ──────────────────────────────────────────────────────────

bool Database::storeDailyMetric(const QDate &date, const QString &metricId,
                                 double value, const QVariantMap &metadata)
{
    CHECK_DB;
    if (!m_stmtInsertDailyMetric) return false;

    sqlite3_reset(m_stmtInsertDailyMetric);
    QByteArray dateStr = date.toString(Qt::ISODate).toUtf8();
    sqlite3_bind_text(m_stmtInsertDailyMetric, 1, dateStr.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertDailyMetric, 2, metricId.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(m_stmtInsertDailyMetric, 3, value);

    if (!metadata.isEmpty()) {
        QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(metadata)).toJson(QJsonDocument::Compact);
        sqlite3_bind_text(m_stmtInsertDailyMetric, 4, json.constData(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(m_stmtInsertDailyMetric, 4);
    }

    return sqlite3_step(m_stmtInsertDailyMetric) == SQLITE_DONE;
}

QVariantList Database::getDailyMetrics(const QString &metricId,
                                        const QDate &from, const QDate &to)
{
    CHECK_DB_VAL(QVariantList());

    sqlite3_stmt *stmt = prepare(
        "SELECT date, value, metadata FROM daily_metrics "
        "WHERE metric_id=? AND date>=? AND date<=? ORDER BY date");
    if (!stmt) return QVariantList();

    sqlite3_bind_text(stmt, 1, metricId.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    QByteArray fromStr = from.toString(Qt::ISODate).toUtf8();
    QByteArray toStr = to.toString(Qt::ISODate).toUtf8();
    sqlite3_bind_text(stmt, 2, fromStr.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, toStr.constData(), -1, SQLITE_TRANSIENT);

    QVariantList result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row[QStringLiteral("date")] = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        row[QStringLiteral("value")] = sqlite3_column_double(stmt, 1);
        result.append(row);
    }
    finalize(stmt);
    return result;
}

double Database::getDailyMetricValue(const QDate &date, const QString &metricId)
{
    CHECK_DB_VAL(0.0);

    sqlite3_stmt *stmt = prepare(
        "SELECT value FROM daily_metrics WHERE date=? AND metric_id=?");
    if (!stmt) return 0.0;

    QByteArray dateStr = date.toString(Qt::ISODate).toUtf8();
    sqlite3_bind_text(stmt, 1, dateStr.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, metricId.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    double result = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_double(stmt, 0);
    }
    finalize(stmt);
    return result;
}

// ─── Sleep (Phase 2 stubs) ──────────────────────────────────────────────────

qint64 Database::createSleepSession(qint64 startTime, qint64 endTime)
{
    CHECK_DB_VAL(-1);

    sqlite3_stmt *stmt = prepare(
        "INSERT INTO sleep_sessions (start_time, end_time, duration_minutes) "
        "VALUES (?, ?, ?)");
    if (!stmt) return -1;

    int duration = static_cast<int>((endTime - startTime) / 60);
    sqlite3_bind_int64(stmt, 1, startTime);
    sqlite3_bind_int64(stmt, 2, endTime);
    sqlite3_bind_int(stmt, 3, duration);

    int rc = sqlite3_step(stmt);
    qint64 id = (rc == SQLITE_DONE) ? sqlite3_last_insert_rowid(m_db) : -1;
    finalize(stmt);
    return id;
}

bool Database::updateSleepSession(qint64 sessionId, const QVariantMap &data)
{
    CHECK_DB;

    sqlite3_stmt *stmt = prepare(
        "UPDATE sleep_sessions SET "
        "quality_score=?, deep_minutes=?, light_minutes=?, rem_minutes=?, "
        "awake_minutes=?, metadata=? WHERE id=?");
    if (!stmt) return false;

    sqlite3_bind_double(stmt, 1, data.value(QStringLiteral("quality_score"), 0.0).toDouble());
    sqlite3_bind_int(stmt, 2, data.value(QStringLiteral("deep_minutes"), 0).toInt());
    sqlite3_bind_int(stmt, 3, data.value(QStringLiteral("light_minutes"), 0).toInt());
    sqlite3_bind_int(stmt, 4, data.value(QStringLiteral("rem_minutes"), 0).toInt());
    sqlite3_bind_int(stmt, 5, data.value(QStringLiteral("awake_minutes"), 0).toInt());

    // Serialize remaining fields as JSON metadata
    QJsonObject metaObj;
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        if (it.key() != QStringLiteral("quality_score") &&
            it.key() != QStringLiteral("deep_minutes") &&
            it.key() != QStringLiteral("light_minutes") &&
            it.key() != QStringLiteral("rem_minutes") &&
            it.key() != QStringLiteral("awake_minutes")) {
            metaObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
    }
    QByteArray metaJson = QJsonDocument(metaObj).toJson(QJsonDocument::Compact);
    sqlite3_bind_text(stmt, 6, metaJson.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, sessionId);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    finalize(stmt);
    return ok;
}

QVariantMap Database::getLatestSleep()
{
    CHECK_DB_VAL(QVariantMap());

    sqlite3_stmt *stmt = prepare(
        "SELECT id, start_time, end_time, duration_minutes, quality_score, "
        "deep_minutes, light_minutes, rem_minutes, awake_minutes "
        "FROM sleep_sessions ORDER BY start_time DESC LIMIT 1");
    if (!stmt) return QVariantMap();

    QVariantMap result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result[QStringLiteral("id")] = static_cast<qint64>(sqlite3_column_int64(stmt, 0));
        result[QStringLiteral("start_time")] = static_cast<qint64>(sqlite3_column_int64(stmt, 1));
        result[QStringLiteral("end_time")] = static_cast<qint64>(sqlite3_column_int64(stmt, 2));
        result[QStringLiteral("duration_minutes")] = sqlite3_column_int(stmt, 3);
        result[QStringLiteral("quality_score")] = sqlite3_column_double(stmt, 4);
        result[QStringLiteral("deep_minutes")] = sqlite3_column_int(stmt, 5);
        result[QStringLiteral("light_minutes")] = sqlite3_column_int(stmt, 6);
        result[QStringLiteral("rem_minutes")] = sqlite3_column_int(stmt, 7);
        result[QStringLiteral("awake_minutes")] = sqlite3_column_int(stmt, 8);
    }
    finalize(stmt);
    return result;
}

QVariantList Database::getSleepHistory(int days)
{
    CHECK_DB_VAL(QVariantList());

    qint64 cutoff = QDateTime::currentDateTime().addDays(-days).toSecsSinceEpoch();

    sqlite3_stmt *stmt = prepare(
        "SELECT id, start_time, end_time, duration_minutes, quality_score, "
        "deep_minutes, light_minutes, rem_minutes, awake_minutes, metadata "
        "FROM sleep_sessions WHERE start_time >= ? ORDER BY start_time DESC");
    if (!stmt) return QVariantList();

    sqlite3_bind_int64(stmt, 1, cutoff);

    QVariantList result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row[QStringLiteral("id")] = static_cast<qint64>(sqlite3_column_int64(stmt, 0));
        row[QStringLiteral("start_time")] = static_cast<qint64>(sqlite3_column_int64(stmt, 1));
        row[QStringLiteral("end_time")] = static_cast<qint64>(sqlite3_column_int64(stmt, 2));
        row[QStringLiteral("duration_minutes")] = sqlite3_column_int(stmt, 3);
        row[QStringLiteral("quality_score")] = sqlite3_column_double(stmt, 4);
        row[QStringLiteral("deep_minutes")] = sqlite3_column_int(stmt, 5);
        row[QStringLiteral("light_minutes")] = sqlite3_column_int(stmt, 6);
        row[QStringLiteral("rem_minutes")] = sqlite3_column_int(stmt, 7);
        row[QStringLiteral("awake_minutes")] = sqlite3_column_int(stmt, 8);

        const char *meta = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        if (meta && meta[0]) {
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray(meta));
            if (doc.isObject()) {
                QVariantMap metaMap = doc.object().toVariantMap();
                for (auto it = metaMap.constBegin(); it != metaMap.constEnd(); ++it)
                    row[it.key()] = it.value();
            }
        }

        result.append(row);
    }
    finalize(stmt);
    return result;
}

// ─── User profile ───────────────────────────────────────────────────────────

bool Database::setUserProfile(const QVariantMap &profile)
{
    CHECK_DB;
    beginTransaction();
    for (auto it = profile.constBegin(); it != profile.constEnd(); ++it) {
        sqlite3_stmt *stmt = prepare(
            "INSERT OR REPLACE INTO user_profile (key, value) VALUES (?, ?)");
        if (!stmt) { rollbackTransaction(); return false; }
        sqlite3_bind_text(stmt, 1, it.key().toUtf8().constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, it.value().toString().toUtf8().constData(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        finalize(stmt);
    }
    return commitTransaction();
}

QVariantMap Database::getUserProfile()
{
    CHECK_DB_VAL(QVariantMap());

    sqlite3_stmt *stmt = prepare("SELECT key, value FROM user_profile");
    if (!stmt) return QVariantMap();

    QVariantMap result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QString key = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        QString value = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        result[key] = value;
    }
    finalize(stmt);
    return result;
}

// ─── Maintenance ────────────────────────────────────────────────────────────

bool Database::pruneOldData(int keepDays)
{
    CHECK_DB;
    qint64 cutoff = QDateTime::currentSecsSinceEpoch() - (keepDays * 86400);

    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM workouts WHERE start_time < %lld", (long long)cutoff);
    if (!execSQL(sql)) return false;

    snprintf(sql, sizeof(sql),
             "DELETE FROM sleep_sessions WHERE start_time < %lld", (long long)cutoff);
    execSQL(sql);

    execSQL("VACUUM");
    return true;
}

qint64 Database::databaseSize() const
{
    if (m_path.isEmpty()) return 0;
    QFileInfo fi(m_path);
    return fi.size();
}

// ─── Private helpers ────────────────────────────────────────────────────────

bool Database::createTables()
{
    return execSQL(SCHEMA_SQL);
}

bool Database::execSQL(const char *sql)
{
    char *errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        QString error = errMsg ? QString::fromUtf8(errMsg) : QStringLiteral("Unknown error");
        sqlite3_free(errMsg);
        emit databaseError(error);
        qWarning() << "SQL error:" << error;
        return false;
    }
    return true;
}

sqlite3_stmt *Database::prepare(const char *sql)
{
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        qWarning() << "SQL prepare error:" << sqlite3_errmsg(m_db);
        return nullptr;
    }
    return stmt;
}

void Database::finalize(sqlite3_stmt *stmt)
{
    if (stmt) sqlite3_finalize(stmt);
}

bool Database::beginTransaction()
{
    return execSQL("BEGIN TRANSACTION");
}

bool Database::commitTransaction()
{
    return execSQL("COMMIT");
}

bool Database::rollbackTransaction()
{
    return execSQL("ROLLBACK");
}
