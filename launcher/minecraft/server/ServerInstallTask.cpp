// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "ServerInstallTask.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

#include <utility>

#include "FileSystem.h"

namespace {
const QString s_manifestUrl = QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");

QString paperBuildsUrl(const QString& version)
{
    return QStringLiteral("https://api.papermc.io/v2/projects/paper/versions/%1/builds").arg(version);
}

QString purpurDownloadUrl(const QString& version)
{
    return QStringLiteral("https://api.purpurmc.org/v2/purpur/%1/latest/download").arg(version);
}

QString fabricLoadersUrl(const QString& version)
{
    return QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1").arg(version);
}

QString fabricServerJarUrl(const QString& version, const QString& loader, const QString& installer)
{
    return QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1/%2/%3/server/jar").arg(version, loader, installer);
}

const QString s_fabricInstallerUrl = QStringLiteral("https://meta.fabricmc.net/v2/versions/installer");

QJsonObject parseObject(const QByteArray& data)
{
    const auto document = QJsonDocument::fromJson(data);
    return document.isObject() ? document.object() : QJsonObject{};
}
}  // namespace

ServerInstallTask::ServerInstallTask(QNetworkAccessManager* network,
                                     QString directory,
                                     ServerManager::Kind kind,
                                     QString minecraftVersion)
    : Task(),
      m_network(network),
      m_directory(std::move(directory)),
      m_kind(kind),
      m_version(std::move(minecraftVersion))
{
    setStatus(tr("Preparing the server folder"));
}

void ServerInstallTask::executeTask()
{
    if (m_network == nullptr) {
        fail(tr("No network access is available."));
        return;
    }
    if (m_version.trimmed().isEmpty()) {
        fail(tr("No Minecraft version was selected."));
        return;
    }
    if (!FS::ensureFolderPathExists(m_directory)) {
        fail(tr("Could not create the server folder:\n%1").arg(m_directory));
        return;
    }

    m_jarName = QStringLiteral("server.jar");

    switch (m_kind) {
        case ServerManager::Kind::Vanilla:
            setStatus(tr("Looking up Minecraft %1").arg(m_version));
            enqueueJson(QUrl(s_manifestUrl), [this](const QByteArray& data) { resolveVanilla(data); });
            break;
        case ServerManager::Kind::Fabric:
            setStatus(tr("Looking up Fabric for Minecraft %1").arg(m_version));
            enqueueJson(QUrl(s_manifestUrl), [this](const QByteArray& data) { resolveFabric(data); });
            break;
        case ServerManager::Kind::Paper:
            setStatus(tr("Looking up Paper builds for Minecraft %1").arg(m_version));
            enqueueJson(QUrl(paperBuildsUrl(m_version)), [this](const QByteArray& data) {
                const auto object = parseObject(data);
                const auto builds = object.value(QStringLiteral("builds")).toArray();
                if (builds.isEmpty()) {
                    fail(tr("Paper has no builds for Minecraft %1.").arg(m_version));
                    return;
                }
                const auto build = builds.last().toObject();
                const auto buildNumber = build.value(QStringLiteral("build")).toVariant().toString();
                const auto downloads = build.value(QStringLiteral("downloads")).toObject();
                const auto application = downloads.value(QStringLiteral("application")).toObject();
                const auto name = application.value(QStringLiteral("name")).toString();
                if (name.isEmpty()) {
                    fail(tr("Paper did not return a download for build %1.").arg(buildNumber));
                    return;
                }
                const auto url = QStringLiteral("https://api.papermc.io/v2/projects/paper/versions/%1/builds/%2/downloads/%3")
                                     .arg(m_version, buildNumber, name);
                m_jarName = name;
                enqueueDownload(QUrl(url), name);
            });
            break;
        case ServerManager::Kind::Purpur:
            setStatus(tr("Downloading Purpur for Minecraft %1").arg(m_version));
            enqueueDownload(QUrl(purpurDownloadUrl(m_version)), QStringLiteral("purpur.jar"));
            m_jarName = QStringLiteral("purpur.jar");
            break;
        case ServerManager::Kind::Forge:
        case ServerManager::Kind::NeoForge:
            fail(tr("Forge and NeoForge servers have to be created with their official installer. "
                    "Create the server elsewhere and use \"Import Server\"."));
            return;
        case ServerManager::Kind::Quilt:
        case ServerManager::Kind::Unknown:
            fail(tr("This server type cannot be created automatically. Create it elsewhere and use \"Import Server\"."));
            return;
    }

    startNext();
}

