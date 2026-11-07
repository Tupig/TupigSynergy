/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Settings.h" // must include first

#include "XWindowsScreen.h"

#include "App.h"
#include "ClientApp.h"
#include "Clipboard.h"
#include "KeyMap.h"
#include "ScreenException.h"
#include "XDGKeyUtil.h"
#include "XWindowsClipboard.h"
#include "XWindowsConfig.h"
#include "XWindowsEventQueueBuffer.h"
#include "XWindowsKeyState.h"
#include "XWindowsScreenSaver.h"
#include "XWindowsUtil.h"
#include "arch/Arch.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "base/Stopwatch.h"

#include <X11/X.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include <X11/keysymdef.h>
#if HAVE_X11_EXTENSIONS_DPMS_H
extern "C"
{
#include <X11/extensions/dpms.h>
}
#endif
#if HAVE_X11_EXTENSIONS_XTEST_H
#include <X11/extensions/XTest.h>
#else
#error The XTest extension is required to build Synergy
#endif
#if HAVE_X11_EXTENSIONS_XINERAMA_H
// Xinerama.h may lack extern "C" for inclusion by C++
extern "C"
{
#include <X11/extensions/Xinerama.h>
}
#endif
#if HAVE_X11_EXTENSIONS_XRANDR_H
#include <X11/extensions/Xrandr.h>
#endif
#if HAVE_XKB_EXTENSION
#include <X11/XKBlib.h>
#endif
#ifdef HAVE_XI2
#include <X11/extensions/XInput2.h>
#endif

uint32_t XWindowsScreen::registerHotKey(KeyID key, KeyModifierMask mask)
{
  // only allow certain modifiers
  if ((mask & ~(KeyModifierShift | KeyModifierControl | KeyModifierAlt | KeyModifierSuper)) != 0) {
    LOG_DEBUG("could not map hotkey id=%04x mask=%04x", key, mask);
    return 0;
  }

  // fail if no keys
  if (key == kKeyNone && mask == 0) {
    return 0;
  }

  // convert to X
  unsigned int modifiers;
  if (!m_keyState->mapModifiersToX(mask, modifiers)) {
    // can't map all modifiers
    LOG_DEBUG("could not map hotkey id=%04x mask=%04x", key, mask);
    return 0;
  }
  XWindowsKeyState::KeycodeList keycodes;
  m_keyState->mapKeyToKeycodes(key, keycodes);
  if (key != kKeyNone && keycodes.empty()) {
    // can't map key
    LOG_DEBUG("could not map hotkey id=%04x mask=%04x", key, mask);
    return 0;
  }

  // choose hotkey id
  uint32_t id;
  if (!m_oldHotKeyIDs.empty()) {
    id = m_oldHotKeyIDs.back();
    m_oldHotKeyIDs.pop_back();
  } else {
    id = m_hotKeys.size() + 1;
  }
  HotKeyList &hotKeys = m_hotKeys[id];

  // all modifier hotkey must be treated specially.  for each modifier
  // we need to grab the modifier key in combination with all the other
  // requested modifiers.
  bool err = false;
  {
    XWindowsUtil::ErrorLock lock(m_display, &err);
    if (key == kKeyNone) {
      static const KeyModifierMask s_hotKeyModifiers[] = {
          KeyModifierShift, KeyModifierControl, KeyModifierAlt, KeyModifierMeta, KeyModifierSuper
      };

      XModifierKeymap *modKeymap = XGetModifierMapping(m_display);
      for (size_t j = 0; j < std::size(s_hotKeyModifiers) && !err; ++j) {
        // skip modifier if not in mask
        if ((mask & s_hotKeyModifiers[j]) == 0) {
          continue;
        }

        // skip with error if we can't map remaining modifiers
        unsigned int modifiers2;
        if (KeyModifierMask mask2 = (mask & ~s_hotKeyModifiers[j]); !m_keyState->mapModifiersToX(mask2, modifiers2)) {
          err = true;
          continue;
        }

        // compute modifier index for modifier.  there should be
        // exactly one X modifier missing
        int index;
        switch (modifiers ^ modifiers2) {
        case ShiftMask:
          index = ShiftMapIndex;
          break;

        case LockMask:
          index = LockMapIndex;
          break;

        case ControlMask:
          index = ControlMapIndex;
          break;

        case Mod1Mask:
          index = Mod1MapIndex;
          break;

        case Mod2Mask:
          index = Mod2MapIndex;
          break;

        case Mod3Mask:
          index = Mod3MapIndex;
          break;

        case Mod4Mask:
          index = Mod4MapIndex;
          break;

        case Mod5Mask:
          index = Mod5MapIndex;
          break;

        default:
          err = true;
          continue;
        }

        // grab each key for the modifier
        const KeyCode *modifiermap = modKeymap->modifiermap + index * modKeymap->max_keypermod;
        for (int k = 0; k < modKeymap->max_keypermod && !err; ++k) {
          KeyCode code = modifiermap[k];
          if (modifiermap[k] != 0) {
            XGrabKey(m_display, code, modifiers2, m_root, False, GrabModeAsync, GrabModeAsync);
            if (!err) {
              hotKeys.push_back(std::make_pair(code, modifiers2));
              m_hotKeyToIDMap[HotKeyItem(code, modifiers2)] = id;
            }
          }
        }
      }
      XFreeModifiermap(modKeymap);
    }

    // a non-modifier key must be insensitive to CapsLock, NumLock and
    // ScrollLock, so we have to grab the key with every combination of
    // those.
    else {
      // collect available toggle modifiers
      unsigned int modifier;
      unsigned int toggleModifiers[3];
      size_t numToggleModifiers = 0;
      if (m_keyState->mapModifiersToX(KeyModifierCapsLock, modifier)) {
        toggleModifiers[numToggleModifiers++] = modifier;
      }
      if (m_keyState->mapModifiersToX(KeyModifierNumLock, modifier)) {
        toggleModifiers[numToggleModifiers++] = modifier;
      }
      if (m_keyState->mapModifiersToX(KeyModifierScrollLock, modifier)) {
        toggleModifiers[numToggleModifiers++] = modifier;
      }

      for (auto j = keycodes.begin(); j != keycodes.end() && !err; ++j) {
        for (size_t i = 0; i < (1u << numToggleModifiers); ++i) {
          // add toggle modifiers for index i
          unsigned int tmpModifiers = modifiers;
          if ((i & 1) != 0) {
            tmpModifiers |= toggleModifiers[0];
          }
          if ((i & 2) != 0) {
            tmpModifiers |= toggleModifiers[1];
          }
          if ((i & 4) != 0) {
            tmpModifiers |= toggleModifiers[2];
          }

          // add grab
          XGrabKey(m_display, *j, tmpModifiers, m_root, False, GrabModeAsync, GrabModeAsync);
          if (!err) {
            hotKeys.push_back(std::make_pair(*j, tmpModifiers));
            m_hotKeyToIDMap[HotKeyItem(*j, tmpModifiers)] = id;
          }
        }
      }
    }
  }

  if (err) {
    // if any failed then unregister any we did get
    for (const auto &[keyCode, keyMask] : hotKeys) {
      XUngrabKey(m_display, keyCode, keyMask, m_root);
      m_hotKeyToIDMap.erase(HotKeyItem(keyCode, keyMask));
    }

    m_oldHotKeyIDs.push_back(id);
    m_hotKeys.erase(id);
    LOG(
        (CLOG_WARN "failed to register hotkey %s (id=%04x mask=%04x)", synergy::KeyMap::formatKey(key, mask).c_str(),
         key, mask)
    );
    return 0;
  }

  LOG(
      (CLOG_DEBUG "registered hotkey %s (id=%04x mask=%04x) as id=%d", synergy::KeyMap::formatKey(key, mask).c_str(),
       key, mask, id)
  );
  return id;
}

