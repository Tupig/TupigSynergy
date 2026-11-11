/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "EventPayloadOwnershipTests.h"

#include "ServerTestFixture.h"
#include "ipc/CoreIpcServer.h"
#include "server/InputFilter.h"

#include <array>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <set>
#include <vector>

namespace {
struct Observation
{
  const void *pointer = nullptr;
  unsigned deletes = 0;
};
thread_local std::array<Observation, 128> observations{};
thread_local size_t observationCount = 0;

size_t observe(const void *pointer)
{
  if (!pointer || observationCount == observations.size())
    std::abort();
  const size_t index = observationCount++;
  observations[index] = {pointer, 0};
  return index;
}

void *allocate(size_t size)
{
  if (void *pointer = std::malloc(size ? size : 1)) {
    // An old free may bypass delete; unrelated address reuse cannot satisfy it.
    for (auto &entry : observations)
      if (entry.pointer == pointer)
        entry.pointer = nullptr;
    return pointer;
  }
  throw std::bad_alloc();
}

void release(void *pointer) noexcept
{
  if (pointer) {
    for (auto &entry : observations) {
      if (entry.pointer == pointer) {
        ++entry.deletes;
        entry.pointer = nullptr;
      }
    }
  }
  std::free(pointer);
}
} // namespace

// Dedicated process only; production allocation and deletion APIs stay intact.
void *operator new(size_t size)
{
  return allocate(size);
}
void *operator new[](size_t size)
{
  return allocate(size);
}
void operator delete(void *pointer) noexcept
{
  release(pointer);
}
void operator delete(void *pointer, size_t) noexcept
{
  release(pointer);
}
void operator delete[](void *pointer) noexcept
{
  release(pointer);
}
void operator delete[](void *pointer, size_t) noexcept
{
  release(pointer);
}

