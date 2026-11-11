/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "KeyInfoOwnershipTests.h"

#include "../synergy/MockKeyState.h"
#include "ServerTestFixture.h"
#include "base/EventQueue.h"
#include "base/IEventQueueBuffer.h"
#include "ipc/CoreIpcServer.h"
#include "server/InputFilter.h"

#include <QProcess>

#include <array>
#include <cstdlib>
#include <deque>
#include <memory>
#include <new>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
struct DeleteObservation
{
  const void *pointer = nullptr;
  unsigned calls = 0;
};
thread_local std::array<DeleteObservation, 128> observations{};
thread_local size_t observationCount = 0;

size_t observe(const void *pointer)
{
  if (!pointer || observationCount == observations.size())
    std::abort();
  const size_t index = observationCount++;
  observations[index] = {pointer, 0};
  return index;
}
void releaseObject(void *pointer) noexcept
{
  if (pointer) {
    for (auto &observation : observations) {
      if (observation.pointer == pointer) {
        ++observation.calls;
        observation.pointer = nullptr;
      }
    }
  }
  std::free(pointer);
}
void *allocateObject(size_t size)
{
  if (void *pointer = std::malloc(size ? size : 1)) {
    // A baseline free can bypass C++ delete. If malloc later reuses that
    // address, retire the old observation before the unrelated new object's
    // delete; address reuse must never turn a missing delete into a pass.
    for (auto &observation : observations)
      if (observation.pointer == pointer)
        observation.pointer = nullptr;
    return pointer;
  }
  throw std::bad_alloc();
}
} // namespace

// Isolated to this test executable. Exact-pointer, fixed-size bookkeeping
// observes the matching C++ delete and the separate long-string allocation.
void *operator new(size_t size)
{
  return allocateObject(size);
}
void *operator new[](size_t size)
{
  return allocateObject(size);
}
void operator delete(void *pointer) noexcept
{
  releaseObject(pointer);
}
void operator delete(void *pointer, size_t) noexcept
{
  releaseObject(pointer);
}
void operator delete[](void *pointer) noexcept
{
  releaseObject(pointer);
}
void operator delete[](void *pointer, size_t) noexcept
{
  releaseObject(pointer);
}

