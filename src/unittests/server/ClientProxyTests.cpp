/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientProxyTests.h"

#include "../synergy/MockEventQueue.h"
#include "ServerTestFixture.h"
#include "ipc/CoreIpcServer.h"
#include "server/ClientProxy1_1.h"
#include "server/ClientProxy1_2.h"
#include "server/ClientProxy1_3.h"
#include "server/ClientProxy1_4.h"
#include "server/ClientProxy1_5.h"
#include "server/ClientProxy1_6.h"
#include "server/ClientProxy1_7.h"
#include "server/ClientProxy1_8.h"
#include "server/ClientProxyUnknown.h"

#include "ProtocolTypes.h"
#include "io/IStream.h"
#include "server/ClientProxy1_0.h"

#include <cstring>
#include <string>
#include <typeinfo>

namespace {

using FakeStream = protocol_test::Stream;

//! Exposes the protected message parsers so tests can drive them directly.
class TestableClientProxy1_0 : public ClientProxy1_0
{
public:
  using ClientProxy1_0::ClientProxy1_0;
  using ClientProxy1_0::parseHandshakeMessage;
  using ClientProxy1_0::parseMessage;
};

void copyCode(const char *code, uint8_t out[4])
{
  std::memcpy(out, code, 4);
}

} // namespace

void ClientProxyTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
#ifdef Q_OS_WIN
  m_arch.init();
#endif
  static synergy::core::ipc::CoreIpcServer ipc(nullptr);
  static protocol_test::App app;
}

void ClientProxyTests::handshakeNoopReturnsTrue()
{
  MockEventQueue events;
  // the proxy adopts the stream and deletes it on destruction
  auto *stream = new FakeStream();
  TestableClientProxy1_0 proxy("test-client", stream, &events);

  uint8_t code[4];
  copyCode(kMsgCNoop, code);
  QVERIFY(proxy.parseHandshakeMessage(code));
}

void ClientProxyTests::handshakeUnknownCodeReturnsFalse()
{
  MockEventQueue events;
  auto *stream = new FakeStream();
  TestableClientProxy1_0 proxy("test-client", stream, &events);

  const uint8_t code[4] = {'Z', 'Z', 'Z', 'Z'};
  QVERIFY(!proxy.parseHandshakeMessage(code));
}

void ClientProxyTests::handshakeInfoCompletesHandshake()
{
  MockEventQueue events;
  auto *stream = new FakeStream();
  TestableClientProxy1_0 proxy("test-client", stream, &events);

  // the payload parsed by recvInfo (format without the leading code)
  stream->push(protocol_test::integers16({10, 20, 800, 600, 0, 5, 6}));

  uint8_t code[4];
  copyCode(kMsgDInfo, code);
  QVERIFY(proxy.parseHandshakeMessage(code));
}

void ClientProxyTests::parseMessageNoopReturnsTrue()
{
  MockEventQueue events;
  auto *stream = new FakeStream();
  TestableClientProxy1_0 proxy("test-client", stream, &events);

  uint8_t code[4];
  copyCode(kMsgCNoop, code);
  QVERIFY(proxy.parseMessage(code));
}

void ClientProxyTests::parseMessageUnknownReturnsFalse()
{
  MockEventQueue events;
  auto *stream = new FakeStream();
  TestableClientProxy1_0 proxy("test-client", stream, &events);

  const uint8_t code[4] = {'Z', 'Z', 'Z', 'Z'};
  QVERIFY(!proxy.parseMessage(code));
}

void ClientProxyTests::parseMessageInfoUpdatesShape()
{
  MockEventQueue events;
  auto *stream = new FakeStream();
  TestableClientProxy1_0 proxy("test-client", stream, &events);

  stream->push(protocol_test::integers16({10, 20, 800, 600, 0, 5, 6}));

  uint8_t code[4];
  copyCode(kMsgDInfo, code);
  QVERIFY(proxy.parseMessage(code));

  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
  proxy.getShape(x, y, w, h);
  QCOMPARE(x, 10);
  QCOMPARE(y, 20);
  QCOMPARE(w, 800);
  QCOMPARE(h, 600);
}

