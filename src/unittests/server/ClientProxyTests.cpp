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

#include "Clipboard.h"
#include "ClipboardChunk.h"
#include "ProtocolTypes.h"
#include "StreamChunker.h"
#include "base/EventQueue.h"
#include "io/IStream.h"
#include "server/ClientProxy1_0.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace {
thread_local std::array<void *, 64> pendingClipboardObjects{};
thread_local unsigned pendingClipboardCount = 0;
void *allocateTestObject(size_t size)
{
  if (void *pointer = std::malloc(size == 0 ? 1 : size))
    return pointer;
  throw std::bad_alloc();
}
void releaseTestObject(void *pointer) noexcept
{
  if (pointer) {
    const auto found = std::find(pendingClipboardObjects.begin(), pendingClipboardObjects.end(), pointer);
    if (found != pendingClipboardObjects.end()) {
      *found = nullptr;
      --pendingClipboardCount;
    }
  }
  std::free(pointer);
}
} // namespace

// Confined to this test process; no failures are injected. Fixed-size bookkeeping
// observes exact pending EventData ownership without allocating from the hooks.
void *operator new(size_t size)
{
  return allocateTestObject(size);
}
void *operator new[](size_t size)
{
  return allocateTestObject(size);
}
void operator delete(void *pointer) noexcept
{
  releaseTestObject(pointer);
}
void operator delete(void *pointer, size_t) noexcept
{
  releaseTestObject(pointer);
}
void operator delete[](void *pointer) noexcept
{
  releaseTestObject(pointer);
}
void operator delete[](void *pointer, size_t) noexcept
{
  releaseTestObject(pointer);
}

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

// Observe deletion of the original event object without replacing its identity,
// type, bytes, flags, or any future lifetime metadata.
template <typename Queue> class ClipboardQueue : public Queue
{
public:
  unsigned liveChunkCount() const
  {
    return pendingClipboardCount;
  }
  ~ClipboardQueue() override
  {
    // A failed lifetime assertion must never dispatch a dangling baseline callback.
    for (const auto &[type, target] : m_handlers)
      Queue::removeHandler(type, target);
    m_handlers.clear();
    drain();
  }
  void addEvent(Event &&event) override
  {
    if (event.getType() == EventTypes::ClipboardSending && event.getDataObject()) {
      void *object = dynamic_cast<void *>(event.getDataObject());
      const auto empty = std::find(pendingClipboardObjects.begin(), pendingClipboardObjects.end(), nullptr);
      if (empty == pendingClipboardObjects.end())
        std::abort();
      *empty = object;
      ++pendingClipboardCount;
    }
    if (event.getType() == EventTypes::ClipboardSending && event.getTarget() == m_redirectFrom && m_redirectTo) {
      // Preserve the actual production EventData and all event metadata; only
      // misroute its envelope to exercise rejection by an unrelated live proxy.
      Event redirected(event.getType(), m_redirectTo, event.getData(), event.getFlags());
      redirected.setDataObject(event.getDataObject());
      Queue::addEvent(std::move(redirected));
    } else {
      Queue::addEvent(std::move(event));
    }
  }
  void redirectClipboard(void *from, void *to)
  {
    m_redirectFrom = from;
    m_redirectTo = to;
  }
  void addHandler(EventTypes type, void *target, const IEventQueue::EventHandler &handler) override
  {
    m_handlers.insert({type, target});
    Queue::addHandler(type, target, handler);
  }
  void removeHandler(EventTypes type, void *target) override
  {
    m_handlers.erase({type, target});
    Queue::removeHandler(type, target);
  }
  void removeHandlers(void *target) override
  {
    std::erase_if(m_handlers, [target](const auto &entry) { return entry.second == target; });
    Queue::removeHandlers(target);
  }
  EventQueueTimer *newTimer(double duration, void *target) override
  {
    auto *timer = Queue::newTimer(duration, target);
    m_timers.insert(timer);
    return timer;
  }
  EventQueueTimer *newOneShotTimer(double duration, void *target) override
  {
    auto *timer = Queue::newOneShotTimer(duration, target);
    m_timers.insert(timer);
    return timer;
  }
  void deleteTimer(EventQueueTimer *timer) override
  {
    m_timers.erase(timer);
    Queue::deleteTimer(timer);
  }
  bool hasHandler(EventTypes type, void *target) const
  {
    return m_handlers.contains({type, target});
  }
  size_t handlerCount() const
  {
    return m_handlers.size();
  }
  size_t timerCount() const
  {
    return m_timers.size();
  }
  void drain()
  {
    if constexpr (std::is_same_v<Queue, ::EventQueue>) {
      Queue::addEvent(Event(EventTypes::Quit));
      Queue::loop();
    } else {
      Queue::drain();
    }
  }

private:
  std::set<std::pair<EventTypes, void *>> m_handlers;
  std::set<EventQueueTimer *> m_timers;
  void *m_redirectFrom = nullptr;
  void *m_redirectTo = nullptr;
};