namespace {
using Flags = Event::EventFlags;

struct PayloadObservation
{
  PayloadObservation(const void *pointer, const std::string *text)
      : object(observe(pointer)),
        string(text && text->size() > 64 ? observe(text->data()) : observations.size())
  {
  }
  ~PayloadObservation()
  {
    // All tracked events are drained before this watcher dies. On the old free
    // path only the saved long-string allocation remains; never touch the object.
    if (string < observations.size() && observations[string].pointer)
      ::operator delete(const_cast<void *>(observations[string].pointer));
  }
  unsigned objectDeletes() const
  {
    return observations[object].deletes;
  }
  unsigned stringDeletes() const
  {
    return string < observations.size() ? observations[string].deletes : 0;
  }
  size_t object;
  size_t string;
};

struct Record
{
  EventTypes type;
  void *target;
  void *raw;
  EventData *object;
  Event::Flags flags;
  int number = 0;
  std::string text;
  std::unique_ptr<PayloadObservation> lifetime;
};

class RecordingQueue : public protocol_test::EventQueue
{
public:
  ~RecordingQueue() override
  {
    onConnection = {};
    drain(); // Dispose before the observation records, including early test exits.
  }
  void addEvent(Event &&event) override
  {
    if ((event.getFlags() & Flags::DontFreeData) == 0) {
      const auto type = event.getType();
      const std::string *text = nullptr;
      int number = 0;
      void *pointer = event.getData() ? event.getData() : event.getDataObject();
      bool selected = true;
      switch (type) {
      case EventTypes::ServerConnected:
        if (pointer)
          text = &static_cast<Server::ScreenConnectedInfo *>(pointer)->m_screen;
        break;
      case EventTypes::ServerLockCursorToScreen:
        if (pointer)
          number = static_cast<Server::LockCursorToScreenInfo *>(pointer)->m_state;
        break;
      case EventTypes::ServerSwitchToScreen:
        if (pointer)
          text = &static_cast<Server::SwitchToScreenInfo *>(pointer)->m_screen;
        break;
      case EventTypes::ServerSwitchInDirection:
        if (pointer)
          number = static_cast<int>(static_cast<Server::SwitchInDirectionInfo *>(pointer)->m_direction);
        break;
      case EventTypes::ServerKeyboardBroadcast: {
        if (pointer) {
          const auto *info = static_cast<Server::KeyboardBroadcastInfo *>(pointer);
          number = info->m_state;
          text = &info->m_screens;
        }
        break;
      }
      case EventTypes::ServerScreenSwitched:
        if (pointer)
          text = &static_cast<Server::SwitchToScreenInfo *>(pointer)->m_screen;
        break;
      default:
        selected = false;
      }
      if (selected) {
        Record record{type,   event.getTarget(), event.getData(), event.getDataObject(), event.getFlags(), 0, {},
                      nullptr};
        record.number = number;
        if (pointer)
          record.lifetime = std::make_unique<PayloadObservation>(pointer, text);
        if (text)
          record.text = *text;
        records.push_back(std::move(record));
      }
    }
    // Same release boundary for handled/unhandled and immediate/queued events.
    protocol_test::EventQueue::addEvent(std::move(event));
  }
  bool dispatchEvent(const Event &event) override
  {
    if (event.getType() == EventTypes::ServerConnected && (event.getFlags() & Flags::DontFreeData) == 0) {
      if (unhandledConnection)
        return false;
      if (onConnection)
        onConnection(event);
    }
    return protocol_test::EventQueue::dispatchEvent(event);
  }
  std::vector<Record> records;
  std::function<void(const Event &)> onConnection;
  bool unhandledConnection = false;
};

class SyntheticServer
{
public:
  explicit SyntheticServer(std::string secondaryName) : name(std::move(secondaryName))
  {
    config.addScreen("primary");
    config.addScreen(name);
    config.connect("primary", Direction::Right, 0.0f, 1.0f, name, 0.0f, 1.0f);
    config.connect(name, Direction::Left, 0.0f, 1.0f, "primary", 0.0f, 1.0f);
    server =
        std::make_unique<Server>(config, &primary, &screen, &events, NetworkProtocol::Synergy, FileTransferSettings{});
  }
  void connect()
  {
    auto *stream = new protocol_test::Stream;
    auto *client = new ClientProxy1_0(name, stream, &events);
    stream->push("DINF" + protocol_test::integers16({0, 0, 800, 600, 0, 400, 300}));
    events.dispatchEvent(Event(EventTypes::StreamInputReady, stream));
    events.drain();
    server->adoptClient(client); // The real producer under test; caller drains it.
  }
  void movePrimary()
  {
    IPrimaryScreen::MotionInfo info{799, 300};
    events.dispatchEvent(
        Event(EventTypes::PrimaryScreenMotionOnPrimary, primary.getEventTarget(), &info, Flags::DontFreeData)
    );
  }

  std::string name;
  protocol_test::PrivateSettings settings;
  RecordingQueue events;
  protocol_test::PlatformScreen *platform = new protocol_test::PlatformScreen(&events);
  synergy::Screen screen{platform, &events};
  PrimaryClient primary{"primary", &screen};
  synergy::server::Config config{&events};
  std::unique_ptr<Server> server;
};

void actionRows()
{
  QTest::addColumn<int>("kind");
  QTest::addColumn<int>("value");
  QTest::addColumn<QString>("destination");
  QTest::addColumn<bool>("handled");
  for (int state = 0; state < 3; ++state) {
    QTest::newRow(qPrintable(QString("lock-%1").arg(state))) << 0 << state << QString() << true;
    QTest::newRow(qPrintable(QString("broadcast-default-%1").arg(state))) << 3 << state << QString() << true;
    QTest::newRow(qPrintable(QString("broadcast-all-%1").arg(state))) << 3 << state << QStringLiteral("*") << true;
    QTest::newRow(qPrintable(QString("broadcast-long-%1").arg(state)))
        << 3 << state << QString(256, QChar('b')) << true;
  }
  QTest::newRow("switch-long") << 1 << 0 << QString(256, QChar('s')) << true;
  QTest::newRow("switch-default") << 1 << 0 << QString() << true;
  QTest::newRow("switch-connected-fallback") << 1 << 1 << QString() << true;
  for (Direction direction : {Direction::Left, Direction::Right, Direction::Top, Direction::Bottom})
    QTest::newRow(qPrintable(QString("direction-%1").arg(static_cast<int>(direction))))
        << 2 << static_cast<int>(direction) << QString() << true;
  QTest::newRow("unhandled-lock") << 0 << 2 << QString() << false;
  QTest::newRow("unhandled-switch") << 1 << 0 << QString(256, QChar('s')) << false;
  QTest::newRow("unhandled-direction") << 2 << static_cast<int>(Direction::Right) << QString() << false;
  QTest::newRow("unhandled-broadcast") << 3 << 2 << QString(256, QChar('b')) << false;
}
void connectionRows()
{
  QTest::addColumn<bool>("handled");
  QTest::newRow("handled") << true;
  QTest::newRow("unhandled") << false;
}
} // namespace

void EventPayloadOwnershipTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
#ifdef Q_OS_WIN
  m_arch.init();
#endif
  static synergy::core::ipc::CoreIpcServer ipc(nullptr);
}

void EventPayloadOwnershipTests::init()
{
  observations.fill({});
  observationCount = 0;
}

void EventPayloadOwnershipTests::actionFieldsAndCallbacks_data()
{
  actionRows();
}
void EventPayloadOwnershipTests::actionFieldsAndCallbacks()
{
  checkAction(false);
}
void EventPayloadOwnershipTests::actionOwnership_data()
{
  actionRows();
}
void EventPayloadOwnershipTests::actionOwnership()
{
  checkAction(true);
}

void EventPayloadOwnershipTests::checkAction(bool ownership)
{
  QFETCH(int, kind);
  QFETCH(int, value);
  QFETCH(QString, destination);
  QFETCH(bool, handled);
  const std::array types{
      EventTypes::ServerLockCursorToScreen, EventTypes::ServerSwitchToScreen, EventTypes::ServerSwitchInDirection,
      EventTypes::ServerKeyboardBroadcast
  };
  int target = 0, callbacks = 0;
  const auto type = types.at(kind);
  const std::string name = destination.toStdString();
  const std::string connectedName(256, 'c');
  const std::string expected = kind == 1                     ? (value ? connectedName : name)
                               : name.empty() || name == "*" ? name
                                                             : ":" + name + ":";
  RecordingQueue queue;
  if (handled)
    queue.addHandler(type, &target, [&](const Event &event) {
      ++callbacks;
      QCOMPARE(event.getType(), type);
      QCOMPARE(event.getTarget(), &target);
      QCOMPARE(event.getFlags(), Flags::DeliverImmediately);
      QVERIFY(event.getDataObject() == nullptr);
      QVERIFY(event.getData() != nullptr);
      QCOMPARE(queue.records.size(), size_t{1});
      const auto &record = queue.records.front();
      QCOMPARE(event.getData(), record.raw);
      QCOMPARE(record.lifetime->objectDeletes(), 0u);
      QCOMPARE(record.lifetime->stringDeletes(), 0u);
      if (kind == 0)
        QCOMPARE(static_cast<int>(static_cast<Server::LockCursorToScreenInfo *>(event.getData())->m_state), value);
      else if (kind == 1)
        QCOMPARE(static_cast<Server::SwitchToScreenInfo *>(event.getData())->m_screen, expected);
      else if (kind == 2)
        QCOMPARE(static_cast<int>(static_cast<Server::SwitchInDirectionInfo *>(event.getData())->m_direction), value);
      else {
        const auto *info = static_cast<Server::KeyboardBroadcastInfo *>(event.getData());
        QCOMPARE(static_cast<int>(info->m_state), value);
        QCOMPARE(info->m_screens, expected);
      }
    });

  std::unique_ptr<InputFilter::Action> action;
  if (kind == 0)
    action = std::make_unique<InputFilter::LockCursorToScreenAction>(
        &queue, static_cast<InputFilter::LockCursorToScreenAction::Mode>(value)
    );
  else if (kind == 1)
    action = std::make_unique<InputFilter::SwitchToScreenAction>(&queue, name);
  else if (kind == 2)
    action = std::make_unique<InputFilter::SwitchInDirectionAction>(&queue, static_cast<Direction>(value));
  else if (name.empty())
    action = std::make_unique<InputFilter::KeyboardBroadcastAction>(
        &queue, static_cast<InputFilter::KeyboardBroadcastAction::Mode>(value)
    );
  else
    action = std::make_unique<InputFilter::KeyboardBroadcastAction>(
        &queue, static_cast<InputFilter::KeyboardBroadcastAction::Mode>(value), std::set<std::string>{name}
    );

  Server::ScreenConnectedInfo connection(connectedName);
  if (kind == 1 && value == 1)
    action->perform(Event(EventTypes::ServerConnected, &target, &connection, Flags::DontFreeData));
  else
    action->perform(Event(EventTypes::PrimaryScreenHotkeyDown, &target));
  QCOMPARE(callbacks, handled ? 1 : 0);
  QCOMPARE(queue.records.size(), size_t{1});
  const auto &record = queue.records.front();
  QCOMPARE(record.type, type);
  QCOMPARE(record.target, &target);
  QCOMPARE(record.flags, Flags::DeliverImmediately);
  QVERIFY(record.raw != nullptr);
  QVERIFY(record.object == nullptr);
  if (kind == 0 || kind == 2 || kind == 3)
    QCOMPARE(record.number, value);
  if (kind == 1 || kind == 3)
    QCOMPARE(record.text, expected);
  QCOMPARE(connection.m_screen, connectedName);
  if (ownership) {
    const bool hasLongString = expected.size() > 64 && (kind == 1 || kind == 3);
    const auto diagnostic = QString("object deletes=%1; long-string deletes=%2; expects object=1, string=%3")
                                .arg(record.lifetime->objectDeletes())
                                .arg(record.lifetime->stringDeletes())
                                .arg(hasLongString ? 1 : 0);
    QVERIFY2(
        record.lifetime->objectDeletes() == 1 && (!hasLongString || record.lifetime->stringDeletes() == 1),
        qPrintable(diagnostic)
    );
  }
}

