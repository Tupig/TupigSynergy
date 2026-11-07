/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers.
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/Server.h"

#include "AppUtil.h"
#include "IPlatformScreen.h"
#include "OptionTypes.h"
#include "PacketStreamFilter.h"
#include "ProtocolTypes.h"
#include "Screen.h"
#include "StreamChunker.h"
#include "SynergyException.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "ipc/CoreIpc.h"
#include "net/TCPSocket.h"
#include "server/ClientListener.h"
#include "server/ClientProxy.h"
#include "server/ClientProxyUnknown.h"
#include "server/PrimaryClient.h"
#include "synergy/protocol/FileChunk.h"
#include "synergy/protocol/FileTransferOutbound.h"

#include <QString>
#include <QStringList>

#ifdef _WIN32
#include <algorithm>
#include <array>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <optional>
#include <utility>

using namespace synergy::server;

void Server::handleKeyDownEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformScreen::KeyInfo *>(event.getData());
  auto lang = getCurrentLanguageCached();
  onKeyDown(info->m_key, info->m_mask, info->m_button, lang, info->m_screens.c_str());
}

void Server::handleKeyUpEvent(const Event &event)
{
  auto *info = static_cast<IPlatformScreen::KeyInfo *>(event.getData());
  onKeyUp(info->m_key, info->m_mask, info->m_button, info->m_screens.c_str());
}

void Server::handleKeyRepeatEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformScreen::KeyInfo *>(event.getData());
  auto lang = getCurrentLanguageCached();
  onKeyRepeat(info->m_key, info->m_mask, info->m_count, info->m_button, lang);
}

const std::string &Server::getCurrentLanguageCached()
{
  // querying the layout is expensive on some platforms (X11 opens a display
  // connection per call), so reuse the last result for a short interval
  constexpr double kCacheSeconds = 2.0;
  const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  if (m_languageCodeCache.empty() || now - m_languageCodeCachedAt > kCacheSeconds) {
    m_languageCodeCache = AppUtil::instance().getCurrentLanguageCode();
    m_languageCodeCachedAt = now;
  }
  return m_languageCodeCache;
}

void Server::handleButtonDownEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformScreen::ButtonInfo *>(event.getData());
  onMouseDown(info->m_button);
}

void Server::handleButtonUpEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformScreen::ButtonInfo *>(event.getData());
  onMouseUp(info->m_button);
}

void Server::handleMotionPrimaryEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformScreen::MotionInfo *>(event.getData());
  onMouseMovePrimary(info->m_x, info->m_y);
}

void Server::handleMotionSecondaryEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformScreen::MotionInfo *>(event.getData());
  onMouseMoveSecondary(info->m_x, info->m_y);
}

void Server::handleWheelEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformScreen::WheelInfo *>(event.getData());
  onMouseWheel(info->m_xDelta, info->m_yDelta);
}

void Server::handleSwitchToScreenEvent(const Event &event)
{
  const auto *info = static_cast<SwitchToScreenInfo *>(event.getData());

  ClientList::const_iterator index = m_clients.find(info->m_screen);
  if (index == m_clients.end()) {
    LOG_VERBOSE("screen \"%s\" not active", info->m_screen.c_str());
  } else {
    jumpToScreen(index->second);
  }
}

void Server::handleSwitchInDirectionEvent(const Event &event)
{
  const auto *info = static_cast<SwitchInDirectionInfo *>(event.getData());

  // jump to screen in chosen direction from center of this screen
  int32_t x = m_x;
  int32_t y = m_y;
  BaseClientProxy *newScreen = getNeighbor(m_active, info->m_direction, x, y);
  if (newScreen == nullptr) {
    LOG_VERBOSE("no neighbor %s", Config::dirName(info->m_direction));
  } else {
    jumpToScreen(newScreen);
  }
}

void Server::handleToggleScreenEvent(const Event &)
{
  // Get the list of connected screens in config order
  std::vector<std::string> screens;
  getClients(screens);

  if (screens.size() < 2) {
    LOG_ERR("not enough screens to toggle");
    return;
  }

  // Find the current active screen
  std::string currentScreen = getName(m_active);
  auto it = std::ranges::find(screens, currentScreen);
  if (it == screens.end()) {
    LOG_ERR("current screen not found in list");
    return;
  }

  // Find the next screen
  auto nextIt = it + 1;
  if (nextIt == screens.end()) {
    nextIt = screens.begin();
  }

  // Find the client for the next screen
  ClientList::const_iterator clientIt = m_clients.find(*nextIt);
  if (clientIt == m_clients.end()) {
    LOG_ERR("next screen not active");
    return;
  }

  jumpToScreen(clientIt->second);
}

