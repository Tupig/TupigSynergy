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
  Socket(IEventQueue *events, std::shared_ptr<int> destroyed, std::shared_ptr<std::string> output)
      : IDataSocket(events),
        stream(std::move(destroyed)),
        output(std::move(output))
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
    output->append(static_cast<const char *>(data), size);
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
  std::shared_ptr<std::string> output;
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
    auto result = std::make_unique<Socket>(events, destroyed, output);
    last = result.get();
    return result;
  }
  std::unique_ptr<IListenSocket> createListen(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    return {};
  }
  IEventQueue *events;
  std::shared_ptr<int> destroyed = std::make_shared<int>(0);
  std::shared_ptr<std::string> output = std::make_shared<std::string>();
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
  const auto handlers = fixture.events.handlerCount();
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
  QCOMPARE(fixture.events.handlerCount(), handlers);
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

void ClientTests::malformedBodyDisconnectsSafely_data()
{
  QTest::addColumn<QByteArray>("payload");
  QTest::addColumn<bool>("handshake");
  const auto addTruncations = [](const std::string &frame, bool handshake) {
    for (size_t size = 4; size < frame.size(); ++size) {
      const auto name =
          std::string(handshake ? "handshake-" : "connected-") + frame.substr(0, 4) + "-" + std::to_string(size - 4);
      QTest::newRow(name.c_str()) << QByteArray(frame.data(), static_cast<qsizetype>(size)) << handshake;
    }
  };
  const auto key = protocol_test::integers16({65, 0, 12});
  const auto text = protocol_test::integer32(2) + "en";
  for (const auto &frame :
       {"DKDN" + key, "DKDL" + key + text, "DKRP" + protocol_test::integers16({65, 0, 1, 12}) + text, "DKUP" + key,
        std::string("DMDN") + char(1), std::string("DMUP") + char(1), "DMMV" + protocol_test::integers16({5, 7}),
        "DMRM" + protocol_test::integers16({5, 7}), "DMWM" + protocol_test::integers16({5, 7}),
        "CINN" + protocol_test::integers16({5, 7}) + protocol_test::integer32(1) + protocol_test::integers16({0}),
        std::string("CCLP") + char(0) + protocol_test::integer32(1), std::string("CSEC") + char(1),
        "DSOP" + protocol_test::integer32(2) + protocol_test::integer32(0) + protocol_test::integer32(0), "SECN" + text,
        std::string("DCLP") + char(0) + protocol_test::integer32(1) + char(0) + text}) {
    addTruncations(frame, false);
  }
  addTruncations(
      "DSOP" + protocol_test::integer32(2) + protocol_test::integer32(0) + protocol_test::integer32(0), true
  );
  addTruncations("LSYN" + text, true);
  addTruncations("EICV" + protocol_test::integers16({1, 8}), true);
  const auto invalidClipboard =
      std::string("DCLP") + char(127) + protocol_test::integer32(1) + char(127) + protocol_test::integer32(0);
  QTest::newRow("connected-invalid-clipboard")
      << QByteArray(invalidClipboard.data(), static_cast<qsizetype>(invalidClipboard.size())) << false;
}

void ClientTests::malformedBodyDisconnectsSafely()
{
  QFETCH(QByteArray, payload);
  QFETCH(bool, handshake);
  Fixture fixture;
  const auto handlers = fixture.events.handlerCount();
  fixture.connect();
  if (handshake) {
    fixture.hello();
  } else {
    fixture.finishHandshake();
  }
  const auto output = *fixture.factory->output;
  const int enables = fixture.platform->enables;
  const int options = fixture.platform->optionsCalls;
  const int clipboards = fixture.platform->clipboardSets;
  fixture.receive(std::string(payload.constData(), static_cast<size_t>(payload.size())));
  QVERIFY(!fixture.client.isConnected());
  QVERIFY(!fixture.client.isConnecting());
  QCOMPARE(fixture.platform->enables, enables);
  QCOMPARE(fixture.platform->optionsCalls, options);
  QCOMPARE(fixture.platform->enters, 0);
  QCOMPARE(fixture.platform->inputCalls(), 0);
  QCOMPARE(fixture.platform->clipboardSets, clipboards);
  QCOMPARE(fixture.platform->screensavers, 0);
  QCOMPARE(fixture.events.count(EventTypes::ClientConnected), handshake ? 0 : 1);
  QCOMPARE(fixture.events.count(EventTypes::ClientConnectionFailed), 1);
  QCOMPARE(*fixture.factory->destroyed, 1);
  QCOMPARE(fixture.events.timerCount(), size_t(0));
  QCOMPARE(*fixture.factory->output, output);
  QCOMPARE(fixture.events.handlerCount(), handlers);
  fixture.connect();
  fixture.finishHandshake();
  QVERIFY(fixture.client.isConnected());
  QCOMPARE(fixture.events.count(EventTypes::ClientConnected), handshake ? 1 : 2);
  QCOMPARE(*fixture.factory->destroyed, 1);
  fixture.client.disconnect(nullptr);
  QCOMPARE(*fixture.factory->destroyed, 2);
  QCOMPARE(fixture.events.handlerCount(), handlers);
  QCOMPARE(fixture.events.timerCount(), size_t(0));
}

QTEST_MAIN(ClientTests)