namespace {
using KeyInfo = IKeyState::KeyInfo;
using Flags = Event::EventFlags;
constexpr bool typedOwnershipAvailable =
    std::is_constructible_v<Event, EventTypes, void *, std::unique_ptr<KeyInfo>, Event::Flags>;

// This helper has no raw fallback: missing new API is reported separately,
// before constructing a typed fixture or entering unsafe baseline self-adoption.
template <typename T>
Event ownedEvent(EventTypes type, void *target, std::unique_ptr<T> info, Event::Flags flags = Flags::NoFlags)
{
  if constexpr (std::is_constructible_v<Event, EventTypes, void *, std::unique_ptr<T>, Event::Flags>)
    return Event(type, target, std::move(info), flags);
  else
    return Event();
}

struct KeyObservation
{
  explicit KeyObservation(const KeyInfo *info)
      : object(observe(info)),
        string(info->m_screens.size() > 64 ? observe(info->m_screens.data()) : observations.size())
  {
  }
  ~KeyObservation()
  {
    // On the defective baseline free(info) already freed the object, but its
    // long std::string allocation remains. Release only that saved allocation,
    // without dereferencing or freeing the already-freed KeyInfo again.
    if (string < observations.size() && observations[string].pointer)
      ::operator delete(const_cast<void *>(observations[string].pointer));
  }
  unsigned objectDeletes() const
  {
    return observations[object].calls;
  }
  unsigned stringDeletes() const
  {
    return string < observations.size() ? observations[string].calls : 0;
  }
  size_t object;
  size_t string;
};

struct RecordedEvent
{
  EventTypes type;
  void *target;
  void *pointer;
  Event::Flags flags;
  KeyInfo key{};
};

class RecordingQueue : public protocol_test::EventQueue
{
public:
  void addEvent(Event &&event) override
  {
    RecordedEvent record{event.getType(), event.getTarget(), event.getData(), event.getFlags(), {}};
    if (event.getType() == EventTypes::KeyStateKeyDown || event.getType() == EventTypes::KeyStateKeyUp ||
        event.getType() == EventTypes::KeyStateKeyRepeat) {
      const auto *info = static_cast<const KeyInfo *>(event.getData());
      record.key = *info;
      if ((event.getFlags() & Flags::DontFreeData) == 0)
        owned.push_back(std::make_unique<KeyObservation>(info));
    }
    records.push_back(std::move(record));
    protocol_test::EventQueue::addEvent(std::move(event));
  }
  std::vector<RecordedEvent> records;
  std::vector<std::unique_ptr<KeyObservation>> owned;
};

struct Payload
{
  explicit Payload(int *destroyed) : destroyed(destroyed)
  {
  }
  ~Payload()
  {
    ++*destroyed;
  }
  int *destroyed;
  std::string text = std::string(256, 'p');
};
struct ObjectPayload : EventData
{
  explicit ObjectPayload(int *destroyed) : destroyed(destroyed)
  {
  }
  ~ObjectPayload() override
  {
    ++*destroyed;
  }
  int *destroyed;
};

// A public buffer implementation makes enqueue failure and discard deterministic.
// Once all accepted IDs have been consumed it returns a non-owning Quit event,
// keeping the real queue loop bounded even when the test payload was rejected.
class FiniteBuffer : public IEventQueueBuffer
{
public:
  explicit FiniteBuffer(bool reject = false) : reject(reject)
  {
  }
  void init() override
  {
  }
  void waitForEvent(double) override
  {
  }
  Type getEvent(Event &event, uint32_t &id) override
  {
    if (!ids.empty()) {
      id = ids.front();
      ids.pop_front();
      return Type::User;
    }
    event = Event(EventTypes::Quit);
    return Type::System;
  }
  bool addEvent(uint32_t id) override
  {
    if (reject)
      return false;
    ids.push_back(id);
    return true;
  }
  bool isEmpty() const override
  {
    return false;
  }
  bool reject;
  std::deque<uint32_t> ids;
};

int selfAdoptionProbe()
{
  if (!typedOwnershipAvailable)
    return 64;
  auto *old = KeyInfo::alloc(kKeyF2, 0, 4, 1, {std::string(256, 'o')});
  KeyObservation oldWatch(old);
  auto *info = KeyInfo::alloc(kKeyF1, 0, 3, 1, {std::string(256, 's')});
  KeyObservation watched(info);
  auto action = std::make_unique<InputFilter::KeystrokeAction>(nullptr, old, true);
  action->adoptInfo(info);
  // Reject an unfixed free-based replacement before self-adoption could leave
  // a dangling owner. Saved counters do not dereference the released object.
  if (oldWatch.objectDeletes() != 1 || oldWatch.stringDeletes() != 1)
    std::_Exit(65);
  action->adoptInfo(info);
  // A delete-without-self-guard regression is detected before dereferencing or
  // destroying the dangling owner. _Exit deliberately bypasses all destructors.
  if (watched.objectDeletes() != 0 || watched.stringDeletes() != 0)
    std::_Exit(2);
  if (action->getInfo() != info || info->m_key != kKeyF1 || info->m_screens != ":" + std::string(256, 's') + ":")
    std::_Exit(3);
  action.reset();
  return watched.objectDeletes() == 1 && watched.stringDeletes() == 1 ? 0 : 4;
}
} // namespace

void KeyInfoOwnershipTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
#ifdef Q_OS_WIN
  m_arch.init();
#endif
  static synergy::core::ipc::CoreIpcServer ipc(nullptr);
}

void KeyInfoOwnershipTests::init()
{
  observations.fill({});
  observationCount = 0;
}

void KeyInfoOwnershipTests::conditionDestroysAdoptedObjectAndString()
{
  auto *info = KeyInfo::alloc(kKeyScrollLock, KeyModifierShift, 7, 3, {std::string(256, 's')});
  KeyObservation watched(info);
  InputFilter::KeystrokeCondition condition(nullptr, info);
  const unsigned objectDeletes = watched.objectDeletes();
  const unsigned stringDeletes = watched.stringDeletes();
  QCOMPARE(condition.getKey(), kKeyScrollLock);
  QCOMPARE(condition.getMask(), static_cast<KeyModifierMask>(KeyModifierShift));
  QCOMPARE(condition.format(), std::string("keystroke(Shift+ScrollLock)"));
  const auto clone = std::unique_ptr<InputFilter::Condition>(condition.clone());
  QCOMPARE(clone->format(), condition.format());
  QCOMPARE(objectDeletes, 1u);
  QCOMPARE(stringDeletes, 1u);
}