void Server::handleKeyboardBroadcastEvent(const Event &event)
{
  const auto *info = static_cast<KeyboardBroadcastInfo *>(event.getData());

  // choose new state
  bool newState;
  switch (info->m_state) {
  default:
  case KeyboardBroadcastInfo::kOn:
    newState = true;
    break;

  case KeyboardBroadcastInfo::kOff:
    newState = false;
    break;

  case KeyboardBroadcastInfo::kToggle:
    newState = !m_keyboardBroadcasting;
    break;
  }

  // enter new state
  if (newState != m_keyboardBroadcasting || info->m_screens != m_keyboardBroadcastingScreens) {
    m_keyboardBroadcasting = newState;
    m_keyboardBroadcastingScreens = info->m_screens;
    LOG(
        (CLOG_DEBUG "keyboard broadcasting %s: %s", m_keyboardBroadcasting ? "on" : "off",
         m_keyboardBroadcastingScreens.c_str())
    );
  }
}

void Server::handleLockCursorToScreenEvent(const Event &event)
{
  const auto *info = static_cast<LockCursorToScreenInfo *>(event.getData());

  // choose new state
  bool newState;
  switch (info->m_state) {
  default:
  case LockCursorToScreenInfo::kOn:
    newState = true;
    break;

  case LockCursorToScreenInfo::kOff:
    newState = false;
    break;

  case LockCursorToScreenInfo::kToggle:
    newState = !m_lockedToScreen;
    break;
  }

  // enter new state
  if (newState != m_lockedToScreen) {
    m_lockedToScreen = newState;
    LOG_INFO("cursor %s current screen", m_lockedToScreen ? "locked to" : "unlocked from");

    m_primaryClient->reconfigure(getActivePrimarySides());
    if (!isLockedToScreenServer()) {
      stopRelativeMoves();
    }
  }
}

void Server::onKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang, const char *screens)
{
  LOG_VERBOSE("onKeyDown id=%d mask=0x%04x button=0x%04x lang=%s", id, mask, button, lang.c_str());
  assert(m_active != nullptr);

  // relay
  if (!m_keyboardBroadcasting && IKeyState::KeyInfo::isDefault(screens)) {
    m_active->keyDown(id, mask, button, lang);
  } else {
    if (!screens && m_keyboardBroadcasting) {
      screens = m_keyboardBroadcastingScreens.c_str();
      if (IKeyState::KeyInfo::isDefault(screens)) {
        screens = "*";
      }
    }
    for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
      if (IKeyState::KeyInfo::contains(screens, index->first)) {
        index->second->keyDown(id, mask, button, lang);
      }
    }
  }
}

void Server::onKeyUp(KeyID id, KeyModifierMask mask, KeyButton button, const char *screens)
{
  LOG_VERBOSE("onKeyUp id=%d mask=0x%04x button=0x%04x", id, mask, button);
  assert(m_active != nullptr);

  // relay
  if (!m_keyboardBroadcasting && IKeyState::KeyInfo::isDefault(screens)) {
    m_active->keyUp(id, mask, button);
  } else {
    if (!screens && m_keyboardBroadcasting) {
      screens = m_keyboardBroadcastingScreens.c_str();
      if (IKeyState::KeyInfo::isDefault(screens)) {
        screens = "*";
      }
    }
    for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
      if (IKeyState::KeyInfo::contains(screens, index->first)) {
        index->second->keyUp(id, mask, button);
      }
    }
  }
}

void Server::onKeyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang)
{
  LOG(
      (CLOG_VERBOSE "onKeyRepeat id=%d mask=0x%04x count=%d button=0x%04x lang=\"%s\"", id, mask, count, button,
       lang.c_str())
  );
  assert(m_active != nullptr);

  // relay
  m_active->keyRepeat(id, mask, count, button, lang);
}

void Server::onMouseDown(ButtonID id)
{
  LOG_VERBOSE("onMouseDown id=%d", id);
  assert(m_active != nullptr);

  // relay
  m_active->mouseDown(id);
}

void Server::onMouseUp(ButtonID id)
{
  LOG_VERBOSE("onMouseUp id=%d", id);
  assert(m_active != nullptr);

  // relay
  m_active->mouseUp(id);
}

