/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "../synergy/MockEventQueue.h"
#include "AppUtil.h"
#include "Clipboard.h"
#include "ClipboardChunk.h"
#include "CoreIpcServer.h"
#include "IPlatformScreen.h"
#include "OptionTypes.h"
#include "ProtocolTypes.h"
#include "Screen.h"
#include "arch/Arch.h"
#include "base/BaseException.h"
#include "base/EventQueue.h"
#include "base/EventQueueTimer.h"
#include "client/Client.h"
#include "common/Settings.h"
#include "net/IDataSocket.h"
#include "net/IListenSocket.h"
#include "net/ISocketFactory.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace client_test {

inline std::string integer(int64_t value, size_t bytes)
{
  const auto encoded = static_cast<uint32_t>(value);
  std::string out(bytes, '\0');
  for (size_t i = 0; i < bytes; ++i)
    out[bytes - 1 - i] = static_cast<char>((encoded >> (8 * i)) & 255U);
  return out;
}
inline std::string text(const std::string &value)
{
  return integer(static_cast<uint32_t>(value.size()), 4) + value;
}
inline std::string packet(const std::string &payload)
{
  return integer(static_cast<uint32_t>(payload.size()), 4) + payload;
}
inline std::string options(const std::vector<uint32_t> &values)
{
  auto out = integer(static_cast<uint32_t>(values.size()), 4);
  for (auto value : values)
    out += integer(value, 4);
  return out;
}
inline std::string hello()
{
  return std::string("Synergy") + integer(kProtocolMajorVersion, 2) + integer(kProtocolMinorVersion, 2);
}
inline std::string clipboardBody(uint8_t id, uint32_t sequence, uint8_t mark, const std::string &data)
{
  return integer(id, 1) + integer(sequence, 4) + integer(mark, 1) + text(data);
}
inline std::string syntheticClipboard()
{
  // One text format: count=1, format=Text(0), size=3, bytes="abc".
  return integer(1, 4) + integer(0, 4) + integer(3, 4) + "abc";
}

