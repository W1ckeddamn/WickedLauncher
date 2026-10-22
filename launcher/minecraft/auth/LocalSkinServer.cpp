// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "LocalSkinServer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

LocalSkinServer& LocalSkinServer::instance()
{
    static auto* server = new LocalSkinServer(QCoreApplication::instance());
    return *server;
}

LocalSkinServer::LocalSkinServer(QObject* parent) : QObject(parent), m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &LocalSkinServer::onNewConnection);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &LocalSkinServer::stop);
}

LocalSkinServer::~LocalSkinServer()
{
    stop();
}

bool LocalSkinServer::ensureRunning()
{
    if (m_server->isListening()) {
        return true;
    }
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        qWarning() << "LocalSkinServer failed to listen on localhost:" << m_server->errorString();
        return false;
    }
    m_port = m_server->serverPort();
    qDebug() << "LocalSkinServer listening on" << url();
    return true;
}

void LocalSkinServer::stop()
{
    if (m_server && m_server->isListening()) {
        m_server->close();
        m_port = 0;
    }
}

QString LocalSkinServer::url() const
{
    if (m_port == 0)
        return QString();
    return QString("http://127.0.0.1:%1").arg(m_port);
}

QString LocalSkinServer::metadata() const
{
    QJsonObject meta;
    meta["serverName"] = "Wicked Local";
    meta["implementationName"] = "WickedLauncher";
    meta["implementationVersion"] = "1.0.0";

    QJsonArray skinDomains;
    skinDomains.append("127.0.0.1");
    skinDomains.append("localhost");

    QJsonObject root;
    root["meta"] = meta;
    root["skinDomains"] = skinDomains;

    return QString::fromLatin1(QJsonDocument(root).toJson(QJsonDocument::Compact).toBase64());
}

void LocalSkinServer::registerAccount(const QString& uuid,
                                     const QString& name,
                                     const QByteArray& skinData,
                                     const QString& skinVariant,
                                     const QByteArray& capeData)
{
    QString cleanUuid = uuid;
    cleanUuid.remove('-');
    cleanUuid.remove('{');
    cleanUuid.remove('}');
    cleanUuid = cleanUuid.toLower();

    ProfileInfo info;
    info.uuid = cleanUuid;
    info.name = name;
    info.skinData = skinData;
    info.skinVariant = skinVariant.isEmpty() ? "classic" : skinVariant;
    info.capeData = capeData;

    m_profiles[cleanUuid] = info;
    m_profiles[name.toLower()] = info;
}

const LocalSkinServer::ProfileInfo* LocalSkinServer::findProfile(const QString& idOrName) const
{
    QString key = idOrName;
    key.remove('-');
    key.remove('{');
    key.remove('}');
    key = key.toLower();

    auto it = m_profiles.constFind(key);
    if (it != m_profiles.constEnd()) {
        return &it.value();
    }
    return nullptr;
}

void LocalSkinServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &LocalSkinServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &LocalSkinServer::onClientDisconnected);
    }
}

void LocalSkinServer::onClientReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    QByteArray data = socket->readAll();
    handleRequest(socket, data);
}

void LocalSkinServer::onClientDisconnected()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

void LocalSkinServer::handleRequest(QTcpSocket* socket, const QByteArray& requestData)
{
    int firstLineEnd = requestData.indexOf("\r\n");
    if (firstLineEnd == -1) {
        firstLineEnd = requestData.indexOf("\n");
    }
    if (firstLineEnd == -1) {
        socket->disconnectFromHost();
        return;
    }

    QString firstLine = QString::fromUtf8(requestData.left(firstLineEnd)).trimmed();
    QStringList parts = firstLine.split(' ');
    if (parts.size() < 2 || parts[0].toUpper() != "GET") {
        QByteArray resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        socket->write(resp);
        socket->disconnectFromHost();
        return;
    }

    QString fullPath = parts[1];
    QString path = fullPath.section('?', 0, 0);

    if (path == "/" || path.isEmpty()) {
        QJsonObject meta;
        meta["serverName"] = "Wicked Local";
        meta["implementationName"] = "WickedLauncher";
        meta["implementationVersion"] = "1.0.0";

        QJsonArray skinDomains;
        skinDomains.append("127.0.0.1");
        skinDomains.append("localhost");

        QJsonObject root;
        root["meta"] = meta;
        root["skinDomains"] = skinDomains;

        QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
        QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " +
                          QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        socket->write(resp);
        socket->disconnectFromHost();
        return;
    }

    if (path.startsWith("/sessionserver/session/minecraft/profile/")) {
        QString id = path.mid(QString("/sessionserver/session/minecraft/profile/").length());
        const ProfileInfo* prof = findProfile(id);
        if (!prof) {
            QByteArray resp = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            socket->write(resp);
            socket->disconnectFromHost();
            return;
        }

        QJsonObject textures;
        if (!prof->skinData.isEmpty()) {
            QJsonObject skin;
            skin["url"] = QString("%1/textures/%2_skin").arg(url(), prof->uuid);
            if (prof->skinVariant == "slim") {
                QJsonObject modelMeta;
                modelMeta["model"] = "slim";
                skin["metadata"] = modelMeta;
            }
            textures["SKIN"] = skin;
        }
        if (!prof->capeData.isEmpty()) {
            QJsonObject cape;
            cape["url"] = QString("%1/textures/%2_cape").arg(url(), prof->uuid);
            textures["CAPE"] = cape;
        }

        QJsonObject texturesJson;
        texturesJson["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        texturesJson["profileId"] = prof->uuid;
        texturesJson["profileName"] = prof->name;
        texturesJson["textures"] = textures;

        QByteArray texturesBase64 = QJsonDocument(texturesJson).toJson(QJsonDocument::Compact).toBase64();

        QJsonObject prop;
        prop["name"] = "textures";
        prop["value"] = QString::fromLatin1(texturesBase64);

        QJsonArray properties;
        properties.append(prop);

        QJsonObject responseJson;
        responseJson["id"] = prof->uuid;
        responseJson["name"] = prof->name;
        responseJson["properties"] = properties;

        QByteArray body = QJsonDocument(responseJson).toJson(QJsonDocument::Compact);
        QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " +
                          QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        socket->write(resp);
        socket->disconnectFromHost();
        return;
    }

    if (path.startsWith("/textures/")) {
        QString texName = path.mid(QString("/textures/").length());
        if (texName.endsWith("_skin")) {
            QString id = texName.left(texName.length() - 5);
            const ProfileInfo* prof = findProfile(id);
            if (prof && !prof->skinData.isEmpty()) {
                QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " +
                                  QByteArray::number(prof->skinData.size()) + "\r\nConnection: close\r\n\r\n" + prof->skinData;
                socket->write(resp);
                socket->disconnectFromHost();
                return;
            }
        } else if (texName.endsWith("_cape")) {
            QString id = texName.left(texName.length() - 5);
            const ProfileInfo* prof = findProfile(id);
            if (prof && !prof->capeData.isEmpty()) {
                QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " +
                                  QByteArray::number(prof->capeData.size()) + "\r\nConnection: close\r\n\r\n" + prof->capeData;
                socket->write(resp);
                socket->disconnectFromHost();
                return;
            }
        }
    }

    QByteArray resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    socket->write(resp);
    socket->disconnectFromHost();
}
