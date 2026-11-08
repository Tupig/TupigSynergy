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

//
// Server
//

bool Server::isLockedToScreenServer() const
{
  // locked if scroll-lock is toggled on
  return m_lockedToScreen;
}

bool Server::isLockedToScreen() const
{
  if (m_disableLockToScreen) {
    return false;
  }

  // locked if we say we're locked
  if (isLockedToScreenServer()) {
    if (!m_defaultLockToScreenState) {
      LOG_INFO("cursor is locked to screen, check scroll lock key");
    }
    return true;
  }

  // locked if primary says we're locked
  if (m_primaryClient->isLockedToScreen()) {
    return true;
  }

  // not locked
  return false;
}

int32_t Server::getJumpZoneSize(const BaseClientProxy *client) const
{
  if (client == m_primaryClient) {
    return m_primaryClient->getJumpZoneSize();
  } else {
    return 0;
  }
}

void Server::switchScreen(BaseClientProxy *dst, int32_t x, int32_t y, bool forScreensaver)
{
  assert(dst != nullptr);

  int32_t dx;
  int32_t dy;
  int32_t dw;
  int32_t dh;
  dst->getShape(dx, dy, dw, dh);

  // any of these conditions seem to trigger when the portal permission dialog
  // is visible on wayland. this was previously an assert, but that's pretty
  // annoying since it makes the mouse unusable on the server and you'll have to
  // ssh into your machine to kill it. better to just log a warning.
  if (x < dx) {
    LOG_WARN(
        "on switch, x (%d) is less than the left boundary dx (%d)", //
        x, dx
    );
  }
  if (y < dy) {
    LOG_WARN(
        "on switch, y (%d) is less than the top boundary dy (%d)", //
        y, dy
    );
  }
  if (x >= dx + dw) {
    LOG_WARN(
        "on switch, x (%d) exceeds the right boundary (dx + width = %d)", //
        x, dx + dw
    );
  }
  if (y >= dy + dh) {
    LOG_WARN(
        "on switch, y (%d) exceeds the bottom boundary (dy + height = %d)", //
        y, dy + dh
    );
  }

  assert(m_active != nullptr);

  LOG_INFO("switch from \"%s\" to \"%s\" at %d,%d", getName(m_active).c_str(), getName(dst).c_str(), x, y);

  // stop waiting to switch
  stopSwitch();

  // record new position
  m_x = x;
  m_y = y;
  m_xDelta = 0;
  m_yDelta = 0;
  m_xDelta2 = 0;
  m_yDelta2 = 0;

  // wrapping means leaving the active screen and entering it again.
  // since that's a waste of time we skip that and just warp the
  // mouse.
  if (m_active != dst) {
    // Capture any in-progress file drag before leave() warps the cursor away
    // from the drop target window (Windows). Other platforms return empty.
    std::vector<std::string> outboundFiles;
    if (m_active == m_primaryClient) {
      outboundFiles = m_screen->takeDraggingPaths();
    }

    // leave active screen
    if (!m_active->leave()) {
      // cannot leave screen
      LOG_WARN("can't leave screen");
      return;
    }

    if (!outboundFiles.empty()) {
      const auto bytes = sendLocalFiles(outboundFiles);
      LOG_INFO(
          "sent drag-and-drop transfer: %zu path(s), %llu byte(s)", outboundFiles.size(),
          static_cast<unsigned long long>(bytes)
      );
    }

    // update the primary client's clipboards if we're leaving the
    // primary screen.
    if (m_active == m_primaryClient && m_enableClipboard) {
      for (ClipboardID id = 0; id < kClipboardEnd; ++id) {
        const ClipboardInfo &clipboard = m_clipboards[id];
        if (clipboard.m_clipboardOwner == getName(m_primaryClient)) {
          onClipboardChanged(m_primaryClient, id, clipboard.m_clipboardSeqNum);
        }
      }
    }

#if defined(__APPLE__)
    if (dst != m_primaryClient) {
      std::string secureInputApplication = m_primaryClient->getSecureInputApp();
      if (secureInputApplication != "") {
        // display notification on the server
        m_primaryClient->secureInputNotification(secureInputApplication);
        // display notification on the client
        dst->secureInputNotification(secureInputApplication);
      }
    }
#endif

    // cut over
    m_active = dst;

    // increment enter sequence number
    ++m_seqNum;

    // enter new screen
    m_active->enter(x, y, m_seqNum, m_primaryClient->getToggleMask(), forScreensaver);

    if (m_enableClipboard) {
      // send the clipboard data to new active screen
      for (ClipboardID id = 0; id < kClipboardEnd; ++id) {
        // Hackity hackity hack
        if (m_clipboards[id].m_clipboard.marshall().size() > (m_maximumClipboardSize * 1024)) {
          continue;
        }
        m_active->setClipboard(id, &m_clipboards[id].m_clipboard);
      }
    }

    auto *info = new Server::SwitchToScreenInfo(m_active->getName());
    m_events->addEvent(Event(EventTypes::ServerScreenSwitched, this, info));
  } else {
    m_active->mouseMove(x, y);
  }
}