// Initialized BEFORE Settings::instance's function-static object. Its destructor
// therefore runs AFTER Settings, its QSettings children and proxy final sync.
// No case-scoped temporary directory or cleanupTestCase removes these files.
class SettingsSandbox
{
public:
  SettingsSandbox()
  {
    if (!root.isValid())
      throw std::runtime_error("cannot create owned settings sandbox");
    const auto base = root.path();
    qputenv("XDG_CONFIG_HOME", (base + "/config").toUtf8());
    qputenv("XDG_STATE_HOME", (base + "/state").toUtf8());
    if (!QDir().mkpath(base + "/state"))
      throw std::runtime_error("cannot create owned state directory");
#ifdef Q_OS_WIN
    // applicationDirPath, NEVER CTest's working directory. This directory is
    // shared only by these two client tests; CTest RESOURCE_LOCK + this OS lock
    // also exclude an independent ctest invocation. No user config is read.
    const auto appDir = QCoreApplication::applicationDirPath();
    rejectLinks(appDir);
    if (QFileInfo(appDir).canonicalFilePath() !=
        QFileInfo(QStringLiteral(CLIENT_TEST_EXECUTABLE_DIRECTORY)).canonicalFilePath())
      throw std::runtime_error("executable is outside its configured client-test directory");
    lock.setFileName(appDir + "/issue270-client-portable-settings.lock");
    // NewOnly is an atomic create: unlike stale-lock recovery it never reads,
    // deletes or takes over an unknown existing file. A crashed run fails closed.
    if (!lock.open(QIODevice::WriteOnly | QIODevice::NewOnly))
      throw std::runtime_error("portable settings directory is occupied");
    ownedDirectory = appDir + "/settings";
    if (QFileInfo::exists(ownedDirectory) || !QDir().mkdir(ownedDirectory))
      throw std::runtime_error("unknown portable settings directory; refusing to inspect or replace");
    rejectLinks(ownedDirectory);
    config = ownedDirectory + "/" + kAppName + ".conf";
#else
    ownedDirectory = base + "/config/" + kAppName;
    if (!QDir().mkpath(ownedDirectory))
      throw std::runtime_error("cannot create owned config directory");
    config = ownedDirectory + "/" + kAppName + ".conf";
#endif
    QFile initial(config);
    if (!initial.open(QIODevice::WriteOnly | QIODevice::NewOnly))
      throw std::runtime_error("settings file already exists or cannot be created");
    initial.close();
    // This explicit IniFormat QSettings does not instantiate Settings or touch
    // NativeFormat/registry. All keys are seeded before ISecondaryScreen loads.
    QSettings seed(config, QSettings::IniFormat);
    seed.setValue(Settings::Core::ComputerName, "synthetic-client");
    seed.setValue(Settings::Core::EnableEnterCommand, false);
    seed.setValue(Settings::Core::EnableExitCommand, false);
    seed.setValue(Settings::FileTransfer::Enabled, false);
    seed.sync();
    if (seed.status() != QSettings::NoError)
      throw std::runtime_error("cannot seed owned configuration");
  }
  ~SettingsSandbox()
  {
#ifdef Q_OS_WIN
    // Only this process-created file/directory; never recursively remove a
    // portable directory or remove unknown entries. Stored paths survive Qt app.
    QFile::remove(config);
    QDir().rmdir(ownedDirectory);
    lock.remove();
#endif
    // root removes only this process's fresh temp tree, after Settings teardown.
  }
  static void rejectLinks(const QString &path)
  {
    auto current = QFileInfo(path).absoluteFilePath();
    while (!current.isEmpty()) {
      const QFileInfo info(current);
      if (!info.exists() || info.isSymLink())
        throw std::runtime_error("settings parent absent or linked");
#ifdef Q_OS_WIN
      const auto attr = GetFileAttributesW(reinterpret_cast<LPCWSTR>(current.utf16()));
      if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        throw std::runtime_error("settings parent has reparse metadata or cannot be checked");
#endif
      const auto parent = info.absolutePath();
      if (parent == current)
        break;
      current = parent;
    }
  }
  QTemporaryDir root;
  QString config;
  QString ownedDirectory;
  QFile lock;
};
class FixedAppUtil final : public AppUtil
{
public:
  int run() override
  {
    throw std::logic_error("test may not run application");
  }
  void startNode() override
  {
    throw std::logic_error("test may not start node");
  }
  std::vector<std::string> getKeyboardLayoutList() override
  {
    return {"en"};
  }
  std::string getCurrentLanguageCode() override
  {
    return "en";
  }
};
struct ProcessRuntime
{
  Arch arch;
  FixedAppUtil util;
  synergy::core::ipc::CoreIpcServer ipc{nullptr}; // NEVER listen().
  ProcessRuntime()
  {
#ifdef Q_OS_WIN
    // Once, process-local Winsock + error mode; no socket endpoint or service.
    arch.init();
#endif
  }
};
inline void initializeProcess()
{
  static SettingsSandbox sandbox;
  static ProcessRuntime runtime;
  Q_UNUSED(runtime);
  // Verifies bootstrap was effective rather than silently reading fallback.
  if (Settings::settingsFile() != QFileInfo(sandbox.config).absoluteFilePath())
    throw std::runtime_error("Settings selected a non-owned file");
}

