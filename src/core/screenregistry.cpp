/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "screenregistry.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

ScreenRegistry *ScreenRegistry::s_instance = nullptr;

ScreenRegistry *ScreenRegistry::instance()
{
    if (!s_instance)
        s_instance = new ScreenRegistry();
    return s_instance;
}

ScreenRegistry::ScreenRegistry(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

ScreenRegistry::~ScreenRegistry()
{
    if (s_instance == this)
        s_instance = nullptr;
}

/* ────────────────────────────────────────────────────────────────────────── */

bool ScreenRegistry::loadLayout(const QString &configDir, const QString &dataDir)
{
    m_configDir = configDir;
    m_dataDir   = dataDir;

    if (!loadDefaultLayout(configDir))
        return false;

    /* Overlay user overrides (reorder, moved screens, pinned workouts) */
    loadUserOverrides(dataDir);

    emit foldersChanged();
    emit pinnedWorkoutsChanged();
    return true;
}

/* ── Queries ─────────────────────────────────────────────────────────────── */

QVariantList ScreenRegistry::folders() const { return m_folders; }
int ScreenRegistry::folderCount() const      { return m_folders.size(); }

QVariantMap ScreenRegistry::folder(const QString &folderId) const
{
    for (const QVariant &v : m_folders) {
        QVariantMap f = v.toMap();
        if (f.value(QStringLiteral("id")).toString() == folderId)
            return f;
    }
    return {};
}

QVariantList ScreenRegistry::screens(const QString &folderId) const
{
    QVariantMap f = folder(folderId);
    return f.value(QStringLiteral("screens")).toList();
}

QVariantMap ScreenRegistry::screen(const QString &screenId) const
{
    for (const QVariant &fv : m_folders) {
        QVariantList sl = fv.toMap().value(QStringLiteral("screens")).toList();
        for (const QVariant &sv : sl) {
            QVariantMap s = sv.toMap();
            if (s.value(QStringLiteral("id")).toString() == screenId)
                return s;
        }
    }
    return {};
}

QString ScreenRegistry::folderForScreen(const QString &screenId) const
{
    for (const QVariant &fv : m_folders) {
        QVariantMap f = fv.toMap();
        QVariantList sl = f.value(QStringLiteral("screens")).toList();
        for (const QVariant &sv : sl) {
            if (sv.toMap().value(QStringLiteral("id")).toString() == screenId)
                return f.value(QStringLiteral("id")).toString();
        }
    }
    return {};
}

QStringList ScreenRegistry::allScreenIds() const
{
    QStringList ids;
    for (const QVariant &fv : m_folders) {
        QVariantList sl = fv.toMap().value(QStringLiteral("screens")).toList();
        for (const QVariant &sv : sl)
            ids.append(sv.toMap().value(QStringLiteral("id")).toString());
    }
    return ids;
}

/* ── Mutations ───────────────────────────────────────────────────────────── */

bool ScreenRegistry::reorderFolders(const QStringList &orderedIds)
{
    QVariantList reordered;
    QVariantMap fixedLast;

    /* Collect the fixedLast folder (Settings). */
    for (const QVariant &v : qAsConst(m_folders)) {
        QVariantMap f = v.toMap();
        if (f.value(QStringLiteral("fixedLast")).toBool()) {
            fixedLast = f;
            break;
        }
    }

    /* Build new order from provided ids, skip immovable/fixedLast. */
    for (const QString &id : orderedIds) {
        int idx = folderIndex(id);
        if (idx < 0) continue;
        QVariantMap f = m_folders.at(idx).toMap();
        if (f.value(QStringLiteral("fixedLast")).toBool()) continue;
        reordered.append(f);
    }

    /* Append any folders not mentioned (safety net). */
    for (const QVariant &v : qAsConst(m_folders)) {
        QVariantMap f = v.toMap();
        if (f.value(QStringLiteral("fixedLast")).toBool()) continue;
        QString id = f.value(QStringLiteral("id")).toString();
        bool found = false;
        for (const QVariant &rv : qAsConst(reordered)) {
            if (rv.toMap().value(QStringLiteral("id")).toString() == id) {
                found = true;
                break;
            }
        }
        if (!found) reordered.append(f);
    }

    /* Settings always at the end. */
    if (!fixedLast.isEmpty())
        reordered.append(fixedLast);

    m_folders = reordered;
    persistUserLayout();
    emit foldersChanged();
    return true;
}

bool ScreenRegistry::moveScreen(const QString &screenId,
                                const QString &targetFolderId,
                                int position)
{
    /* Find and remove the screen from its current folder. */
    QVariantMap screenData;
    bool removed = false;

    for (int fi = 0; fi < m_folders.size(); ++fi) {
        QVariantMap f = m_folders[fi].toMap();
        QVariantList sl = f.value(QStringLiteral("screens")).toList();
        for (int si = 0; si < sl.size(); ++si) {
            if (sl[si].toMap().value(QStringLiteral("id")).toString() == screenId) {
                screenData = sl[si].toMap();
                sl.removeAt(si);
                f[QStringLiteral("screens")] = sl;
                m_folders[fi] = f;
                removed = true;
                break;
            }
        }
        if (removed) break;
    }
    if (!removed) return false;

    /* Insert into target folder. */
    int ti = folderIndex(targetFolderId);
    if (ti < 0) return false;

    QVariantMap tf = m_folders[ti].toMap();
    QVariantList tsl = tf.value(QStringLiteral("screens")).toList();
    if (position < 0 || position >= tsl.size())
        tsl.append(screenData);
    else
        tsl.insert(position, screenData);
    tf[QStringLiteral("screens")] = tsl;
    m_folders[ti] = tf;

    persistUserLayout();
    emit foldersChanged();
    return true;
}

bool ScreenRegistry::reorderScreens(const QString &folderId,
                                    const QStringList &orderedIds)
{
    int fi = folderIndex(folderId);
    if (fi < 0) return false;

    QVariantMap f = m_folders[fi].toMap();
    QVariantList oldList = f.value(QStringLiteral("screens")).toList();
    QVariantList newList;

    for (const QString &id : orderedIds) {
        for (const QVariant &sv : qAsConst(oldList)) {
            if (sv.toMap().value(QStringLiteral("id")).toString() == id) {
                newList.append(sv);
                break;
            }
        }
    }

    /* Append any not mentioned (safety net). */
    for (const QVariant &sv : qAsConst(oldList)) {
        QString id = sv.toMap().value(QStringLiteral("id")).toString();
        if (!orderedIds.contains(id))
            newList.append(sv);
    }

    f[QStringLiteral("screens")] = newList;
    m_folders[fi] = f;

    persistUserLayout();
    emit foldersChanged();
    return true;
}

void ScreenRegistry::resetToDefault()
{
    m_folders.clear();
    m_pinnedWorkouts.clear();
    loadDefaultLayout(m_configDir);

    /* Remove user override file. */
    QFile::remove(m_dataDir + QStringLiteral("/navigation_layout.json"));

    emit foldersChanged();
    emit pinnedWorkoutsChanged();
}

/* ── Workout pinning ─────────────────────────────────────────────────────── */

QStringList ScreenRegistry::pinnedWorkouts() const { return m_pinnedWorkouts; }

bool ScreenRegistry::pinWorkout(const QString &workoutTypeId)
{
    if (m_pinnedWorkouts.size() >= 5) return false;
    if (m_pinnedWorkouts.contains(workoutTypeId)) return false;
    m_pinnedWorkouts.append(workoutTypeId);
    persistUserLayout();
    emit pinnedWorkoutsChanged();
    return true;
}

void ScreenRegistry::unpinWorkout(const QString &workoutTypeId)
{
    if (m_pinnedWorkouts.removeAll(workoutTypeId) > 0) {
        persistUserLayout();
        emit pinnedWorkoutsChanged();
    }
}

bool ScreenRegistry::isWorkoutPinned(const QString &workoutTypeId) const
{
    return m_pinnedWorkouts.contains(workoutTypeId);
}

/* ── Private helpers ─────────────────────────────────────────────────────── */

bool ScreenRegistry::loadDefaultLayout(const QString &configDir)
{
    const QString path = configDir + QStringLiteral("/navigation/default_layout.json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("ScreenRegistry: cannot open %s", qPrintable(path));
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning("ScreenRegistry: JSON parse error in %s: %s",
                 qPrintable(path), qPrintable(err.errorString()));
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray foldersArr = root.value(QStringLiteral("folders")).toArray();

    m_folders.clear();
    for (const QJsonValue &fv : foldersArr) {
        QJsonObject fo = fv.toObject();
        QVariantMap folder;
        folder[QStringLiteral("id")]         = fo.value(QStringLiteral("id")).toString();
        folder[QStringLiteral("name")]       = fo.value(QStringLiteral("name")).toString();
        folder[QStringLiteral("icon")]       = fo.value(QStringLiteral("icon")).toString();
        folder[QStringLiteral("accentColor")]= fo.value(QStringLiteral("accentColor")).toString();
        folder[QStringLiteral("type")]       = fo.value(QStringLiteral("type")).toString();
        folder[QStringLiteral("immovable")]  = fo.value(QStringLiteral("immovable")).toBool();
        folder[QStringLiteral("fixedLast")]  = fo.value(QStringLiteral("fixedLast")).toBool();

        QVariantList screens;
        QJsonArray screensArr = fo.value(QStringLiteral("screens")).toArray();
        for (const QJsonValue &sv : screensArr)
            screens.append(nodeFromJson(sv.toObject()));

        folder[QStringLiteral("screens")] = screens;
        m_folders.append(folder);
    }

    /* Default pinned workouts (first 3 workout types). */
    if (m_pinnedWorkouts.isEmpty()) {
        m_pinnedWorkouts = QStringList{
            QStringLiteral("outdoor_run"),
            QStringLiteral("cycling"),
            QStringLiteral("hike")
        };
    }

    return true;
}

bool ScreenRegistry::loadUserOverrides(const QString &dataDir)
{
    const QString path = dataDir + QStringLiteral("/navigation_layout.json");
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError)
        return false;

    QJsonObject root = doc.object();

    /* Restore folder order. */
    if (root.contains(QStringLiteral("folderOrder"))) {
        QJsonArray orderArr = root.value(QStringLiteral("folderOrder")).toArray();
        QStringList order;
        for (const QJsonValue &v : orderArr)
            order.append(v.toString());
        reorderFolders(order);
    }

    /* Restore screen assignments (moved screens). */
    if (root.contains(QStringLiteral("screenAssignments"))) {
        QJsonObject assignments = root.value(QStringLiteral("screenAssignments")).toObject();
        for (auto it = assignments.begin(); it != assignments.end(); ++it) {
            QJsonObject a = it.value().toObject();
            moveScreen(it.key(),
                       a.value(QStringLiteral("folder")).toString(),
                       a.value(QStringLiteral("position")).toInt(-1));
        }
    }

    /* Restore pinned workouts. */
    if (root.contains(QStringLiteral("pinnedWorkouts"))) {
        m_pinnedWorkouts.clear();
        QJsonArray pins = root.value(QStringLiteral("pinnedWorkouts")).toArray();
        for (const QJsonValue &v : pins)
            m_pinnedWorkouts.append(v.toString());
    }

    return true;
}

bool ScreenRegistry::persistUserLayout()
{
    if (m_dataDir.isEmpty()) return false;

    QDir().mkpath(m_dataDir);
    const QString path = m_dataDir + QStringLiteral("/navigation_layout.json");

    QJsonObject root;

    /* Save folder order. */
    QJsonArray folderOrder;
    for (const QVariant &v : qAsConst(m_folders))
        folderOrder.append(v.toMap().value(QStringLiteral("id")).toString());
    root[QStringLiteral("folderOrder")] = folderOrder;

    /* Save pinned workouts. */
    QJsonArray pins;
    for (const QString &id : qAsConst(m_pinnedWorkouts))
        pins.append(id);
    root[QStringLiteral("pinnedWorkouts")] = pins;

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("ScreenRegistry: cannot write %s", qPrintable(path));
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Compact));
    emit layoutPersisted();
    return true;
}

QVariantMap ScreenRegistry::nodeFromJson(const QJsonObject &obj) const
{
    QVariantMap node;
    node[QStringLiteral("id")]             = obj.value(QStringLiteral("id")).toString();
    node[QStringLiteral("name")]           = obj.value(QStringLiteral("name")).toString();
    node[QStringLiteral("icon")]           = obj.value(QStringLiteral("icon")).toString();
    node[QStringLiteral("qmlSource")]      = obj.value(QStringLiteral("qmlSource")).toString();
    node[QStringLiteral("glanceProvider")] = obj.value(QStringLiteral("glanceProvider")).toString();
    node[QStringLiteral("accentColor")]    = obj.value(QStringLiteral("accentColor")).toString();
    return node;
}

int ScreenRegistry::folderIndex(const QString &folderId) const
{
    for (int i = 0; i < m_folders.size(); ++i) {
        if (m_folders[i].toMap().value(QStringLiteral("id")).toString() == folderId)
            return i;
    }
    return -1;
}
