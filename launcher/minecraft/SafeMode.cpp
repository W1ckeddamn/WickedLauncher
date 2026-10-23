// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "SafeMode.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include "Application.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ModFolderModel.h"

namespace {
const QString s_markerName = QStringLiteral(".safemode.json");
const QString s_backupSuffix = QStringLiteral(".safemode");

bool writeMarker(const QString& markerPath, const QJsonObject& object)
{
    QSaveFile file(markerPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonObject readMarker(const QString& markerPath)
{
    QFile file(markerPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

/**
 * Move everything from `source` into `target`. Entries that already exist in the target are
 * renamed instead of overwritten: a player may have dropped new mods in while safe mode was on,
 * and silently deleting either copy would be unacceptable.
 */
bool mergeDirectory(const QString& source, const QString& target, QString* error)
{
    QDir sourceDir(source);
    if (!sourceDir.exists()) {
        return true;
    }
    if (!FS::ensureFolderPathExists(target)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create %1").arg(target);
        }
        return false;
    }

    const auto entries = sourceDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const auto& entry : entries) {
        QString targetPath = FS::PathCombine(target, entry.fileName());
        int suffix = 0;
        while (QFileInfo::exists(targetPath)) {
            targetPath = FS::PathCombine(target, entry.fileName() + QStringLiteral(".restored%1").arg(++suffix));
        }
        if (!FS::move(entry.absoluteFilePath(), targetPath)) {
            if (error != nullptr) {
                *error = QStringLiteral("Could not restore %1").arg(entry.absoluteFilePath());
            }
            return false;
        }
    }

    QDir(source).removeRecursively();
    return true;
}

struct DisabledFolder {
    QString original;
    QString backup;
};

QList<DisabledFolder> folderPairs(const MinecraftInstance* instance)
{
    QList<DisabledFolder> pairs;
    // modsRoot() covers instances that keep their game files outside of <instance>/minecraft.
    const auto mods = instance->modsRoot();
    pairs.append({ mods, mods + s_backupSuffix });

    const auto coreMods = FS::PathCombine(instance->gameRoot(), QStringLiteral("coremods"));
    // Only touch coremods when it actually exists, so we don't create stray folders.
    if (QFileInfo::exists(coreMods)) {
        pairs.append({ coreMods, coreMods + s_backupSuffix });
    }
    return pairs;
}

void refreshModLists(MinecraftInstance* instance)
{
    if (auto* mods = instance->loaderModList(); mods != nullptr) {
        mods->update();
    }
    if (auto* coreMods = instance->coreModList(); coreMods != nullptr) {
        coreMods->update();
    }
}
}  // namespace

namespace SafeMode {

QString backupDir(const MinecraftInstance* instance)
{
    return instance->modsRoot() + s_backupSuffix;
}

QString markerPath(const MinecraftInstance* instance)
{
    return FS::PathCombine(instance->instanceRoot(), s_markerName);
}

bool isActive(const MinecraftInstance* instance)
{
    return QFileInfo::exists(markerPath(instance));
}

bool hasMods(const MinecraftInstance* instance)
{
    for (const auto& pair : folderPairs(instance)) {
        const QDir dir(pair.original);
        if (!dir.exists()) {
            continue;
        }
        if (!dir.entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
            return true;
        }
        if (!dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
            return true;
        }
    }
    return false;
}

bool begin(MinecraftInstance* instance, QString* error)
{
    const auto marker = markerPath(instance);

    // A marker means a previous session did not finish cleanly. Put the old mods back first.
    if (QFileInfo::exists(marker)) {
        if (!end(instance, error)) {
            return false;
        }
    }

    QList<DisabledFolder> moved;
    for (const auto& pair : folderPairs(instance)) {
        if (!QFileInfo::exists(pair.original)) {
            continue;
        }
        if (QFileInfo::exists(pair.backup)) {
            // A backup without a marker is a leftover from an interrupted restore. Try to merge it.
            if (!mergeDirectory(pair.backup, pair.original, error)) {
                return false;
            }
        }
        if (!FS::move(pair.original, pair.backup)) {
            if (error != nullptr) {
                *error = QStringLiteral("Could not move %1 out of the way.").arg(pair.original);
            }
            // Roll back whatever was already moved.
            for (const auto& alreadyMoved : moved) {
                FS::move(alreadyMoved.backup, alreadyMoved.original);
            }
            return false;
        }
        moved.append(pair);
    }

    if (moved.isEmpty()) {
        return true;
    }

    QJsonArray folders;
    for (const auto& pair : moved) {
        folders.append(QJsonObject{ { QStringLiteral("original"), pair.original }, { QStringLiteral("backup"), pair.backup } });
    }

    QJsonObject markerData{
        { QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
        { QStringLiteral("folders"), folders },
    };
    if (!writeMarker(marker, markerData)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write %1").arg(marker);
        }
        for (const auto& pair : moved) {
            FS::move(pair.backup, pair.original);
        }
        return false;
    }

    refreshModLists(instance);
    qInfo() << "Safe mode: disabled" << moved.size() << "mod folder(s) for instance" << instance->id();
    return true;
}

bool end(MinecraftInstance* instance, QString* error)
{
    const auto marker = markerPath(instance);
    const auto markerData = readMarker(marker);

    QList<DisabledFolder> pairs;
    const auto folders = markerData.value(QStringLiteral("folders")).toArray();
    if (!folders.isEmpty()) {
        for (const auto& value : folders) {
            const auto object = value.toObject();
            const auto original = object.value(QStringLiteral("original")).toString();
            const auto backup = object.value(QStringLiteral("backup")).toString();
            if (!original.isEmpty() && !backup.isEmpty()) {
                pairs.append({ original, backup });
            }
        }
    } else {
        // No usable marker (for example after a downgrade): fall back to the standard locations.
        pairs = folderPairs(instance);
        if (!QFileInfo::exists(pairs.first().backup)) {
            if (QFileInfo::exists(marker)) {
                QFile::remove(marker);
            }
            return true;
        }
    }

    bool ok = true;
    bool restored = false;
    for (const auto& pair : pairs) {
        if (!QFileInfo::exists(pair.backup)) {
            continue;
        }
        restored = true;
        if (QFileInfo::exists(pair.original)) {
            if (!mergeDirectory(pair.backup, pair.original, error)) {
                ok = false;
                continue;
            }
        } else if (!FS::move(pair.backup, pair.original)) {
            if (error != nullptr) {
                *error = QStringLiteral("Could not restore %1").arg(pair.original);
            }
            ok = false;
            continue;
        }
    }

    if (ok) {
        QFile::remove(marker);
    }
    if (restored) {
        refreshModLists(instance);
        qInfo() << "Safe mode: restored mods for instance" << instance->id();
    }
    return ok;
}

bool recover(MinecraftInstance* instance)
{
    if (!isActive(instance)) {
        return false;
    }
    QString error;
    if (!end(instance, &error)) {
        qWarning() << "Safe mode: could not restore mods for" << instance->id() << ":" << error;
        return false;
    }
    return true;
}

int recoverAll()
{
    int restored = 0;
    auto* instances = APPLICATION->instances();
    if (instances == nullptr) {
        return restored;
    }
    for (int i = 0; i < instances->count(); i++) {
        if (auto* instance = instances->at(i); instance != nullptr && recover(instance)) {
            restored++;
        }
    }
    return restored;
}

}  // namespace SafeMode