void Server::jumpToScreen(BaseClientProxy *newScreen)
{
  assert(newScreen != nullptr);

  // record the current cursor position on the active screen
  m_active->setJumpCursorPos(m_x, m_y);

  // get the last cursor position on the target screen
  int32_t x;
  int32_t y;
  newScreen->getJumpCursorPos(x, y);

  switchScreen(newScreen, x, y, false);
}

float Server::mapToFraction(const BaseClientProxy *client, Direction dir, int32_t x, int32_t y) const
{
  int32_t sx;
  int32_t sy;
  int32_t sw;
  int32_t sh;
  client->getShape(sx, sy, sw, sh);
  switch (dir) {
    using enum Direction;
  case Left:
  case Right:
    return (y - sy + 0.5f) / static_cast<float>(sh);

  case Top:
  case Bottom:
    return (x - sx + 0.5f) / static_cast<float>(sw);

  case NoDirection:
    throw InvalidProtocolException();
  }
  return 0.0f;
}

void Server::mapToPixel(const BaseClientProxy *client, Direction dir, float f, int32_t &x, int32_t &y) const
{
  int32_t sx;
  int32_t sy;
  int32_t sw;
  int32_t sh;
  client->getShape(sx, sy, sw, sh);
  switch (dir) {
    using enum Direction;
  case Left:
  case Right:
    y = static_cast<int32_t>(f * sh) + sy;
    break;

  case Top:
  case Bottom:
    x = static_cast<int32_t>(f * sw) + sx;
    break;

  case NoDirection:
    throw InvalidProtocolException();
  }
}

bool Server::hasAnyNeighbor(const BaseClientProxy *client, Direction dir) const
{
  assert(client != nullptr);

  return m_config->hasNeighbor(getName(client), dir);
}

BaseClientProxy *Server::getNeighbor(const BaseClientProxy *src, Direction dir, int32_t &x, int32_t &y) const
{
  // note -- must be locked on entry

  assert(src != nullptr);

  // get source screen name
  std::string srcName = getName(src);
  assert(!srcName.empty());
  LOG_VERBOSE("find neighbor on %s of \"%s\"", Config::dirName(dir), srcName.c_str());

  // convert position to fraction
  float t = mapToFraction(src, dir, x, y);

  // search for the closest neighbor that exists in direction dir
  float tTmp;
  for (;;) {
    std::string dstName(m_config->getNeighbor(srcName, dir, t, &tTmp));

    // if nothing in that direction then return nullptr. if the
    // destination is the source then we can make no more
    // progress in this direction.  since we haven't found a
    // connected neighbor we return nullptr.
    if (dstName.empty()) {
      LOG_VERBOSE("no neighbor on %s of \"%s\"", Config::dirName(dir), srcName.c_str());
      return nullptr;
    }

    // look up neighbor cell.  if the screen is connected and
    // ready then we can stop.
    if (ClientList::const_iterator index = m_clients.find(dstName); index != m_clients.end()) {
      LOG_VERBOSE("\"%s\" is on %s of \"%s\" at %f", dstName.c_str(), Config::dirName(dir), srcName.c_str(), t);
      mapToPixel(index->second, dir, tTmp, x, y);
      return index->second;
    }

    // skip over unconnected screen
    LOG_VERBOSE("ignored \"%s\" on %s of \"%s\"", dstName.c_str(), Config::dirName(dir), srcName.c_str());
    srcName = dstName;

    // use position on skipped screen
    t = tTmp;
  }
}