void KeyInfoOwnershipTests::parsedConditionAndActionsPreserveFields()
{
  std::istringstream empty;
  synergy::server::ConfigReadContext context(empty);
  auto *info = context.parseKeystroke("Control+F1", {std::string(256, 's')});
  KeyObservation watched(info);
  InputFilter::KeystrokeCondition condition(nullptr, info);
  const unsigned objectDeletes = watched.objectDeletes();
  const unsigned stringDeletes = watched.stringDeletes();
  QCOMPARE(condition.getKey(), kKeyF1);
  QCOMPARE(condition.getMask(), static_cast<KeyModifierMask>(KeyModifierControl));
  QCOMPARE(condition.format(), std::string("keystroke(Control+F1)"));

  const std::string destination(256, 'd');
  std::istringstream input(
      "section: screens\n  primary:\n  " + destination +
      ":\nend\n"
      "section: options\n"
      "  keystroke(Control+F1) = keystroke(Shift+F2," +
      destination +
      ")\n"
      "  keystroke(F3) = keyDown(Alt+F4,*)\n"
      "  keystroke(F5) = keyUp(F6)\nend\n"
  );
  synergy::server::Config config(nullptr);
  synergy::server::ConfigReadContext read(input);
  config.read(read);
  auto *filter = config.getInputFilter();
  QCOMPARE(filter->getNumRules(), 3u);
  auto &paired = filter->getRule(0);
  QCOMPARE(paired.getCondition()->format(), std::string("keystroke(Control+F1)"));
  QCOMPARE(paired.getNumActions(true), 1u);
  QCOMPARE(paired.getNumActions(false), 1u);
  const auto *down = dynamic_cast<const InputFilter::KeystrokeAction *>(&paired.getAction(true, 0));
  const auto *up = dynamic_cast<const InputFilter::KeystrokeAction *>(&paired.getAction(false, 0));
  QVERIFY(down && up);
  QVERIFY(down->isOnPress());
  QVERIFY(!up->isOnPress());
  QVERIFY(down->getInfo() != up->getInfo());
  QVERIFY(KeyInfo::equal(down->getInfo(), up->getInfo()));
  QCOMPARE(down->getInfo()->m_key, kKeyF2);
  QCOMPARE(down->getInfo()->m_mask, static_cast<KeyModifierMask>(KeyModifierShift));
  QCOMPARE(down->getInfo()->m_screens, ":" + destination + ":");
  auto &downRule = filter->getRule(1);
  QCOMPARE(downRule.getNumActions(true), 1u);
  QCOMPARE(downRule.getNumActions(false), 0u);
  const auto *onlyDown = dynamic_cast<const InputFilter::KeystrokeAction *>(&downRule.getAction(true, 0));
  QVERIFY(onlyDown && onlyDown->isOnPress());
  QCOMPARE(onlyDown->getInfo()->m_key, kKeyF4);
  QCOMPARE(onlyDown->getInfo()->m_mask, static_cast<KeyModifierMask>(KeyModifierAlt));
  QCOMPARE(onlyDown->getInfo()->m_screens, std::string("*"));
  auto &upRule = filter->getRule(2);
  QCOMPARE(upRule.getNumActions(true), 1u);
  QCOMPARE(upRule.getNumActions(false), 0u);
  const auto *onlyUp = dynamic_cast<const InputFilter::KeystrokeAction *>(&upRule.getAction(true, 0));
  QVERIFY(onlyUp && !onlyUp->isOnPress());
  QCOMPARE(onlyUp->getInfo()->m_key, kKeyF6);
  QCOMPARE(onlyUp->getInfo()->m_mask, static_cast<KeyModifierMask>(0));
  QVERIFY(onlyUp->getInfo()->m_screens.empty());
  QCOMPARE(objectDeletes, 1u);
  QCOMPARE(stringDeletes, 1u);
}