void XWindowsScreen::unregisterHotKey(uint32_t id)
{
  // look up hotkey
  HotKeyMap::iterator i = m_hotKeys.find(id);
  if (i == m_hotKeys.end()) {
    return;
  }

  // unregister with OS
  bool err = false;
  {
    XWindowsUtil::ErrorLock lock(m_display, &err);
    const HotKeyList &hotKeys = i->second;
    for (const auto &[keyCode, keyMask] : hotKeys) {
      XUngrabKey(m_display, keyCode, keyMask, m_root);
      m_hotKeyToIDMap.erase(HotKeyItem(keyCode, keyMask));
    }
  }
  if (err) {
    LOG_WARN("failed to unregister hotkey id=%d", id);
  } else {
    LOG_DEBUG("unregistered hotkey id=%d", id);
  }

  // discard hot key from map and record old id for reuse
  m_hotKeys.erase(i);
  m_oldHotKeyIDs.push_back(id);
}

void XWindowsScreen::fakeInputBegin()
{
  // FIXME -- not implemented
}

void XWindowsScreen::fakeInputEnd()
{
  // FIXME -- not implemented
}

int32_t XWindowsScreen::getJumpZoneSize() const
{
  return 1;
}

bool XWindowsScreen::isAnyMouseButtonDown(uint32_t &) const
{
  // query the pointer to get the button state
  Window root;
  Window window;
  int xRoot;
  int yRoot;
  int xWindow;
  int yWindow;
  if (unsigned int state;
      XQueryPointer(m_display, m_root, &root, &window, &xRoot, &yRoot, &xWindow, &yWindow, &state)) {
    return ((state & (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask)) != 0);
  }

  return false;
}

void XWindowsScreen::getCursorCenter(int32_t &x, int32_t &y) const
{
  x = m_xCenter;
  y = m_yCenter;
}

void XWindowsScreen::fakeMouseButton(ButtonID button, bool press)
{
  if (m_displayLost) {
    return;
  }

  const unsigned int xButton = mapButtonToX(button);
  if (xButton > 0 && xButton < 11) {
    XTestFakeButtonEvent(m_display, xButton, press ? True : False, CurrentTime);
    XFlush(m_display);
  }
}

