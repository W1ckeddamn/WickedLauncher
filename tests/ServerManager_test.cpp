#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <iostream>

#include "archive/ArchiveWriter.h"
#include "minecraft/server/ServerManager.h"

namespace {
bool writeJar(const QString& path, const QByteArray& versionJson)
{
    MMCZip::ArchiveWriter writer(path);
    if (!writer.open()) {
        return false;
    }
    if (!writer.addFile(QStringLiteral("version.json"), versionJson)) {
        return false;
    }
    return static_cast<bool>(writer.close());
}

bool writeTextFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}
}  // namespace

class ServerManagerTest : public QObject {
    Q_OBJECT

   private slots:
    void restartCancelsStopTimeout()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeTextFile(dir.filePath(QStringLiteral("server.jar")), "unused"));
        ServerManager::writeEula(dir.path(), true);
        ServerManager manager;
        ServerManager::Config config;
        config.directory = dir.path();
        config.jar = QStringLiteral("server.jar");
        config.javaPath = QCoreApplication::applicationFilePath();
        config.extraArgs = { QStringLiteral("--server-process-helper") };
        manager.setConfig(config);
        auto* timer = manager.findChild<QTimer*>();
        QVERIFY(timer != nullptr);
        timer->setInterval(500);
        QString error;
        QVERIFY2(manager.start(&error), qPrintable(error));
        manager.stop();
        QVERIFY(timer->isActive());
        QTRY_VERIFY(!manager.isRunning());
        QVERIFY(!timer->isActive());
        QVERIFY2(manager.start(&error), qPrintable(error));
        QTest::qWait(600);
        QVERIFY(manager.isRunning());
        manager.stop();
        QTRY_VERIFY(!manager.isRunning());
    }

    void buffersSplitUtf8Output()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeTextFile(dir.filePath(QStringLiteral("server.jar")), "unused"));
        ServerManager::writeEula(dir.path(), true);
        ServerManager manager;
        ServerManager::Config config;
        config.directory = dir.path();
        config.jar = QStringLiteral("server.jar");
        config.javaPath = QCoreApplication::applicationFilePath();
        config.extraArgs = { QStringLiteral("--server-process-helper") };
        manager.setConfig(config);
        QString error;
        QVERIFY2(manager.start(&error), qPrintable(error));
        QSignalSpy output(&manager, &ServerManager::outputReady);
        manager.sendCommand(QStringLiteral("output-first"));
        QTest::qWait(50);
        QVERIFY(output.isEmpty());
        manager.sendCommand(QStringLiteral("output-second"));
        QTRY_VERIFY(!output.isEmpty());
        QCOMPARE(output.first().first().toString().trimmed(), QString::fromUtf8("\xD0\x96"));
        manager.stop();
        QTRY_VERIFY(!manager.isRunning());
        bool hasTrailingOutput = false;
        for (const auto& signal : output) {
            hasTrailingOutput |= signal.first().toString() == QStringLiteral("trailing");
        }
        QVERIFY(hasTrailingOutput);
    }

    void inspectRejectsAmbiguousForgeVersions()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto root = QStringLiteral("libraries/net/minecraftforge/forge/");
#if defined(Q_OS_WIN)
        const auto name = QStringLiteral("win_args.txt");
#else
        const auto name = QStringLiteral("unix_args.txt");