void KeyInfoOwnershipTests::serverDefaultScrollLockStillToggles()
{
  protocol_test::ServerFixture fixture;
  auto toggle = [&fixture] {
    auto *hotkey = IPrimaryScreen::HotKeyInfo::alloc(1);
    Event event(EventTypes::PrimaryScreenHotkeyDown, fixture.primary.getEventTarget(), hotkey);
    fixture.events.dispatchEvent(event);
    Event::deleteData(event);
  };
  toggle();
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.secondaryStream->written.empty());
  toggle();
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 1);
  QVERIFY(fixture.secondaryStream->written.starts_with("CINN"));
}

void KeyInfoOwnershipTests::actionCloneAndReplacementOwnIndependentStrings()
{
  auto *original = KeyInfo::alloc(kKeyF2, KeyModifierAlt, 9, 4, {std::string(256, 'a')});
  KeyObservation originalWatch(original);
  auto action = std::make_unique<InputFilter::KeystrokeAction>(nullptr, original, true);
  auto cloned = std::unique_ptr<InputFilter::Action>(action->clone());
  auto *clone = dynamic_cast<InputFilter::KeystrokeAction *>(cloned.get());
  QVERIFY(clone);
  QVERIFY(clone->getInfo() != original);
  QVERIFY(clone->getInfo()->m_screens.data() != original->m_screens.data());
  QVERIFY(KeyInfo::equal(clone->getInfo(), original));
  KeyObservation cloneWatch(clone->getInfo());
  auto *replacement = KeyInfo::alloc(kKeyF3, KeyModifierControl, 5, 2, {std::string(256, 'b')});
  KeyObservation replacementWatch(replacement);
  action->adoptInfo(replacement);
  const bool oldDeleted = originalWatch.objectDeletes() == 1 && originalWatch.stringDeletes() == 1;
  QCOMPARE(action->getInfo(), replacement);
  QCOMPARE(action->getInfo()->m_count, 2);
  QVERIFY(clone->getInfo()->m_screens.find(std::string(256, 'a')) != std::string::npos);
  action.reset();
  cloned.reset();
  QVERIFY(oldDeleted);
  QCOMPARE(replacementWatch.objectDeletes(), 1u);
  QCOMPARE(replacementWatch.stringDeletes(), 1u);
  QCOMPARE(cloneWatch.objectDeletes(), 1u);
  QCOMPARE(cloneWatch.stringDeletes(), 1u);
}

void KeyInfoOwnershipTests::selfAdoptionKeepsPayloadAlive()
{
  // The existing baseline frees the argument here. Do not dereference freed
  // memory merely to obtain RED; the new API guard fails before that operation.
  QVERIFY2(typedOwnershipAvailable, "typed-owned Event API is not available on this baseline");
  QProcess probe;
  probe.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--self-adoption-probe")});
  QVERIFY(probe.waitForStarted(2000));
  QVERIFY2(probe.waitForFinished(5000), "self-adoption subprocess exceeded its bounded deadline");
  QCOMPARE(probe.exitStatus(), QProcess::NormalExit);
  QCOMPARE(probe.exitCode(), 0);
}

void KeyInfoOwnershipTests::repeatedActionsBorrowPayload_data()
{
  QTest::addColumn<bool>("press");
  QTest::addColumn<QString>("destination");
  for (bool press : {false, true}) {
    QTest::newRow(press ? "down-default" : "up-default") << press << QString();
    QTest::newRow(press ? "down-broadcast" : "up-broadcast") << press << QStringLiteral("*");
    QTest::newRow(press ? "down-explicit" : "up-explicit") << press << QString(256, QChar('d'));
  }
}

