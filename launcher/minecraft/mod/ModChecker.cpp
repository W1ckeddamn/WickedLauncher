// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "ModChecker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QPair>
#include <algorithm>
#include <utility>

#include <toml++/toml.h>

#include "archive/ArchiveReader.h"

namespace {
const QStringList s_ignoredDependencies = {
    QStringLiteral(""),
    QStringLiteral("minecraft"),
    QStringLiteral("forge"),
    QStringLiteral("neoforge"),
    QStringLiteral("fabricloader"),
    QStringLiteral("fabric-loader"),
    QStringLiteral("quilt_loader"),
    QStringLiteral("quiltloader"),
    QStringLiteral("java"),
    QStringLiteral("mcp"),
    QStringLiteral("lowcodefml"),
    QStringLiteral("javafml"),
};

//: Pairs of mod ids that cannot coexist. Ids are compared lower-case.
const QList<QPair<QString, QString>> s_knownConflicts = {
    { QStringLiteral("optifine"), QStringLiteral("sodium") },
    { QStringLiteral("optifine"), QStringLiteral("embeddium") },
    { QStringLiteral("optifine"), QStringLiteral("rubidium") },
    { QStringLiteral("optifine"), QStringLiteral("magnesium") },
    { QStringLiteral("optifine"), QStringLiteral("iris") },
    { QStringLiteral("sodium"), QStringLiteral("embeddium") },
    { QStringLiteral("sodium"), QStringLiteral("rubidium") },
    { QStringLiteral("sodium"), QStringLiteral("magnesium") },
    { QStringLiteral("embeddium"), QStringLiteral("rubidium") },
};

QString normalize(const QString& id)
{
    return id.trimmed().toLower();
}

QString displayName(const ModChecker::ModInfo& mod)
{
    if (!mod.name.isEmpty() && mod.name != mod.id) {
        return QStringLiteral("%1 (%2)").arg(mod.name, mod.id);
    }
    return mod.id.isEmpty() ? mod.file : mod.id;
}

QString valueToString(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    return {};
}

QJsonObject parseObject(const QByteArray& data)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}
}  // namespace

