/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcServerTests.h"

#include "common/IpcToken.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"
#include "ipc/CoreIpc.h"
#include "ipc/CoreIpcServer.h"
#include "ipc/IpcServer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QUuid>

#include <memory>

namespace {

class TestIpcServer : public synergy::core::ipc::IpcServer
{
public:
  using IpcServer::broadcastToAuthenticatedClients;
  using IpcServer::IpcServer;

  QStringList commands;
  QString authenticatedSnapshot;

protected:
  void clientAuthenticated(QLocalSocket *client) override
  {
    if (!authenticatedSnapshot.isEmpty())
      writeToClientSocket(client, authenticatedSnapshot);
  }

  void processCommand(QLocalSocket *, const QString &command, const QStringList &) override
  {
    commands.append(command);
  }
};

std::unique_ptr<QLocalSocket> openClient(const QString &serverName)
{
  auto client = std::make_unique<QLocalSocket>();
  client->connectToServer(serverName);
  if (!client->waitForConnected(5000)) {
    return nullptr;
  }
  return client;
}

const QString kErrorHelloRequired = QStringLiteral("error=hello required\n");

} // namespace

void IpcServerTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);

  QVERIFY(synergy::runtime::isIsolatedRuntime());
  const auto root = synergy::runtime::runtimeRoot();
  const auto userDir = root + QStringLiteral("/user");
  QVERIFY(QDir().mkpath(userDir));
  QCOMPARE(QFileInfo(userDir).canonicalFilePath(), userDir);
  const auto settingsFile = userDir + QStringLiteral("/Synergy.conf");
  Settings::setSettingsFile(settingsFile);
  Settings::setStateFile(synergy::runtime::runtimeStateFile());

  m_serverName = synergy::runtime::runtimeInstanceKey({});
  QVERIFY(!m_serverName.isEmpty());
  QCOMPARE(Settings::settingsFile(), settingsFile);
  QCOMPARE(QDir::cleanPath(Settings::settingsPath()), userDir);
  QCOMPARE(
      QFileInfo(synergy::ipcTokenPath(m_serverName)).absoluteFilePath(),
      userDir + QLatin1Char('/') + m_serverName + QStringLiteral(".ipc-token")
  );
}

void IpcServerTests::cleanupTestCase()
{
  if (!m_serverName.isEmpty())
    QLocalServer::removeServer(m_serverName);
}

void IpcServerTests::commandBeforeHelloRejected()
{
  TestIpcServer server(nullptr, m_serverName, QStringLiteral("test"));
  QVERIFY(server.listen());

  auto client = openClient(m_serverName);
  QVERIFY(client != nullptr);

  client->write("start\n");
  client->flush();

  QTRY_VERIFY(client->state() == QLocalSocket::UnconnectedState);
  QCOMPARE(QString::fromUtf8(client->readAll()), kErrorHelloRequired);
  QVERIFY(server.commands.isEmpty());
}

void IpcServerTests::helloWithoutTokenRejected()
{
  TestIpcServer server(nullptr, m_serverName, QStringLiteral("test"));
  QVERIFY(server.listen());

  auto client = openClient(m_serverName);
  QVERIFY(client != nullptr);

  client->write("hello=some-version\n");
  client->flush();

  QTRY_VERIFY(client->state() == QLocalSocket::UnconnectedState);
  QCOMPARE(QString::fromUtf8(client->readAll()), kErrorHelloRequired);
}

void IpcServerTests::helloWithWrongTokenRejected()
{
  TestIpcServer server(nullptr, m_serverName, QStringLiteral("test"));
  QVERIFY(server.listen());

  auto client = openClient(m_serverName);
  QVERIFY(client != nullptr);

  client->write("hello=some-version wrong-token\n");
  client->flush();

  QTRY_VERIFY(client->state() == QLocalSocket::UnconnectedState);
  QCOMPARE(QString::fromUtf8(client->readAll()), kErrorHelloRequired);
}

void IpcServerTests::helloWithWrongVersionReportsMismatch()
{
  TestIpcServer server(nullptr, m_serverName, QStringLiteral("test"));
  QVERIFY(server.listen());

  const auto token = synergy::readIpcToken(m_serverName);
  QVERIFY(!token.isEmpty());

  auto client = openClient(m_serverName);
  QVERIFY(client != nullptr);

  client->write(QStringLiteral("hello=not-the-server-version %1\n").arg(token).toUtf8());
  client->flush();

  QTRY_VERIFY(client->bytesAvailable() > 0);
  QCOMPARE(
      QString::fromUtf8(client->readAll()), QStringLiteral("versionMismatch=%1\n").arg(QString::fromUtf8(kVersion))
  );
}

void IpcServerTests::validHelloAcceptsCommands()
{
  TestIpcServer server(nullptr, m_serverName, QStringLiteral("test"));
  QVERIFY(server.listen());

  const auto token = synergy::readIpcToken(m_serverName);
  QVERIFY(!token.isEmpty());

  auto client = openClient(m_serverName);
  QVERIFY(client != nullptr);

  const auto version = QString::fromUtf8(kVersion);
  client->write(QStringLiteral("hello=%1 %2\n").arg(version, token).toUtf8());
  client->flush();

  QTRY_VERIFY(client->bytesAvailable() > 0);
  QCOMPARE(QString::fromUtf8(client->readAll()), QStringLiteral("hello=%1\n").arg(version));

  client->write("start\n");
  client->flush();

  QTRY_COMPARE(server.commands.size(), 1);
  QCOMPARE(server.commands.at(0), QStringLiteral("start"));
}