bool ServerInstallTask::abort()
{
    if (m_aborted) {
        return true;
    }
    m_aborted = true;
    if (m_reply != nullptr) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file != nullptr) {
        m_file->cancelWriting();
        delete m_file;
        m_file = nullptr;
    }
    m_steps.clear();
    emitAborted();
    return true;
}

void ServerInstallTask::enqueueJson(const QUrl& url, JsonHandler handler)
{
    m_steps.enqueue({ url, QString(), std::move(handler) });
}

void ServerInstallTask::enqueueDownload(const QUrl& url, const QString& fileName)
{
    m_steps.enqueue({ url, fileName, nullptr });
}

void ServerInstallTask::startNext()
{
    if (m_aborted) {
        return;
    }
    if (m_steps.isEmpty()) {
        emitSucceeded();
        return;
    }

    const auto step = m_steps.dequeue();
    if (step.target.isEmpty()) {
        m_handler = step.handler;
        startJson(step.url);
    } else {
        m_handler = nullptr;
        startDownload(step.url, step.target);
    }
}

void ServerInstallTask::startJson(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);

    m_json.clear();
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_reply != nullptr) {
            m_json.append(m_reply->readAll());
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, &ServerInstallTask::finishJson);
}

void ServerInstallTask::startDownload(const QUrl& url, const QString& target)
{
    const auto path = FS::PathCombine(m_directory, target);
    m_file = new QSaveFile(path);
    if (!m_file->open(QIODevice::WriteOnly)) {
        delete m_file;
        m_file = nullptr;
        fail(tr("Could not write %1").arg(path));
        return;
    }

    setStatus(tr("Downloading %1").arg(target));
    setDetails(path);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(120000);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_reply != nullptr && m_file != nullptr) {
            m_file->write(m_reply->readAll());
        }
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(received, total);
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, &ServerInstallTask::finishDownload);
}

void ServerInstallTask::finishJson()
{
    auto* reply = m_reply;
    m_reply = nullptr;
    if (reply == nullptr) {
        return;
    }
    const auto error = reply->error();
    const auto errorString = reply->errorString();
    const auto data = m_json;
    reply->deleteLater();

    if (m_aborted) {
        return;
    }
    if (error != QNetworkReply::NoError) {
        fail(tr("The download failed: %1").arg(errorString));
        return;
    }

    if (m_handler) {
        auto handler = std::move(m_handler);
        m_handler = nullptr;
        handler(data);
    }
    if (!m_aborted) {
        startNext();
    }
}

void ServerInstallTask::finishDownload()
{
    auto* reply = m_reply;
    m_reply = nullptr;
    if (reply == nullptr) {
        return;
    }
    const auto error = reply->error();
    const auto errorString = reply->errorString();
    reply->deleteLater();

    if (m_aborted) {
        return;
    }
    if (error != QNetworkReply::NoError) {
        if (m_file != nullptr) {
            m_file->cancelWriting();
            delete m_file;
            m_file = nullptr;
        }
        fail(tr("The download failed: %1").arg(errorString));
        return;
    }

    if (m_file != nullptr) {
        m_file->write(reply->readAll());
        if (!m_file->commit()) {
            const auto fileError = m_file->errorString();
            delete m_file;
            m_file = nullptr;
            fail(tr("The downloaded file could not be saved: %1").arg(fileError));
            return;
        }
        delete m_file;
        m_file = nullptr;
    }
    startNext();
}

void ServerInstallTask::fail(const QString& reason)
{
    if (m_aborted) {
        return;
    }
    m_aborted = true;
    m_steps.clear();
    if (m_reply != nullptr) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file != nullptr) {
        m_file->cancelWriting();
        delete m_file;
        m_file = nullptr;
    }
    emitFailed(reason);
}

