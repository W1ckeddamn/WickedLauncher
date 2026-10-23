// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "ServerManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

#include "Application.h"
#include "FileSystem.h"
#include "archive/ArchiveReader.h"
#include "minecraft/MinecraftInstance.h"

namespace {
const QString s_configName = QStringLiteral("wicked-server.json");

//! Strip ANSI colour codes; the server emits them when a console is detected.
QString stripAnsi(QString text)
{
    static const QRegularExpression ansi(QStringLiteral("\x1b\\[[0-9;]*[A-Za-z]"));
    return text.remove(ansi);
}

bool looksLikeInstaller(const QString& fileName)
{
    const auto lower = fileName.toLower();
    return lower.contains(QStringLiteral("installer")) || lower.contains(QStringLiteral("-sources")) ||
           lower.contains(QStringLiteral("-javadoc"));
}

QString pickJar(const QDir& dir, const QString& configured)
{
    if (!configured.isEmpty() && QFileInfo::exists(dir.filePath(configured))) {
        return configured;
    }

    for (const auto& name : { QStringLiteral("fabric-server-launch.jar"), QStringLiteral("quilt-server-launch.jar"),
                              QStringLiteral("server.jar"), QStringLiteral("paper.jar"), QStringLiteral("purpur.jar") }) {
        if (QFileInfo::exists(dir.filePath(name))) {
            return name;
        }
    }

    const auto entries = dir.entryList({ QStringLiteral("*.jar") }, QDir::Files, QDir::Name);
    for (const auto& name : entries) {
        if (looksLikeInstaller(name)) {
            continue;
        }
        const auto lower = name.toLower();
        if (lower.contains(QStringLiteral("server")) || lower.contains(QStringLiteral("paper")) ||
            lower.contains(QStringLiteral("purpur")) || lower.contains(QStringLiteral("forge"))) {
            return name;
        }
    }
    for (const auto& name : entries) {
        if (!looksLikeInstaller(name)) {
            return name;
        }
    }
    return {};
}

ServerManager::Kind detectKind(const QDir& dir, const QString& jar)
{
    if (QFileInfo::exists(dir.filePath(QStringLiteral("libraries/net/neoforged/neoforge")))) {
        return ServerManager::Kind::NeoForge;
    }
    if (QFileInfo::exists(dir.filePath(QStringLiteral("libraries/net/minecraftforge/forge")))) {
        return ServerManager::Kind::Forge;
    }
    if (QFileInfo::exists(dir.filePath(QStringLiteral("libraries/net/fabricmc/fabric-loader")))) {
        return ServerManager::Kind::Fabric;
    }
    if (QFileInfo::exists(dir.filePath(QStringLiteral("libraries/org/quiltmc/quilt-loader")))) {
        return ServerManager::Kind::Quilt;
    }
    if (QFileInfo::exists(dir.filePath(QStringLiteral("fabric-server-launch.jar")))) {
        return ServerManager::Kind::Fabric;
    }
    if (QFileInfo::exists(dir.filePath(QStringLiteral("quilt-server-launch.jar")))) {
        return ServerManager::Kind::Quilt;
    }

    const auto lower = jar.toLower();
    if (lower.contains(QStringLiteral("neoforge"))) {
        return ServerManager::Kind::NeoForge;
    }
    if (lower.contains(QStringLiteral("forge"))) {
        return ServerManager::Kind::Forge;
    }
    if (lower.contains(QStringLiteral("fabric")) ||
        QFileInfo::exists(dir.filePath(QStringLiteral("fabric-server-launcher.properties")))) {
        return ServerManager::Kind::Fabric;
    }
    if (lower.contains(QStringLiteral("quilt"))) {
        return ServerManager::Kind::Quilt;
    }
    if (lower.contains(QStringLiteral("purpur"))) {
        return ServerManager::Kind::Purpur;
    }
    if (lower.contains(QStringLiteral("paper"))) {
        return ServerManager::Kind::Paper;
    }
    return ServerManager::Kind::Vanilla;
}

//! First version-looking directory below `path`, if any.
QString firstSubdirectory(const QDir& path)
{
    if (!path.exists()) {
        return {};
    }
    const auto entries = path.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    return entries.isEmpty() ? QString() : entries.first();
}

QString versionFromJar(const QString& jarPath)
{
    if (jarPath.isEmpty()) {
        return {};
    }
    MMCZip::ArchiveReader reader(jarPath);
    const auto data = reader.readFile(QStringLiteral("version.json"));
    if (!data) {
        return {};
    }
    const auto document = QJsonDocument::fromJson(*data);
    if (!document.isObject()) {
        return {};
    }
    const auto object = document.object();
    return object.value(QStringLiteral("id")).toString(object.value(QStringLiteral("name")).toString());
}

QString versionFromFolderName(const QString& name)
{
    // Forge: "<minecraft>-<forge>", e.g. 1.20.1-47.2.0
    const int separator = name.indexOf(QLatin1Char('-'));
    if (separator > 0) {
        return name.left(separator);
    }
    // NeoForge: "<major>.<minor>.<patch>", e.g. 20.4.237 -> 1.20.4, 21.0.167 -> 1.21
    const auto parts = name.split(QLatin1Char('.'));
    if (parts.size() >= 2) {
        if (parts.at(1) == QStringLiteral("0")) {
            return QStringLiteral("1.%1").arg(parts.at(0));
        }
        return QStringLiteral("1.%1.%2").arg(parts.at(0), parts.at(1));
    }
    return {};
}

QString detectVersion(const QDir& dir, const QString& jar, ServerManager::Kind kind, const QString& configured)
{
    if (!configured.isEmpty()) {
        return configured;
    }

    if (const auto fromJar = versionFromJar(dir.filePath(jar)); !fromJar.isEmpty()) {
        return fromJar;
    }

    if (kind == ServerManager::Kind::Forge) {
        const auto folder = firstSubdirectory(QDir(dir.filePath(QStringLiteral("libraries/net/minecraftforge/forge"))));
        return versionFromFolderName(folder);
    }
    if (kind == ServerManager::Kind::NeoForge) {
        const auto folder = firstSubdirectory(QDir(dir.filePath(QStringLiteral("libraries/net/neoforged/neoforge"))));
        return versionFromFolderName(folder);
    }
    return {};
}

QString detectArgsFile(const QDir& dir, ServerManager::Kind kind)
{
    if (kind != ServerManager::Kind::Forge && kind != ServerManager::Kind::NeoForge) {
        return {};
    }

    const QString libraryRoot = kind == ServerManager::Kind::Forge ? QStringLiteral("libraries/net/minecraftforge/forge")
                                                                   : QStringLiteral("libraries/net/neoforged/neoforge");
    const auto folder = firstSubdirectory(QDir(dir.filePath(libraryRoot)));
    if (folder.isEmpty()) {
        return {};
    }

#if defined(Q_OS_WIN)
    const QString fileName = QStringLiteral("win_args.txt");
#else
    const QString fileName = QStringLiteral("unix_args.txt");
#endif
    const auto relative = QStringLiteral("%1/%2/%3").arg(libraryRoot, folder, fileName);
    if (QFileInfo::exists(dir.filePath(relative))) {
        return relative;
    }
    return {};
}
}  // namespace