namespace ModChecker {

ModInfo parseFabric(const QByteArray& json, const QString& file)
{
    ModInfo info;
    info.file = file;
    info.loader = QStringLiteral("fabric");

    const auto object = parseObject(json);
    if (object.isEmpty()) {
        info.error = QStringLiteral("fabric.mod.json is not valid JSON");
        return info;
    }

    info.id = normalize(object.value(QStringLiteral("id")).toString());
    info.name = object.value(QStringLiteral("name")).toString();
    info.version = valueToString(object.value(QStringLiteral("version")));

    for (const auto& provide : object.value(QStringLiteral("provides")).toArray()) {
        info.provides.insert(normalize(provide.toString()));
    }

    const auto depends = object.value(QStringLiteral("depends")).toObject();
    for (auto it = depends.begin(); it != depends.end(); ++it) {
        info.required.insert(normalize(it.key()));
    }

    const auto breaks = object.value(QStringLiteral("breaks")).toObject();
    for (auto it = breaks.begin(); it != breaks.end(); ++it) {
        info.conflicts.insert(normalize(it.key()));
    }

    return info;
}

ModInfo parseQuilt(const QByteArray& json, const QString& file)
{
    ModInfo info;
    info.file = file;
    info.loader = QStringLiteral("quilt");

    const auto object = parseObject(json);
    const auto loader = object.value(QStringLiteral("quilt_loader")).toObject();
    if (loader.isEmpty()) {
        info.error = QStringLiteral("quilt.mod.json has no quilt_loader object");
        return info;
    }

    info.id = normalize(loader.value(QStringLiteral("id")).toString());
    info.version = valueToString(loader.value(QStringLiteral("version")));
    info.name = loader.value(QStringLiteral("metadata")).toObject().value(QStringLiteral("name")).toString();

    for (const auto& value : loader.value(QStringLiteral("provides")).toArray()) {
        info.provides.insert(normalize(value.toObject().value(QStringLiteral("id")).toString()));
    }

    for (const auto& value : loader.value(QStringLiteral("depends")).toArray()) {
        const auto dependency = value.toObject();
        // Optional dependencies are only needed when the other mod is present.
        if (dependency.value(QStringLiteral("optional")).toBool()) {
            continue;
        }
        info.required.insert(normalize(dependency.value(QStringLiteral("id")).toString()));
    }

    for (const auto& value : loader.value(QStringLiteral("breaks")).toArray()) {
        info.conflicts.insert(normalize(value.toObject().value(QStringLiteral("id")).toString()));
    }

    return info;
}

ModInfo parseForgeToml(const QByteArray& tomlData, const QString& file, bool neoForge)
{
    ModInfo info;
    info.file = file;
    info.loader = neoForge ? QStringLiteral("neoforge") : QStringLiteral("forge");

    toml::table table;
#if TOML_EXCEPTIONS
    try {
        table = toml::parse(tomlData.toStdString());
    } catch (const std::exception& error) {
        info.error = QString::fromStdString(error.what());
        return info;
    }
#else
    auto result = toml::parse(tomlData.toStdString());
    if (!result) {
        const auto description = result.error().description();
        info.error = QString::fromUtf8(description.data(), static_cast<qsizetype>(description.size()));
        return info;
    }
    table = result.table();
#endif

    const auto mods = table["mods"].as_array();
    const auto firstMod = mods ? mods->get(0) : nullptr;
    const auto modTable = firstMod ? firstMod->as_table() : nullptr;
    if (modTable == nullptr) {
        info.error = QStringLiteral("mods.toml has no [[mods]] entry");
        return info;
    }

    if (auto modId = (*modTable)["modId"].as_string()) {
        info.id = normalize(QString::fromStdString(modId->get()));
    }
    if (auto displayName = (*modTable)["displayName"].as_string()) {
        info.name = QString::fromStdString(displayName->get());
    }
    if (auto version = (*modTable)["version"].as_string()) {
        info.version = QString::fromStdString(version->get());
    }

    auto parseDependencies = [&info](const toml::array& dependencies) {
        for (const auto& value : dependencies) {
            const auto dependency = value.as_table();
            if (dependency == nullptr) {
                continue;
            }
            auto modId = (*dependency)["modId"].as_string();
            if (!modId) {
                continue;
            }
            const auto id = normalize(QString::fromStdString(modId->get()));
            if (id.isEmpty() || id == info.id) {
                continue;
            }

            QString type;
            if (auto typeValue = (*dependency)["type"].as_string()) {
                type = QString::fromStdString(typeValue->get());
            }
            bool mandatory = false;
            if (auto mandatoryValue = (*dependency)["mandatory"].as_boolean()) {
                mandatory = mandatoryValue->get();
            }

            if (type == QStringLiteral("incompatible")) {
                info.conflicts.insert(id);
            } else if (type == QStringLiteral("required") || mandatory) {
                info.required.insert(id);
            }
        }
    };

    const auto dependencies = table["dependencies"];
    if (const auto array = dependencies.as_array(); array != nullptr) {
        parseDependencies(*array);
    } else if (const auto dependencyTable = dependencies.as_table(); dependencyTable != nullptr) {
        for (const auto& entry : *dependencyTable) {
            if (const auto entryArray = entry.second.as_array(); entryArray != nullptr) {
                parseDependencies(*entryArray);
            }
        }
    }

    return info;
}

ModInfo parseLegacyInfo(const QByteArray& json, const QString& file)
{
    ModInfo info;
    info.file = file;
    info.loader = QStringLiteral("forge");

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError) {
        info.error = QStringLiteral("mcmod.info is not valid JSON");
        return info;
    }

    QJsonArray mods;
    if (document.isArray()) {
        mods = document.array();
    } else if (document.isObject()) {
        const auto object = document.object();
        mods = object.value(QStringLiteral("modList")).toArray();
        if (mods.isEmpty()) {
            mods = object.value(QStringLiteral("modlist")).toArray();
        }
    }

    if (mods.isEmpty() || !mods.first().isObject()) {
        info.error = QStringLiteral("mcmod.info contains no mod entries");
        return info;
    }

    const auto first = mods.first().toObject();
    info.id = normalize(first.value(QStringLiteral("modid")).toString());
    if (info.id.startsWith(QStringLiteral("mod_"))) {
        info.id.remove(0, 4);
    }
    info.name = first.value(QStringLiteral("name")).toString();
    info.version = valueToString(first.value(QStringLiteral("version")));