void XWindowsScreen::fakeMouseMove(int32_t x, int32_t y)
{
  if (m_displayLost) {
    return;
  }

  if (m_xinerama && m_xtestIsXineramaUnaware) {
    XWarpPointer(m_display, None, m_root, 0, 0, 0, 0, x, y);
  } else {
    XTestFakeMotionEvent(m_display, DefaultScreen(m_display), x, y, CurrentTime);
  }
  XFlush(m_display);
}

void XWindowsScreen::fakeMouseRelativeMove(int32_t dx, int32_t dy) const
{
  // FIXME -- ignore xinerama for now
  XTestFakeRelativeMotionEvent(m_display, dx, dy, CurrentTime);
  XFlush(m_display);
}

void XWindowsScreen::fakeMouseWheel(ScrollDelta delta) const
{
  if (delta.y == 0 && delta.x == 0) {
    return;
  }
  delta = applyScrollModifier(delta);
  bool isYScroll = delta.y != 0;
  int32_t axisDelta = delta.y == 0 ? delta.x : delta.y;

  ButtonID btn = kButtonNone;
  if (isYScroll) {
    btn = axisDelta >= 0 ? kX11ScrollWheelUp : kX11ScrollWheelDown;
  } else {
    btn = axisDelta >= 0 ? kX11ScrollWheelLeft : kX11ScrollWheelRight;
  }
  const unsigned int button = mapButtonToX(btn);

  // If the XServer does not support the Y scroll send PageUp/PageDown keystrokes instead.
  if (const KeyCode keycode = XKeysymToKeycode(m_display, delta.y >= 0 ? XK_Page_Up : XK_Page_Down);
      button == 0 && isYScroll && keycode != 0) {
    XTestFakeKeyEvent(m_display, keycode, True, CurrentTime);
    XTestFakeKeyEvent(m_display, keycode, False, CurrentTime);
    return;
  }

  // now use absolute value of delta
  if (axisDelta < 0) {
    axisDelta = -axisDelta;
  }

  // send as many clicks as necessary
  while (axisDelta > 0) {
    XTestFakeButtonEvent(m_display, button, True, CurrentTime);
    XTestFakeButtonEvent(m_display, button, False, CurrentTime);
    axisDelta -= s_scrollDelta;
  }
  XFlush(m_display);
}

void XWindowsScreen::warpCursorNoFlush(int32_t x, int32_t y)
{
  assert(m_window != None);

  // send an event that we can recognize before the mouse warp
  XEvent eventBefore;
  eventBefore.type = MotionNotify;
  eventBefore.xmotion.display = m_display;
  eventBefore.xmotion.window = m_window;
  eventBefore.xmotion.root = m_root;
  eventBefore.xmotion.subwindow = m_window;
  eventBefore.xmotion.time = CurrentTime;
  eventBefore.xmotion.x = x;
  eventBefore.xmotion.y = y;
  eventBefore.xmotion.x_root = x;
  eventBefore.xmotion.y_root = y;
  eventBefore.xmotion.state = 0;
  eventBefore.xmotion.is_hint = NotifyNormal;
  eventBefore.xmotion.same_screen = True;
  XEvent eventAfter = eventBefore;
  XSendEvent(m_display, m_window, False, 0, &eventBefore);

  // warp mouse
  XWarpPointer(m_display, None, m_root, 0, 0, 0, 0, x, y);

  // send an event that we can recognize after the mouse warp
  XSendEvent(m_display, m_window, False, 0, &eventAfter);
  XSync(m_display, False);

  LOG_VERBOSE("warped to %d,%d", x, y);
}

bool XWindowsScreen::grabMouseAndKeyboard()
{
  unsigned int event_mask = ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask;

  // grab the mouse and keyboard.  keep trying until we get them.
  // if we can't grab one after grabbing the other then ungrab
  // and wait before retrying.  give up after s_timeout seconds.
  static const double s_timeout = 1.0;
  int result;
  Stopwatch timer;
  do {
    // keyboard first
    do {
      result = XGrabKeyboard(m_display, m_window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
      assert(result != GrabNotViewable);
      if (result != GrabSuccess) {
        LOG_VERBOSE("waiting to grab keyboard");
        Arch::sleep(0.05);
        if (timer.getTime() >= s_timeout) {
          LOG_VERBOSE("grab keyboard timed out");
          return false;
        }
      }
    } while (result != GrabSuccess);
    LOG_VERBOSE("grabbed keyboard");

    // now the mouse --- use event_mask to get EnterNotify, LeaveNotify events
    result =
        XGrabPointer(m_display, m_window, False, event_mask, GrabModeAsync, GrabModeAsync, m_window, None, CurrentTime);
    assert(result != GrabNotViewable);
    if (result != GrabSuccess) {
      // back off to avoid grab deadlock
      XUngrabKeyboard(m_display, CurrentTime);
      LOG_VERBOSE("ungrabbed keyboard, waiting to grab pointer");
      Arch::sleep(0.05);
      if (timer.getTime() >= s_timeout) {
        LOG_VERBOSE("grab pointer timed out");
        return false;
      }
    }
  } while (result != GrabSuccess);

  LOG_VERBOSE("grabbed pointer and keyboard");
  return true;
}
