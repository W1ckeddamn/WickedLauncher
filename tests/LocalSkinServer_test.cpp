#include <QTcpSocket>
#include <QTest>
#include <QUrl>

#include "minecraft/auth/LocalSkinServer.h"

class LocalSkinServerTest : public QObject {
    Q_OBJECT

   private slots:
    void cleanup() { LocalSkinServer::instance().stop(); }

    void fragmentedRequest()
    {
        auto& server = LocalSkinServer::instance();
        QVERIFY(server.ensureRunning());
        QTcpSocket socket;
        socket.connectToHost(QStringLiteral("127.0.0.1"), QUrl(server.url()).port());
        QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
        socket.write("GE");
        socket.flush();
        QTest::qWait(30);
        QCOMPARE(socket.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(socket.bytesAvailable(), qint64(0));
        socket.write("T / HTTP/1.1\r\nHost: localhost\r\n");
        socket.flush();
        QTest::qWait(30);
        QCOMPARE(socket.bytesAvailable(), qint64(0));
        socket.write("\r\n");
        QTRY_VERIFY(socket.bytesAvailable() > 0);
        QVERIFY(socket.readAll().startsWith("HTTP/1.1 200 OK\r\n"));
    }

    void oversizedHeader()
    {
        auto& server = LocalSkinServer::instance();
        QVERIFY(server.ensureRunning());
        QTcpSocket socket;
        socket.connectToHost(QStringLiteral("127.0.0.1"), QUrl(server.url()).port());
        QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
        socket.write(QByteArray(16 * 1024 + 1, 'x'));
        QTRY_VERIFY(socket.bytesAvailable() > 0);
        QVERIFY(socket.readAll().startsWith("HTTP/1.1 431 "));
    }
};

QTEST_GUILESS_MAIN(LocalSkinServerTest)

#include "LocalSkinServer_test.moc"