void IpcServerTests::listenerAddressNotificationsRequireAuthenticatedHello()
{
  TestIpcServer server(nullptr, m_serverName, QStringLiteral("test"));
  server.authenticatedSnapshot = QStringLiteral("serverBindAddress=192.0.2.10");
  QVERIFY(server.listen());

  const auto token = synergy::readIpcToken(m_serverName);
  const auto version = QString::fromUtf8(kVersion);
  QVERIFY(!token.isEmpty());

  auto mismatched = openClient(m_serverName);
  QVERIFY(mismatched);
  mismatched->write(QStringLiteral("hello=not-the-server-version %1\n").arg(token).toUtf8());
  mismatched->flush();
  QTRY_VERIFY(mismatched->canReadLine());
  QCOMPARE(mismatched->readAll(), QStringLiteral("versionMismatch=%1\n").arg(version).toUtf8());

  auto authenticated = openClient(m_serverName);
  QVERIFY(authenticated);
  authenticated->write(QStringLiteral("hello=%1 %2\n").arg(version, token).toUtf8());
  authenticated->flush();
  const auto expected = QStringLiteral("hello=%1\n%2\n").arg(version, server.authenticatedSnapshot).toUtf8();
  QTRY_COMPARE(authenticated->bytesAvailable(), expected.size());
  QCOMPARE(authenticated->readAll(), expected);

  server.broadcastToAuthenticatedClients(QStringLiteral("serverBindAddress=192.0.2.11"));
  QTRY_VERIFY(authenticated->canReadLine());
  QCOMPARE(authenticated->readAll(), QByteArray("serverBindAddress=192.0.2.11\n"));

  // The later rejection also establishes that no address was queued to the
  // mismatched client while it remained connected.
  mismatched->write("noop\n");
  mismatched->flush();
  QTRY_COMPARE(mismatched->state(), QLocalSocket::UnconnectedState);
  QCOMPARE(QString::fromUtf8(mismatched->readAll()), kErrorHelloRequired);
}

void IpcServerTests::oversizedMessageDisconnects()
{
  TestIpcServer server(nullptr, m_serverName, QStringLiteral("test"));
  QVERIFY(server.listen());

  auto client = openClient(m_serverName);
  QVERIFY(client != nullptr);

  // no newline, so this accumulates in the server's read buffer until it
  // crosses the 64 KiB message limit
  client->write(QByteArray(70 * 1024, 'a'));
  client->flush();

  QTRY_VERIFY(client->state() == QLocalSocket::UnconnectedState);
}

void IpcServerTests::lateGuiReceivesCurrentCoreState()
{
  using synergy::core::ConnectionState;
  QVERIFY(synergy::runtime::isIsolatedRuntime());
  // Use a production CoreIpcServer with a disposable endpoint.
  const auto endpoint = synergy::runtime::runtimeInstanceKey({}) + QStringLiteral("-core");
  synergy::core::ipc::CoreIpcServer server(nullptr);
  QVERIFY(server.listen());
  const auto token = synergy::readIpcToken(endpoint);
  const auto version = QString::fromUtf8(kVersion);
  QVERIFY(!token.isEmpty());

  ipcSendServerBindAddress(QStringLiteral("192.0.2.10"));
  ipcSendConnectionState(ConnectionState::Listening);
  QCoreApplication::processEvents();

  const auto receiveSnapshot = [&](const QByteArray &expected) {
    auto client = openClient(endpoint);
    QVERIFY(client);
    client->write(QStringLiteral("hello=%1 %2\n").arg(version, token).toUtf8());
    client->flush();
    QByteArray received;
    QTRY_VERIFY_WITH_TIMEOUT((received += client->readAll()).contains(expected), 1500);
    QVERIFY(received.startsWith("hello="));
  };
  receiveSnapshot("serverBindAddress=192.0.2.10\nconnectionState=Listening\n");

  ipcSendServerBindAddress({});
  ipcSendConnectionState(ConnectionState::Disconnected);
  QCoreApplication::processEvents();
  receiveSnapshot(QByteArray("hello=") + kVersion + "\nconnectionState=Disconnected\n");
}

int main(int argc, char **argv)
{
  QTemporaryDir fixture;
  if (!fixture.isValid()) {
    qCritical() << "Could not create the IPC server test fixture";
    return 1;
  }

  const auto root = QFileInfo(fixture.path()).canonicalFilePath();
  const auto instanceKey = QStringLiteral("ts-ipc-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
  QString error;
  if (!synergy::runtime::configureIsolatedRuntime({root, instanceKey}, &error)) {
    qCritical().noquote() << error;
    return 1;
  }

  QCoreApplication app(argc, argv);
  IpcServerTests tests;
  return QTest::qExec(&tests, argc, argv);
}