void ClientProxyTests::parseMessageTruncatedInfoReturnsFalse()
{
  MockEventQueue events;
  auto *stream = new FakeStream();
  TestableClientProxy1_0 proxy("test-client", stream, &events);

  // the stream does not contain the payload the parser expects
  uint8_t code[4];
  copyCode(kMsgDInfo, code);
  QVERIFY(!proxy.parseMessage(code));
}

void ClientProxyTests::negotiatesSupportedVersions_data()
{
  QTest::addColumn<int>("minor");
  for (int minor = 0; minor <= 8; ++minor)
    QTest::newRow(qPrintable(QString("1.%1").arg(minor))) << minor;
}

void ClientProxyTests::negotiatesSupportedVersions()
{
  QFETCH(int, minor);
  protocol_test::ServerFixture fixture(false);
  const auto destroyed = std::make_shared<int>(0);
  auto *stream = new FakeStream(destroyed);
  ClientProxyUnknown unknown(stream, 5.0, fixture.server.get(), &fixture.events);
  QCOMPARE(
      stream->written,
      std::string("Synergy") + protocol_test::integers16({kProtocolMajorVersion, kProtocolMinorVersion})
  );
  stream->written.clear();
  const std::string name = "synthetic-client";
  stream->push("Synergy" + protocol_test::integers16({1, minor}) + protocol_test::integer32(name.size()) + name);
  QVERIFY(fixture.events.dispatchEvent(Event(EventTypes::StreamInputReady, stream)));
  QVERIFY(unknown.getStream() == nullptr);
  QVERIFY(unknown.orphanClientProxy() == nullptr);
  QVERIFY(stream->written.starts_with("QINF"));
  QCOMPARE(fixture.events.count(EventTypes::ClientProxyUnknownSuccess), 0);

  stream->push("DINF" + protocol_test::integers16({0, 0, 800, 600, 0, 10, 20}));
  QVERIFY(fixture.events.dispatchEvent(Event(EventTypes::StreamInputReady, stream)));
  fixture.events.drain();
  QCOMPARE(fixture.events.count(EventTypes::ClientProxyUnknownSuccess), 1);
  QCOMPARE(fixture.events.count(EventTypes::ClientProxyUnknownFailure), 0);
  std::unique_ptr<ClientProxy> proxy(unknown.orphanClientProxy());
  QVERIFY(proxy != nullptr);
  QCOMPARE(proxy->getName(), name);
  const std::type_info *versions[] = {&typeid(ClientProxy1_0), &typeid(ClientProxy1_1), &typeid(ClientProxy1_2),
                                      &typeid(ClientProxy1_3), &typeid(ClientProxy1_4), &typeid(ClientProxy1_5),
                                      &typeid(ClientProxy1_6), &typeid(ClientProxy1_7), &typeid(ClientProxy1_8)};
  QVERIFY(typeid(*proxy) == *versions[minor]);
  QVERIFY(!fixture.events.hasHandler(EventTypes::Timer, &unknown));
  proxy.reset();
  QCOMPARE(*destroyed, 1);
  QVERIFY(!fixture.events.hasHandler(EventTypes::StreamInputReady, stream));
}

void ClientProxyTests::rejectsUnsupportedVersions_data()
{
  QTest::addColumn<int>("major");
  QTest::addColumn<int>("minor");
  QTest::newRow("zero-major") << 0 << 0;
  QTest::newRow("future-major") << 2 << 0;
  QTest::newRow("future-minor") << 1 << 9;
  QTest::newRow("negative-major") << -1 << 0;
  QTest::newRow("negative-minor") << 1 << -1;
}