struct Call
{
  Call(
      std::string callName, int64_t first = 0, int64_t second = 0, int64_t third = 0, int64_t fourth = 0,
      std::string lang = {}
  )
      : name(std::move(callName)),
        a(first),
        b(second),
        c(third),
        d(fourth),
        language(std::move(lang))
  {
  }
  std::string name;
  int64_t a = 0, b = 0, c = 0, d = 0;
  std::string language;
  bool operator==(const Call &) const = default;
};
struct ScreenState
{
  int enabled = 0, disabled = 0, destroyed = 0, clipboardSets = 0, clipboardClears = 0, drags = 0;
  std::vector<Call> calls;
  std::string clipboardText;
};
class FakePlatformScreen final : public IPlatformScreen
{
public:
  FakePlatformScreen(IEventQueue *events, std::shared_ptr<ScreenState> state)
      : IPlatformScreen(events),
        s(std::move(state))
  {
  }
  ~FakePlatformScreen() override
  {
    ++s->destroyed;
  }
  void enable() override
  {
    ++s->enabled;
  }
  void disable() override
  {
    ++s->disabled;
  }
  void enter() override
  {
    record("enter");
  }
  bool canLeave() override
  {
    return true;
  }
  void leave() override
  {
    record("leave");
  }
  bool isPrimary() const override
  {
    return false;
  }
  void *getEventTarget() const override
  {
    return const_cast<FakePlatformScreen *>(this);
  }
  bool getClipboard(ClipboardID, IClipboard *out) const override
  {
    out->open(17);
    out->empty();
    out->add(IClipboard::Format::Text, "abc");
    out->close();
    return true;
  }
  bool setClipboard(ClipboardID id, const IClipboard *in) override
  {
    if (!in) {
      ++s->clipboardClears;
      return true;
    }
    ++s->clipboardSets;
    in->open(0);
    s->clipboardText = in->get(IClipboard::Format::Text);
    in->close();
    record("clipboard", id);
    return true;
  }
  void getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const override
  {
    x = -7;
    y = 9;
    w = 800;
    h = 600;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = 31;
    y = 43;
  }
  void checkClipboards() override
  {
  }
  void handleSystemEvent(const Event &) override
  {
    throw std::logic_error("system screen event prohibited");
  }
  void openScreensaver(bool) override
  {
  }
  void closeScreensaver() override
  {
  }
  void screensaver(bool on) override
  {
    record("screensaver", on);
  }
  void resetOptions() override
  {
    record("reset");
  }
  void setOptions(const OptionsList &v) override
  {
    record("options", static_cast<int64_t>(v.size()));
  }
  void setSequenceNumber(uint32_t n) override
  {
    record("sequence", n);
  }
  std::string getSecureInputApp() const override
  {
    return "synthetic";
  }
  bool startDraggingFiles(const std::vector<std::string> &) override
  {
    ++s->drags;
    return true;
  }
  void reconfigure(uint32_t) override
  {
  }
  uint32_t activeSides() override
  {
    return 0;
  }
  void warpCursor(int32_t, int32_t) override
  {
    throw std::logic_error("primary path unexpected");
  }
  uint32_t registerHotKey(KeyID, KeyModifierMask) override
  {
    return 0;
  }
  void unregisterHotKey(uint32_t) override
  {
  }
  void fakeInputBegin() override
  {
  }
  void fakeInputEnd() override
  {
  }
  int32_t getJumpZoneSize() const override
  {
    return 0;
  }
  bool isAnyMouseButtonDown(uint32_t &id) const override
  {
    id = 0;
    return false;
  }
  void getCursorCenter(int32_t &x, int32_t &y) const override
  {
    x = 400;
    y = 300;
  }
  void fakeMouseButton(ButtonID id, bool down) override
  {
    record("button", id, down);
  }
  void fakeMouseMove(int32_t x, int32_t y) override
  {
    record("move", x, y);
  }
  void fakeMouseRelativeMove(int32_t x, int32_t y) const override
  {
    record("relative", x, y);
  }
  void fakeMouseWheel(ScrollDelta delta) const override
  {
    record("wheel", delta.x, delta.y);
  }
  void updateKeyMap() override
  {
  }
  void updateKeyState() override
  {
  }
  void setHalfDuplexMask(KeyModifierMask mask) override
  {
    record("halfDuplex", mask);
  }
  void fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang) override
  {
    record("keyDown", id, mask, button, 0, lang);
  }
  bool fakeKeyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang) override
  {
    record("keyRepeat", id, mask, count, button, lang);
    return true;
  }
  bool fakeKeyUp(KeyButton button) override
  {
    record("keyUp", button);
    return true;
  }
  void fakeAllKeysUp() override
  {
    record("allKeysUp");
  }
  bool fakeCtrlAltDel() override
  {
    return false;
  }
  bool fakeMediaKey(KeyID) override
  {
    return false;
  }
  bool isKeyDown(KeyButton) const override
  {
    return false;
  }
  KeyModifierMask getActiveModifiers() const override
  {
    return 0;
  }
  KeyModifierMask pollActiveModifiers() const override
  {
    return 0;
  }
  int32_t pollActiveGroup() const override
  {
    return 0;
  }
  void pollPressedKeys(KeyButtonSet &) const override
  {
  }

private:
  void record(
      const std::string &name, int64_t a = 0, int64_t b = 0, int64_t c = 0, int64_t d = 0, const std::string &lang = ""
  ) const
  {
    s->calls.push_back({name, a, b, c, d, lang});
  }
  std::shared_ptr<ScreenState> s;
};

