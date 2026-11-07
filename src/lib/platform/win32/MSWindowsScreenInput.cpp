/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsScreen.h"

#include "App.h"
#include "ClientApp.h"
#include "Clipboard.h"
#include "KeyMap.h"
#include "MSWindowsClipboard.h"
#include "MSWindowsDesks.h"
#include "MSWindowsDropTarget.h"
#include "MSWindowsEventQueueBuffer.h"
#include "MSWindowsKeyState.h"
#include "MSWindowsScreenSaver.h"
#include "ScreenException.h"
#include "Win32DropData.h"
#include "arch/Arch.h"
#include "arch/win32/ArchMiscWindows.h"
#include "arch/win32/XArchWindows.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "base/TMethodJob.h"
#include "client/Client.h"
#include "common/Constants.h"
#include "common/Settings.h"

#include <Shlobj.h>
#include <algorithm>
#include <comutil.h>
#include <string.h>

// suppress warning about GetVersionEx, which is used indirectly in this
// compilation unit.
#pragma warning(disable : 4996)

// X button stuff
#if !defined(WM_XBUTTONDOWN)
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_XBUTTONDBLCLK 0x020D
#define WM_NCXBUTTONDOWN 0x00AB
#define WM_NCXBUTTONUP 0x00AC
#define WM_NCXBUTTONDBLCLK 0x00AD
#define MOUSEEVENTF_XDOWN 0x0080
#define MOUSEEVENTF_XUP 0x0100
#define XBUTTON1 0x0001
#define XBUTTON2 0x0002
#endif
#if !defined(VK_XBUTTON1)
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#endif

// WM_POWERBROADCAST stuff
#if !defined(PBT_APMRESUMEAUTOMATIC)
#define PBT_APMRESUMEAUTOMATIC 0x0012
#endif

//
// MSWindowsScreen
//

void MSWindowsScreen::warpCursor(int32_t x, int32_t y)
{
  // warp mouse
  warpCursorNoFlush(x, y);

  // remove all input events before and including warp
  MSG msg;
  while (PeekMessage(&msg, nullptr, SYNERGY_MSG_INPUT_FIRST, SYNERGY_MSG_INPUT_LAST, PM_REMOVE)) {
    // do nothing
  }

  // save position to compute delta of next motion
  saveMousePosition(x, y);
}

void MSWindowsScreen::saveMousePosition(int32_t x, int32_t y)
{
  m_xCursor = x;
  m_yCursor = y;

  LOG_VERBOSE("saved mouse position for next delta: %+d,%+d", x, y);
}