void ClientProxyTests::rejectsUnsupportedVersions()
{
  QFETCH(int, major);
  QFETCH(int, minor);
  protocol_test::ServerFixture fixture(false);
  auto *stream = new FakeStream;
  ClientProxyUnknown unknown(stream, 5.0, fixture.server.get(), &fixture.events);
  stream->written.clear();
  stream->push("Synergy" + protocol_test::integers16({major, minor}) + protocol_test::integer32(4) + "test");
  fixture.events.dispatchEvent(Event(EventTypes::StreamInputReady, stream));
  QCOMPARE(
      stream->written, std::string("EICV") + protocol_test::integers16({kProtocolMajorVersion, kProtocolMinorVersion})
  );
  QCOMPARE(fixture.events.count(EventTypes::ClientProxyUnknownFailure), 1);
  QVERIFY(unknown.orphanClientProxy() == nullptr);
  QVERIFY(!fixture.events.hasHandler(EventTypes::StreamInputReady, stream));
  QVERIFY(!fixture.events.hasHandler(EventTypes::Timer, &unknown));
}

void ClientProxyTests::rejectsMalformedHello_data()
{
  QTest::addColumn<QByteArray>("hello");
  QTest::newRow("empty") << QByteArray();
  QTest::newRow("truncated-version") << QByteArray("Synergy\0", 8);
  QTest::newRow("truncated-name") << QByteArray::fromStdString(
      "Synergy" + protocol_test::integers16({1, 0}) + protocol_test::integer32(10) + "short"
  );
  QTest::newRow("oversized") << QByteArray(kMaxHelloLength + 1, 'x');
}

void ClientProxyTests::rejectsMalformedHello()
{
  QFETCH(QByteArray, hello);
  protocol_test::ServerFixture fixture(false);
  auto *stream = new FakeStream;
  ClientProxyUnknown unknown(stream, 5.0, fixture.server.get(), &fixture.events);
  stream->written.clear();
  if (!hello.isEmpty())
    stream->push(hello.toStdString());
  fixture.events.dispatchEvent(Event(EventTypes::StreamInputReady, stream));
  QCOMPARE(stream->written, std::string("EBAD"));
  QCOMPARE(fixture.events.count(EventTypes::ClientProxyUnknownFailure), 1);
  QVERIFY(unknown.orphanClientProxy() == nullptr);
}

void ClientProxyTests::failedHandshakeCleansUp_data()
{
  QTest::addColumn<int>("eventType");
  QTest::addColumn<bool>("afterHello");
  QTest::newRow("hello-timeout") << static_cast<int>(EventTypes::Timer) << false;
  QTest::newRow("info-timeout") << static_cast<int>(EventTypes::Timer) << true;
  QTest::newRow("output-error") << static_cast<int>(EventTypes::StreamOutputError) << false;
  QTest::newRow("input-shutdown") << static_cast<int>(EventTypes::StreamInputShutdown) << false;
}

void ClientProxyTests::failedHandshakeCleansUp()
{
  QFETCH(int, eventType);
  QFETCH(bool, afterHello);
  protocol_test::ServerFixture fixture(false);
  const auto initialHandlers = fixture.events.handlerCount();
  const auto destroyed = std::make_shared<int>(0);
  auto *stream = new FakeStream(destroyed);
  {
    ClientProxyUnknown unknown(stream, 5.0, fixture.server.get(), &fixture.events);
    if (afterHello) {
      stream->push("Synergy" + protocol_test::integers16({1, 0}) + protocol_test::integer32(4) + "test");
      fixture.events.dispatchEvent(Event(EventTypes::StreamInputReady, stream));
    }
    const auto type = static_cast<EventTypes>(eventType);
    void *target = type == EventTypes::Timer ? static_cast<void *>(&unknown) : static_cast<void *>(stream);
    QVERIFY(fixture.events.dispatchEvent(Event(type, target)));
    QCOMPARE(fixture.events.count(EventTypes::ClientProxyUnknownFailure), 1);
    QVERIFY(unknown.orphanClientProxy() == nullptr);
    QVERIFY(!fixture.events.hasHandler(EventTypes::Timer, &unknown));
    QVERIFY(!fixture.events.hasHandler(EventTypes::StreamInputReady, stream));
  }
  QCOMPARE(*destroyed, 1);
  QCOMPARE(fixture.events.handlerCount(), initialHandlers);
}

QTEST_MAIN(ClientProxyTests)