using HandlerKey = std::pair<EventTypes, void *>;
struct Failure
{
  EventTypes type;
  QString what;
  bool retry;
  void *target;
};
struct PayloadRecord
{
  const void *identity;
  int dispatched = 0, dropped = 0, disposed = 0;
};
struct QueueState
{
  std::map<HandlerKey, IEventQueue::EventHandler> handlers;
  struct Pending
  {
    explicit Pending(Event &&e) : event(std::move(e))
    {
    }
    Event event;
    size_t payload = 0;
  };
  std::deque<std::unique_ptr<Pending>> pending;
  std::map<EventQueueTimer *, double> timers;
  std::set<void *> retiredTargets;
  std::vector<EventTypes> observed;
  std::vector<HandlerKey> observedKeys;
  std::vector<Failure> failures;
  std::vector<PayloadRecord> chunks;
  int timerCreates = 0, timerDeletes = 0, duplicateTimerDeletes = 0, unsafeDrops = 0, failureDisposals = 0;
  bool has(EventTypes type, void *target) const
  {
    return handlers.contains({type, target});
  }
  void retire(void *target)
  {
    retiredTargets.insert(target);
  }
  void add(Event &&event)
  {
    auto item = std::make_unique<Pending>(std::move(event));
    if (item->event.getType() == EventTypes::ClipboardSending) {
      item->payload = chunks.size();
      chunks.push_back({item->event.getDataObject()});
    }
    pending.push_back(std::move(item));
    if (pending.size() > 128)
      throw std::logic_error("unbounded fixture event queue");
  }
  EventQueueTimer *timer(double duration)
  {
    auto *t = new EventQueueTimer;
    timers[t] = duration;
    ++timerCreates;
    return t;
  }
  void removeTimer(EventQueueTimer *timer)
  {
    if (timers.erase(timer) != 1) {
      ++duplicateTimerDeletes;
      return;
    }
    ++timerDeletes;
    delete timer;
  }
  EventQueueTimer *timerFor(double duration) const
  {
    for (const auto &[t, d] : timers)
      if (d == duration)
        return t;
    return nullptr;
  }
  size_t count(EventTypes type) const
  {
    return std::count(observed.begin(), observed.end(), type);
  }
  // Exactly one disposal owner, independent of Event's default pointer-preserving
  // move. Borrowed Events from StreamFilter are never passed here for disposal.
  void drain(IEventQueue &queue)
  {
    size_t limit = 0;
    while (!pending.empty()) {
      if (++limit > 256)
        throw std::logic_error("unbounded fixture dispatch");
      auto item = std::move(pending.front());
      pending.pop_front();
      auto &e = item->event;
      const auto type = e.getType();
      observed.push_back(type);
      observedKeys.emplace_back(type, e.getTarget());
      if (type == EventTypes::ClientConnectionFailed || type == EventTypes::ClientConnectionRefused) {
        std::unique_ptr<Client::FailInfo> info(static_cast<Client::FailInfo *>(e.getData()));
        if ((e.getFlags() & Event::EventFlags::DontFreeData) == 0)
          throw std::logic_error("production failure ownership flag changed");
        failures.push_back({type, info->m_what, info->m_retry, e.getTarget()});
        ++failureDisposals;
        // The fixture is the typed consumer replacing ClientApp, not a second
        // generic Event::deleteData owner. info dies exactly once here.
        continue;
      }
      bool dispatched = false;
      if (retiredTargets.contains(e.getTarget()) &&
          (has(type, e.getTarget()) || has(EventTypes::Unknown, e.getTarget()))) {
        // Contain an unresolved stale-handler defect without invoking it. Tests
        // fail on the key and unsafeDrops; this is NOT module cleanup success.
        ++unsafeDrops;
      } else {
        dispatched = queue.dispatchEvent(e);
      }
      if (type == EventTypes::ClipboardSending) {
        auto &r = chunks.at(item->payload);
        dispatched ? ++r.dispatched : ++r.dropped;
        Event::deleteData(e);
        ++r.disposed;
      } else {
        Event::deleteData(e);
      }
    }
  }
  ~QueueState()
  {
    // No silent blanket clear: normal fixture destructor first drains and records
    // all pending owners. Residual timer resources are contained but assertions
    // inspect timerCreates/deletes/live identities BEFORE this final safeguard.
    for (const auto &[t, d] : timers) {
      Q_UNUSED(d);
      delete t;
    }
    if (!pending.empty())
      qFatal("fixture abandoned pending payload ownership");
  }
};
class RecordingQueue final : public MockEventQueue
{
public:
  QueueState state;
  void addHandler(EventTypes t, void *target, const EventHandler &h) override
  {
    state.retiredTargets.erase(target);
    state.handlers[{t, target}] = h;
  }
  void removeHandler(EventTypes t, void *target) override
  {
    state.handlers.erase({t, target});
  }
  void removeHandlers(void *target) override
  {
    std::erase_if(state.handlers, [target](const auto &p) { return p.first.second == target; });
  }
  bool dispatchEvent(const Event &e) override
  {
    auto it = state.handlers.find({e.getType(), e.getTarget()});
    if (it == state.handlers.end())
      it = state.handlers.find({EventTypes::Unknown, e.getTarget()});
    if (it == state.handlers.end())
      return false;
    auto callback = it->second;
    callback(e);
    return true;
  }
  void addEvent(Event &&e) override
  {
    state.add(std::move(e));
  }
  EventQueueTimer *newOneShotTimer(double d, void *) override
  {
    return state.timer(d);
  }
  EventQueueTimer *newTimer(double d, void *) override
  {
    return state.timer(d);
  }
  void deleteTimer(EventQueueTimer *t) override
  {
    state.removeTimer(t);
  }
};
class RealDispatchQueue final : public EventQueue
{
public:
  QueueState state;
  void addHandler(EventTypes t, void *target, const EventHandler &h) override
  {
    EventQueue::addHandler(t, target, h);
    state.retiredTargets.erase(target);
    state.handlers[{t, target}] = {};
  }
  void removeHandler(EventTypes t, void *target) override
  {
    EventQueue::removeHandler(t, target);
    state.handlers.erase({t, target});
  }
  void removeHandlers(void *target) override
  {
    EventQueue::removeHandlers(target);
    std::erase_if(state.handlers, [target](const auto &p) { return p.first.second == target; });
  }
  // dispatchEvent/getHandler are deliberately NOT overridden or copied.
  void addEvent(Event &&e) override
  {
    state.add(std::move(e));
  }
  EventQueueTimer *newOneShotTimer(double d, void *) override
  {
    return state.timer(d);
  }
  EventQueueTimer *newTimer(double d, void *) override
  {
    return state.timer(d);
  }
  void deleteTimer(EventQueueTimer *t) override
  {
    state.removeTimer(t);
  }
  int loop() override
  {
    throw std::logic_error("host loop prohibited");
  }
  bool getEvent(Event &, double) override
  {
    throw std::logic_error("host event read prohibited");
  }
  void adoptBuffer(IEventQueueBuffer *) override
  {
    throw std::logic_error("host buffer prohibited");
  }
};
struct SocketState
{
  int connects = 0, destroyed = 0, binds = 0;
  bool throwConnect = false;
  std::deque<std::string> input;
  std::string output;
  void *target = nullptr;
  std::function<void()> onDestroy;
};
class FakeSocket final : public IDataSocket
{
public:
  explicit FakeSocket(std::shared_ptr<SocketState> state) : IDataSocket(nullptr), s(std::move(state))
  {
    s->target = this;
  }
  ~FakeSocket() override
  {
    ++s->destroyed;
    if (s->onDestroy)
      s->onDestroy();
  }
  void connect(const NetworkAddress &) override
  {
    ++s->connects;
    if (s->throwConnect)
      throw BaseException(QStringLiteral("synthetic connect failure"));
  }
  void bind(const NetworkAddress &) override
  {
    ++s->binds;
    throw std::logic_error("bind forbidden");
  }
  void close() override
  {
  }
  void *getEventTarget() const override
  {
    return const_cast<FakeSocket *>(this);
  }
  bool isFatal() const override
  {
    return false;
  }
  ArchSocket getSocket() const override
  {
    return nullptr;
  }
  uint32_t read(void *buffer, uint32_t n) override
  {
    if (s->input.empty())
      return 0;
    auto &front = s->input.front();
    const auto take = std::min<size_t>(n, front.size());
    if (buffer)
      std::memcpy(buffer, front.data(), take);
    front.erase(0, take);
    if (front.empty())
      s->input.pop_front();
    return static_cast<uint32_t>(take);
  }
  void write(const void *data, uint32_t size) override
  {
    s->output.append(static_cast<const char *>(data), size);
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
  }
  void shutdownOutput() override
  {
  }
  bool isReady() const override
  {
    return !s->input.empty();
  }
  uint32_t getSize() const override
  {
    size_t n = 0;
    for (const auto &b : s->input)
      n += b.size();
    return static_cast<uint32_t>(n);
  }

private:
  std::shared_ptr<SocketState> s;
};
struct FactoryState
{
  bool throwCreate = false, throwConnect = false;
  int creates = 0;
  std::vector<SecurityLevel> levels;
  std::vector<IArchNetwork::AddressFamily> families;
  std::vector<std::shared_ptr<SocketState>> sockets;
};
class FakeFactory final : public ISocketFactory
{
public:
  explicit FakeFactory(std::shared_ptr<FactoryState> state) : s(std::move(state))
  {
  }
  std::unique_ptr<IDataSocket> create(IArchNetwork::AddressFamily family, SecurityLevel level) const override
  {
    ++s->creates;
    s->levels.push_back(level);
    s->families.push_back(family);
    if (s->throwCreate)
      throw BaseException(QStringLiteral("synthetic factory failure"));
    auto socket = std::make_shared<SocketState>();
    socket->throwConnect = s->throwConnect;
    s->sockets.push_back(socket);
    return std::make_unique<FakeSocket>(socket);
  }
  std::unique_ptr<IListenSocket> createListen(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    throw std::logic_error("listen forbidden");
  }

private:
  std::shared_ptr<FactoryState> s;
};

