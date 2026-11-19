/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerTests.h"

#include "ServerTestFixture.h"
#include "base/EventQueue.h"
#include "ipc/CoreIpcServer.h"
#include "net/FingerprintDatabase.h"
#include "net/QtSocketFactory.h"
#include "net/SecureUtils.h"
#include "server/ClientListener.h"
#include "server/ClientProxy.h"
#include "server/Server.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QSslCertificate>
#include <QTimer>

#include <functional>

namespace {

class LoopbackQtSocketFactory final : public QtSocketFactory
{
public:
  using QtSocketFactory::QtSocketFactory;

  std::unique_ptr<IListenSocket>
  createListen(IArchNetwork::AddressFamily family, SecurityLevel securityLevel) const override
  {
    auto listener = QtSocketFactory::createListen(family, securityLevel);
    m_listener = static_cast<QtListenSocket *>(listener.get());
    return listener;
  }

  quint16 serverPort() const
  {
    return m_listener->serverPort();
  }

private:
  mutable QtListenSocket *m_listener = nullptr;
};

bool pumpUntil(EventQueue &events, const QString &connectionFailure, const std::function<bool()> &done)
{
  if (!connectionFailure.isEmpty() || done()) {
    return connectionFailure.isEmpty();
  }

  QElapsedTimer elapsed;
  elapsed.start();
  QTimer check;
  bool quitPosted = false;
  QObject::connect(&check, &QTimer::timeout, [&] {
    if (!quitPosted && (!connectionFailure.isEmpty() || done() || elapsed.elapsed() >= 10000)) {
      quitPosted = true;
      events.addEvent(Event(EventTypes::Quit));
    }
  });
  check.start(10);
  events.loop();
  return connectionFailure.isEmpty() && done();
}

bool trustCertificate(const QString &certificatePath, const std::string &databasePath)
{
  const auto certificates = QSslCertificate::fromPath(certificatePath);
  if (certificates.isEmpty()) {
    return false;
  }
  FingerprintDatabase database;
  database.addTrusted({QCryptographicHash::Sha256, certificates.first().digest(QCryptographicHash::Sha256)});
  return database.write(QString::fromStdString(databasePath));
}

} // namespace

void ServerTests::SwitchToScreenInfo_alloc_screen()
{
  auto actual = new Server::SwitchToScreenInfo("test");
  QCOMPARE(actual->m_screen, "test");
  delete actual;
}

void ServerTests::KeyboardBroadcastInfo_alloc_stateAndSceens()
{
  auto info = new Server::KeyboardBroadcastInfo(Server::KeyboardBroadcastInfo::State::kOn, "test");
  QCOMPARE(info->m_state, Server::KeyboardBroadcastInfo::State::kOn);
  QCOMPARE(info->m_screens, "test");
  delete info;
}

void ServerTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
#ifdef Q_OS_WIN
  m_arch.init();
#endif
  static protocol_test::App app;
  static synergy::core::ipc::CoreIpcServer ipc(nullptr);
}

void ServerTests::motionInsideScreenDoesNotSwitch()
{
  protocol_test::ServerFixture fixture;
  QCOMPARE(fixture.server->getNumClients(), 2u);
  fixture.movePrimary(798, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.secondaryStream->written.empty());
}

void ServerTests::crossingConnectedEdgeEntersSecondary_data()
{
  QTest::addColumn<int>("direction");
  QTest::addColumn<int>("x");
  QTest::addColumn<int>("y");
  QTest::addColumn<int>("expectedX");
  QTest::addColumn<int>("expectedY");
  QTest::newRow("right") << static_cast<int>(Direction::Right) << 799 << 300 << 0 << 300;
  QTest::newRow("left") << static_cast<int>(Direction::Left) << 0 << 300 << 799 << 300;
  QTest::newRow("top") << static_cast<int>(Direction::Top) << 400 << 0 << 400 << 599;
  QTest::newRow("bottom") << static_cast<int>(Direction::Bottom) << 400 << 599 << 400 << 0;
}

void ServerTests::crossingConnectedEdgeEntersSecondary()
{
  QFETCH(int, direction);
  QFETCH(int, x);
  QFETCH(int, y);
  QFETCH(int, expectedX);
  QFETCH(int, expectedY);
  protocol_test::ServerFixture fixture(true, static_cast<Direction>(direction));
  fixture.movePrimary(x, y);
  QCOMPARE(fixture.platform->leaves, 1);
  QVERIFY(!fixture.screen.isOnScreen());
  const auto &output = fixture.secondaryStream->written;
  QVERIFY(output.starts_with("CINN"));
  QCOMPARE(output.substr(4, 4), protocol_test::integers16({expectedX, expectedY}));
}