void ServerInstallTask::resolveVanilla(const QByteArray& data)
{
    const auto object = parseObject(data);
    for (const auto& value : object.value(QStringLiteral("versions")).toArray()) {
        const auto version = value.toObject();
        if (version.value(QStringLiteral("id")).toString() != m_version) {
            continue;
        }
        const auto url = version.value(QStringLiteral("url")).toString();
        if (url.isEmpty()) {
            break;
        }
        enqueueJson(QUrl(url), [this](const QByteArray& versionData) {
            const auto downloads = parseObject(versionData).value(QStringLiteral("downloads")).toObject();
            const auto server = downloads.value(QStringLiteral("server")).toObject();
            const auto serverUrl = server.value(QStringLiteral("url")).toString();
            if (serverUrl.isEmpty()) {
                fail(tr("Minecraft %1 has no downloadable server jar.").arg(m_version));
                return;
            }
            enqueueDownload(QUrl(serverUrl), QStringLiteral("server.jar"));
        });
        return;
    }
    fail(tr("Minecraft %1 was not found in Mojang's version list.").arg(m_version));
}

void ServerInstallTask::resolveFabric(const QByteArray& data)
{
    const auto object = parseObject(data);
    QString versionUrl;
    for (const auto& value : object.value(QStringLiteral("versions")).toArray()) {
        const auto version = value.toObject();
        if (version.value(QStringLiteral("id")).toString() == m_version) {
            versionUrl = version.value(QStringLiteral("url")).toString();
            break;
        }
    }
    if (versionUrl.isEmpty()) {
        fail(tr("Minecraft %1 was not found in Mojang's version list.").arg(m_version));
        return;
    }

    enqueueJson(QUrl(versionUrl), [this](const QByteArray& versionData) {
        const auto downloads = parseObject(versionData).value(QStringLiteral("downloads")).toObject();
        m_vanillaServerUrl = downloads.value(QStringLiteral("server")).toObject().value(QStringLiteral("url")).toString();
        if (m_vanillaServerUrl.isEmpty()) {
            fail(tr("Minecraft %1 has no downloadable server jar.").arg(m_version));
            return;
        }

        enqueueJson(QUrl(fabricLoadersUrl(m_version)), [this](const QByteArray& loaderData) {
            const auto loaders = QJsonDocument::fromJson(loaderData).array();
            QString loaderVersion;
            for (const auto& value : loaders) {
                const auto loader = value.toObject().value(QStringLiteral("loader")).toObject();
                const auto candidate = loader.value(QStringLiteral("version")).toString();
                if (candidate.isEmpty()) {
                    continue;
                }
                loaderVersion = candidate;
                if (loader.value(QStringLiteral("stable")).toBool()) {
                    break;
                }
            }
            if (loaderVersion.isEmpty()) {
                fail(tr("Fabric has no loader for Minecraft %1.").arg(m_version));
                return;
            }

            enqueueJson(QUrl(s_fabricInstallerUrl), [this, loaderVersion](const QByteArray& installerData) {
                QString installerVersion;
                for (const auto& value : QJsonDocument::fromJson(installerData).array()) {
                    const auto installer = value.toObject();
                    const auto candidate = installer.value(QStringLiteral("version")).toString();
                    if (candidate.isEmpty()) {
                        continue;
                    }
                    installerVersion = candidate;
                    if (installer.value(QStringLiteral("stable")).toBool()) {
                        break;
                    }
                }
                if (installerVersion.isEmpty()) {
                    fail(tr("Fabric returned no installer version."));
                    return;
                }

                m_jarName = QStringLiteral("fabric-server-launch.jar");
                enqueueDownload(QUrl(m_vanillaServerUrl), QStringLiteral("server.jar"));
                enqueueDownload(QUrl(fabricServerJarUrl(m_version, loaderVersion, installerVersion)),
                                QStringLiteral("fabric-server-launch.jar"));
            });
        });
    });
}