uint32_t MSWindowsScreen::registerHotKey(KeyID key, KeyModifierMask mask)
{
  // only allow certain modifiers
  if ((mask & ~(KeyModifierShift | KeyModifierControl | KeyModifierAlt | KeyModifierSuper)) != 0) {
    // this should be a warning, but this can confuse users,
    // as this warning happens almost always.
    LOG_DEBUG("could not map hotkey id=%04x mask=%04x", key, mask);
    return 0;
  }

  // fail if no keys
  if (key == kKeyNone && mask == 0) {
    return 0;
  }

  // convert to win32
  UINT modifiers = 0;
  if ((mask & KeyModifierShift) != 0) {
    modifiers |= MOD_SHIFT;
  }
  if ((mask & KeyModifierControl) != 0) {
    modifiers |= MOD_CONTROL;
  }
  if ((mask & KeyModifierAlt) != 0) {
    modifiers |= MOD_ALT;
  }
  if ((mask & KeyModifierSuper) != 0) {
    modifiers |= MOD_WIN;
  }
  UINT vk = m_keyState->mapKeyToVirtualKey(key);
  if (key != kKeyNone && vk == 0) {
    // can't map key
    // this should be a warning, but this can confuse users,
    // as this warning happens almost always.
    LOG_DEBUG("could not map hotkey id=%04x mask=%04x", key, mask);
    return 0;
  }

  // choose hotkey id
  uint32_t id;
  if (!m_oldHotKeyIDs.empty()) {
    id = m_oldHotKeyIDs.back();
    m_oldHotKeyIDs.pop_back();
  } else {
    // id = m_hotKeys.size() + 1;
    id = (uint32_t)m_hotKeys.size() + 1;
  }

  // if this hot key has modifiers only then we'll handle it specially
  bool err;
  if (key == kKeyNone) {
    // check if already registered
    err = (m_hotKeyToIDMap.count(HotKeyItem(vk, modifiers)) > 0);
  } else {
    // register with OS
    err = (RegisterHotKey(nullptr, id, modifiers, vk) == 0);
  }

  if (!err) {
    m_hotKeys.insert(std::make_pair(id, HotKeyItem(vk, modifiers)));
    m_hotKeyToIDMap[HotKeyItem(vk, modifiers)] = id;
  } else {
    m_oldHotKeyIDs.push_back(id);
    m_hotKeys.erase(id);
    LOG_WARN(
        "failed to register hotkey %s (id=%04x mask=%04x)", synergy::KeyMap::formatKey(key, mask).c_str(), key, mask
    );

    return 0;
  }

  LOG_DEBUG(
      "registered hotkey %s (id=%04x mask=%04x) as id=%d", synergy::KeyMap::formatKey(key, mask).c_str(), key, mask, id
  );
  return id;
}

void MSWindowsScreen::unregisterHotKey(uint32_t id)
{
  // look up hotkey
  HotKeyMap::iterator i = m_hotKeys.find(id);
  if (i == m_hotKeys.end()) {
    return;
  }

  // unregister with OS
  bool err;
  if (i->second.getVirtualKey() != 0) {
    err = !UnregisterHotKey(nullptr, id);
  } else {
    err = false;
  }
  if (err) {
    LOG_WARN("failed to unregister hotkey id=%d", id);
  } else {
    LOG_DEBUG("unregistered hotkey id=%d", id);
  }

  // discard hot key from map and record old id for reuse
  m_hotKeyToIDMap.erase(i->second);
  m_hotKeys.erase(i);
  m_oldHotKeyIDs.push_back(id);
}

void MSWindowsScreen::fakeInputBegin()
{
  assert(m_isPrimary);

  if (!m_isOnScreen) {
    m_keyState->useSavedModifiers(true);
  }
  m_desks->fakeInputBegin();
}

void MSWindowsScreen::fakeInputEnd()
{
  assert(m_isPrimary);

  m_desks->fakeInputEnd();
  if (!m_isOnScreen) {
    m_keyState->useSavedModifiers(false);
  }
}

int32_t MSWindowsScreen::getJumpZoneSize() const
{
  return 1;
}

bool MSWindowsScreen::isAnyMouseButtonDown(uint32_t &buttonID) const
{
  static const char *buttonToName[] = {"<invalid>",    "Left Button", "Middle Button",
                                       "Right Button", "X Button 1",  "X Button 2"};

  for (uint32_t i = 1; i < sizeof(m_buttons) / sizeof(m_buttons[0]); ++i) {
    if (m_buttons[i]) {
      buttonID = i;
      LOG_DEBUG("locked by \"%s\"", buttonToName[i]);
      return true;
    }
  }

  return false;
}

void MSWindowsScreen::getCursorCenter(int32_t &x, int32_t &y) const
{
  x = m_xCenter;
  y = m_yCenter;
}

void MSWindowsScreen::fakeMouseButton(ButtonID id, bool press)
{
  m_desks->fakeMouseButton(id, press);

  if (id == kButtonLeft) {
    m_buttons[kButtonLeft] = press;
  }
}

void MSWindowsScreen::fakeMouseMove(int32_t x, int32_t y)
{
  m_desks->fakeMouseMove(x, y);
}

