/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "QtNetworkTransportTests.h"
#include "net/NetworkAddress.h"
#include "net/QtNetworkTransport.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <functional>
#include <memory>

namespace {
bool waitFor(const std::function<bool()> &done)
{
  if (done())
    return true;
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  QTimer condition;
  QObject::connect(&condition, &QTimer::timeout, &loop, [&] {
    if (done())
      loop.quit();
  });
  timeout.start(5000);
  condition.start(1);
  loop.exec();
  return done();
}

std::unique_ptr<QTcpSocket> connectPeer(QtNetworkTransport &transport, QTcpServer &server)
{
  transport.connect(NetworkAddress("127.0.0.1", server.serverPort()));
  if (!waitFor([&] { return transport.isConnected() && server.hasPendingConnections(); }))
    return {};
  return std::unique_ptr<QTcpSocket>(server.nextPendingConnection());
}

bool sendBytes(QTcpSocket &peer, QtNetworkTransport &transport, const QByteArray &bytes)
{
  if (peer.write(bytes) != bytes.size())
    return false;
  peer.flush();
  return waitFor([&] { return transport.getSize() == static_cast<uint32_t>(bytes.size()); });
}
} // namespace

void QtNetworkTransportTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
#ifdef Q_OS_WIN
  m_arch.init();
#endif
}

void QtNetworkTransportTests::reconnectPreservesOnlyNewBytes_data()
{
  QTest::addColumn<int>("consumed");
  QTest::addColumn<bool>("doubleClose");
  QTest::newRow("unread") << 0 << false;
  QTest::newRow("partial") << 3 << false;
  QTest::newRow("fully-read") << 10 << false;
  QTest::newRow("partial-double-close") << 3 << true;
}

void QtNetworkTransportTests::reconnectPreservesOnlyNewBytes()
{
  QFETCH(int, consumed);
  QFETCH(bool, doubleClose);
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QtNetworkTransport transport;
  auto peer = connectPeer(transport, server);
  QVERIFY(peer);
  QVERIFY(sendBytes(*peer, transport, QByteArray("OLD-012345", 10)));
  char discarded[10];
  QCOMPARE(transport.read(discarded, consumed), static_cast<uint32_t>(consumed));
  transport.close();
  if (doubleClose)
    transport.close();
  QVERIFY(waitFor([&] { return peer->state() == QAbstractSocket::UnconnectedState; }));
  QCOMPARE(transport.getSize(), 0u);
  peer.reset();
  peer = connectPeer(transport, server);
  QVERIFY(peer);
  QCOMPARE(peer->write("NEW-xyz", 7), qint64{7});
  peer->flush();
  QVERIFY(waitFor([&] { return transport.isReady(); }));
  char result[10] = {};
  QCOMPARE(transport.read(result, sizeof(result)), 7u);
  QCOMPARE(QByteArray(result, 7), QByteArray("NEW-xyz", 7));
  QCOMPARE(transport.getSize(), 0u);
}

void QtNetworkTransportTests::repeatedReconnect()
{
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QtNetworkTransport transport;
  for (unsigned cycle = 0; cycle < 100; ++cycle) {
    auto old = connectPeer(transport, server);
    QVERIFY(old);
    QByteArray previous("OLD-012345", 10);
    previous[9] = static_cast<char>(cycle);
    QVERIFY(sendBytes(*old, transport, previous));
    char bytes[10];
    QCOMPARE(transport.read(bytes, 3), 3u);
    transport.close();
    QVERIFY(waitFor([&] { return old->state() == QAbstractSocket::UnconnectedState; }));
    old.reset();
    auto fresh = connectPeer(transport, server);
    QVERIFY(fresh);
    QByteArray next("NEW-xyz", 7);
    next[6] = static_cast<char>(cycle);
    QCOMPARE(fresh->write(next), qint64{7});
    fresh->flush();
    QVERIFY(waitFor([&] { return transport.isReady(); }));
    QCOMPARE(transport.read(bytes, sizeof(bytes)), 7u);
    QCOMPARE(QByteArray(bytes, 7), next);
    transport.close();
    QVERIFY(waitFor([&] { return fresh->state() == QAbstractSocket::UnconnectedState; }));
  }
}

void QtNetworkTransportTests::failedConnectionCanRetry()
{
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  const auto port = server.serverPort();
  server.close();
  QtNetworkTransport transport;
  QSignalSpy errors(&transport, &QtNetworkTransport::transportError);
  transport.connect(NetworkAddress("127.0.0.1", port));
  QVERIFY(waitFor([&] { return !errors.empty(); }));
  QVERIFY(!transport.isConnected());
  transport.close();
  QVERIFY(server.listen(QHostAddress::LocalHost, port));
  auto peer = connectPeer(transport, server);
  QVERIFY(peer);
  QVERIFY(!transport.isFatal());
  QVERIFY(sendBytes(*peer, transport, QByteArray("retry", 5)));
  char result[5];
  QCOMPARE(transport.read(result, sizeof(result)), 5u);
  QCOMPARE(QByteArray(result, 5), QByteArray("retry", 5));
}

void QtNetworkTransportTests::nullableReadDiscards()
{
  // Run this selected slot in an isolated test process for the baseline crash probe.
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QtNetworkTransport transport;
  auto peer = connectPeer(transport, server);
  QVERIFY(peer);
  QCOMPARE(transport.read(nullptr, 10), 0u);
  QVERIFY(sendBytes(*peer, transport, QByteArray("0123456789", 10)));
  QCOMPARE(transport.read(nullptr, 0), 0u);
  QCOMPARE(transport.getSize(), 10u);
  QCOMPARE(transport.read(nullptr, 3), 3u);
  QCOMPARE(transport.getSize(), 7u);
  char result[10];
  QCOMPARE(transport.read(result, sizeof(result)), 7u);
  QCOMPARE(QByteArray(result, 7), QByteArray("3456789", 7));
  QVERIFY(sendBytes(*peer, transport, QByteArray("abcdefghij", 10)));
  QCOMPARE(transport.read(result, 2), 2u);
  QCOMPARE(transport.read(nullptr, 3), 3u);
  QCOMPARE(transport.read(result, sizeof(result)), 5u);
  QCOMPARE(QByteArray(result, 5), QByteArray("fghij", 5));
  QVERIFY(sendBytes(*peer, transport, QByteArray("klmnopqrst", 10)));
  QCOMPARE(transport.read(nullptr, 100), 10u);
  QCOMPARE(transport.read(nullptr, 1), 0u);
  QVERIFY(sendBytes(*peer, transport, QByteArray("uvwxyzABCD", 10)));
  QCOMPARE(transport.read(nullptr, 3), 3u);
  transport.close();
  QVERIFY(waitFor([&] { return peer->state() == QAbstractSocket::UnconnectedState; }));
  peer.reset();
  peer = connectPeer(transport, server);
  QVERIFY(peer);
  QCOMPARE(peer->write("fresh!!", 7), qint64{7});
  peer->flush();
  QVERIFY(waitFor([&] { return transport.isReady(); }));
  QCOMPARE(transport.read(result, sizeof(result)), 7u);
  QCOMPARE(QByteArray(result, 7), QByteArray("fresh!!", 7));
}

QTEST_GUILESS_MAIN(QtNetworkTransportTests)