#endif
        for (const auto& version : { QStringLiteral("1.20.1-47.2.0"), QStringLiteral("1.21-51.0.0") }) {
            QVERIFY(QDir().mkpath(dir.filePath(root + version)));
            QVERIFY(writeTextFile(dir.filePath(root + version + QLatin1Char('/') + name), "--nogui\n"));
        }
        ServerManager::Config config;
        config.directory = dir.path();
        QVERIFY(!ServerManager::inspect(config).valid);
        config.argsFile = root + QStringLiteral("1.21-51.0.0/") + name;
        const auto info = ServerManager::inspect(config);
        QVERIFY(info.valid);
        QCOMPARE(info.config.minecraftVersion, QStringLiteral("1.21"));
    }

    void configRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("wicked-server.json"));

        ServerManager::Config config;
        config.directory = dir.filePath(QStringLiteral("srv"));
        config.jar = QStringLiteral("fabric-server-launch.jar");
        config.argsFile = QStringLiteral("libraries/x/win_args.txt");
        config.javaPath = QStringLiteral("C:/Java/bin/java.exe");
        config.memoryMb = 6144;
        config.extraArgs = { QStringLiteral("-Dfoo=bar"), QStringLiteral("-Dwith space=1") };
        config.minecraftVersion = QStringLiteral("1.20.4");
        config.kind = ServerManager::Kind::Fabric;

        QString error;
        QVERIFY2(ServerManager::saveConfig(path, config, &error), qPrintable(error));

        const auto loaded = ServerManager::loadConfig(path, {});
        QCOMPARE(loaded.directory, config.directory);
        QCOMPARE(loaded.jar, config.jar);
        QCOMPARE(loaded.argsFile, config.argsFile);
        QCOMPARE(loaded.javaPath, config.javaPath);
        QCOMPARE(loaded.memoryMb, config.memoryMb);
        QCOMPARE(loaded.extraArgs, config.extraArgs);
        QCOMPARE(loaded.minecraftVersion, config.minecraftVersion);
        QCOMPARE(loaded.kind, ServerManager::Kind::Fabric);
    }

    void inspectVanillaServer()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto folder = dir.filePath(QStringLiteral("server"));
        QVERIFY(QDir().mkpath(folder));
        QVERIFY(writeJar(QDir(folder).filePath(QStringLiteral("server.jar")), R"({ "id": "1.20.4", "name": "1.20.4" })"));
        QVERIFY(writeTextFile(QDir(folder).filePath(QStringLiteral("eula.txt")), "eula=true\n"));

        ServerManager::Config config;
        config.directory = folder;
        const auto info = ServerManager::inspect(config);

        QVERIFY(info.valid);
        QCOMPARE(info.config.jar, QStringLiteral("server.jar"));
        QCOMPARE(info.config.kind, ServerManager::Kind::Vanilla);
        QCOMPARE(info.config.minecraftVersion, QStringLiteral("1.20.4"));
        QVERIFY(info.eulaAccepted);
        QVERIFY(info.warnings.isEmpty());
    }

    void inspectDetectsForgeArguments()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto folder = dir.filePath(QStringLiteral("server"));
        const auto forgeArgs = QStringLiteral("libraries/net/minecraftforge/forge/1.20.1-47.2.0");
        QVERIFY(QDir().mkpath(QDir(folder).filePath(forgeArgs)));
        QVERIFY(writeJar(QDir(folder).filePath(QStringLiteral("server.jar")), "{}"));
        QVERIFY(writeTextFile(QDir(folder).filePath(forgeArgs + QStringLiteral("/win_args.txt")), "--nogui\n"));
        QVERIFY(writeTextFile(QDir(folder).filePath(forgeArgs + QStringLiteral("/unix_args.txt")), "--nogui\n"));

        ServerManager::Config config;
        config.directory = folder;
        const auto info = ServerManager::inspect(config);

        QVERIFY(info.valid);
        QCOMPARE(info.config.kind, ServerManager::Kind::Forge);
        QCOMPARE(info.config.minecraftVersion, QStringLiteral("1.20.1"));
#if defined(Q_OS_WIN)
        QCOMPARE(info.config.argsFile, forgeArgs + QStringLiteral("/win_args.txt"));
#else
        QCOMPARE(info.config.argsFile, forgeArgs + QStringLiteral("/unix_args.txt"));
#endif
    }

    void inspectMissingFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        ServerManager::Config config;
        config.directory = dir.filePath(QStringLiteral("does-not-exist"));
        const auto info = ServerManager::inspect(config);

        QVERIFY(!info.valid);
        QVERIFY(!info.warnings.isEmpty());
    }

    void eulaHandling()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto folder = dir.filePath(QStringLiteral("server"));
        QVERIFY(QDir().mkpath(folder));

        ServerManager::writeEula(folder, true);
        QFile eula(QDir(folder).filePath(QStringLiteral("eula.txt")));
        QVERIFY(eula.open(QIODevice::ReadOnly | QIODevice::Text));
        QVERIFY(QString::fromUtf8(eula.readAll()).contains(QStringLiteral("eula=true")));
    }
};

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--server-process-helper")) {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (line == "output-first") {
                    std::cout << "\xD0" << std::flush;
                } else if (line == "output-second") {
                    std::cout << "\x96\ntrailing" << std::flush;
                }
                if (line == "stop") {
                    return 0;
                }
            }
            return 0;
        }
    }
    QCoreApplication application(argc, argv);
    ServerManagerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ServerManager_test.moc"