void KeyInfoOwnershipTests::repeatedActionsBorrowPayload()
{
  QFETCH(bool, press);
  QFETCH(QString, destination);
  RecordingQueue queue;
  std::set<std::string> destinations;
  if (!destination.isEmpty())
    destinations.insert(destination.toStdString());
  auto *info = KeyInfo::alloc(kKeyF4, KeyModifierShift, 12, 7, destinations);
  KeyObservation watched(info);
  const KeyInfo expected = *info;
  int target = 0;
  {
    InputFilter::KeystrokeAction action(&queue, info, press);
    for (int repeat = 0; repeat < 3; ++repeat)
      action.perform(Event(EventTypes::PrimaryScreenHotkeyDown, &target));
    QCOMPARE(queue.records.size(), size_t{9});
    for (size_t i = 0; i < queue.records.size(); i += 3) {
      QCOMPARE(queue.records[i].type, EventTypes::PrimaryScreenFakeInputBegin);
      QCOMPARE(queue.records[i + 1].type, press ? EventTypes::KeyStateKeyDown : EventTypes::KeyStateKeyUp);
      QCOMPARE(queue.records[i + 2].type, EventTypes::PrimaryScreenFakeInputEnd);
      for (size_t j = 0; j < 3; ++j)
        QCOMPARE(queue.records[i + j].target, &target);
      QCOMPARE(queue.records[i + 1].pointer, info);
      QCOMPARE(queue.records[i + 1].flags, Flags::DeliverImmediately | Flags::DontFreeData);
      QVERIFY(KeyInfo::equal(&queue.records[i + 1].key, &expected));
    }
    QCOMPARE(watched.objectDeletes(), 0u);
    QCOMPARE(watched.stringDeletes(), 0u);
  }
  QCOMPARE(watched.objectDeletes(), 1u);
  if (!destination.isEmpty() && destination != QStringLiteral("*"))
    QCOMPARE(watched.stringDeletes(), 1u);
}

void KeyInfoOwnershipTests::keyStateProducersReleaseObjects_data()
{
  QTest::addColumn<bool>("press");
  QTest::addColumn<bool>("repeat");
  QTest::addColumn<bool>("halfDuplex");
  QTest::newRow("down") << true << false << false;
  QTest::newRow("up") << false << false << false;
  QTest::newRow("repeat") << true << true << false;
  QTest::newRow("half-duplex-pair") << true << false << true;
  QTest::newRow("half-duplex-repeat-ignored") << true << true << true;
}

void KeyInfoOwnershipTests::keyStateProducersReleaseObjects()
{
  QFETCH(bool, press);
  QFETCH(bool, repeat);
  QFETCH(bool, halfDuplex);
  RecordingQueue queue;
  synergy::KeyMap map;
  if (halfDuplex)
    map.addHalfDuplexButton(11);
  MockKeyState state(queue, map);
  int target = 0;
  state.sendKeyEvent(&target, press, repeat, kKeyF5, KeyModifierAlt, 6, 11);
  const size_t expected = halfDuplex ? (repeat ? 0 : 2) : 1;
  QCOMPARE(queue.records.size(), expected);
  for (size_t i = 0; i < expected; ++i) {
    const auto &event = queue.records[i];
    const auto type = halfDuplex ? (i ? EventTypes::KeyStateKeyUp : EventTypes::KeyStateKeyDown)
                                 : (repeat ? EventTypes::KeyStateKeyRepeat
                                           : (press ? EventTypes::KeyStateKeyDown : EventTypes::KeyStateKeyUp));
    QCOMPARE(event.type, type);
    QCOMPARE(event.target, &target);
    QCOMPARE(event.flags, Flags::NoFlags);
    QCOMPARE(event.key.m_key, kKeyF5);
    QCOMPARE(event.key.m_mask, static_cast<KeyModifierMask>(KeyModifierAlt));
    QCOMPARE(event.key.m_button, 11);
    QCOMPARE(event.key.m_count, repeat ? 6 : 1);
    QVERIFY(event.key.m_screens.empty());
  }
  queue.drain();
  for (const auto &watched : queue.owned)
    QCOMPARE(watched->objectDeletes(), 1u);
}

void KeyInfoOwnershipTests::mouseModifiersReleaseOwnedPayload_data()
{
  QTest::addColumn<bool>("press");
  QTest::newRow("set") << true;
  QTest::newRow("clear") << false;
}

