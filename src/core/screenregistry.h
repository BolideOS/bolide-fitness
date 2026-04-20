/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCREENREGISTRY_H
#define SCREENREGISTRY_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>

/**
 * @brief Manages the navigation tree: folders (L1) containing screens (L2).
 *
 * The navigation model is a two-level tree:
 *   Level 1 (L1): Folders – vertical scroll, infinite loop
 *   Level 2 (L2): Screens within a folder – vertical scroll, swipe-right to go back
 *
 * Each folder and screen is a "node" with:
 *   - id, name (translation key), icon, accentColor
 *   - qmlSource (for screens: the detail QML file)
 *   - glanceProvider (provider key for glance data)
 *   - immovable / fixedLast flags (for Settings)
 *
 * The default layout is loaded from configs/navigation/default_layout.json.
 * User customizations (reordering, moving screens) are persisted to a user
 * config file in the app's data directory.
 *
 * Designed to be registered as a QML singleton (org.bolide.fitness / ScreenRegistry).
 */
class ScreenRegistry : public QObject
{
    Q_OBJECT

    /** List of L1 folder objects (QVariantMap each). */
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)

    /** Total number of L1 folders. */
    Q_PROPERTY(int folderCount READ folderCount NOTIFY foldersChanged)

public:
    static ScreenRegistry *instance();

    explicit ScreenRegistry(QObject *parent = nullptr);
    ~ScreenRegistry() override;

    /** Load the default layout from configDir/navigation/default_layout.json,
     *  then overlay user customizations if present. */
    bool loadLayout(const QString &configDir, const QString &dataDir);

    QVariantList folders() const;
    int folderCount() const;

    // ── QML-invokable queries ───────────────────────────────────────────

    /** Get a folder by its id. Returns empty map if not found. */
    Q_INVOKABLE QVariantMap folder(const QString &folderId) const;

    /** Get the screens list for a given folder. */
    Q_INVOKABLE QVariantList screens(const QString &folderId) const;

    /** Get a specific screen by its id (searches all folders). */
    Q_INVOKABLE QVariantMap screen(const QString &screenId) const;

    /** Get the folder id that contains a given screen. */
    Q_INVOKABLE QString folderForScreen(const QString &screenId) const;

    /** Get all screen ids across all folders (flat list). */
    Q_INVOKABLE QStringList allScreenIds() const;

    // ── QML-invokable mutations ─────────────────────────────────────────

    /** Reorder L1 folders. Provide the new ordered list of folder ids.
     *  Settings (fixedLast) is automatically kept at the end. */
    Q_INVOKABLE bool reorderFolders(const QStringList &orderedIds);

    /** Move a screen to a different folder at the given position. */
    Q_INVOKABLE bool moveScreen(const QString &screenId,
                                const QString &targetFolderId,
                                int position = -1);

    /** Reorder screens within a folder. */
    Q_INVOKABLE bool reorderScreens(const QString &folderId,
                                    const QStringList &orderedIds);

    /** Reset to default layout (discards user customizations). */
    Q_INVOKABLE void resetToDefault();

    // ── Workout pinning (stored in user config) ─────────────────────────

    /** Get list of pinned workout type ids. */
    Q_INVOKABLE QStringList pinnedWorkouts() const;

    /** Pin a workout type. Returns false if already at max (5). */
    Q_INVOKABLE bool pinWorkout(const QString &workoutTypeId);

    /** Unpin a workout type. */
    Q_INVOKABLE void unpinWorkout(const QString &workoutTypeId);

    /** Check if a workout type is pinned. */
    Q_INVOKABLE bool isWorkoutPinned(const QString &workoutTypeId) const;

signals:
    void foldersChanged();
    void pinnedWorkoutsChanged();
    void layoutPersisted();

private:
    bool loadDefaultLayout(const QString &configDir);
    bool loadUserOverrides(const QString &dataDir);
    bool persistUserLayout();

    QVariantMap nodeFromJson(const QJsonObject &obj) const;
    int folderIndex(const QString &folderId) const;

    static ScreenRegistry *s_instance;
    QVariantList m_folders;
    QStringList  m_pinnedWorkouts;
    QString      m_configDir;
    QString      m_dataDir;
};

#endif // SCREENREGISTRY_H
