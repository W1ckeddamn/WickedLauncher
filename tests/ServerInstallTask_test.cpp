#include <QDir>
#include <QFile>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <algorithm>
#include <cstring>

#include "minecraft/server/ServerInstallTask.h"

class ServerInstallTaskTest : public QObject {
    Q_OBJECT

   private slots:
    void refusesExistingFiles();
    void refusesHiddenFiles();
    void installsVanilla();
    void fabricFailureLeavesFolderEmpty();
    void abortHandlesSynchronousFinished();
    void publishDoesNotReplaceFiles();
};

namespace {
struct Response {
    QByteArray body;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
};

class Reply : public QNetworkReply {
   public:
    Reply(const QNetworkRequest& request, Response response, QObject* parent) : QNetworkReply(parent), m_body(response.body)
    {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this, response] {
            if (isFinished()) {
                return;
            }
            if (response.error != NoError) {
                setError(response.error, QStringLiteral("Test network failure"));
            }
            emit readyRead();
            setFinished(true);
            emit finished();
        });
    }

    void abort() override
    {
        if (!isFinished()) {
            setError(OperationCanceledError, QStringLiteral("Cancelled"));
            setFinished(true);
            emit finished();
        }
    }

    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QNetworkReply::bytesAvailable(); }

   protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        const auto size = std::min(maxSize, qint64(m_body.size() - m_offset));
        if (size <= 0) {
            return -1;
        }
        std::memcpy(data, m_body.constData() + m_offset, size);
        m_offset += size;
        return size;
    }

   private:
    QByteArray m_body;
    qint64 m_offset = 0;
};

class Network : public QNetworkAccessManager {
   public:
    QList<Response> responses;
    int requests = 0;

   protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override
    {
        ++requests;
        return new Reply(request, responses.isEmpty() ? Response{ {}, QNetworkReply::ContentNotFoundError } : responses.takeFirst(), this);
    }
};

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

const QByteArray manifest = R"({"versions":[{"id":"1.20.4","url":"https://test.invalid/version"}]})";
const QByteArray version = R"({"downloads":{"server":{"url":"https://test.invalid/server.jar"}}})";
}  // namespace

void ServerInstallTaskTest::refusesExistingFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.filePath(QStringLiteral("purpur.jar")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("original"), qint64(8));
    file.close();
    Network network;
    ServerInstallTask task(&network, dir.path(), ServerManager::Kind::Purpur, QStringLiteral("1.20.4"));
    task.start();
    QVERIFY(task.isFinished());
    QVERIFY(!task.wasSuccessful());
    QCOMPARE(network.requests, 0);
    QCOMPARE(readFile(file.fileName()), QByteArray("original"));
}

void ServerInstallTaskTest::installsVanilla()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Network network;
    network.responses = { { manifest }, { version }, { "server contents" } };
    ServerInstallTask task(&network, dir.path(), ServerManager::Kind::Vanilla, QStringLiteral("1.20.4"));
    task.start();
    QTRY_VERIFY(task.isFinished());
    QVERIFY2(task.wasSuccessful(), qPrintable(task.failReason()));
    QCOMPARE(readFile(dir.filePath(task.jarName())), QByteArray("server contents"));
    QCOMPARE(QDir(dir.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{ "server.jar" });
}

void ServerInstallTaskTest::refusesHiddenFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.filePath(QStringLiteral(".keep")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    Network network;
    ServerInstallTask task(&network, dir.path(), ServerManager::Kind::Purpur, QStringLiteral("1.20.4"));
    task.start();
    QVERIFY(task.isFinished());
    QVERIFY(!task.wasSuccessful());
    QCOMPARE(network.requests, 0);
    QVERIFY(file.exists());
}

void ServerInstallTaskTest::fabricFailureLeavesFolderEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Network network;
    network.responses = { { manifest },
                          { version },
                          { R"([{"loader":{"version":"0.16.0","stable":true}}])" },
                          { R"([{"version":"1.0.0","stable":true}])" },
                          { "downloaded vanilla jar" },
                          { {}, QNetworkReply::RemoteHostClosedError } };
    ServerInstallTask task(&network, dir.path(), ServerManager::Kind::Fabric, QStringLiteral("1.20.4"));
    task.start();
    QTRY_VERIFY(task.isFinished());
    QVERIFY(!task.wasSuccessful());
    QCOMPARE(network.requests, 6);
    QVERIFY(QDir(dir.path()).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot));
}

void ServerInstallTaskTest::abortHandlesSynchronousFinished()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Network network;
    network.responses = { { "partial download" } };
    ServerInstallTask task(&network, dir.path(), ServerManager::Kind::Purpur, QStringLiteral("1.20.4"));
    task.start();
    QVERIFY(task.canAbort());
    QVERIFY(static_cast<Task&>(task).abort());
    QCOMPARE(task.getState(), Task::State::AbortedByUser);
    QCoreApplication::processEvents();
    QVERIFY(QDir(dir.path()).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot));
}

void ServerInstallTaskTest::publishDoesNotReplaceFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Network network;
    network.responses = { { "new server" } };
    ServerInstallTask task(&network, dir.path(), ServerManager::Kind::Purpur, QStringLiteral("1.20.4"));
    task.start();
    // Simulate a file appearing while the network request is in flight.
    QFile file(dir.filePath(QStringLiteral("purpur.jar")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("original"), qint64(8));
    file.close();
    QTRY_VERIFY(task.isFinished());
    QVERIFY(!task.wasSuccessful());
    QCOMPARE(readFile(file.fileName()), QByteArray("original"));
}

QTEST_GUILESS_MAIN(ServerInstallTaskTest)

#include "ServerInstallTask_test.moc"