void ServerTests::crossingMissingNeighborDoesNotSwitch()
{
  protocol_test::ServerFixture fixture(false);
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.screen.isOnScreen());
}

void ServerTests::deniedLeaveDoesNotEnterSecondary()
{
  protocol_test::ServerFixture fixture;
  fixture.platform->allowLeave = false;
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.screen.isOnScreen());
  QVERIFY(fixture.secondaryStream->written.empty());
}

void ServerTests::pressedMousePreventsEdgeSwitch()
{
  protocol_test::ServerFixture fixture;
  fixture.platform->mousePressed = true;
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.secondaryStream->written.empty());
}

void ServerTests::explicitSwitchReturnsToPrimary()
{
  protocol_test::ServerFixture fixture;
  fixture.switchTo("secondary");
  QCOMPARE(fixture.platform->leaves, 1);
  QVERIFY(fixture.secondaryStream->written.starts_with("CINN"));
  fixture.secondaryStream->written.clear();
  fixture.switchTo("primary");
  QCOMPARE(fixture.platform->enters, 1);
  QVERIFY(fixture.screen.isOnScreen());
  QVERIFY(fixture.secondaryStream->written.starts_with("COUT"));
}

void ServerTests::unknownTargetDoesNotSwitch()
{
  protocol_test::ServerFixture fixture;
  fixture.switchTo("missing");
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.secondaryStream->written.empty());
}

void ServerTests::configurationWithoutPrimaryIsRejected()
{
  protocol_test::ServerFixture fixture;
  synergy::server::Config invalid(&fixture.events);
  invalid.addScreen("secondary");
  QVERIFY(!fixture.server->setConfig(invalid));
  QCOMPARE(fixture.server->getNumClients(), 2u);
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 1);
}

void ServerTests::loopbackClientSwitchesAndReconnects_data()
{
  QTest::addColumn<bool>("useTls");
  QTest::newRow("tcp") << false;
  QTest::newRow("tls-peer-auth") << true;
}