ServerManager::ServerManager(QObject* parent) : QObject(parent) {}

ServerManager::~ServerManager()
{
    if (m_process == nullptr || m_process->state() == QProcess::NotRunning) {
        return;
    }
    // "stop" saves the world and shuts the server down cleanly. Only kill it if it does not react.
    m_process->write("stop\n");
    if (!m_process->waitForFinished(10000)) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

QString ServerManager::kindName(Kind kind)
{
    switch (kind) {
        case Kind::Vanilla:
            return QStringLiteral("Vanilla");
        case Kind::Fabric:
            return QStringLiteral("Fabric");
        case Kind::Quilt:
            return QStringLiteral("Quilt");
        case Kind::Forge:
            return QStringLiteral("Forge");
        case Kind::NeoForge:
            return QStringLiteral("NeoForge");
        case Kind::Paper:
            return QStringLiteral("Paper");
        case Kind::Purpur:
            return QStringLiteral("Purpur");
        case Kind::Unknown:
            break;
    }
    return QStringLiteral("Unknown");
}

QString ServerManager::kindId(Kind kind)
{
    switch (kind) {
        case Kind::Vanilla:
            return QStringLiteral("vanilla");
        case Kind::Fabric:
            return QStringLiteral("fabric");
        case Kind::Quilt:
            return QStringLiteral("quilt");
        case Kind::Forge:
            return QStringLiteral("forge");
        case Kind::NeoForge:
            return QStringLiteral("neoforge");
        case Kind::Paper:
            return QStringLiteral("paper");
        case Kind::Purpur:
            return QStringLiteral("purpur");
        case Kind::Unknown:
            break;
    }
    return QStringLiteral("unknown");
}

ServerManager::Kind ServerManager::kindFromId(const QString& id)
{
    const auto lower = id.trimmed().toLower();
    if (lower == QStringLiteral("vanilla")) {
        return Kind::Vanilla;
    }
    if (lower == QStringLiteral("fabric")) {
        return Kind::Fabric;
    }
    if (lower == QStringLiteral("quilt")) {
        return Kind::Quilt;
    }
    if (lower == QStringLiteral("forge")) {
        return Kind::Forge;
    }
    if (lower == QStringLiteral("neoforge")) {
        return Kind::NeoForge;
    }
    if (lower == QStringLiteral("paper")) {
        return Kind::Paper;
    }
    if (lower == QStringLiteral("purpur")) {
        return Kind::Purpur;
    }
    return Kind::Unknown;
}

void ServerManager::setInstance(MinecraftInstance* instance)
{
    m_instance = instance;
}

ServerManager::Config ServerManager::defaultConfig(MinecraftInstance* instance)
{
    Config config;
    if (instance == nullptr) {
        return config;
    }

    config.directory = FS::PathCombine(instance->instanceRoot(), QStringLiteral("server"));
    config.javaPath = instance->settings()->get(QStringLiteral("JavaPath")).toString();
    config.memoryMb = instance->settings()->get(QStringLiteral("MaxMemAlloc")).toInt();
    if (config.memoryMb <= 0) {
        config.memoryMb = 2048;
    }
    return config;
}

QString ServerManager::configPath() const
{
    if (m_instance == nullptr) {
        return {};
    }
    return FS::PathCombine(m_instance->instanceRoot(), s_configName);
}

ServerManager::Config ServerManager::loadConfig(const QString& configPath, const Config& fallback)
{
    Config config = fallback;

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return config;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return config;
    }

    const auto object = document.object();
    if (object.contains(QStringLiteral("directory"))) {
        config.directory = object.value(QStringLiteral("directory")).toString();
    }
    if (object.contains(QStringLiteral("jar"))) {
        config.jar = object.value(QStringLiteral("jar")).toString();
    }
    if (object.contains(QStringLiteral("argsFile"))) {
        config.argsFile = object.value(QStringLiteral("argsFile")).toString();
    }
    if (object.contains(QStringLiteral("javaPath"))) {
        config.javaPath = object.value(QStringLiteral("javaPath")).toString();
    }
    if (object.contains(QStringLiteral("memoryMb"))) {
        const int memory = object.value(QStringLiteral("memoryMb")).toInt();
        if (memory > 0) {
            config.memoryMb = memory;
        }
    }
    if (object.contains(QStringLiteral("extraArgs"))) {
        config.extraArgs.clear();
        for (const auto& value : object.value(QStringLiteral("extraArgs")).toArray()) {
            config.extraArgs.append(value.toString());
        }
    }
    if (object.contains(QStringLiteral("minecraftVersion"))) {
        config.minecraftVersion = object.value(QStringLiteral("minecraftVersion")).toString();
    }
    if (object.contains(QStringLiteral("kind"))) {
        config.kind = kindFromId(object.value(QStringLiteral("kind")).toString());
    }
    return config;
}

