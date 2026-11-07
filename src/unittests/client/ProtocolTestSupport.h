/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "../synergy/MockEventQueue.h"
#include "../synergy/TestStreams.h"
#include "AppUtil.h"
#include "IPlatformScreen.h"
#include "base/EventQueueTimer.h"
#include "client/Client.h"
#include "common/Settings.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <utility>

namespace protocol_test {

// Recorded, explicitly dispatched events avoid wall-clock waits and OS input.
class EventQueue : public MockEventQueue
{
public:
  ~EventQueue() override
  {
    for (auto &event : pending)
      release(event);
    for (auto *timer : timers)
      delete timer;
  }

  void addHandler(EventTypes type, void *target, const EventHandler &handler) override
  {
    handlers[{type, target}] = handler;
  }
  void removeHandler(EventTypes type, void *target) override
  {
    handlers.erase({type, target});
  }
  void removeHandlers(void *target) override
  {
    std::erase_if(handlers, [target](const auto &entry) { return entry.first.second == target; });
  }
  bool dispatchEvent(const Event &event) override
  {
    auto it = handlers.find({event.getType(), event.getTarget()});
    if (it == handlers.end())
      it = handlers.find({EventTypes::Unknown, event.getTarget()});
    if (it == handlers.end())
      return false;
    // A callback may replace/remove itself or destroy its owner.
    auto handler = it->second;
    handler(event);
    return true;
  }
  void addEvent(Event &&event) override
  {
    ++counts[event.getType()];
    if ((event.getFlags() & Event::EventFlags::DeliverImmediately) != 0) {
      dispatchEvent(event);
      release(event);
    } else {
      pending.push_back(std::move(event));
    }
  }
  void drain()
  {
    while (!pending.empty()) {
      auto event = std::move(pending.front());
      pending.pop_front();
      dispatchEvent(event);
      release(event);
    }
  }
  EventQueueTimer *newOneShotTimer(double, void *) override
  {
    auto *timer = new EventQueueTimer;
    timers.insert(timer);
    return timer;
  }
  EventQueueTimer *newTimer(double duration, void *target) override
  {
    return newOneShotTimer(duration, target);
  }
  void deleteTimer(EventQueueTimer *timer) override
  {
    if (timers.erase(timer))
      delete timer;
  }
  int count(EventTypes type) const
  {
    const auto it = counts.find(type);
    return it == counts.end() ? 0 : it->second;
  }
  bool hasHandler(EventTypes type, void *target) const
  {
    return handlers.contains({type, target});
  }
  size_t handlerCount() const
  {
    return handlers.size();
  }
  size_t timerCount() const
  {
    return timers.size();
  }
  EventQueueTimer *firstTimer() const
  {
    return timers.empty() ? nullptr : *timers.begin();
  }

private:
  static void release(const Event &event)
  {
    if (event.getType() == EventTypes::ClientConnectionFailed || event.getType() == EventTypes::ClientConnectionRefused)
      delete static_cast<Client::FailInfo *>(event.getData());
    else
      Event::deleteData(event);
  }
  std::map<std::pair<EventTypes, void *>, EventHandler> handlers;
  std::map<EventTypes, int> counts;
  std::deque<Event> pending;
  std::set<EventQueueTimer *> timers;
};

class Stream : public MemoryStream
{
public:
  explicit Stream(std::shared_ptr<int> destructionCount = {}) : destroyed(std::move(destructionCount))
  {
  }
  ~Stream() override
  {
    if (destroyed)
      ++*destroyed;
  }
  void write(const void *data, uint32_t size) override
  {
    written.append(static_cast<const char *>(data), size);
  }
  std::string written;

private:
  std::shared_ptr<int> destroyed;
};

class App : public AppUtil
{
public:
  int run() override
  {
    return 0;
  }
  void startNode() override
  {
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

// Must precede Screen construction: never read a developer's settings/commands.
class PrivateSettings
{
public:
  PrivateSettings()
  {
    if (!directory.isValid())
      qFatal("cannot create isolated test settings directory");
    qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
    qputenv("XDG_STATE_HOME", directory.path().toUtf8());
    // Settings::setSettingsFile first constructs the singleton. Seed its
    // initial path too, before it can inspect or upgrade a real user profile.
#ifdef Q_OS_WIN
    markerPath = Settings::portableSettingsFile();
    if (!QDir().mkpath(QFileInfo(markerPath).absolutePath()))
      qFatal("cannot create isolated portable settings directory");
    QFile marker(markerPath);
    if (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly))
      qFatal("refusing to overwrite existing portable settings");
    marker.close();
    seed(markerPath);
#else
    seed(directory.filePath(QStringLiteral("%1/%1.conf").arg(kAppName)));
#endif
    const auto settingsPath = directory.filePath("settings.ini");
    const auto statePath = directory.filePath("state.ini");
    seed(settingsPath);
    Settings::setStateFile(statePath);
    Settings::setSettingsFile(settingsPath);
    if (QFileInfo(Settings::settingsFile()).absoluteFilePath() != QFileInfo(settingsPath).absoluteFilePath())
      qFatal("settings escaped the isolated test directory");
    if (Settings::value(Settings::Core::ComputerName).toString() != QStringLiteral("synthetic-client"))
      qFatal("unexpected non-synthetic test computer name");
    Settings::setValue(Settings::Core::EnableEnterCommand, false);
    Settings::setValue(Settings::Core::EnableExitCommand, false);
    // Dispose the replaced initial QSettings before removing its marker.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }

  ~PrivateSettings()
  {
    Settings::save(false);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
#ifdef Q_OS_WIN
    QFile::remove(markerPath); // only the file exclusively created above
#endif
    restore("XDG_CONFIG_HOME", oldConfig, hadConfig);
    restore("XDG_STATE_HOME", oldState, hadState);
  }

private:
  static void seed(const QString &path)
  {
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
      qFatal("cannot create private settings parent directory");
    QSettings initial(path, QSettings::IniFormat);
    initial.setValue(Settings::Core::ComputerName, "synthetic-client");
    initial.sync();
    if (initial.status() != QSettings::NoError)
      qFatal("cannot write synthetic test settings");
  }
  static void restore(const char *name, const QByteArray &value, bool existed)
  {
    if (existed)
      qputenv(name, value);
    else
      qunsetenv(name);
  }
  QTemporaryDir directory;
  const QByteArray oldConfig = qgetenv("XDG_CONFIG_HOME");
  const QByteArray oldState = qgetenv("XDG_STATE_HOME");
  const bool hadConfig = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
  const bool hadState = qEnvironmentVariableIsSet("XDG_STATE_HOME");
#ifdef Q_OS_WIN
  QString markerPath;
#endif
};

class PlatformScreen : public IPlatformScreen
{
public:
  explicit PlatformScreen(const IEventQueue *events, bool primary = true) : IPlatformScreen(events), primary(primary)
  {
  }
  int enables = 0, disables = 0, enters = 0, leaves = 0;
  bool allowLeave = true, mousePressed = false;
  int32_t cursorX = 400, cursorY = 300;
  int keyDowns = 0, keyUps = 0, keyRepeats = 0, mouseMoves = 0, mouseButtons = 0;
  mutable int relativeMoves = 0, mouseWheels = 0;
  int clipboardSets = 0, screensavers = 0, optionsCalls = 0;
  int inputCalls() const
  {
    return keyDowns + keyUps + keyRepeats + mouseMoves + mouseButtons + relativeMoves + mouseWheels;
  }
  KeyModifierMask modifiers = 0;
  void enable() override
  {
    ++enables;
  }
  void disable() override
  {
    ++disables;
  }
  void enter() override
  {
    ++enters;
  }
  bool canLeave() override
  {
    return allowLeave;
  }
  void leave() override
  {
    ++leaves;
  }
  bool setClipboard(ClipboardID, const IClipboard *) override
  {
    ++clipboardSets;
    return true;
  }
  bool getClipboard(ClipboardID, IClipboard *) const override
  {
    return false;
  }
  void checkClipboards() override
  {
  }
  void openScreensaver(bool) override
  {
  }
  void closeScreensaver() override
  {
  }
  void screensaver(bool) override
  {
    ++screensavers;
  }
  void resetOptions() override
  {
  }
  void setOptions(const OptionsList &) override
  {
    ++optionsCalls;
  }
  void setSequenceNumber(uint32_t) override
  {
  }
  std::string getSecureInputApp() const override
  {
    return {};
  }
  bool isPrimary() const override
  {
    return primary;
  }
  void *getEventTarget() const override
  {
    return const_cast<PlatformScreen *>(this);
  }
  void getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const override
  {
    x = y = 0;
    w = 800;
    h = 600;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = cursorX;
    y = cursorY;
  }
  void reconfigure(uint32_t sides) override
  {
    sidesMask = sides;
  }
  uint32_t activeSides() override
  {
    return sidesMask;
  }
  void warpCursor(int32_t x, int32_t y) override
  {
    cursorX = x;
    cursorY = y;
  }
  uint32_t registerHotKey(KeyID, KeyModifierMask) override
  {
    return 1;
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
    return 1;
  }
  bool isAnyMouseButtonDown(uint32_t &button) const override
  {
    button = 1;
    return mousePressed;
  }
  void getCursorCenter(int32_t &x, int32_t &y) const override
  {
    x = 400;
    y = 300;
  }
  void fakeMouseButton(ButtonID, bool) override
  {
    ++mouseButtons;
  }
  void fakeMouseMove(int32_t x, int32_t y) override
  {
    ++mouseMoves;
    cursorX = x;
    cursorY = y;
  }
  void fakeMouseRelativeMove(int32_t, int32_t) const override
  {
    ++relativeMoves;
  }
  void fakeMouseWheel(ScrollDelta) const override
  {
    ++mouseWheels;
  }
  void updateKeyMap() override
  {
  }
  void updateKeyState() override
  {
  }
  void setHalfDuplexMask(KeyModifierMask) override
  {
  }
  void fakeKeyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
    ++keyDowns;
  }
  bool fakeKeyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
    ++keyRepeats;
    return true;
  }
  bool fakeKeyUp(KeyButton) override
  {
    ++keyUps;
    return true;
  }
  void fakeAllKeysUp() override
  {
  }
  bool fakeCtrlAltDel() override
  {
    return false;
  }
  bool isKeyDown(KeyButton) const override
  {
    return false;
  }
  KeyModifierMask getActiveModifiers() const override
  {
    return modifiers;
  }
  KeyModifierMask pollActiveModifiers() const override
  {
    return modifiers;
  }
  int32_t pollActiveGroup() const override
  {
    return 0;
  }
  void pollPressedKeys(KeyButtonSet &) const override
  {
  }

protected:
  void handleSystemEvent(const Event &) override
  {
  }

private:
  bool primary;
  uint32_t sidesMask = 0;
};

inline std::string integers16(std::initializer_list<int> values)
{
  std::string bytes;
  for (const int value : values) {
    bytes += static_cast<char>((value >> 8) & 0xff);
    bytes += static_cast<char>(value & 0xff);
  }
  return bytes;
}

inline std::string integer32(uint32_t value)
{
  std::string bytes;
  for (int shift = 24; shift >= 0; shift -= 8)
    bytes += static_cast<char>((value >> shift) & 0xff);
  return bytes;
}

inline std::string packet(const std::string &payload)
{
  return integer32(static_cast<uint32_t>(payload.size())) + payload;
}

} // namespace protocol_test