ClientProxy1_6 *
constructClipboardProxy(int minor, void *storage, FakeStream *stream, Server *server, IEventQueue *events)
{
  if (minor == 6)
    return new (storage) ClientProxy1_6("synthetic-clipboard", stream, server, events);
  if (minor == 7)
    return new (storage) ClientProxy1_7("synthetic-clipboard", stream, server, events);
  return new (storage) ClientProxy1_8("synthetic-clipboard", stream, server, events);
}

struct RetireClipboardProxy
{
  int minor;
  void operator()(ClientProxy1_6 *proxy) const
  {
    if (minor == 6)
      proxy->~ClientProxy1_6();
    else if (minor == 7)
      static_cast<ClientProxy1_7 *>(proxy)->~ClientProxy1_7();
    else
      static_cast<ClientProxy1_8 *>(proxy)->~ClientProxy1_8();
  }
};

std::string clipboardWire(ClipboardID id, uint32_t sequence, uint8_t mark, const std::string &data)
{
  return std::string("DCLP") + static_cast<char>(id) + protocol_test::integer32(sequence) + static_cast<char>(mark) +
         protocol_test::integer32(static_cast<uint32_t>(data.size())) + data;
}

// These independent goldens freeze the existing format law before product edits.
// They do not call Clipboard::marshall, ProtocolUtil, or StreamChunker::chunkSize.
std::string marshalledText(const std::string &payload)
{
  return protocol_test::integer32(1) + protocol_test::integer32(0) +
         protocol_test::integer32(static_cast<uint32_t>(payload.size())) + payload;
}

std::string clipboardTransferWire(const std::string &payload)
{
  const auto marshalled = marshalledText(payload);
  auto expected = clipboardWire(0, 0, 1, std::to_string(marshalled.size()));
  constexpr size_t wireChunkSize = 65536;
  for (size_t offset = 0; offset < marshalled.size(); offset += wireChunkSize)
    expected += clipboardWire(0, 0, 2, marshalled.substr(offset, wireChunkSize));
  return expected + clipboardWire(0, 0, 3, "");
}

void fillMemoryClipboard(Clipboard &clipboard, const std::string &payload)
{
  clipboard.open(0);
  clipboard.empty();
  clipboard.add(IClipboard::Format::Text, payload);
  clipboard.close();
}

template <typename Queue>
void checkClipboardLifetime(int minor, bool reuseAddress, bool liveOnly, const std::string &payload)
{
  protocol_test::ServerFixture fixture(false);
  ClipboardQueue<Queue> events;
  int unrelatedTarget = 0;
  events.addHandler(EventTypes::ClipboardSending, &unrelatedTarget, [](const auto &) {});
  const auto initialHandlers = events.handlerCount();
  const auto initialTimers = events.timerCount();
  const auto destroyed = std::make_shared<int>(0);
  Clipboard clipboard;
  fillMemoryClipboard(clipboard, payload);
  const auto expected = clipboardTransferWire(payload);
  const auto expectedChunks = 2u + static_cast<unsigned>((12 + payload.size() + 65535) / 65536);
  alignas(ClientProxy1_8) unsigned char storage[sizeof(ClientProxy1_8)];
  for (unsigned cycle = 0; cycle < (liveOnly ? 1u : 100u); ++cycle) {
    auto *stream = new FakeStream(destroyed);
    std::unique_ptr<ClientProxy1_6, RetireClipboardProxy> proxy(
        constructClipboardProxy(minor, storage, stream, fixture.server.get(), &events), {minor}
    );
    void *target = proxy.get();
    QVERIFY(events.hasHandler(EventTypes::ClipboardSending, target));
    stream->written.clear();
    proxy->setClipboard(0, &clipboard);
    QCOMPARE(events.liveChunkCount(), expectedChunks);
    if (liveOnly) {
      events.drain();
      QCOMPARE(events.liveChunkCount(), 0u);
      QCOMPARE(stream->written, expected);
      proxy->setClipboard(0, &clipboard);
      QCOMPARE(events.liveChunkCount(), 0u);
      events.drain();
      QCOMPARE(stream->written, expected);
      proxy->setClipboardDirty(0, true);
      proxy->setClipboard(0, &clipboard);
      events.drain();
      QCOMPARE(events.liveChunkCount(), 0u);
      QCOMPARE(stream->written, expected + expected);
      proxy.reset();
      QCOMPARE(*destroyed, 1);
    } else if (reuseAddress) {
      proxy.reset();
      QCOMPARE(*destroyed, static_cast<int>(2 * cycle + 1));
      QVERIFY2(!events.hasHandler(EventTypes::ClipboardSending, target), "retired proxy retains clipboard callback");
      auto *replacementStream = new FakeStream(destroyed);
      std::unique_ptr<ClientProxy1_6, RetireClipboardProxy> replacement(
          constructClipboardProxy(minor, storage, replacementStream, fixture.server.get(), &events), {minor}
      );
      QCOMPARE(static_cast<void *>(replacement.get()), target);
      replacementStream->written.clear();
      events.drain();
      QCOMPARE(events.liveChunkCount(), 0u);
      QCOMPARE(replacementStream->written, std::string());
      const std::string freshPayload("fresh\0clipboard", 15);
      Clipboard fresh;
      fillMemoryClipboard(fresh, freshPayload);
      replacement->setClipboard(0, &fresh);
      events.drain();
      QCOMPARE(events.liveChunkCount(), 0u);
      QCOMPARE(replacementStream->written, clipboardTransferWire(freshPayload));
      replacement.reset();
      QCOMPARE(*destroyed, static_cast<int>(2 * (cycle + 1)));
    } else {
      proxy.reset();
      QCOMPARE(*destroyed, static_cast<int>(cycle + 1));
      QVERIFY2(!events.hasHandler(EventTypes::ClipboardSending, target), "retired proxy retains clipboard callback");
      events.drain();
      QCOMPARE(events.liveChunkCount(), 0u);
    }
    QVERIFY(events.hasHandler(EventTypes::ClipboardSending, &unrelatedTarget));
    QCOMPARE(events.handlerCount(), initialHandlers);
    QCOMPARE(events.timerCount(), initialTimers);
  }
}