template <class Queue = RecordingQueue> class Fixture
{
public:
  Queue events;
  std::shared_ptr<ScreenState> screenState = std::make_shared<ScreenState>();
  std::shared_ptr<FactoryState> factoryState = std::make_shared<FactoryState>();
  std::unique_ptr<synergy::Screen> screen;
  std::unique_ptr<Client> client;
  void *screenTarget = nullptr;
  void *filterTarget = nullptr;
  bool secure;
  explicit Fixture(bool encrypted = false, ServerProxyConfig proxyConfig = {}) : secure(encrypted)
  {
    auto *fake = new FakePlatformScreen(&events, screenState);
    screenTarget = fake;
    screen = std::make_unique<synergy::Screen>(fake, &events);
    ClientConfig config;
    config.useSecureNetwork = secure;
    config.maxClipboardReceiveBytes = 128;
    config.serverProxy = std::move(proxyConfig);
    client = std::make_unique<Client>(
        &events, "synthetic-client", NetworkAddress(24800), std::make_unique<FakeFactory>(factoryState), screen.get(),
        config
    );
  }
  ~Fixture()
  {
    retireOwners();
    client.reset();
    screen.reset();
    events.state.drain(events);
  }
  void retireOwners()
  {
    if (filterTarget)
      events.state.retire(filterTarget);
    for (const auto &[key, h] : events.state.handlers) {
      Q_UNUSED(h);
      if (key.first == EventTypes::ClipboardSending)
        events.state.retire(key.second);
    }
    for (const auto &socket : factoryState->sockets)
      events.state.retire(socket->target);
  }
  std::shared_ptr<SocketState> socket() const
  {
    return factoryState->sockets.back();
  }
  void begin()
  {
    // Bounds even queued IPC closures without pumping the global Qt queue.
    static size_t attempts = 0;
    if (++attempts > 2048)
      throw std::logic_error("synthetic connection-attempt budget exceeded");
    client->connect();
    filterTarget = nullptr;
    for (const auto &[key, h] : events.state.handlers) {
      Q_UNUSED(h);
      if (key.first == EventTypes::DataSocketConnectionFailed)
        filterTarget = key.second;
    }
    if (!factoryState->sockets.empty() && socket()->destroyed == 0) {
      socket()->onDestroy = [this] { retireOwners(); };
    }
  }
  void connected()
  {
    // Resume connects from production directly, bypassing begin(). Capture the
    // current registered owner instead of delivering to a retired filter.
    const auto type = secure ? EventTypes::DataSocketSecureConnected : EventTypes::DataSocketConnected;
    void *currentTarget = nullptr;
    size_t targets = 0;
    for (const auto &[key, h] : events.state.handlers) {
      Q_UNUSED(h);
      if (key.first == type) {
        currentTarget = key.second;
        ++targets;
      }
    }
    if (targets != 1 || currentTarget == nullptr || factoryState->sockets.empty() || socket()->destroyed != 0 ||
        socket()->target == nullptr || events.state.retiredTargets.contains(currentTarget))
      throw std::logic_error("missing or ambiguous current synthetic connection owner");
    filterTarget = currentTarget;
    socket()->onDestroy = [this] { retireOwners(); };
    if (!events.dispatchEvent(Event(type, filterTarget)))
      throw std::logic_error("current synthetic connection event was not delivered");
  }
  void raw(const std::string &bytes)
  {
    socket()->input.push_back(bytes);
    events.dispatchEvent(Event(EventTypes::StreamInputReady, socket()->target));
    events.state.drain(events);
  }
  void send(const std::string &payload)
  {
    raw(packet(payload));
  }
  void awaitingHandshake()
  {
    begin();
    connected();
    send(hello());
  }
  void ready()
  {
    awaitingHandshake();
    send("DSOP" + options({}));
  }
  void drain()
  {
    events.state.drain(events);
  }
  void stop()
  {
    retireOwners();
    client->disconnect(nullptr);
    drain();
  }
  void destroyClient()
  {
    retireOwners();
    client.reset();
    drain();
  }
  size_t inputCalls() const
  {
    return std::count_if(screenState->calls.begin(), screenState->calls.end(), [](const Call &c) {
      return c.name == "keyDown" || c.name == "keyRepeat" || c.name == "keyUp" || c.name == "button" ||
             c.name == "move" || c.name == "relative" || c.name == "wheel" || c.name == "clipboard" ||
             c.name == "enter";
    });
  }
  bool idleHandlersOnly() const
  {
    return events.state.handlers.size() == 2 && events.state.has(EventTypes::ScreenSuspend, screenTarget) &&
           events.state.has(EventTypes::ScreenResume, screenTarget);
  }
  void assertReleased(bool wasReady, bool failure = true)
  {
    drain();
    QVERIFY(!client->isConnected());
    QVERIFY(!client->isConnecting());
    QCOMPARE(screenState->disabled, wasReady ? 1 : 0);
    QCOMPARE(socket()->destroyed, 1);
    QCOMPARE(socket()->binds, 0);
    QVERIFY(events.state.timers.empty());
    QVERIFY(idleHandlersOnly());
    QCOMPARE(events.state.timerCreates, events.state.timerDeletes);
    QCOMPARE(events.state.duplicateTimerDeletes, 0);
    if (failure) {
      QCOMPARE(events.state.failures.size(), size_t(1));
      QVERIFY(events.state.failures.back().retry);
      QCOMPARE(events.state.failures.back().target, screenTarget);
    }
  }
};

