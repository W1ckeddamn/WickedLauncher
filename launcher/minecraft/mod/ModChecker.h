// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

/**
 * Pre-launch sanity checks for the mods folder.
 *
 * Only metadata is read, and only the parts needed to detect broken setups:
 *  - duplicate mod ids (two versions of the same mod installed),
 *  - required dependencies that are not installed,
 *  - declared and well-known incompatibilities (OptiFine vs Sodium/Embeddium, ...).
 *
 * The checks are intentionally conservative: a missing dependency is a warning, not an error,
 * because some mods declare optional integrations as required.
 */
namespace ModChecker {

struct ModInfo {
    QString file;
    QString id;
    QString name;
    QString version;
    QString loader;
    QSet<QString> provides;
    QSet<QString> required;
    QSet<QString> conflicts;
    QString error;
};

struct Issue {
    enum class Severity {
        Warning,
        Error,
    };

    Severity severity = Severity::Warning;
    QString summary;
    QString details;
    QStringList files;
};

//! Parse a single mod jar. Returns an info object with `error` set when the archive is unreadable.
ModInfo parseJar(const QString& path);

//! Parse every enabled mod in `modsDir` (files ending in .disabled are skipped).
QList<ModInfo> scan(const QString& modsDir);

//! Turn parsed metadata into human-readable issues, highest severity first.
QList<Issue> analyze(const QList<ModInfo>& mods);

//! Convenience wrapper around scan() + analyze().
QList<Issue> check(const QString& modsDir);

// Parsers exposed for tests.
ModInfo parseFabric(const QByteArray& json, const QString& file = {});
ModInfo parseQuilt(const QByteArray& json, const QString& file = {});
ModInfo parseForgeToml(const QByteArray& toml, const QString& file = {}, bool neoForge = false);
ModInfo parseLegacyInfo(const QByteArray& json, const QString& file = {});

}  // namespace ModChecker