void EventPayloadOwnershipTests::connectionFieldsAndCallbacks_data()
{
  connectionRows();
}
void EventPayloadOwnershipTests::connectionFieldsAndCallbacks()
{
  checkConnection(false);
}
void EventPayloadOwnershipTests::connectionOwnership_data()
{
  connectionRows();
}
void EventPayloadOwnershipTests::connectionOwnership()
{
  checkConnection(true);
}

void EventPayloadOwnershipTests::checkConnection(bool ownership)
{
  QFETCH(bool, handled);
  int callbacks = 0;
  const std::string name(256, 'n');
  SyntheticServer fixture(name);
  fixture.events.unhandledConnection = !handled;
  fixture.events.onConnection = [&](const Event &event) {
    ++callbacks;
    QCOMPARE(event.getTarget(), fixture.primary.getEventTarget());
    QCOMPARE(event.getFlags(), Flags::NoFlags);
    QVERIFY(event.getDataObject() == nullptr);
    QVERIFY(event.getData() != nullptr);
    QCOMPARE(static_cast<Server::ScreenConnectedInfo *>(event.getData())->m_screen, name);
    InputFilter::ScreenConnectedCondition matching(nullptr, name);
    InputFilter::ScreenConnectedCondition any(nullptr, "");
    InputFilter::ScreenConnectedCondition other(nullptr, "other");
    QCOMPARE(matching.match(event), InputFilter::FilterStatus::Activate);
    QCOMPARE(any.match(event), InputFilter::FilterStatus::Activate);
    QCOMPARE(other.match(event), InputFilter::FilterStatus::NoMatch);
    const auto &watched = *fixture.events.records.front().lifetime;
    QCOMPARE(watched.objectDeletes(), 0u);
    QCOMPARE(watched.stringDeletes(), 0u);
  };
  fixture.connect();
  QCOMPARE(fixture.server->getNumClients(), 2u);
  QCOMPARE(callbacks, 0);
  QCOMPARE(fixture.events.records.size(), size_t{1});
  const auto &record = fixture.events.records.front();
  QCOMPARE(record.type, EventTypes::ServerConnected);
  QCOMPARE(record.target, fixture.primary.getEventTarget());
  QCOMPARE(record.flags, Flags::NoFlags);
  QVERIFY(record.raw != nullptr);
  QVERIFY(record.object == nullptr);
  QCOMPARE(record.text, name);
  QCOMPARE(record.lifetime->objectDeletes(), 0u);
  QCOMPARE(record.lifetime->stringDeletes(), 0u);
  fixture.events.drain();
  QCOMPARE(callbacks, handled ? 1 : 0);
  if (ownership) {
    const auto diagnostic = QString("object deletes=%1; long-string deletes=%2; expects both=1")
                                .arg(record.lifetime->objectDeletes())
                                .arg(record.lifetime->stringDeletes());
    QVERIFY2(record.lifetime->objectDeletes() == 1 && record.lifetime->stringDeletes() == 1, qPrintable(diagnostic));
  }
}