BaseClientProxy *Server::mapToNeighbor(BaseClientProxy *src, Direction srcSide, int32_t &x, int32_t &y) const
{
  // note -- must be locked on entry

  assert(src != nullptr);

  // get the first neighbor
  BaseClientProxy *dst = getNeighbor(src, srcSide, x, y);
  if (dst == nullptr) {
    return nullptr;
  }

  // get the source screen's size
  int32_t dx;
  int32_t dy;
  int32_t dw;
  int32_t dh;
  BaseClientProxy *lastGoodScreen = src;
  lastGoodScreen->getShape(dx, dy, dw, dh);

  // find destination screen, adjusting x or y (but not both).  the
  // searches are done in a sort of canonical screen space where
  // the upper-left corner is 0,0 for each screen.  we adjust from
  // actual to canonical position on entry to and from canonical to
  // actual on exit from the search.
  switch (srcSide) {
    using enum Direction;
  case Left:
    x -= dx;
    while (dst != nullptr) {
      lastGoodScreen = dst;
      lastGoodScreen->getShape(dx, dy, dw, dh);
      x += dw;
      if (x >= 0) {
        break;
      }
      LOG_VERBOSE("skipping over screen %s", getName(dst).c_str());
      dst = getNeighbor(lastGoodScreen, srcSide, x, y);
    }
    assert(lastGoodScreen != nullptr);
    x += dx;
    break;

  case Right:
    x -= dx;
    while (dst != nullptr) {
      x -= dw;
      lastGoodScreen = dst;
      lastGoodScreen->getShape(dx, dy, dw, dh);
      if (x < dw) {
        break;
      }
      LOG_VERBOSE("skipping over screen %s", getName(dst).c_str());
      dst = getNeighbor(lastGoodScreen, srcSide, x, y);
    }
    assert(lastGoodScreen != nullptr);
    x += dx;
    break;

  case Top:
    y -= dy;
    while (dst != nullptr) {
      lastGoodScreen = dst;
      lastGoodScreen->getShape(dx, dy, dw, dh);
      y += dh;
      if (y >= 0) {
        break;
      }
      LOG_VERBOSE("skipping over screen %s", getName(dst).c_str());
      dst = getNeighbor(lastGoodScreen, srcSide, x, y);
    }
    assert(lastGoodScreen != nullptr);
    y += dy;
    break;

  case Bottom:
    y -= dy;
    while (dst != nullptr) {
      y -= dh;
      lastGoodScreen = dst;
      lastGoodScreen->getShape(dx, dy, dw, dh);
      if (y < dh) {
        break;
      }
      LOG_VERBOSE("skipping over screen %s", getName(dst).c_str());
      dst = getNeighbor(lastGoodScreen, srcSide, x, y);
    }
    assert(lastGoodScreen != nullptr);
    y += dy;
    break;

  case NoDirection:
    throw InvalidProtocolException();
  }

  // save destination screen
  assert(lastGoodScreen != nullptr);
  dst = lastGoodScreen;

  // if entering primary screen then be sure to move in far enough
  // to avoid the jump zone.  if entering a side that doesn't have
  // a neighbor (i.e. an asymmetrical side) then we don't need to
  // move inwards because that side can't provoke a jump.
  avoidJumpZone(dst, srcSide, x, y);

  return dst;
}

void Server::avoidJumpZone(const BaseClientProxy *dst, Direction dir, int32_t &x, int32_t &y) const
{
  // we only need to avoid jump zones on the primary screen
  if (dst != m_primaryClient) {
    return;
  }

  const std::string dstName(getName(dst));
  int32_t dx;
  int32_t dy;
  int32_t dw;
  int32_t dh;
  dst->getShape(dx, dy, dw, dh);
  float t = mapToFraction(dst, dir, x, y);
  int32_t z = getJumpZoneSize(dst);

  // move in far enough to avoid the jump zone.  if entering a side
  // that doesn't have a neighbor (i.e. an asymmetrical side) then we
  // don't need to move inwards because that side can't provoke a jump.
  switch (dir) {
    using enum Direction;
  case Left:
    if (!m_config->getNeighbor(dstName, Right, t, nullptr).empty() && x > dx + dw - 1 - z)
      x = dx + dw - 1 - z;
    break;

  case Right:
    if (!m_config->getNeighbor(dstName, Left, t, nullptr).empty() && x < dx + z)
      x = dx + z;
    break;

  case Top:
    if (!m_config->getNeighbor(dstName, Bottom, t, nullptr).empty() && y > dy + dh - 1 - z)
      y = dy + dh - 1 - z;
    break;

  case Bottom:
    if (!m_config->getNeighbor(dstName, Top, t, nullptr).empty() && y < dy + z)
      y = dy + z;
    break;

  case NoDirection:
    throw InvalidProtocolException();
  }
}