bool ServerManager::saveConfig(const QString& configPath, const Config& config, QString* error)
{
    if (configPath.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("No instance folder to store the server settings in.");
        }
        return false;
    }

    if (!FS::ensureFolderPathExists(QFileInfo(configPath).absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create %1").arg(QFileInfo(configPath).absolutePath());
        }
        return false;
    }

    QJsonArray extraArgs;
    for (const auto& argument : config.extraArgs) {
        extraArgs.append(argument);
    }

    const QJsonObject object{
        { QStringLiteral("directory"), config.directory },
        { QStringLiteral("jar"), config.jar },
        { QStringLiteral("argsFile"), config.argsFile },
        { QStringLiteral("javaPath"), config.javaPath },
        { QStringLiteral("memoryMb"), config.memoryMb },
        { QStringLiteral("extraArgs"), extraArgs },
        { QStringLiteral("minecraftVersion"), config.minecraftVersion },
        { QStringLiteral("kind"), kindId(config.kind) },
    };

    QSaveFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not open %1").arg(configPath);
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write %1").arg(configPath);
        }
        return false;
    }
    return true;
}

bool ServerManager::persist(QString* error)
{
    return saveConfig(configPath(), m_config, error);
}

ServerManager::Info ServerManager::inspect(const Config& input)
{
    Info info;
    info.config = input;

    QDir dir(input.directory);
    if (input.directory.isEmpty() || !dir.exists()) {
        info.warnings.append(tr("The server folder does not exist yet."));
        return info;
    }

    const auto jar = pickJar(dir, input.jar);
    const auto kind = input.kind == Kind::Unknown ? detectKind(dir, jar) : input.kind;
    const auto argsFile = input.argsFile.isEmpty() ? detectArgsFile(dir, kind) : input.argsFile;

    // Modern Forge/NeoForge servers have no launch jar at all; they only use their argument file.
    if (jar.isEmpty() && argsFile.isEmpty()) {
        info.warnings.append(tr("No server jar was found in the folder."));
        return info;
    }

    info.config.jar = jar;
    info.config.argsFile = argsFile;
    info.config.kind = kind;
    info.config.minecraftVersion = detectVersion(dir, jar, kind, input.minecraftVersion);

    QFile eula(dir.filePath(QStringLiteral("eula.txt")));
    if (eula.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const auto contents = QString::fromUtf8(eula.readAll()).toLower();
        info.eulaAccepted = contents.contains(QStringLiteral("eula=true"));
    }
    if (!info.eulaAccepted) {
        info.warnings.append(tr("The Minecraft EULA has not been accepted for this server."));
    }

    info.valid = true;
    return info;
}