void MSWindowsScreen::fakeMouseRelativeMove(int32_t dx, int32_t dy) const
{
  m_desks->fakeMouseRelativeMove(dx, dy);
}

void MSWindowsScreen::fakeMouseWheel(ScrollDelta delta) const
{
  delta = applyScrollModifier(delta);
  m_desks->fakeMouseWheel(delta.x, delta.y);
}

void MSWindowsScreen::updateKeys()
{
  m_desks->updateKeys();
}

void MSWindowsScreen::fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang)
{
  PlatformScreen::fakeKeyDown(id, mask, button, lang);
  updateMouseKeys();
}

bool MSWindowsScreen::fakeKeyRepeat(
    KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang
)
{
  bool result = PlatformScreen::fakeKeyRepeat(id, mask, count, button, lang);
  updateMouseKeys();
  return result;
}

bool MSWindowsScreen::fakeKeyUp(KeyButton button)
{
  bool result = PlatformScreen::fakeKeyUp(button);
  updateMouseKeys();
  return result;
}

void MSWindowsScreen::fakeAllKeysUp()
{
  PlatformScreen::fakeAllKeysUp();
  updateMouseKeys();
}

void MSWindowsScreen::warpCursorNoFlush(int32_t x, int32_t y)
{
  // send an event that we can recognize before the mouse warp
  PostThreadMessage(GetCurrentThreadId(), SYNERGY_MSG_PRE_WARP, x, y);

  // warp mouse.  hopefully this inserts a mouse motion event
  // between the previous message and the following message.
  setThisCursorPos(x, y);

  // check to see if the mouse pos was set correctly
  POINT cursorPos;
  getThisCursorPos(&cursorPos);

  // there is a bug or round error in SetCursorPos and GetCursorPos on
  // a high DPI setting. The check here is for Vista/7 login screen.
  // since this feature is mainly for client, so only check on client.
  if (!isPrimary()) {
    if ((cursorPos.x != x) && (cursorPos.y != y)) {
      LOG_DEBUG("function 'SetCursorPos' failed; trying 'fakeMouseMove'");
      LOG_DEBUG("cursor pos %d, %d expected pos %d, %d", cursorPos.x, cursorPos.y, x, y);
      // when at Vista/7 login screen, SetCursorPos does not work (which could
      // be an MS security feature). instead we can use fakeMouseMove, which
      // calls mouse_event. IMPORTANT: as of implementing this function, it has
      // an annoying side effect; instead of the mouse returning to the correct
      // exit point, it returns to the center of the screen. this could have
      // something to do with the center screen warping technique used (see
      // comments for onMouseMove definition).
      fakeMouseMove(x, y);
    }
  }

  // yield the CPU.  there's a race condition when warping:
  //   a hardware mouse event occurs
  //   the mouse hook is not called because that process doesn't have the CPU
  //   we send PRE_WARP, SetCursorPos(), send POST_WARP
  //   we process all of those events and update m_x, m_y
  //   we finish our time slice
  //   the hook is called
  //   the hook sends us a mouse event from the pre-warp position
  //   we get the CPU
  //   we compute a bogus warp
  // we need the hook to process all mouse events that occur
  // before we warp before we do the warp but i'm not sure how
  // to guarantee that.  yielding the CPU here may reduce the
  // chance of undesired behavior.  we'll also check for very
  // large motions that look suspiciously like about half width
  // or height of the screen.
  Arch::sleep(0.0);

  // send an event that we can recognize after the mouse warp
  PostThreadMessage(GetCurrentThreadId(), SYNERGY_MSG_POST_WARP, 0, 0);
}

