// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class MinecraftInstance;

/**
 * Manages one dedicated Minecraft server for an instance.
 *
 * The server lives in its own folder (by default <instance>/server) and is described by a small
 * JSON file in the instance root, so importing an existing server never rewrites its files.
 * Creation, detection and launching are all kept here; the UI only calls into this class.
 */
class ServerManager : public QObject {
    Q_OBJECT

   public:
    enum class Kind { Vanilla, Fabric, Quilt, Forge, NeoForge, Paper, Purpur, Unknown };

    static QString kindName(Kind kind);
    static QString kindId(Kind kind);
    static Kind kindFromId(const QString& id);

    struct Config {
        QString directory;
        QString jar;
        //! Forge/NeoForge argument file (win_args.txt / unix_args.txt), relative to `directory`.
        QString argsFile;
        QString javaPath;
        int memoryMb = 2048;
        QStringList extraArgs;
        QString minecraftVersion;
        Kind kind = Kind::Unknown;

        bool isEmpty() const { return directory.isEmpty(); }
    };

    struct Info {
        bool valid = false;
        Config config;
        bool eulaAccepted = false;
        QStringList warnings;
    };

    explicit ServerManager(QObject* parent = nullptr);
    ~ServerManager() override;

    void setInstance(MinecraftInstance* instance);

    //! Default folder and Java/memory take from the instance settings.
    static Config defaultConfig(MinecraftInstance* instance);
    //! Read <instanceRoot>/wicked-server.json, keeping `fallback` values for missing fields.
    static Config loadConfig(const QString& configPath, const Config& fallback);
    static bool saveConfig(const QString& configPath, const Config& config, QString* error = nullptr);

    //! Fill in jar/kind/version/eula and warnings for an existing folder.
    static Info inspect(const Config& config);
    static void writeEula(const QString& directory, bool accepted);

    QString configPath() const;
    const Config& config() const { return m_config; }
    void setConfig(const Config& config) { m_config = config; }
    bool persist(QString* error = nullptr);

    bool isRunning() const;
    bool start(QString* error = nullptr);
    void stop();
    void kill();
    void sendCommand(const QString& command);

   signals:
    void outputReady(const QString& text);
    void runningChanged(bool running);
    void stateChanged();

   private slots:
    void onProcessStarted();
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

   private:
    QString resolveJava() const;
    QStringList buildArguments(QString* error) const;
    void appendOutput(const QString& text);

    MinecraftInstance* m_instance = nullptr;
    Config m_config;
    QProcess* m_process = nullptr;
    bool m_stopping = false;
};