void ServerManager::writeEula(const QString& directory, bool accepted)
{
    if (directory.isEmpty()) {
        return;
    }
    FS::ensureFolderPathExists(directory);
    QSaveFile file(FS::PathCombine(directory, QStringLiteral("eula.txt")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not write eula.txt in" << directory;
        return;
    }
    file.write(QStringLiteral("# Generated by Wicked Launcher\neula=%1\n")
                   .arg(accepted ? QStringLiteral("true") : QStringLiteral("false"))
                   .toUtf8());
    if (!file.commit()) {
        qWarning() << "Could not save eula.txt in" << directory;
    }
}

QString ServerManager::resolveJava() const
{
    if (!m_config.javaPath.isEmpty()) {
        return m_config.javaPath;
    }
    if (m_instance != nullptr) {
        const auto fromInstance = m_instance->settings()->get(QStringLiteral("JavaPath")).toString();
        if (!fromInstance.isEmpty()) {
            return fromInstance;
        }
    }
    return APPLICATION->javaPath();
}

QStringList ServerManager::buildArguments(QString* error) const
{
    QStringList arguments;

    const auto memory = qBound(256, m_config.memoryMb, 1024 * 1024);
    arguments << QStringLiteral("-Xms%1M").arg(qMin(512, memory));
    arguments << QStringLiteral("-Xmx%1M").arg(memory);
    arguments << m_config.extraArgs;

    if (!m_config.argsFile.isEmpty()) {
        const auto argsPath = FS::PathCombine(m_config.directory, m_config.argsFile);
        if (!QFileInfo::exists(argsPath)) {
            if (error != nullptr) {
                *error = tr("The Forge/NeoForge argument file is missing:\n%1").arg(argsPath);
            }
            return {};
        }
        arguments << QStringLiteral("@%1").arg(m_config.argsFile);
    } else {
        const auto jarPath = FS::PathCombine(m_config.directory, m_config.jar);
        if (m_config.jar.isEmpty() || !QFileInfo::exists(jarPath)) {
            if (error != nullptr) {
                *error = tr("The server jar was not found:\n%1").arg(jarPath);
            }
            return {};
        }
        arguments << QStringLiteral("-jar") << jarPath;
    }

    arguments << QStringLiteral("nogui");
    return arguments;
}

bool ServerManager::isRunning() const
{
    return m_process != nullptr && m_process->state() != QProcess::NotRunning;
}

bool ServerManager::start(QString* error)
{
    if (isRunning()) {
        if (error != nullptr) {
            *error = tr("The server is already running.");
        }
        return false;
    }

    const QDir dir(m_config.directory);
    if (m_config.directory.isEmpty() || !dir.exists()) {
        if (error != nullptr) {
            *error = tr("The server folder does not exist. Create or import a server first.");
        }
        return false;
    }

    const auto info = inspect(m_config);
    if (!info.valid) {
        if (error != nullptr) {
            *error = info.warnings.join(QLatin1Char('\n'));
        }
        return false;
    }
    if (!info.eulaAccepted) {
        if (error != nullptr) {
            *error = tr("The Minecraft EULA has not been accepted for this server.");
        }
        return false;
    }
    m_config = info.config;

    const auto java = resolveJava();
    if (java.isEmpty() || !QFileInfo::exists(java)) {
        if (error != nullptr) {
            *error = tr("No Java runtime was found. Set the Java path for this server first.");
        }
        return false;
    }

    auto arguments = buildArguments(error);
    if (arguments.isEmpty()) {
        return false;
    }

    if (m_process == nullptr) {
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_process, &QProcess::started, this, &ServerManager::onProcessStarted);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &ServerManager::onProcessReadyRead);
        connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &ServerManager::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &ServerManager::onProcessError);
    }

    m_stopping = false;
    m_process->setWorkingDirectory(m_config.directory);
    m_process->setProgram(java);
    m_process->setArguments(arguments);

    appendOutput(tr("[launcher] Starting %1 %2 with Java %3")
                     .arg(kindName(m_config.kind), m_config.minecraftVersion, java));
    appendOutput(QStringLiteral("[launcher] %1 %2").arg(java, arguments.join(QLatin1Char(' '))));

    m_process->start();
    if (!m_process->waitForStarted(5000)) {
        if (error != nullptr) {
            *error = tr("Java could not be started:\n%1").arg(m_process->errorString());
        }
        appendOutput(tr("[launcher] Could not start Java: %1").arg(m_process->errorString()));
        return false;
    }
    return true;
}