bool Server::onMouseMovePrimary(int32_t x, int32_t y)
{
  LOG_VERBOSE("onMouseMovePrimary %d,%d", x, y);

  // mouse move on primary (server's) screen
  if (m_active != m_primaryClient) {
    // stale event -- we're actually on a secondary screen
    return false;
  }

  // save last delta
  m_xDelta2 = m_xDelta;
  m_yDelta2 = m_yDelta;

  // save current delta
  m_xDelta = x - m_x;
  m_yDelta = y - m_y;

  // save position
  m_x = x;
  m_y = y;

  // get screen shape
  int32_t ax;
  int32_t ay;
  int32_t aw;
  int32_t ah;
  m_active->getShape(ax, ay, aw, ah);
  int32_t zoneSize = getJumpZoneSize(m_active);

  // clamp position to screen
  int32_t xc = x;
  int32_t yc = y;
  if (xc < ax + zoneSize) {
    xc = ax;
  } else if (xc >= ax + aw - zoneSize) {
    xc = ax + aw - 1;
  }
  if (yc < ay + zoneSize) {
    yc = ay;
  } else if (yc >= ay + ah - zoneSize) {
    yc = ay + ah - 1;
  }

  // see if we should change screens
  // when the cursor is in a corner, there may be a screen either
  // horizontally or vertically.  check both directions.
  using enum Direction;
  auto dirh = NoDirection;
  auto dirv = NoDirection;
  int32_t xh = x;
  int32_t yv = y;
  if (x < ax + zoneSize) {
    xh -= zoneSize;
    dirh = Left;
  } else if (x >= ax + aw - zoneSize) {
    xh += zoneSize;
    dirh = Right;
  }
  if (y < ay + zoneSize) {
    yv -= zoneSize;
    dirv = Top;
  } else if (y >= ay + ah - zoneSize) {
    yv += zoneSize;
    dirv = Bottom;
  }
  if (dirh == NoDirection && dirv == NoDirection) {
    // still on local screen
    noSwitch(x, y);
    return false;
  }

  // check both horizontally and vertically
  std::array<Direction, 2> dirs = {dirh, dirv};
  std::array<int32_t, 2> xs = {xh, x};
  std::array<int32_t, 2> ys = {y, yv};
  for (int i = 0; i < 2; ++i) {
    Direction dir = dirs.at(i);
    if (dir == NoDirection) {
      continue;
    }
    x = xs.at(i);
    y = ys.at(i);
    // get jump destination
    BaseClientProxy *newScreen = mapToNeighbor(m_active, dir, x, y);

    // should we switch or not?
    if (isSwitchOkay(newScreen, dir, x, y, xc, yc)) {
      // switch screen
      switchScreen(newScreen, x, y, false);
      return true;
    }
  }

  return false;
}