void EventPayloadOwnershipTests::correctScreenSwitchedEventDataKeepsItsContract()
{
  const std::string name(256, 'v');
  SyntheticServer fixture(name);
  fixture.connect();
  fixture.events.drain();
  fixture.movePrimary();
  QCOMPARE(fixture.events.records.size(), size_t{2});
  const auto &record = fixture.events.records.back();
  QCOMPARE(record.type, EventTypes::ServerScreenSwitched);
  QCOMPARE(record.target, fixture.server.get());
  QCOMPARE(record.flags, Flags::NoFlags);
  QVERIFY(record.raw == nullptr);
  QVERIFY(record.object != nullptr);
  QCOMPARE(record.text, name);
  QCOMPARE(record.lifetime->objectDeletes(), 0u);
  QCOMPARE(record.lifetime->stringDeletes(), 0u);
  fixture.events.drain();
  QCOMPARE(record.lifetime->objectDeletes(), 1u);
  QCOMPARE(record.lifetime->stringDeletes(), 1u);
}

void EventPayloadOwnershipTests::borrowedConnectionRetainsOriginalOwner()
{
  const std::string name(256, 'f');
  auto payload = std::make_unique<Server::ScreenConnectedInfo>(name);
  auto *pointer = payload.get();
  PayloadObservation watched(pointer, &pointer->m_screen);
  int target = 0, callbacks = 0;
  RecordingQueue queue;
  queue.addHandler(EventTypes::ServerConnected, &target, [&](const Event &event) {
    ++callbacks;
    QCOMPARE(event.getData(), pointer);
    QCOMPARE(event.getFlags(), Flags::DontFreeData | Flags::DeliverImmediately);
    QCOMPARE(static_cast<Server::ScreenConnectedInfo *>(event.getData())->m_screen, name);
    QCOMPARE(watched.objectDeletes(), 0u);
    QCOMPARE(watched.stringDeletes(), 0u);
  });
  Event owner(EventTypes::ServerConnected, &target, std::move(payload));
  queue.addEvent(Event(EventTypes::ServerConnected, &target, pointer, Flags::DontFreeData | Flags::DeliverImmediately));
  QCOMPARE(callbacks, 1);
  QVERIFY(queue.records.empty());
  QCOMPARE(watched.objectDeletes(), 0u);
  QCOMPARE(watched.stringDeletes(), 0u);
  Event::deleteData(owner);
  QCOMPARE(watched.objectDeletes(), 1u);
  QCOMPARE(watched.stringDeletes(), 1u);
}

QTEST_MAIN(EventPayloadOwnershipTests)