void ServerManager::stop()
{
    if (!isRunning()) {
        return;
    }
    m_stopping = true;
    appendOutput(tr("[launcher] Sending 'stop'…"));
    sendCommand(QStringLiteral("stop"));

    // A server that ignores "stop" is killed after half a minute so the launcher stays responsive.
    QTimer::singleShot(30000, this, [this] {
        if (isRunning()) {
            appendOutput(tr("[launcher] The server did not stop in time; killing it."));
            kill();
        }
    });
}

void ServerManager::kill()
{
    if (!isRunning()) {
        return;
    }
    appendOutput(tr("[launcher] Killing the server process."));
    m_process->kill();
}

void ServerManager::sendCommand(const QString& command)
{
    if (!isRunning()) {
        return;
    }
    auto line = command;
    if (!line.endsWith(QLatin1Char('\n'))) {
        line.append(QLatin1Char('\n'));
    }
    m_process->write(line.toUtf8());
}

void ServerManager::appendOutput(const QString& text)
{
    emit outputReady(stripAnsi(text));
}

void ServerManager::onProcessStarted()
{
    emit runningChanged(true);
    emit stateChanged();
}

void ServerManager::onProcessReadyRead()
{
    if (m_process == nullptr) {
        return;
    }
    auto text = QString::fromUtf8(m_process->readAllStandardOutput());
    if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n'))) {
        text.append(QLatin1Char('\n'));
    }
    appendOutput(text);
}

void ServerManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode)
    if (status == QProcess::CrashExit) {
        appendOutput(tr("[launcher] The server process crashed."));
    } else if (m_stopping) {
        appendOutput(tr("[launcher] The server stopped."));
    } else {
        appendOutput(tr("[launcher] The server exited."));
    }
    m_stopping = false;
    emit runningChanged(false);
    emit stateChanged();
}

void ServerManager::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        appendOutput(tr("[launcher] Java could not be started: %1").arg(m_process->errorString()));
        emit runningChanged(false);
        emit stateChanged();
    }
}