void MSWindowsScreen::setupMouseKeys()
{
  // we only need to enable the mouse keys feature when on a secondary screen.
  // this tricks windows into showing the mouse cursor when there is no real mouse.
  if (m_isPrimary) {
    // silent return to avoid noise.
    return;
  }

  // this is the case when there is some kind of a mouse (real or simulated by mouse keys).
  m_hasMouse = (GetSystemMetrics(SM_MOUSEPRESENT) != 0);
  if (m_hasMouse) {
    // silent return to avoid noise.
    return;
  }

  // prevents mouse keys being configured again when the program is shutting down since this function
  // is called based on system events such as system setting changes or hardware changes which
  // can occur at any time.
  if (!m_isEnabled) {
    LOG_DEBUG("mouse keys setup skipped, screen is not enabled");
    return;
  }

  m_mouseKeys.cbSize = sizeof(m_mouseKeys);
  m_gotMouseKeys = (SystemParametersInfo(SPI_GETMOUSEKEYS, m_mouseKeys.cbSize, &m_mouseKeys, 0) != 0);
  if (!m_gotMouseKeys) {
    LOG_ERR("unable to get old mouse keys settings, error: %d", GetLastError());
    return;
  }

  updateMouseKeys();
}

void MSWindowsScreen::updateMouseKeys()
{
  if (m_hasMouse || !m_gotMouseKeys || m_isPrimary) {
    // silent return to avoid noise.
    return;
  }

  DWORD oldFlags = m_mouseKeys.dwFlags;

  // turn on the windows mouse keys accessibility feature.
  // this is referred to as 'MouseKeys' in the docs.
  // makes the mouse cursor visible if there is no real mouse.
  //
  // historically, we would only set the `MKF_REPLACENUMBERS` flag when num lock is on.
  // however, this was a strange hidden feature that the user will most likely not expect;
  // it's probably more sensible to always set the `MKF_REPLACENUMBERS` flag, so that when the
  // mouse keys feature is left on after the program exits, the num pad on a local keyboard still
  // types numbers instead of moving the mouse cursor around (which would surprise most users).
  //
  // by default, windows 11 shows the mouse keys status in the system tray, but turning this on
  // might actually cause confusion for users who are not familiar with the mouse keys feature.
  m_mouseKeys.dwFlags = MKF_AVAILABLE | MKF_MOUSEKEYSON | MKF_REPLACENUMBERS;

  // only update the mouse keys settings if different to avoid noise.
  if (oldFlags == m_mouseKeys.dwFlags) {
    // silent return to avoid noise.
    return;
  }

  // we used to restore the old mouse keys settings but toggling the mouse keys feature on and off
  // causes the mouse cursor to be come stuck in an invisible state even when there is a real mouse.
  // we may want to reintroduce it (restore old mouse keys flags) as a user option in the future,
  // e.g. for users who use periodically use their windows client directly and use the numpad for cursor keys.
  LOG_INFO("enabling mouse keys for cursor visibility");
  LOG_DEBUG("setting mouse keys flags: 0x%08x", m_mouseKeys.dwFlags);
  const auto ok = SystemParametersInfo(SPI_SETMOUSEKEYS, m_mouseKeys.cbSize, &m_mouseKeys, SPIF_SENDCHANGE);
  if (!ok) {
    LOG_ERR("failed to set mouse keys, error: %d", GetLastError());
  } else {
    LOG_VERBOSE("mouse keys enabled successfully");
  }
}

void MSWindowsScreen::fakeLocalKey(KeyButton button, bool press) const
{
  INPUT input;
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = m_keyState->mapButtonToVirtualKey(button);
  DWORD pressFlag = press ? KEYEVENTF_EXTENDEDKEY : KEYEVENTF_KEYUP;
  input.ki.dwFlags = pressFlag;
  input.ki.time = 0;
  input.ki.dwExtraInfo = 0;
  if (SendInput(1, &input, sizeof(input)) == 0) {
    // UIPI blocks injection into elevated windows; log once, not per keystroke
    static bool s_loggedBlock = false;
    if (!s_loggedBlock) {
      s_loggedBlock = true;
      LOG_WARN("SendInput blocked, input dropped (target window may run elevated)");
    }
  }
}

//
// MSWindowsScreen::HotKeyItem
//
