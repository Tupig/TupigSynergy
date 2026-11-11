/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

#if defined(Q_OS_WIN)
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "net/FingerprintDatabase.h"
#include "net/NetworkAddress.h"
#include "net/SecureListenSocket.h"
#include "net/SecureSocket.h"
#include "net/SecureUtils.h"
#include "net/SocketMultiplexer.h"
#include "net/TCPSocketFactory.h"

#include <QElapsedTimer>
#include <QFile>
#include <QSslCertificate>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>
#include <memory>
#include <string>

namespace {

// Query the ephemeral bound port without adding a test accessor to production.
template <class Listener> class LoopbackListener : public Listener
{
public:
  using Listener::Listener;

  uint16_t port() const
  {
    sockaddr_in address{};
#if defined(Q_OS_WIN)
    int size = sizeof(address);
    const auto result = getsockname(this->socket()->m_socket, reinterpret_cast<sockaddr *>(&address), &size);
#else
    socklen_t size = sizeof(address);
    const auto result = getsockname(this->socket()->m_fd, reinterpret_cast<sockaddr *>(&address), &size);
#endif
    return result == 0 ? ntohs(address.sin_port) : 0;
  }
};

class Connection
{
public:
  Connection(SecurityLevel level, const TlsConfig &tls) : m_factory(&events, &multiplexer, tls), m_level(level)
  {
    if (level == SecurityLevel::PlainText) {
      listener =
          std::make_unique<LoopbackListener<TCPListenSocket>>(&events, &multiplexer, IArchNetwork::AddressFamily::INet);
    } else {
      listener = std::make_unique<LoopbackListener<SecureListenSocket>>(
          &events, &multiplexer, IArchNetwork::AddressFamily::INet, level, tls
      );
    }
    client = m_factory.create(IArchNetwork::AddressFamily::INet, level);
    events.addHandler(EventTypes::ListenSocketConnecting, listener->getEventTarget(), [this](const auto &) {
      server = listener->accept();
      if (!server)
        return;
      if (m_level == SecurityLevel::PlainText)
        accepted = true;
      events.addHandler(EventTypes::ClientListenerAccepted, server->getEventTarget(), [this](const auto &) {
        accepted = true;
      });
      events.addHandler(EventTypes::ClientListenerDisconnectedOnAccept, server->getEventTarget(), [this](const auto &) {
        serverRejected = true;
      });
      events.addHandler(EventTypes::StreamInputReady, server->getEventTarget(), [this](const auto &) {
        char data[64];
        received.append(data, server->read(data, sizeof(data)));
      });
    });
    events.addHandler(
        level == SecurityLevel::PlainText ? EventTypes::DataSocketConnected : EventTypes::DataSocketSecureConnected,
        client->getEventTarget(), [this](const auto &) { connected = true; }
    );
    events.addHandler(EventTypes::SocketDisconnected, client->getEventTarget(), [this](const auto &) {
      disconnected = true;
    });
    events.addHandler(EventTypes::StreamOutputError, client->getEventTarget(), [this](const auto &) {
      outputError = true;
    });
    events.addHandler(EventTypes::StreamInputReady, client->getEventTarget(), [this](const auto &) {
      char data[64];
      response.append(data, client->read(data, sizeof(data)));
    });
  }

  bool start()
  {
    NetworkAddress address("127.0.0.1", 0);
    address.resolve();
    listener->bind(address);
    const uint16_t port = m_level == SecurityLevel::PlainText
                              ? static_cast<LoopbackListener<TCPListenSocket> *>(listener.get())->port()
                              : static_cast<LoopbackListener<SecureListenSocket> *>(listener.get())->port();
    if (port == 0)
      return false;
    NetworkAddress remote("127.0.0.1", port);
    remote.resolve();
    client->connect(remote);
    return true;
  }

  bool pump(const std::function<bool()> &done, int timeoutMs = 10000)
  {
    QElapsedTimer timer;
    timer.start();
    QTimer checker;
    bool quitPosted = false;
    QObject::connect(&checker, &QTimer::timeout, [&] {
      if (!quitPosted && (done() || timer.elapsed() >= timeoutMs)) {
        quitPosted = true;
        events.addEvent(Event(EventTypes::Quit));
      }
    });
    checker.start(10);
    events.loop();
    return done();
  }

  EventQueue events;
  SocketMultiplexer multiplexer;
  TCPSocketFactory m_factory;
  SecurityLevel m_level;
  // Destruction order: sockets, factory, multiplexer, event queue.
  std::unique_ptr<IListenSocket> listener;
  std::unique_ptr<IDataSocket> client;
  std::unique_ptr<IDataSocket> server;
  bool connected = false;
  bool accepted = false;
  bool disconnected = false;
  bool serverRejected = false;
  bool outputError = false;
  std::string received;
  std::string response;
};

} // namespace

class SecureSocketTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase()
  {
    m_arch.init();
    m_log.setFilter(LogLevel::Level::Warning);
    QVERIFY(m_directory.isValid());
    m_tls.certificatePath = m_directory.filePath("identity.pem").toStdString();
    m_tls.trustedServersDbPath = m_directory.filePath("servers.txt").toStdString();
    m_tls.trustedClientsDbPath = m_directory.filePath("clients.txt").toStdString();
    synergy::generatePemSelfSignedCert(QString::fromStdString(m_tls.certificatePath), kMinimumRsaBits);
    const auto certificates = QSslCertificate::fromPath(QString::fromStdString(m_tls.certificatePath));
    QVERIFY(!certificates.isEmpty());
    m_fingerprint = certificates.first().digest(QCryptographicHash::Sha256);
  }

  void init()
  {
    QFile::remove(QString::fromStdString(m_tls.trustedServersDbPath));
    QFile::remove(QString::fromStdString(m_tls.trustedClientsDbPath));
  }

  void plaintextRoundTrip()
  {
    Connection connection(SecurityLevel::PlainText, m_tls);
    QVERIFY(dynamic_cast<TCPSocket *>(connection.client.get()) != nullptr);
    QVERIFY(dynamic_cast<SecureSocket *>(connection.client.get()) == nullptr);
    QVERIFY(connection.start());
    QVERIFY(connection.pump([&] { return connection.connected && connection.accepted; }));
    verifyRoundTrip(connection);
  }

  void trustedPeerRoundTripAndReconnect()
  {
    QVERIFY(trust(m_tls.trustedServersDbPath, m_fingerprint));
    QVERIFY(trust(m_tls.trustedClientsDbPath, m_fingerprint));
    // Each iteration tears down real sockets and their service thread before
    // a new queue/multiplexer is created. No fixed ports or sleeps are used.
    for (int attempt = 0; attempt != 2; ++attempt) {
      Connection connection(SecurityLevel::PeerAuth, m_tls);
      QVERIFY(dynamic_cast<SecureSocket *>(connection.client.get()) != nullptr);
      QVERIFY(connection.start());
      QVERIFY(connection.pump([&] { return connection.connected && connection.accepted; }));
      verifyRoundTrip(connection);
      connection.client->close();
      QVERIFY(!static_cast<SecureSocket *>(connection.client.get())->isSecureReady());
      QVERIFY(connection.pump([&] { return connection.disconnected; }));
    }
  }

  void rejectsUntrustedServer_data()
  {
    QTest::addColumn<bool>("wrongFingerprint");
    QTest::newRow("missing-trust") << false;
    QTest::newRow("wrong-trust") << true;
  }

  void rejectsUntrustedServer()
  {
    QFETCH(bool, wrongFingerprint);
    if (wrongFingerprint)
      QVERIFY(trust(m_tls.trustedServersDbPath, QByteArray(32, '\x5a')));
    QVERIFY(trust(m_tls.trustedClientsDbPath, m_fingerprint));
    Connection connection(SecurityLevel::PeerAuth, m_tls);
    QVERIFY(connection.start());
    QVERIFY(connection.pump([&] { return connection.disconnected || connection.connected; }));
    QVERIFY(connection.disconnected);
    QVERIFY(!connection.connected);
    QVERIFY(!static_cast<SecureSocket *>(connection.client.get())->isSecureReady());
    connection.client->write("must not arrive", 15);
    QVERIFY(connection.pump([&] { return connection.outputError || !connection.received.empty(); }));
    QVERIFY(connection.outputError);
    QVERIFY(connection.received.empty());
    QVERIFY(connection.response.empty());
  }

  void rejectsUntrustedClient()
  {
    QVERIFY(trust(m_tls.trustedServersDbPath, m_fingerprint));
    Connection connection(SecurityLevel::PeerAuth, m_tls);
    QVERIFY(connection.start());
    QVERIFY(connection.pump([&] { return connection.serverRejected || connection.accepted; }));
    QVERIFY(connection.serverRejected);
    QVERIFY(!connection.accepted);
    QVERIFY(!static_cast<SecureSocket *>(connection.server.get())->isSecureReady());
    QVERIFY(connection.received.empty());
    QVERIFY(connection.response.empty());
  }

private:
  bool trust(const std::string &path, const QByteArray &fingerprint)
  {
    FingerprintDatabase database;
    database.addTrusted({QCryptographicHash::Sha256, fingerprint});
    return database.write(QString::fromStdString(path));
  }

  void verifyRoundTrip(Connection &connection)
  {
    connection.client->write("legacy ping", 11);
    QVERIFY(connection.pump([&] { return connection.received.size() >= 11; }));
    QCOMPARE(connection.received, std::string("legacy ping"));
    connection.server->write("legacy pong", 11);
    QVERIFY(connection.pump([&] { return connection.response.size() >= 11; }));
    QCOMPARE(connection.response, std::string("legacy pong"));
  }

  Arch m_arch;
  Log m_log;
  QTemporaryDir m_directory;
  TlsConfig m_tls;
  QByteArray m_fingerprint;
};

QTEST_GUILESS_MAIN(SecureSocketTests)
#include "SecureSocketTests.moc"
