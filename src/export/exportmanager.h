/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Export manager — QML-facing controller for workout data export.
 * Coordinates format plugins and transport backends.
 */

#ifndef EXPORTMANAGER_H
#define EXPORTMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <memory>

class ExportPlugin;
class ExportTransport;
class Database;

/**
 * @brief QML-facing export orchestrator.
 *
 * Manages format plugins (FIT, TCX, GPX) and transport backends
 * (file, BLE). Provides a simple API for QML to trigger exports.
 *
 * Usage from QML:
 *   ExportManager.exportWorkout(workoutId, "gpx", "file")
 *   ExportManager.availableFormats  // ["fit", "tcx", "gpx"]
 *   ExportManager.availableTransports  // ["file", "ble"]
 */
class ExportManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList availableFormats READ availableFormats CONSTANT)
    Q_PROPERTY(QVariantList availableTransports READ availableTransports NOTIFY transportsChanged)
    Q_PROPERTY(bool exporting READ isExporting NOTIFY exportingChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ExportManager(QObject *parent = nullptr);
    ~ExportManager() override;

    void setDatabase(Database *db);

    // ── Plugin registration ─────────────────────────────────────────────

    void registerFormat(ExportPlugin *plugin);
    void registerTransport(ExportTransport *transport);

    /** Register all built-in format plugins and transports. */
    void registerBuiltins();

    // ── QML API ─────────────────────────────────────────────────────────

    QVariantList availableFormats() const;
    QVariantList availableTransports() const;

    bool isExporting() const { return m_exporting; }
    double progress() const { return m_progress; }
    QString lastError() const { return m_lastError; }

    /**
     * Export a workout.
     * @param workoutId  Database workout ID
     * @param formatId   Format plugin ID ("fit", "tcx", "gpx")
     * @param transportId Transport ID ("file", "ble")
     * @return true if export started successfully
     */
    Q_INVOKABLE bool exportWorkout(qint64 workoutId,
                                    const QString &formatId,
                                    const QString &transportId = QStringLiteral("file"));

    /**
     * Export to a specific file path (bypasses transport).
     */
    Q_INVOKABLE bool exportToFile(qint64 workoutId,
                                   const QString &formatId,
                                   const QString &filePath);

    /**
     * Get raw export bytes (for custom handling).
     */
    Q_INVOKABLE QByteArray exportBytes(qint64 workoutId, const QString &formatId);

    /**
     * Generate the default filename for an export.
     */
    Q_INVOKABLE QString suggestedFilename(qint64 workoutId, const QString &formatId);

signals:
    void exportingChanged();
    void progressChanged();
    void lastErrorChanged();
    void transportsChanged();
    void exportComplete(const QString &formatId, const QString &transportId);
    void exportFailed(const QString &error);

private:
    QByteArray generateExportData(qint64 workoutId, const QString &formatId);

    Database *m_database;
    QMap<QString, ExportPlugin*> m_formats;
    QMap<QString, ExportTransport*> m_transports;

    bool m_exporting;
    double m_progress;
    QString m_lastError;
};

#endif // EXPORTMANAGER_H