void KeyInfoOwnershipTests::mouseModifiersReleaseOwnedPayload()
{
  QFETCH(bool, press);
  RecordingQueue queue;
  int target = 0;
  const IPlatformScreen::ButtonInfo button{2, KeyModifierShift};
  InputFilter::MouseButtonAction action(&queue, button, press);
  action.perform(Event(EventTypes::PrimaryScreenHotkeyDown, &target));
  QCOMPARE(queue.records.size(), size_t{2});
  const auto &key = queue.records[0];
  QCOMPARE(key.type, EventTypes::KeyStateKeyDown);
  QCOMPARE(key.target, &target);
  QCOMPARE(key.flags, Flags::DeliverImmediately);
  QCOMPARE(key.key.m_key, press ? kKeySetModifiers : kKeyClearModifiers);
  QCOMPARE(key.key.m_mask, static_cast<KeyModifierMask>(KeyModifierShift));
  QCOMPARE(key.key.m_button, 0);
  QCOMPARE(key.key.m_count, 1);
  QCOMPARE(queue.records[1].type, press ? EventTypes::PrimaryScreenButtonDown : EventTypes::PrimaryScreenButtonUp);
  QCOMPARE(queue.records[1].flags, Flags::DeliverImmediately | Flags::DontFreeData);
  QCOMPARE(action.getInfo().m_button, static_cast<ButtonID>(2));
  QCOMPARE(queue.owned.size(), size_t{1});
  QCOMPARE(queue.owned.front()->objectDeletes(), 1u);
}

void KeyInfoOwnershipTests::typedEventMovesTransferDisposalRight()
{
  QVERIFY2(typedOwnershipAvailable, "typed-owned Event API is not available on this baseline");
  int destroyed = 0, target = 0;
  auto payload = std::make_unique<Payload>(&destroyed);
  auto *pointer = payload.get();
  const auto string = observe(pointer->text.data());
  Event source = ownedEvent(EventTypes::ScreenSuspend, &target, std::move(payload));
  QCOMPARE(source.getData(), pointer);
  Event moved(std::move(source));
  Event::deleteData(source);
  QCOMPARE(destroyed, 0);
  Event receiver;
  receiver = std::move(moved);
  Event::deleteData(moved);
  QCOMPARE(destroyed, 0);
  QCOMPARE(receiver.getType(), EventTypes::ScreenSuspend);
  QCOMPARE(receiver.getTarget(), &target);
  QCOMPARE(receiver.getFlags(), Flags::NoFlags);
  QCOMPARE(receiver.getData(), pointer);
  QCOMPARE(static_cast<Payload *>(receiver.getData())->text, std::string(256, 'p'));
  Event::deleteData(receiver);
  QCOMPARE(destroyed, 1);
  QCOMPARE(observations[string].calls, 1u);
}

void KeyInfoOwnershipTests::legacyAndBorrowedPayloadsKeepTheirRules()
{
  int destroyed = 0;
  {
    Payload borrowed(&destroyed);
    Event event(EventTypes::ScreenSuspend, nullptr, &borrowed, Flags::DontFreeData);
    Event::deleteData(event);
    QCOMPARE(destroyed, 0);
    QCOMPARE(borrowed.text, std::string(256, 'p'));
  }
  QCOMPARE(destroyed, 1);
  Event object(EventTypes::ScreenSuspend, nullptr, new ObjectPayload(&destroyed));
  QVERIFY(object.getData() == nullptr);
  QVERIFY(object.getDataObject() != nullptr);
  Event::deleteData(object);
  QCOMPARE(destroyed, 2);
  Event assigned(EventTypes::ScreenSuspend);
  assigned.setDataObject(new ObjectPayload(&destroyed));
  Event::deleteData(assigned);
  QCOMPARE(destroyed, 3);
  auto *hotkey = IPrimaryScreen::HotKeyInfo::alloc(17);
  const auto watched = observe(hotkey);
  Event pod(EventTypes::PrimaryScreenHotkeyDown, nullptr, hotkey);
  QCOMPARE(static_cast<IPrimaryScreen::HotKeyInfo *>(pod.getData())->m_id, 17u);
  Event::deleteData(pod);
  QCOMPARE(observations[watched].calls, 0u); // malloc path must not call C++ delete.
  observations[watched].pointer = nullptr;   // already freed by production free.
  for (EventTypes type : {EventTypes::Unknown, EventTypes::System, EventTypes::Timer}) {
    int stackPayload = 8;
    Event excluded(type, nullptr, &stackPayload);
    Event::deleteData(excluded);
    QCOMPARE(stackPayload, 8);
  }
}

