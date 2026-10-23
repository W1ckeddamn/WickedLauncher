// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include <QString>

class MinecraftInstance;

/**
 * Temporary "safe mode" for an instance.
 *
 * The mods folder is renamed next to the instance before the game starts, and restored when the
 * game process exits. If the launcher (or the machine) dies while the game is running, the marker
 * file left in the instance directory makes the next launcher start restore the folder.
 *
 * This is the only reliable way to disable all mods for every loader: Fabric, Quilt, Forge and
 * NeoForge all read <game dir>/mods, and none of them has a portable "disable everything" flag.
 */
namespace SafeMode {

//! Suffix appended to the mods folder while safe mode is active.
QString backupDir(const MinecraftInstance* instance);

//! Marker file proving that a safe-mode session is in progress.
QString markerPath(const MinecraftInstance* instance);

//! True when a previous safe-mode launch has not been cleaned up yet.
bool isActive(const MinecraftInstance* instance);

//! True when the instance actually has mods to disable.
bool hasMods(const MinecraftInstance* instance);

/**
 * Move the mods folder aside and write the marker file.
 * Fails if the folder cannot be moved, so the caller must not launch in that case.
 */
bool begin(MinecraftInstance* instance, QString* error = nullptr);

/**
 * Restore the mods folder. Safe to call when safe mode is not active.
 * If a mods folder was created while the game was running, its contents are merged.
 */
bool end(MinecraftInstance* instance, QString* error = nullptr);

//! Restore an instance if it has a stale safe-mode marker.
bool recover(MinecraftInstance* instance);

//! Restore every instance with a stale marker. Returns the number of restored instances.
int recoverAll();

}  // namespace SafeMode