template <typename Queue> void checkForeignClipboardIdentity(int minor)
{
  protocol_test::ServerFixture fixture(false);
  ClipboardQueue<Queue> events;
  const auto initialHandlers = events.handlerCount();
  const auto initialTimers = events.timerCount();
  const auto destroyed = std::make_shared<int>(0);
  auto *firstStream = new FakeStream(destroyed);
  auto *secondStream = new FakeStream(destroyed);
  alignas(ClientProxy1_8) unsigned char firstStorage[sizeof(ClientProxy1_8)];
  alignas(ClientProxy1_8) unsigned char secondStorage[sizeof(ClientProxy1_8)];
  std::unique_ptr<ClientProxy1_6, RetireClipboardProxy> first(
      constructClipboardProxy(minor, firstStorage, firstStream, fixture.server.get(), &events), {minor}
  );
  std::unique_ptr<ClientProxy1_6, RetireClipboardProxy> second(
      constructClipboardProxy(minor, secondStorage, secondStream, fixture.server.get(), &events), {minor}
  );
  firstStream->written.clear();
  secondStream->written.clear();
  const std::string payload("synthetic\0clipboard", 19);
  Clipboard clipboard;
  fillMemoryClipboard(clipboard, payload);
  events.redirectClipboard(first.get(), second.get());
  first->setClipboard(0, &clipboard);
  QCOMPARE(events.liveChunkCount(), 3u);
  events.drain();
  QCOMPARE(events.liveChunkCount(), 0u);
  QCOMPARE(firstStream->written, std::string());
  QCOMPARE(secondStream->written, std::string());
  events.redirectClipboard(nullptr, nullptr);
  StreamChunker::sendClipboard(payload, payload.size(), 0, 42, &events, second.get());
  events.drain();
  QCOMPARE(events.liveChunkCount(), 0u);
  QCOMPARE(secondStream->written, std::string());
  first->setClipboardDirty(0, true);
  first->setClipboard(0, &clipboard);
  second->setClipboard(0, &clipboard);
  events.drain();
  QCOMPARE(events.liveChunkCount(), 0u);
  QCOMPARE(firstStream->written, clipboardTransferWire(payload));
  QCOMPARE(secondStream->written, clipboardTransferWire(payload));
  first.reset();
  second.reset();
  QCOMPARE(*destroyed, 2);
  QCOMPARE(events.handlerCount(), initialHandlers);
  QCOMPARE(events.timerCount(), initialTimers);
}