void KeyInfoOwnershipTests::realQueueDisposesPayload_data()
{
  QTest::addColumn<QString>("mode");
  for (const char *mode : {"ordinary", "immediate", "enqueue-failure", "buffer-discard"})
    QTest::newRow(mode) << QString::fromLatin1(mode);
}

void KeyInfoOwnershipTests::realQueueDisposesPayload()
{
  QFETCH(QString, mode);
  QVERIFY2(typedOwnershipAvailable, "typed-owned Event API is not available on this baseline");
  EventQueue queue;
  queue.adoptBuffer(new FiniteBuffer(mode == QStringLiteral("enqueue-failure")));
  int destroyed = 0, target = 0, delivered = 0;
  auto payload = std::make_unique<Payload>(&destroyed);
  auto *pointer = payload.get();
  queue.addHandler(EventTypes::ScreenSuspend, &target, [&](const Event &event) {
    QCOMPARE(event.getData(), pointer);
    QCOMPARE(destroyed, 0);
    QCOMPARE(static_cast<Payload *>(event.getData())->text, std::string(256, 'p'));
    ++delivered;
  });
  if (mode == QStringLiteral("buffer-discard")) {
    // First bounded empty loop sets readiness. The next add stores a live ID.
    QCOMPARE(queue.loop(), 0);
    queue.addEvent(ownedEvent(EventTypes::ScreenSuspend, &target, std::move(payload)));
    QCOMPARE(destroyed, 0);
    queue.adoptBuffer(new FiniteBuffer());
  } else {
    const auto flags = mode == QStringLiteral("immediate") ? Flags::DeliverImmediately : Flags::NoFlags;
    queue.addEvent(ownedEvent(EventTypes::ScreenSuspend, &target, std::move(payload), flags));
    if (mode != QStringLiteral("immediate"))
      QCOMPARE(queue.loop(), 0);
  }
  QCOMPARE(delivered, mode == QStringLiteral("ordinary") || mode == QStringLiteral("immediate") ? 1 : 0);
  QCOMPARE(destroyed, 1);
}

void KeyInfoOwnershipTests::filterForwardingBorrowsOriginalPayload()
{
  QVERIFY2(typedOwnershipAvailable, "typed-owned Event API is not available on this baseline");
  protocol_test::PrivateSettings settings;
  RecordingQueue queue;
  auto *platform = new protocol_test::PlatformScreen(&queue);
  synergy::Screen screen(platform, &queue);
  PrimaryClient primary("primary", &screen);
  InputFilter filter(&queue);
  filter.setPrimaryClient(&primary);
  auto payload = std::unique_ptr<KeyInfo>(KeyInfo::alloc(kKeyF6, KeyModifierAlt, 7, 5, {std::string(256, 't')}));
  auto *pointer = payload.get();
  KeyObservation watched(pointer);
  const KeyInfo expected = *pointer;
  int forwarded = 0;
  queue.addHandler(EventTypes::KeyStateKeyDown, &filter, [&](const Event &event) {
    ++forwarded;
    QCOMPARE(event.getData(), pointer);
    QCOMPARE(event.getFlags(), Flags::DeliverImmediately | Flags::DontFreeData);
    QVERIFY(KeyInfo::equal(static_cast<KeyInfo *>(event.getData()), &expected));
    QCOMPARE(watched.objectDeletes(), 0u);
    QCOMPARE(watched.stringDeletes(), 0u);
  });
  Event original = ownedEvent(EventTypes::KeyStateKeyDown, primary.getEventTarget(), std::move(payload));
  QVERIFY(queue.dispatchEvent(original));
  QCOMPARE(forwarded, 1);
  QCOMPARE(watched.objectDeletes(), 0u);
  QCOMPARE(watched.stringDeletes(), 0u);
  Event::deleteData(original);
  QCOMPARE(watched.objectDeletes(), 1u);
  QCOMPARE(watched.stringDeletes(), 1u);
  filter.setPrimaryClient(nullptr);
}

int main(int argc, char **argv)
{
  if (argc == 2 && std::string_view(argv[1]) == "--self-adoption-probe")
    return selfAdoptionProbe();
  QCoreApplication app(argc, argv);
  KeyInfoOwnershipTests tests;
  return QTest::qExec(&tests, argc, argv);
}