    auto addDependency = [&info](QString dependency) {
        dependency = dependency.trimmed();
        if (dependency.contains(QLatin1Char(':'))) {
            dependency = dependency.section(QLatin1Char(':'), 1);
        }
        if (dependency.contains(QLatin1Char('@'))) {
            dependency = dependency.section(QLatin1Char('@'), 0, 0);
        }
        if (dependency.startsWith(QStringLiteral("mod_"))) {
            dependency.remove(0, 4);
        }
        if (!dependency.isEmpty()) {
            info.required.insert(normalize(dependency));
        }
    };

    const auto requiredMods = first.value(QStringLiteral("requiredMods")).toArray();
    if (!requiredMods.isEmpty()) {
        for (const auto& value : requiredMods) {
            addDependency(value.toString());
        }
    } else {
        for (const auto& value : first.value(QStringLiteral("dependencies")).toArray()) {
            addDependency(value.toString());
        }
    }

    return info;
}

ModInfo parseJar(const QString& path)
{
    ModInfo info;
    info.file = QFileInfo(path).fileName();

    MMCZip::ArchiveReader reader(path);
    const auto result = reader.parse([&info](MMCZip::ArchiveReader::File* file) -> Result<bool> {
        const auto name = file->filename();
        const bool isFabric = name == QStringLiteral("fabric.mod.json");
        const bool isQuilt = name == QStringLiteral("quilt.mod.json");
        const bool isNeoForge = name == QStringLiteral("META-INF/neoforge.mods.toml");
        const bool isForge = name == QStringLiteral("META-INF/mods.toml");
        const bool isLegacy = name == QStringLiteral("mcmod.info");
        const bool isLiteLoader = name == QStringLiteral("litemod.json");

        if (isFabric || isQuilt || isNeoForge || isForge || isLegacy || isLiteLoader) {
            const auto data = file->readAll();
            if (!data) {
                info.error = data.error();
                return true;
            }
            if (isFabric) {
                info = parseFabric(*data, info.file);
            } else if (isQuilt) {
                info = parseQuilt(*data, info.file);
            } else if (isNeoForge) {
                info = parseForgeToml(*data, info.file, true);
            } else if (isForge) {
                info = parseForgeToml(*data, info.file, false);
            } else if (isLegacy) {
                info = parseLegacyInfo(*data, info.file);
            } else {
                info.loader = QStringLiteral("liteloader");
                const auto object = parseObject(*data);
                info.name = object.value(QStringLiteral("name")).toString();
                info.version = valueToString(object.value(QStringLiteral("version")));
            }
            return true;
        }

        if (const auto skipped = file->skip(); !skipped) {
            info.error = skipped.error();
            return true;
        }
        return false;
    });

    if (!result && info.error.isEmpty()) {
        info.error = result.error();
    }
    return info;
}

QList<ModInfo> scan(const QString& modsDir)
{
    QList<ModInfo> mods;
    QDir dir(modsDir);
    if (!dir.exists()) {
        return mods;
    }

    const auto entries = dir.entryInfoList({ QStringLiteral("*.jar"), QStringLiteral("*.litemod") }, QDir::Files, QDir::Name);
    mods.reserve(entries.size());
    for (const auto& entry : entries) {
        // Prism disables resources by appending .disabled, which the filters already exclude.
        if (entry.fileName().endsWith(QStringLiteral(".disabled"))) {
            continue;
        }
        mods.append(parseJar(entry.absoluteFilePath()));
    }
    return mods;
}