void ServerTests::loopbackClientSwitchesAndReconnects()
{
  QFETCH(bool, useTls);
  protocol_test::PrivateSettings settings;
  QTemporaryDir certificates;
  QVERIFY(certificates.isValid());
  TlsConfig serverTls;
  TlsConfig clientTls;
  if (useTls) {
    const auto serverCertificate = certificates.filePath("server.pem");
    const auto clientCertificate = certificates.filePath("client.pem");
    synergy::generatePemSelfSignedCert(serverCertificate, kMinimumRsaBits);
    synergy::generatePemSelfSignedCert(clientCertificate, kMinimumRsaBits);
    serverTls.certificatePath = serverCertificate.toStdString();
    serverTls.trustedClientsDbPath = certificates.filePath("trusted-clients").toStdString();
    clientTls.certificatePath = clientCertificate.toStdString();
    clientTls.trustedServersDbPath = certificates.filePath("trusted-servers").toStdString();
    QVERIFY(trustCertificate(clientCertificate, serverTls.trustedClientsDbPath));
    QVERIFY(trustCertificate(serverCertificate, clientTls.trustedServersDbPath));
  }

  // Only the display endpoints are simulated. Use production protocol objects
  // and real Qt sockets on a private ephemeral loopback port.
  int connected = 0;
  int disconnected = 0;
  QString connectionFailure;
  EventQueue events;
  auto *primaryPlatform = new protocol_test::PlatformScreen(&events, true);
  auto *secondaryPlatform = new protocol_test::PlatformScreen(&events, false);
  synergy::Screen primaryScreen(primaryPlatform, &events);
  synergy::Screen secondaryScreen(secondaryPlatform, &events);
  PrimaryClient primary("primary", &primaryScreen);
  synergy::server::Config config(&events);
  QVERIFY(config.addScreen("primary"));
  QVERIFY(config.addScreen("secondary"));
  QVERIFY(config.connect("primary", Direction::Right, 0.0f, 1.0f, "secondary", 0.0f, 1.0f));
  QVERIFY(config.connect("secondary", Direction::Left, 0.0f, 1.0f, "primary", 0.0f, 1.0f));
  QVERIFY(config.addOption("", kOptionClipboardSharing, 0));

  NetworkAddress listenAddress("127.0.0.1", 0);
  listenAddress.resolve();
  auto serverFactory = std::make_unique<LoopbackQtSocketFactory>(&events, serverTls);
  auto *boundFactory = serverFactory.get();
  ClientListener listener(
      listenAddress, std::move(serverFactory), &events, useTls ? SecurityLevel::PeerAuth : SecurityLevel::PlainText
  );
  const auto port = boundFactory->serverPort();
  QVERIFY(port != 0);
  Server server(config, &primary, &primaryScreen, &events, NetworkProtocol::Synergy, FileTransferSettings{});
  listener.setServer(&server);
  server.setListener(&listener);
  events.addHandler(EventTypes::ClientListenerAccepted, &listener, [&](const auto &) {
    if (auto *proxy = listener.getNextClient()) {
      server.adoptClient(proxy);
    }
  });

  ClientConfig clientConfig;
  clientConfig.useSecureNetwork = useTls;
  Client client(
      &events, "secondary", NetworkAddress("127.0.0.1", port), std::make_unique<QtSocketFactory>(&events, clientTls),
      &secondaryScreen, clientConfig
  );
  events.addHandler(EventTypes::ClientConnected, client.getEventTarget(), [&](const auto &) { ++connected; });
  events.addHandler(EventTypes::ClientDisconnected, client.getEventTarget(), [&](const auto &) { ++disconnected; });
  const auto recordFailure = [&connectionFailure](const Event &event) {
    // These Client events use DontFreeData, so their handler owns the payload.
    const std::unique_ptr<Client::FailInfo> info(static_cast<Client::FailInfo *>(event.getData()));
    if (connectionFailure.isEmpty()) {
      connectionFailure = event.getType() == EventTypes::ClientConnectionRefused
                              ? QStringLiteral("Client connection refused: %1").arg(info->m_what)
                              : QStringLiteral("Client connection failed: %1").arg(info->m_what);
    }
  };
  events.addHandler(EventTypes::ClientConnectionFailed, client.getEventTarget(), recordFailure);
  events.addHandler(EventTypes::ClientConnectionRefused, client.getEventTarget(), recordFailure);

  for (int cycle = 1; cycle <= 2; ++cycle) {
    client.connect();
    const bool handshakeCompleted =
        pumpUntil(events, connectionFailure, [&] { return connected == cycle && server.getNumClients() == 2; });
    QVERIFY2(
        handshakeCompleted,
        connectionFailure.isEmpty() ? "Timed out waiting for the client handshake" : qPrintable(connectionFailure)
    );
    QCOMPARE(secondaryPlatform->enables, cycle);

    IPrimaryScreen::MotionInfo edge{799, 300};
    QVERIFY(events.dispatchEvent(Event(
        EventTypes::PrimaryScreenMotionOnPrimary, primary.getEventTarget(), &edge, Event::EventFlags::DontFreeData
    )));
    const bool enteredClient = pumpUntil(events, connectionFailure, [&] { return secondaryPlatform->enters == cycle; });
    QVERIFY2(
        enteredClient,
        connectionFailure.isEmpty() ? "Timed out waiting for the client screen to enter" : qPrintable(connectionFailure)
    );
    QVERIFY(!primaryScreen.isOnScreen());
    QCOMPARE(secondaryPlatform->cursorX, 0);
    QCOMPARE(secondaryPlatform->cursorY, 300);

    Server::SwitchToScreenInfo back("primary");
    QVERIFY(events.dispatchEvent(
        Event(EventTypes::ServerSwitchToScreen, config.getInputFilter(), &back, Event::EventFlags::DontFreeData)
    ));
    const bool leftClient = pumpUntil(events, connectionFailure, [&] { return secondaryPlatform->leaves == cycle; });
    QVERIFY2(
        leftClient,
        connectionFailure.isEmpty() ? "Timed out waiting for the client screen to leave" : qPrintable(connectionFailure)
    );
    QVERIFY(primaryScreen.isOnScreen());

    client.disconnect(nullptr);
    const bool bothDisconnected =
        pumpUntil(events, connectionFailure, [&] { return disconnected == cycle && server.getNumClients() == 1; });
    QVERIFY2(
        bothDisconnected,
        connectionFailure.isEmpty() ? "Timed out waiting for both peers to disconnect" : qPrintable(connectionFailure)
    );
    QVERIFY(!client.isConnected());
    QCOMPARE(secondaryPlatform->disables, cycle);
  }
}

QTEST_MAIN(ServerTests)
