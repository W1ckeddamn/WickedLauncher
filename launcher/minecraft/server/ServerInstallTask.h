// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include <QByteArray>
#include <QQueue>
#include <QString>
#include <QUrl>
#include <functional>

#include "ServerManager.h"
#include "tasks/Task.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QSaveFile;

/**
 * Downloads and prepares a new server folder.
 *
 * Supported providers are Vanilla, Paper, Purpur and Fabric (loader downloaded from Fabric's meta
 * service, no installer run required). Forge and NeoForge need their official installer and are
 * therefore only supported through importing an existing folder.
 */
class ServerInstallTask : public Task {
    Q_OBJECT

   public:
    ServerInstallTask(QNetworkAccessManager* network,
                      QString directory,
                      ServerManager::Kind kind,
                      QString minecraftVersion);

    //! Name of the downloaded jar inside the server folder.
    QString jarName() const { return m_jarName; }
    //! Filled in when the provider knows the runtime arguments file.
    QString argsFile() const { return m_argsFile; }

   protected:
    void executeTask() override;
    bool abort() override;

   private:
    using JsonHandler = std::function<void(const QByteArray&)>;

    struct Step {
        QUrl url;
        //! Empty for JSON requests, otherwise the file the response is written to.
        QString target;
        JsonHandler handler;
    };

    void enqueueJson(const QUrl& url, JsonHandler handler);
    void enqueueDownload(const QUrl& url, const QString& fileName);
    void startNext();
    void startJson(const QUrl& url);
    void startDownload(const QUrl& url, const QString& target);
    void finishJson();
    void finishDownload();
    void fail(const QString& reason);
    void resolveVanilla(const QByteArray& json);
    void resolveFabric(const QByteArray& json);

    QNetworkAccessManager* m_network = nullptr;
    QQueue<Step> m_steps;
    QNetworkReply* m_reply = nullptr;
    QSaveFile* m_file = nullptr;
    JsonHandler m_handler;
    QByteArray m_json;

    QString m_directory;
    ServerManager::Kind m_kind = ServerManager::Kind::Unknown;
    QString m_version;
    QString m_jarName;
    QString m_argsFile;
    //! Vanilla server jar URL discovered while resolving Fabric's dependencies.
    QString m_vanillaServerUrl;
    bool m_aborted = false;
};