QList<Issue> analyze(const QList<ModInfo>& mods)
{
    QList<Issue> issues;

    QMap<QString, QList<const ModInfo*>> byId;
    QSet<QString> present;
    for (const auto& mod : mods) {
        if (!mod.id.isEmpty()) {
            byId[mod.id].append(&mod);
            present.insert(mod.id);
        }
        for (const auto& provided : mod.provides) {
            if (!provided.isEmpty()) {
                present.insert(provided);
            }
        }
    }

    for (auto it = byId.begin(); it != byId.end(); ++it) {
        if (it.value().size() < 2) {
            continue;
        }
        QStringList details;
        QStringList files;
        for (const auto* mod : it.value()) {
            details.append(QStringLiteral("%1 %2 — %3").arg(displayName(*mod), mod->version, mod->file));
            files.append(mod->file);
        }
        issues.append({ Issue::Severity::Error,
                        QCoreApplication::translate("ModChecker", "Duplicate mod: %1").arg(it.key()),
                        details.join(QLatin1Char('\n')),
                        files });
    }

    QSet<QString> fabricModules;
    QMap<QString, QStringList> missing;
    for (const auto& mod : mods) {
        for (const auto& dependency : mod.required) {
            if (dependency.isEmpty() || present.contains(dependency) || s_ignoredDependencies.contains(dependency)) {
                continue;
            }
            if (dependency.startsWith(QStringLiteral("fabric-"))) {
                fabricModules.insert(dependency);
            } else {
                missing[dependency].append(mod.file);
            }
        }
    }

    // Fabric modules live in nested jars inside the Fabric API bundle, so their ids never show up
    // as top-level mods. Seeing "fabric-api" is enough to know the modules are available.
    const bool hasFabricApi = present.contains(QStringLiteral("fabric-api")) || present.contains(QStringLiteral("fabric_api"));
    if (!fabricModules.isEmpty() && !hasFabricApi) {
        QStringList modules;
        for (const auto& module : fabricModules) {
            modules.append(module);
        }
        std::sort(modules.begin(), modules.end());
        issues.append({ Issue::Severity::Warning,
                        QCoreApplication::translate("ModChecker", "Fabric API is missing or incomplete"),
                        QCoreApplication::translate("ModChecker",
                                                    "These required modules are not provided by any installed mod: %1")
                            .arg(modules.join(QStringLiteral(", "))),
                        {} });
    }

    for (auto it = missing.begin(); it != missing.end(); ++it) {
        issues.append({ Issue::Severity::Warning,
                        QCoreApplication::translate("ModChecker", "Missing dependency: %1").arg(it.key()),
                        QCoreApplication::translate("ModChecker", "Required by: %1").arg(it.value().join(QStringLiteral(", "))),
                        it.value() });
    }

    QSet<QString> reportedConflicts;
    auto reportConflict = [&issues, &reportedConflicts, &byId](const QString& first, const QString& second, const QString& source) {
        QString left = first;
        QString right = second;
        if (left > right) {
            std::swap(left, right);
        }
        const auto key = left + QLatin1Char('|') + right;
        if (reportedConflicts.contains(key)) {
            return;
        }
        reportedConflicts.insert(key);

        const auto names = byId.value(left).isEmpty() ? QStringList{ left } : QStringList{ displayName(*byId.value(left).first()) };
        const auto otherNames = byId.value(right).isEmpty() ? QStringList{ right } : QStringList{ displayName(*byId.value(right).first()) };
        issues.append({ Issue::Severity::Error,
                        QCoreApplication::translate("ModChecker", "Incompatible mods: %1 and %2").arg(names.first(), otherNames.first()),
                        source,
                        {} });
    };

    for (const auto& [first, second] : s_knownConflicts) {
        if (present.contains(first) && present.contains(second)) {
            reportConflict(first, second,
                           QCoreApplication::translate("ModChecker", "%1 and %2 are known to crash or misbehave together.")
                               .arg(first, second));
        }
    }

    for (const auto& mod : mods) {
        for (const auto& conflict : mod.conflicts) {
            if (conflict.isEmpty() || !present.contains(conflict)) {
                continue;
            }
            reportConflict(mod.id,
                           conflict,
                           QCoreApplication::translate("ModChecker", "%1 declares a conflict with %2.").arg(displayName(mod), conflict));
        }
    }

    for (const auto& mod : mods) {
        if (mod.error.isEmpty()) {
            continue;
        }
        issues.append({ Issue::Severity::Warning,
                        QCoreApplication::translate("ModChecker", "Could not read mod metadata: %1").arg(mod.file),
                        mod.error,
                        { mod.file } });
    }

    std::stable_sort(issues.begin(), issues.end(), [](const Issue& left, const Issue& right) {
        return static_cast<int>(left.severity) > static_cast<int>(right.severity);
    });
    return issues;
}

QList<Issue> check(const QString& modsDir)
{
    return analyze(scan(modsDir));
}

}  // namespace ModChecker
