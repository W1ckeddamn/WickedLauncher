// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTcpServer>

class LocalSkinServer : public QObject {
    Q_OBJECT
public:
    struct ProfileInfo {
        QString uuid;
        QString name;
        QByteArray skinData;
        QString skinVariant;
        QByteArray capeData;
    };

    static LocalSkinServer& instance();

    bool ensureRunning();
    void stop();

    void registerAccount(const QString& uuid,
                         const QString& name,
                         const QByteArray& skinData,
                         const QString& skinVariant,
                         const QByteArray& capeData = {});

    QString url() const;
    QString metadata() const;

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    explicit LocalSkinServer(QObject* parent = nullptr);
    ~LocalSkinServer() override;

    void handleRequest(class QTcpSocket* socket, const QByteArray& requestData);
    const ProfileInfo* findProfile(const QString& idOrName) const;

    QTcpServer* m_server = nullptr;
    quint16 m_port = 0;
    QHash<QString, ProfileInfo> m_profiles;
};