void Server::onMouseMoveSecondary(int32_t dx, int32_t dy)
{
  LOG_VERBOSE("mouse move on secondary: %+d,%+d", dx, dy);

  // TODO: move this to client side and use a qt setting or cli arg instead of env var.
  // parse once and cache: getenv + stod on every mouse event is wasteful
  const static auto adjustMultiplier = []() -> std::optional<double> {
    constexpr auto kAdjustEnv = "SYNERGY_MOUSE_ADJUSTMENT";
    const char *envVal = std::getenv(kAdjustEnv);
    if (envVal == nullptr) {
      return std::nullopt;
    }
    try {
      return std::stod(envVal);
    } catch (const std::exception &e) {
      LOG_ERR("invalid %s value: %s", kAdjustEnv, e.what());
      return std::nullopt;
    }
  }();
  if (adjustMultiplier.has_value()) {
    const double multiplier = adjustMultiplier.value();
    dx = static_cast<int32_t>(std::round(dx * multiplier));
    dy = static_cast<int32_t>(std::round(dy * multiplier));
    LOG_VERBOSE("adjusted mouse x %.2f: %+d,%+d", multiplier, dx, dy);
  }

  // mouse move on secondary (client's) screen
  assert(m_active != nullptr);
  if (m_active == m_primaryClient) {
    // stale event -- we're actually on the primary screen
    return;
  }

  // if doing relative motion on secondary screens and we're locked
  // to the screen (which activates relative moves) then send a
  // relative mouse motion.  when we're doing this we pretend as if
  // the mouse isn't actually moving because we're expecting some
  // program on the secondary screen to warp the mouse on us, so we
  // have no idea where it really is.
  if (m_relativeMoves && isLockedToScreenServer()) {
    LOG_VERBOSE("relative move on %s by %d,%d", getName(m_active).c_str(), dx, dy);
    m_active->mouseRelativeMove(dx, dy);
    return;
  }

  // save old position
  const int32_t xOld = m_x;
  const int32_t yOld = m_y;

  // save last delta
  m_xDelta2 = m_xDelta;
  m_yDelta2 = m_yDelta;

  // save current delta
  m_xDelta = dx;
  m_yDelta = dy;

  // accumulate motion
  m_x += dx;
  m_y += dy;

  // get screen shape
  int32_t ax;
  int32_t ay;
  int32_t aw;
  int32_t ah;
  m_active->getShape(ax, ay, aw, ah);

  // find direction of neighbor and get the neighbor
  bool jump = true;
  BaseClientProxy *newScreen;
  do {
    // clamp position to screen
    int32_t xc = m_x;
    int32_t yc = m_y;
    if (xc < ax) {
      xc = ax;
    } else if (xc >= ax + aw) {
      xc = ax + aw - 1;
    }
    if (yc < ay) {
      yc = ay;
    } else if (yc >= ay + ah) {
      yc = ay + ah - 1;
    }

    Direction dir;
    using enum Direction;
    if (m_x < ax) {
      dir = Left;
    } else if (m_x > ax + aw - 1) {
      dir = Right;
    } else if (m_y < ay) {
      dir = Top;
    } else if (m_y > ay + ah - 1) {
      dir = Bottom;
    } else {
      // we haven't left the screen
      newScreen = m_active;
      jump = false;

      // if waiting and mouse is not on the border we're waiting
      // on then stop waiting.  also if it's not on the border
      // then arm the double tap.
      if (m_switchScreen != nullptr) {
        bool clearWait;
        int32_t zoneSize = m_primaryClient->getJumpZoneSize();
        switch (m_switchDir) {
        case Left:
          clearWait = (m_x >= ax + zoneSize);
          break;

        case Right:
          clearWait = (m_x <= ax + aw - 1 - zoneSize);
          break;

        case Top:
          clearWait = (m_y >= ay + zoneSize);
          break;

        case Bottom:
          clearWait = (m_y <= ay + ah - 1 + zoneSize);
          break;

        default:
          clearWait = false;
          break;
        }
        if (clearWait) {
          // still on local screen
          noSwitch(m_x, m_y);
        }
      }

      // skip rest of block
      break;
    }

    // try to switch screen.  get the neighbor.
    newScreen = mapToNeighbor(m_active, dir, m_x, m_y);

    // see if we should switch
    if (!isSwitchOkay(newScreen, dir, m_x, m_y, xc, yc)) {
      newScreen = m_active;
      jump = false;
    }
  } while (false);

  if (jump) {
    int32_t newX = m_x;
    int32_t newY = m_y;

    // switch screens
    switchScreen(newScreen, newX, newY, false);
  } else {
    // same screen.  clamp mouse to edge.
    m_x = xOld + dx;
    m_y = yOld + dy;
    // these diagnostics run for every motion event; only build the client
    // name when verbose logging is actually enabled
    const bool verboseLogs = CLOG->getFilter() >= LogLevel::Level::Verbose;
    if (m_x < ax) {
      m_x = ax;
      if (verboseLogs) {
        LOG_VERBOSE("clamp to left of \"%s\"", getName(m_active).c_str());
      }
    } else if (m_x > ax + aw - 1) {
      m_x = ax + aw - 1;
      if (verboseLogs) {
        LOG_VERBOSE("clamp to right of \"%s\"", getName(m_active).c_str());
      }
    }
    if (m_y < ay) {
      m_y = ay;
      if (verboseLogs) {
        LOG_VERBOSE("clamp to top of \"%s\"", getName(m_active).c_str());
      }
    } else if (m_y > ay + ah - 1) {
      m_y = ay + ah - 1;
      if (verboseLogs) {
        LOG_VERBOSE("clamp to bottom of \"%s\"", getName(m_active).c_str());
      }
    }

    // warp cursor if it moved.
    if (m_x != xOld || m_y != yOld) {
      if (verboseLogs) {
        LOG_VERBOSE("move on %s to %d,%d", getName(m_active).c_str(), m_x, m_y);
      }
      m_active->mouseMove(m_x, m_y);
    }
  }
}

void Server::onMouseWheel(int32_t xDelta, int32_t yDelta)
{
  LOG_VERBOSE("onMouseWheel %+d,%+d", xDelta, yDelta);
  assert(m_active != nullptr);

  // relay
  m_active->mouseWheel(xDelta, yDelta);
}