bool Server::isSwitchOkay(
    BaseClientProxy *newScreen, Direction dir, int32_t x, int32_t y, int32_t xActive, int32_t yActive
)
{
  LOG_VERBOSE("try to leave \"%s\" on %s", getName(m_active).c_str(), Config::dirName(dir));

  // is there a neighbor?
  if (newScreen == nullptr) {
    // there's no neighbor.  we don't want to switch and we don't
    // want to try to switch later.
    LOG_VERBOSE("no neighbor %s", Config::dirName(dir));
    stopSwitch();
    return false;
  }

  // should we switch or not?
  bool preventSwitch = false;
  bool allowSwitch = false;

  // note if the switch direction has changed.  save the new
  // direction and screen if so.
  bool isNewDirection = (dir != m_switchDir);
  if (isNewDirection || m_switchScreen == nullptr) {
    m_switchDir = dir;
    m_switchScreen = newScreen;
  }

  // is this a double tap and do we care?
  if (!allowSwitch && m_switchTwoTapDelay > 0.0) {
    if (isNewDirection || !isSwitchTwoTapStarted() || !shouldSwitchTwoTap()) {
      // tapping a different or new edge or second tap not
      // fast enough.  prepare for second tap.
      preventSwitch = true;
      startSwitchTwoTap();
    } else {
      // got second tap
      allowSwitch = true;
    }
  }

  // if waiting before a switch then prepare to switch later
  if (!allowSwitch && m_switchWaitDelay > 0.0) {
    if (isNewDirection || !isSwitchWaitStarted()) {
      startSwitchWait(x, y);
    }
    preventSwitch = true;
  }

  // are we in a locked corner?  first check if screen has the option set
  // and, if not, check the global options.
  const Config::ScreenOptions *options = m_config->getOptions(getName(m_active));
  if (options == nullptr || !options->contains(kOptionScreenSwitchCorners)) {
    options = m_config->getOptions("");
  }
  if (options != nullptr && options->contains(kOptionScreenSwitchCorners)) {
    // get corner mask and size
    Config::ScreenOptions::const_iterator i = options->find(kOptionScreenSwitchCorners);
    auto corners = static_cast<uint32_t>(i->second);
    i = options->find(kOptionScreenSwitchCornerSize);
    int32_t size = 0;
    if (i != options->end()) {
      size = i->second;
    }

    // see if we're in a locked corner
    if ((getCorner(m_active, xActive, yActive, size) & corners) != 0) {
      // yep, no switching
      LOG_VERBOSE("locked in corner");
      preventSwitch = true;
      stopSwitch();
    }
  }

  // ignore if mouse is locked to screen and don't try to switch later
  if (!preventSwitch && isLockedToScreen()) {
    LOG_VERBOSE("locked to screen");
    preventSwitch = true;
    stopSwitch();
  }

  // check for optional needed modifiers
  if (KeyModifierMask mods = this->m_primaryClient->getToggleMask();
      !preventSwitch && ((this->m_switchNeedsShift && ((mods & KeyModifierShift) != KeyModifierShift)) ||
                         (this->m_switchNeedsControl && ((mods & KeyModifierControl) != KeyModifierControl)) ||
                         (this->m_switchNeedsAlt && ((mods & KeyModifierAlt) != KeyModifierAlt)))) {
    LOG_VERBOSE("need modifiers to switch");
    preventSwitch = true;
    stopSwitch();
  }

  return !preventSwitch;
}

void Server::noSwitch(int32_t x, int32_t y)
{
  armSwitchTwoTap(x, y);
  stopSwitchWait();
}

void Server::stopSwitch()
{
  if (m_switchScreen != nullptr) {
    m_switchScreen = nullptr;
    m_switchDir = Direction::NoDirection;
    stopSwitchTwoTap();
    stopSwitchWait();
  }
}

void Server::startSwitchTwoTap()
{
  m_switchTwoTapEngaged = true;
  m_switchTwoTapArmed = false;
  m_switchTwoTapTimer.reset();
  LOG_VERBOSE("waiting for second tap");
}