template <typename Queue> void checkChunkIdentityOwnership()
{
  ClipboardQueue<Queue> events;
  FakeStream stream;
  int target = 0;
  events.addHandler(EventTypes::ClipboardSending, &target, [&stream](const auto &event) {
    ClipboardChunk::send(&stream, event.getDataObject());
  });
  const std::string payload("synthetic\0clipboard", 19);
  const auto expected = clipboardWire(0, 42, 1, "19") + clipboardWire(0, 42, 2, payload) + clipboardWire(0, 42, 3, "");
  // The original untagged producer remains valid for unrestricted consumers.
  StreamChunker::sendClipboard(payload, payload.size(), 0, 42, &events, &target);
  events.drain();
  QCOMPARE(events.liveChunkCount(), 0u);
  QCOMPARE(stream.written, expected);
  stream.written.clear();
  auto lifetime = std::make_shared<const ClipboardChunk::Lifetime>();
  std::weak_ptr<const ClipboardChunk::Lifetime> observed = lifetime;
  StreamChunker::sendClipboard(payload, payload.size(), 0, 42, &events, &target, lifetime);
  QCOMPARE(events.liveChunkCount(), 3u);
  lifetime.reset();
  QVERIFY2(!observed.expired(), "pending original EventData must retain its producing identity");
  events.drain();
  QCOMPARE(events.liveChunkCount(), 0u);
  QVERIFY2(observed.expired(), "identity must release after final pending EventData destruction");
  QCOMPARE(stream.written, expected);
  events.removeHandler(EventTypes::ClipboardSending, &target);
  QCOMPARE(events.handlerCount(), size_t(0));
  QCOMPARE(events.timerCount(), size_t(0));
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
  stream->push(
      "Synergy" + protocol_test::integers16({1, minor}) + protocol_test::integer32(static_cast<uint32_t>(name.size())) +
      name
  );
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
  const auto *negotiatedProxy = proxy.get();
  QVERIFY(typeid(*negotiatedProxy) == *versions[minor]);
  QCOMPARE(fixture.events.hasHandler(EventTypes::ClipboardSending, proxy.get()), minor >= 6);
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

void ClientProxyTests::clipboardLifetime_data()
{
  QTest::addColumn<int>("minor");
  QTest::addColumn<bool>("productionQueue");
  for (int minor : {6, 7, 8}) {
    QTest::newRow(qPrintable(QString("1.%1-controlled").arg(minor))) << minor << false;
    QTest::newRow(qPrintable(QString("1.%1-production").arg(minor))) << minor << true;
  }
}

void ClientProxyTests::clipboardLifetime()
{
  QFETCH(int, minor);
  QFETCH(bool, productionQueue);
  if (productionQueue)
    checkClipboardLifetime<::EventQueue>(minor, false, false, std::string("synthetic\0clipboard", 19));
  else
    checkClipboardLifetime<protocol_test::EventQueue>(minor, false, false, std::string("synthetic\0clipboard", 19));
}

void ClientProxyTests::clipboardPendingAddressReuse_data()
{
  clipboardLifetime_data();
}
void ClientProxyTests::clipboardPendingAddressReuse()
{
  QFETCH(int, minor);
  QFETCH(bool, productionQueue);
  if (productionQueue)
    checkClipboardLifetime<::EventQueue>(minor, true, false, std::string("synthetic\0clipboard", 19));
  else
    checkClipboardLifetime<protocol_test::EventQueue>(minor, true, false, std::string("synthetic\0clipboard", 19));
}

void ClientProxyTests::liveClipboardBytes_data()
{
  QTest::addColumn<int>("minor");
  QTest::addColumn<bool>("productionQueue");
  QTest::addColumn<QByteArray>("payload");
  for (int minor : {6, 7, 8}) {
    for (bool production : {false, true}) {
      for (const auto &payload : {QByteArray(), QByteArray("synthetic\0clipboard", 19), QByteArray(65553, '\x9d')}) {
        const auto name =
            QString("1.%1-%2-%3").arg(minor).arg(production ? "production" : "controlled").arg(payload.size());
        QTest::newRow(qPrintable(name)) << minor << production << payload;
      }
    }
  }
}
void ClientProxyTests::liveClipboardBytes()
{
  QFETCH(int, minor);
  QFETCH(bool, productionQueue);
  QFETCH(QByteArray, payload);
  if (productionQueue)
    checkClipboardLifetime<::EventQueue>(minor, false, true, payload.toStdString());
  else
    checkClipboardLifetime<protocol_test::EventQueue>(minor, false, true, payload.toStdString());
}

void ClientProxyTests::foreignClipboardIdentity_data()
{
  clipboardLifetime_data();
}

void ClientProxyTests::foreignClipboardIdentity()
{
  QFETCH(int, minor);
  QFETCH(bool, productionQueue);
  if (productionQueue)
    checkForeignClipboardIdentity<::EventQueue>(minor);
  else
    checkForeignClipboardIdentity<protocol_test::EventQueue>(minor);
}

void ClientProxyTests::chunkIdentityOwnership_data()
{
  QTest::addColumn<bool>("productionQueue");
  QTest::newRow("controlled") << false;
  QTest::newRow("production") << true;
}

void ClientProxyTests::chunkIdentityOwnership()
{
  QFETCH(bool, productionQueue);
  if (productionQueue)
    checkChunkIdentityOwnership<::EventQueue>();
  else
    checkChunkIdentityOwnership<protocol_test::EventQueue>();
}

QTEST_MAIN(ClientProxyTests)
