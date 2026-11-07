/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "ClientTests.h"
#include "ProtocolTestSupport.h"
#include "Screen.h"
#include "client/Client.h"
#include "ipc/CoreIpcServer.h"
#include "net/IDataSocket.h"
#include "net/IListenSocket.h"
#include "net/ISocketFactory.h"

namespace {

class Socket : public IDataSocket
{
public:
  Socket(IEventQueue *events, std::shared_ptr<int> destroyed) : IDataSocket(events), stream(std::move(destroyed))
  {
  }
  void connect(const NetworkAddress &) override
  {
    ++connectCalls;
  }
  void bind(const NetworkAddress &) override
  {
  }
  void close() override
  {
    stream.close();
  }
  uint32_t read(void *data, uint32_t size) override
  {
    return stream.read(data, size);
  }
  void write(const void *data, uint32_t size) override
  {
    stream.write(data, size);
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
    stream.shutdownInput();
  }
  void shutdownOutput() override
  {
  }
  bool isReady() const override
  {
    return stream.isReady();
  }
  uint32_t getSize() const override
  {
    return stream.getSize();
  }
  void *getEventTarget() const override
  {
    return const_cast<Socket *>(this);
  }
  bool isFatal() const override
  {
    return false;
  }
  ArchSocket getSocket() const override
  {
    return nullptr;
  }
  protocol_test::Stream stream;
  int connectCalls = 0;
};

class SocketFactory : public ISocketFactory
{
public:
  explicit SocketFactory(IEventQueue *events) : events(events)
  {
  }
  std::unique_ptr<IDataSocket> create(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    ++creates;
    auto result = std::make_unique<Socket>(events, destroyed);
    last = result.get();
    return result;
  }
  std::unique_ptr<IListenSocket> createListen(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    return {};
  }
  IEventQueue *events;
  std::shared_ptr<int> destroyed = std::make_shared<int>(0);
  mutable Socket *last = nullptr;
  mutable int creates = 0;
};

struct Fixture
{
  protocol_test::PrivateSettings settings;
  protocol_test::EventQueue events;
  protocol_test::PlatformScreen *platform = new protocol_test::PlatformScreen(&events, false);
  synergy::Screen screen{platform, &events};
  SocketFactory *factory = new SocketFactory(&events);
  Client client{&events, "synthetic-client", NetworkAddress(24800), std::unique_ptr<ISocketFactory>(factory),
                &screen, ClientConfig{}};

  void connect()
  {
    client.connect();
    events.dispatchEvent(Event(EventTypes::DataSocketConnected, factory->last));
  }
  void receive(const std::string &payload)
  {
    auto *socket = factory->last;
    socket->stream.push(protocol_test::packet(payload));
    events.dispatchEvent(Event(EventTypes::StreamInputReady, socket));
    events.drain();
  }
  void hello()
  {
    receive("Synergy" + protocol_test::integers16({1, 8}));
  }
  void finishHandshake()
  {
    hello();
    receive("DSOP" + protocol_test::integer32(0));
  }
};

} // namespace

void ClientTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
#ifdef Q_OS_WIN
  m_arch.init();
#endif
  static synergy::core::ipc::CoreIpcServer ipc(nullptr);
  static protocol_test::App app;
}

void ClientTests::handshakeEnablesScreenAndDisconnectCleansUp()
{
  Fixture fixture;
  QVERIFY(!fixture.client.isConnected());
  QVERIFY(!fixture.client.isConnecting());
  fixture.connect();
  QVERIFY(fixture.client.isConnecting());
  QCOMPARE(fixture.platform->enables, 0);
  fixture.finishHandshake();
  QVERIFY(fixture.client.isConnected());
  QVERIFY(!fixture.client.isConnecting());
  QCOMPARE(fixture.platform->enables, 1);
  QCOMPARE(fixture.events.count(EventTypes::ClientConnected), 1);
  QVERIFY(fixture.factory->last->stream.written.find("synthetic-client") != std::string::npos);
  fixture.receive(
      "CINN" + protocol_test::integers16({12, 34}) + protocol_test::integer32(7) + protocol_test::integers16({0})
  );
  QCOMPARE(fixture.platform->enters, 1);
  QCOMPARE(fixture.platform->cursorX, 12);
  QCOMPARE(fixture.platform->cursorY, 34);
  fixture.client.disconnect(nullptr);
  QVERIFY(!fixture.client.isConnected());
  QVERIFY(!fixture.client.isConnecting());
  QCOMPARE(fixture.platform->disables, 1);
  QCOMPARE(*fixture.factory->destroyed, 1);
  QCOMPARE(fixture.events.count(EventTypes::ClientDisconnected), 1);
  QCOMPARE(fixture.events.timerCount(), size_t(0));
}

void ClientTests::connectIsIgnoredWhileConnecting()
{
  Fixture fixture;
  fixture.client.connect();
  fixture.client.connect();
  QCOMPARE(fixture.factory->creates, 1);
  QCOMPARE(fixture.factory->last->connectCalls, 1);
  QVERIFY(fixture.client.isConnecting());
}

void ClientTests::connectionTimeoutCleansUp()
{
  Fixture fixture;
  fixture.client.connect();
  QVERIFY(fixture.events.firstTimer() != nullptr);
  fixture.events.dispatchEvent(Event(EventTypes::Timer, fixture.events.firstTimer()));
  QVERIFY(!fixture.client.isConnecting());
  QVERIFY(!fixture.client.isConnected());
  QCOMPARE(*fixture.factory->destroyed, 1);
  QCOMPARE(fixture.events.count(EventTypes::ClientConnectionFailed), 1);
  QCOMPARE(fixture.events.timerCount(), size_t(0));
}

void ClientTests::serverRefusalDoesNotActivateScreen()
{
  Fixture fixture;
  fixture.connect();
  fixture.hello();
  fixture.receive("EBSY");
  QVERIFY(!fixture.client.isConnected());
  QCOMPARE(fixture.platform->enables, 0);
  QCOMPARE(fixture.events.count(EventTypes::ClientConnectionRefused), 1);
  QCOMPARE(*fixture.factory->destroyed, 1);
}

void ClientTests::suspendAndResumeReconnects()
{
  Fixture fixture;
  fixture.connect();
  fixture.finishHandshake();
  fixture.events.dispatchEvent(Event(EventTypes::ScreenSuspend, fixture.client.getEventTarget()));
  QVERIFY(!fixture.client.isConnected());
  QCOMPARE(fixture.platform->disables, 1);
  fixture.events.dispatchEvent(Event(EventTypes::ScreenResume, fixture.client.getEventTarget()));
  QCOMPARE(fixture.factory->creates, 2);
  QVERIFY(fixture.client.isConnecting());
}

void ClientTests::truncatedHelloCannotConnect()
{
  Fixture fixture;
  fixture.connect();
  fixture.receive("Syner");
  QVERIFY(!fixture.client.isConnected());
  QCOMPARE(fixture.platform->enables, 0);
  QCOMPARE(fixture.events.count(EventTypes::ClientConnectionFailed), 1);
  QCOMPARE(*fixture.factory->destroyed, 1);
}

QTEST_MAIN(ClientTests)