void Server::armSwitchTwoTap(int32_t x, int32_t y)
{
  if (m_switchTwoTapEngaged) {
    if (m_switchTwoTapTimer.getTime() > m_switchTwoTapDelay) {
      // second tap took too long.  disengage.
      stopSwitchTwoTap();
    } else if (!m_switchTwoTapArmed) {
      // still time for a double tap.  see if we left the tap
      // zone and, if so, arm the two tap.
      int32_t ax;
      int32_t ay;
      int32_t aw;
      int32_t ah;
      m_active->getShape(ax, ay, aw, ah);
      int32_t tapZone = m_primaryClient->getJumpZoneSize();
      if (tapZone < m_switchTwoTapZone) {
        tapZone = m_switchTwoTapZone;
      }
      if (x >= ax + tapZone && x < ax + aw - tapZone && y >= ay + tapZone && y < ay + ah - tapZone) {
        // win32 can generate bogus mouse events that appear to
        // move in the opposite direction that the mouse actually
        // moved.  try to ignore that crap here.
        switch (m_switchDir) {
          using enum Direction;
        case Left:
          m_switchTwoTapArmed = (m_xDelta > 0 && m_xDelta2 > 0);
          break;

        case Right:
          m_switchTwoTapArmed = (m_xDelta < 0 && m_xDelta2 < 0);
          break;

        case Top:
          m_switchTwoTapArmed = (m_yDelta > 0 && m_yDelta2 > 0);
          break;

        case Bottom:
          m_switchTwoTapArmed = (m_yDelta < 0 && m_yDelta2 < 0);
          break;

        default:
          break;
        }
      }
    }
  }
}

void Server::stopSwitchTwoTap()
{
  m_switchTwoTapEngaged = false;
  m_switchTwoTapArmed = false;
}

bool Server::isSwitchTwoTapStarted() const
{
  return m_switchTwoTapEngaged;
}

bool Server::shouldSwitchTwoTap() const
{
  // this is the second tap if two-tap is armed and this tap
  // came fast enough
  return (m_switchTwoTapArmed && m_switchTwoTapTimer.getTime() <= m_switchTwoTapDelay);
}

void Server::startSwitchWait(int32_t x, int32_t y)
{
  stopSwitchWait();
  m_switchWaitX = x;
  m_switchWaitY = y;
  m_switchWaitTimer = m_events->newOneShotTimer(m_switchWaitDelay, this);
  LOG_VERBOSE("waiting to switch");
}

void Server::stopSwitchWait()
{
  if (m_switchWaitTimer != nullptr) {
    m_events->deleteTimer(m_switchWaitTimer);
    m_switchWaitTimer = nullptr;
  }
}

bool Server::isSwitchWaitStarted() const
{
  return (m_switchWaitTimer != nullptr);
}

uint32_t Server::getCorner(const BaseClientProxy *client, int32_t x, int32_t y, int32_t size) const
{
  assert(client != nullptr);

  // get client screen shape
  int32_t ax;
  int32_t ay;
  int32_t aw;
  int32_t ah;
  client->getShape(ax, ay, aw, ah);

  // check for x,y on the left or right
  int32_t xSide;
  if (x <= ax) {
    xSide = -1;
  } else if (x >= ax + aw - 1) {
    xSide = 1;
  } else {
    xSide = 0;
  }

  // check for x,y on the top or bottom
  int32_t ySide;
  if (y <= ay) {
    ySide = -1;
  } else if (y >= ay + ah - 1) {
    ySide = 1;
  } else {
    ySide = 0;
  }

  // if against the left or right then check if y is within size
  if (xSide != 0) {
    if (y < ay + size) {
      return (xSide < 0) ? s_topLeftCornerMask : s_topRightCornerMask;
    } else if (y >= ay + ah - size) {
      return (xSide < 0) ? s_bottomLeftCornerMask : s_bottomRightCornerMask;
    }
  }

  // if against the left or right then check if y is within size
  if (ySide != 0) {
    if (x < ax + size) {
      return (ySide < 0) ? s_topLeftCornerMask : s_bottomLeftCornerMask;
    } else if (x >= ax + aw - size) {
      return (ySide < 0) ? s_topRightCornerMask : s_bottomRightCornerMask;
    }
  }

  return s_noCornerMask;
}