struct BodyRow
{
  std::string code, body, prelude;
};
inline std::vector<BodyRow> normalBodies()
{
  return {
      {"DKDN", integer(65, 2) + integer(0, 2) + integer(7, 2), {}},
      {"DKDL", integer(66, 2) + integer(0, 2) + integer(8, 2) + text(""), {}},
      {"DKRP", integer(67, 2) + integer(0, 2) + integer(2, 2) + integer(9, 2) + text(""), {}},
      {"DKUP", integer(65, 2) + integer(0, 2) + integer(7, 2), {}},
      {"DMDN", integer(1, 1), {}},
      {"DMUP", integer(1, 1), {}},
      {"DMMV", integer(12, 2) + integer(34, 2), {}},
      {"DMRM", integer(-3, 2) + integer(4, 2), {}},
      {"DMWM", integer(-120, 2) + integer(240, 2), {}},
      {"CINN", integer(12, 2) + integer(34, 2) + integer(7, 4) + integer(0, 2), {}},
      {"CCLP", integer(0, 1) + integer(7, 4), {}},
      {"CSEC", integer(1, 1), {}},
      {"DSOP", options({kOptionHeartbeat, 1000}), {}},
      {"SECN", text("xy"), {}},
      {"DCLP", clipboardBody(0, 7, 1, "15"), {}},
      {"DCLP", clipboardBody(0, 7, 2, syntheticClipboard()), "DCLP" + clipboardBody(0, 7, 1, "15")},
      {"DCLP", clipboardBody(0, 7, 3, ""),
       "DCLP" + clipboardBody(0, 7, 1, "15") + "DCLP" + clipboardBody(0, 7, 2, syntheticClipboard())}
  };
}
inline std::vector<BodyRow> handshakeBodies()
{
  return {
      {"DSOP", options({kOptionHeartbeat, 1000}), {}},
      {"LSYN", text("en"), {}},
      {"EICV", integer(1, 2) + integer(99, 2), {}}
  };
}

} // namespace client_test
