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

//
// add backwards compatible multihead support (and suppress bogus warning).
// this isn't supported on MinGW yet AFAICT.
//
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4706) // assignment within conditional
#include <multimon.h>
#pragma warning(pop)
#endif

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

void MSWindowsScreen::sendEvent(EventTypes type, void *data)
{
  m_events->addEvent(Event(type, getEventTarget(), data));
}

void MSWindowsScreen::sendClipboardEvent(EventTypes type, ClipboardID id)
{
  ClipboardInfo *info = (ClipboardInfo *)malloc(sizeof(ClipboardInfo));
  if (info == nullptr) {
    LOG_ERR("malloc failed on %s:%s", __FILE__, __LINE__);
    return;
  }
  info->m_id = id;
  info->m_sequenceNumber = m_sequenceNumber;
  sendEvent(type, info);
}

void MSWindowsScreen::handleSystemEvent(const Event &event)
{
  MSG *msg = static_cast<MSG *>(event.getData());
  assert(msg != nullptr);

  if (onPreDispatch(msg->hwnd, msg->message, msg->wParam, msg->lParam)) {
    return;
  }
  TranslateMessage(msg);
  DispatchMessage(msg);
}

void MSWindowsScreen::updateButtons()
{
  int numButtons = GetSystemMetrics(SM_CMOUSEBUTTONS);
  m_buttons[kButtonNone] = false;
  m_buttons[kButtonLeft] = (GetKeyState(VK_LBUTTON) < 0);
  m_buttons[kButtonRight] = (GetKeyState(VK_RBUTTON) < 0);
  m_buttons[kButtonMiddle] = (GetKeyState(VK_MBUTTON) < 0);
  m_buttons[kButtonExtra0 + 0] = (numButtons >= 4) && (GetKeyState(VK_XBUTTON1) < 0);
  m_buttons[kButtonExtra0 + 1] = (numButtons >= 5) && (GetKeyState(VK_XBUTTON2) < 0);
}

IKeyState *MSWindowsScreen::getKeyState() const
{
  return m_keyState;
}

bool MSWindowsScreen::onPreDispatch(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // handle event
  switch (message) {
  case SYNERGY_MSG_SCREEN_SAVER:
    return onScreensaver(wParam != 0);

  case SYNERGY_MSG_DEBUG:
    LOG_VERBOSE("hook: 0x%08x 0x%08x", wParam, lParam);
    return true;
  }

  if (m_isPrimary) {
    return onPreDispatchPrimary(hwnd, message, wParam, lParam);
  }

  return false;
}

bool MSWindowsScreen::onPreDispatchPrimary(HWND, UINT message, WPARAM wParam, LPARAM lParam)
{
  LOG_VERBOSE("handling pre-dispatch primary");

  // handle event
  switch (message) {
  case SYNERGY_MSG_MARK:
    return onMark(static_cast<uint32_t>(wParam));

  case SYNERGY_MSG_KEY:
    return onKey(wParam, lParam);

  case SYNERGY_MSG_MOUSE_BUTTON:
    return onMouseButton(wParam, lParam);

  case SYNERGY_MSG_MOUSE_MOVE:
    return onMouseMove(static_cast<int32_t>(wParam), static_cast<int32_t>(lParam));

  case SYNERGY_MSG_MOUSE_WHEEL:
    return onMouseWheel(static_cast<int32_t>(lParam), static_cast<int32_t>(wParam));

  case SYNERGY_MSG_PRE_WARP: {
    // save position to compute delta of next motion
    saveMousePosition(static_cast<int32_t>(wParam), static_cast<int32_t>(lParam));

    // we warped the mouse.  discard events until we find the
    // matching post warp event.  see warpCursorNoFlush() for
    // where the events are sent.  we discard the matching
    // post warp event and can be sure we've skipped the warp
    // event.
    MSG msg;
    do {
      GetMessage(&msg, nullptr, SYNERGY_MSG_MOUSE_MOVE, SYNERGY_MSG_POST_WARP);
    } while (msg.message != SYNERGY_MSG_POST_WARP);
  }
    return true;

  case SYNERGY_MSG_POST_WARP:
    LOG_WARN("unmatched post warp");
    return true;

  case WM_HOTKEY:
    // we discard these messages.  we'll catch the hot key in the
    // regular key event handling, where we can detect both key
    // press and release.  we only register the hot key so no other
    // app will act on the key combination.
    break;
  }

  return false;
}

bool MSWindowsScreen::onEvent(HWND, UINT msg, WPARAM wParam, LPARAM, LRESULT *result)
{
  switch (msg) {

  case WM_CLIPBOARDUPDATE: {
    DWORD clipboardSequenceNumber = GetClipboardSequenceNumber();
    LOG_DEBUG("clipboard update: sequence number %d, current %d", clipboardSequenceNumber, m_clipboardSequenceNumber);

    if (clipboardSequenceNumber && (clipboardSequenceNumber != m_clipboardSequenceNumber)) {
      m_clipboardSequenceNumber = clipboardSequenceNumber;
      onClipboardChange();
    }
    return 0; // message processed
  }

  case WM_DISPLAYCHANGE:
    return onDisplayChange();

  /* On windows 10 we don't receive WM_POWERBROADCAST after sleep.
   We receive only WM_TIMECHANGE hence this message is used to resume.*/
  case WM_TIMECHANGE:
    m_events->addEvent( //
        Event(EventTypes::ScreenResume, getEventTarget(), nullptr, Event::EventFlags::DeliverImmediately)
    );
    break;

  case WM_POWERBROADCAST:
    switch (wParam) {
    case PBT_APMRESUMEAUTOMATIC:
    case PBT_APMRESUMECRITICAL:
    case PBT_APMRESUMESUSPEND:
      m_events->addEvent(
          Event(EventTypes::ScreenResume, getEventTarget(), nullptr, Event::EventFlags::DeliverImmediately)
      );
      break;

    case PBT_APMSUSPEND:
      m_events->addEvent(
          Event(EventTypes::ScreenSuspend, getEventTarget(), nullptr, Event::EventFlags::DeliverImmediately)
      );
      break;
    }
    *result = TRUE;
    return true;

  case WM_DEVICECHANGE: {
    // re-run mouse keys setup in case a mouse was plugged in or unplugged; i.e. if a mouse was
    // unplugged from the client, make sure the mouse cursor is still visible.
    // the device change event happens for every discreet hardware change, so if you're using a
    // usb switcher, this generates many device change events. it would be nice to log here but
    // the log would be too noisy.
    setupMouseKeys();
  } break;

  case WM_SETTINGCHANGE:
    // sometimes fired when the mouse keys setting is changed, but doesn't seem very reliable.
    // these events may arrive at any time (e.g. when the program is shutting down) if the message
    // loop stops being processed for any reason. this may be a bug or something out of our control.
    // forcing mouse keys on may help in scenarios where mouse keys are being turned off by another app.
    if (wParam == SPI_SETMOUSEKEYS) {
      LOG_DEBUG("mouse keys setting was changed");
      setupMouseKeys();
    }
    break;
  }

  return false;
}

bool MSWindowsScreen::onMark(uint32_t mark)
{
  m_markReceived = mark;
  return true;
}

bool MSWindowsScreen::onKey(WPARAM wParam, LPARAM lParam)
{
  static const KeyModifierMask s_ctrlAlt = KeyModifierControl | KeyModifierAlt;

  LOG_VERBOSE(
      "event: Key char=%d, vk=0x%02x, nagr=%d, lParam=0x%08x", (wParam & 0xffffu), (wParam >> 16) & 0xffu,
      (wParam & 0x1000000u) ? 1 : 0, lParam
  );

  // get event info
  KeyButton button = (KeyButton)((lParam & 0x01ff0000) >> 16);
  bool down = ((lParam & 0x80000000u) == 0x00000000u);
  bool wasDown = isKeyDown(button);
  KeyModifierMask oldState = pollActiveModifiers();

  // check for autorepeat
  if (m_keyState->testAutoRepeat(down, (lParam & 0x40000000u), button)) {
    lParam |= 0x40000000u;
  }

  // if the button is zero then guess what the button should be.
  // these are badly synthesized key events and logitech software
  // that maps mouse buttons to keys is known to do this.
  // alternatively, we could just throw these events out.
  if (button == 0) {
    button = m_keyState->virtualKeyToButton((wParam >> 16) & 0xffu);
    if (button == 0) {
      return true;
    }
    wasDown = isKeyDown(button);
  }

  // record keyboard state
  m_keyState->onKey(button, down, oldState);

  if (!down && m_isPrimary && !m_isOnScreen) {
    PrimaryKeyDownList::iterator find = std::find(m_primaryKeyDownList.begin(), m_primaryKeyDownList.end(), button);
    if (find != m_primaryKeyDownList.end()) {
      LOG_VERBOSE("release key button %d on primary", *find);
      m_hook.setMode(kHOOK_WATCH_JUMP_ZONE);
      fakeLocalKey(*find, false);
      m_primaryKeyDownList.erase(find);
      m_hook.setMode(kHOOK_RELAY_EVENTS);
      return true;
    }
  }

  // windows doesn't tell us the modifier key state on mouse or key
  // events so we have to figure it out.  most apps would use
  // GetKeyState() or even GetAsyncKeyState() for that but we can't
  // because our hook doesn't pass on key events for several modifiers.
  // it can't otherwise the system would interpret them normally on
  // the primary screen even when on a secondary screen.  so tapping
  // alt would activate menus and tapping the windows key would open
  // the start menu.  if you don't pass those events on in the hook
  // then GetKeyState() understandably doesn't reflect the effect of
  // the event.  curiously, neither does GetAsyncKeyState(), which is
  // surprising.
  //
  // so anyway, we have to track the modifier state ourselves for
  // at least those modifiers we don't pass on.  pollActiveModifiers()
  // does that but we have to update the keyboard state before calling
  // pollActiveModifiers() to get the right answer.  but the only way
  // to set the modifier state or to set the up/down state of a key
  // is via onKey().  so we have to call onKey() twice.
  KeyModifierMask state = pollActiveModifiers();
  m_keyState->onKey(button, down, state);

  // check for hot keys
  if (oldState != state) {
    // modifier key was pressed/released
    if (onHotKey(0, lParam)) {
      return true;
    }
  } else {
    // non-modifier was pressed/released
    if (onHotKey(wParam, lParam)) {
      return true;
    }
  }

  // stop sending modifier keys over and over again
  if (isModifierRepeat(oldState, state, wParam)) {
    return true;
  }

  // ignore message if posted prior to last mark change
  if (!ignore()) {
    // check for ctrl+alt+del.  we do not want to pass that to the
    // client.  the user can use ctrl+alt+pause to emulate it.
    UINT virtKey = ((wParam >> 16) & 0xffu);
    if (virtKey == VK_DELETE && (state & s_ctrlAlt) == s_ctrlAlt) {
      LOG_DEBUG("discard ctrl+alt+del");
      return true;
    }

    // check for ctrl+alt+del emulation
    if ((virtKey == VK_PAUSE || virtKey == VK_CANCEL) && (state & s_ctrlAlt) == s_ctrlAlt) {
      LOG_DEBUG("emulate ctrl+alt+del");
      // switch wParam and lParam to be as if VK_DELETE was
      // pressed or released.  when mapping the key we require that
      // we not use AltGr (the 0x10000 flag in wParam) and we not
      // use the keypad delete key (the 0x01000000 flag in lParam).
      wParam = (VK_DELETE << 16) | 0x01000000u;
      lParam &= 0xfe000000;
      lParam |= m_keyState->virtualKeyToButton(VK_DELETE) << 16;
      lParam |= 0x01000001;
    }

    // process key
    KeyModifierMask mask;
    KeyID key = m_keyState->mapKeyFromEvent(wParam, lParam, &mask);
    button = static_cast<KeyButton>((lParam & 0x01ff0000u) >> 16);
    if (key != kKeyNone) {
      // do it
      m_keyState->sendKeyEvent(
          getEventTarget(), ((lParam & 0x80000000u) == 0), ((lParam & 0x40000000u) != 0), key, mask,
          (int32_t)(lParam & 0xffff), button
      );
    } else {
      LOG_VERBOSE("cannot map key");
    }
  }

  return true;
}

bool MSWindowsScreen::onHotKey(WPARAM wParam, LPARAM lParam)
{
  // get the key info
  KeyModifierMask state = getActiveModifiers();
  UINT virtKey = ((wParam >> 16) & 0xffu);
  UINT modifiers = 0;
  if ((state & KeyModifierShift) != 0) {
    modifiers |= MOD_SHIFT;
  }
  if ((state & KeyModifierControl) != 0) {
    modifiers |= MOD_CONTROL;
  }
  if ((state & KeyModifierAlt) != 0) {
    modifiers |= MOD_ALT;
  }
  if ((state & KeyModifierSuper) != 0) {
    modifiers |= MOD_WIN;
  }

  // find the hot key id
  HotKeyToIDMap::const_iterator i = m_hotKeyToIDMap.find(HotKeyItem(virtKey, modifiers));
  if (i == m_hotKeyToIDMap.end()) {
    return false;
  }

  // find what kind of event
  EventTypes type;
  if ((lParam & 0x80000000u) == 0u) {
    if ((lParam & 0x40000000u) != 0u) {
      // ignore key repeats but it counts as a hot key
      return true;
    }
    type = EventTypes::PrimaryScreenHotkeyDown;
  } else {
    type = EventTypes::PrimaryScreenHotkeyUp;
  }

  // generate event
  m_events->addEvent(Event(type, getEventTarget(), HotKeyInfo::alloc(i->second)));

  return true;
}

bool MSWindowsScreen::onMouseButton(WPARAM wParam, LPARAM lParam)
{
  // get which button
  bool pressed = mapPressFromEvent(wParam, lParam);
  ButtonID button = mapButtonFromEvent(wParam, lParam);

  // keep our shadow key state up to date
  if (button >= kButtonLeft && button <= kButtonExtra0 + 1) {
    m_buttons[button] = pressed;
  }

  // ignore message if posted prior to last mark change
  if (!ignore()) {
    KeyModifierMask mask = m_keyState->getActiveModifiers();
    if (pressed) {
      LOG_VERBOSE("event: button press button=%d", button);
      if (button != kButtonNone) {
        sendEvent(EventTypes::PrimaryScreenButtonDown, ButtonInfo::alloc(button, mask));
      }
    } else {
      LOG_VERBOSE("event: button release button=%d", button);
      if (button != kButtonNone) {
        sendEvent(EventTypes::PrimaryScreenButtonUp, ButtonInfo::alloc(button, mask));
      }
    }
  }

  return true;
}

// here's how mouse movements are sent across the network to a client:
//   1. Synergy checks the mouse position on server screen
//   2. records the delta (current x,y minus last x,y)
//   3. records the current x,y as "last" (so we can calc delta next time)
//   4. on the server, puts the cursor back to the center of the screen
//      - remember the cursor is hidden on the server at this point
//      - this actually records the current x,y as "last" a second time (it
//      seems)
//   5. sends the delta movement to the client (could be +1,+1 or -1,+4 for
//   example)
bool MSWindowsScreen::onMouseMove(int32_t mx, int32_t my)
{
  // compute motion delta (relative to the last known
  // mouse position)
  int32_t x = mx - m_xCursor;
  int32_t y = my - m_yCursor;

  LOG_VERBOSE("mouse move - motion delta: %+d=(%+d - %+d),%+d=(%+d - %+d)", x, mx, m_xCursor, y, my, m_yCursor);

  // ignore if the mouse didn't move or if message posted prior
  // to last mark change.
  if (ignore() || (x == 0 && y == 0)) {
    return true;
  }

  // save position to compute delta of next motion
  saveMousePosition(mx, my);

  if (m_isOnScreen) {
    // motion on primary screen
    sendEvent(EventTypes::PrimaryScreenMotionOnPrimary, MotionInfo::alloc(m_xCursor, m_yCursor));
  } else {
    // the motion is on the secondary screen, so we warp mouse back to
    // center on the server screen. if we don't do this, then the mouse
    // will always try to return to the original entry point on the
    // secondary screen.
    LOG_VERBOSE("centering cursor on motion: %+d,%+d", m_xCenter, m_yCenter);
    warpCursorNoFlush(m_xCenter, m_yCenter);

    // examine the motion.  if it's about the distance
    // from the center of the screen to an edge then
    // it's probably a bogus motion that we want to
    // ignore (see warpCursorNoFlush() for a further
    // description).
    static int32_t bogusZoneSize = 10;
    if (-x + bogusZoneSize > m_xCenter - m_x || x + bogusZoneSize > m_x + m_w - m_xCenter ||
        -y + bogusZoneSize > m_yCenter - m_y || y + bogusZoneSize > m_y + m_h - m_yCenter) {

      LOG_DEBUG("dropped bogus delta motion: %+d,%+d", x, y);
    } else {
      // send motion
      sendEvent(EventTypes::PrimaryScreenMotionOnSecondary, MotionInfo::alloc(x, y));
    }
  }

  return true;
}

bool MSWindowsScreen::onMouseWheel(int32_t xDelta, int32_t yDelta)
{
  // ignore message if posted prior to last mark change
  if (!ignore()) {
    LOG_VERBOSE("event: button wheel delta=%+d,%+d", xDelta, yDelta);
    sendEvent(EventTypes::PrimaryScreenWheel, WheelInfo::alloc(xDelta, yDelta));
  }
  return true;
}

bool MSWindowsScreen::onScreensaver(bool activated)
{
  // ignore this message if there are any other screen saver
  // messages already in the queue.  this is important because
  // our checkStarted() function has a deliberate delay, so it
  // can't respond to events at full CPU speed and will fall
  // behind if a lot of screen saver events are generated.
  // that can easily happen because windows will continually
  // send SC_SCREENSAVE until the screen saver starts, even if
  // the screen saver is disabled!
  MSG msg;
  if (PeekMessage(&msg, nullptr, SYNERGY_MSG_SCREEN_SAVER, SYNERGY_MSG_SCREEN_SAVER, PM_NOREMOVE)) {
    return true;
  }

  if (activated) {
    if (!m_screensaverActive && m_screensaver->checkStarted(SYNERGY_MSG_SCREEN_SAVER, FALSE, 0)) {
      m_screensaverActive = true;
      sendEvent(EventTypes::PrimaryScreenSaverActivated);
    }
  } else {
    if (m_screensaverActive) {
      m_screensaverActive = false;
      sendEvent(EventTypes::PrimaryScreenSaverDeactivated);
    }
  }

  return true;
}

bool MSWindowsScreen::onDisplayChange()
{
  // screen resolution may have changed.  save old shape.
  int32_t xOld = m_x, yOld = m_y, wOld = m_w, hOld = m_h;

  // update shape
  updateScreenShape();

  // do nothing if resolution hasn't changed
  if (xOld != m_x || yOld != m_y || wOld != m_w || hOld != m_h) {
    if (m_isPrimary) {
      if (!m_isOnScreen) {
        LOG_VERBOSE("centering cursor on display change: %+d, %+d", m_xCenter, m_yCenter);
        warpCursor(m_xCenter, m_yCenter);
      }

      // tell hook about resize if on screen
      else {
        m_hook.setZone(m_x, m_y, m_w, m_h, getJumpZoneSize());
      }
    }

    // send new screen info
    sendEvent(EventTypes::ScreenShapeChanged);

    LOG_DEBUG("screen shape: %d,%d %dx%d %s", m_x, m_y, m_w, m_h, m_multimon ? "(multi-monitor)" : "");
  }

  return true;
}

void MSWindowsScreen::onClipboardChange()
{
  // now notify client that somebody changed the clipboard (unless
  // we're the owner).
  if (!MSWindowsClipboard::isOwnedBySynergy()) {
    if (m_ownClipboard) {
      LOG_DEBUG("clipboard changed: lost ownership");
      m_ownClipboard = false;
      sendClipboardEvent(EventTypes::ClipboardGrabbed, kClipboardClipboard);
      sendClipboardEvent(EventTypes::ClipboardGrabbed, kClipboardSelection);
    }
  } else if (!m_ownClipboard) {
    LOG_DEBUG("clipboard changed: %s owned", kAppId);
    m_ownClipboard = true;
  }
}

ButtonID MSWindowsScreen::mapButtonFromEvent(WPARAM msg, LPARAM button) const
{
  switch (msg) {
  case WM_LBUTTONDOWN:
  case WM_LBUTTONDBLCLK:
  case WM_LBUTTONUP:
  case WM_NCLBUTTONDOWN:
  case WM_NCLBUTTONDBLCLK:
  case WM_NCLBUTTONUP:
    return kButtonLeft;

  case WM_MBUTTONDOWN:
  case WM_MBUTTONDBLCLK:
  case WM_MBUTTONUP:
  case WM_NCMBUTTONDOWN:
  case WM_NCMBUTTONDBLCLK:
  case WM_NCMBUTTONUP:
    return kButtonMiddle;

  case WM_RBUTTONDOWN:
  case WM_RBUTTONDBLCLK:
  case WM_RBUTTONUP:
  case WM_NCRBUTTONDOWN:
  case WM_NCRBUTTONDBLCLK:
  case WM_NCRBUTTONUP:
    return kButtonRight;

  case WM_XBUTTONDOWN:
  case WM_XBUTTONDBLCLK:
  case WM_XBUTTONUP:
  case WM_NCXBUTTONDOWN:
  case WM_NCXBUTTONDBLCLK:
  case WM_NCXBUTTONUP:
    switch (button) {
    case XBUTTON1:
      if (GetSystemMetrics(SM_CMOUSEBUTTONS) >= 4) {
        return kButtonExtra0;
      }
      break;

    case XBUTTON2:
      if (GetSystemMetrics(SM_CMOUSEBUTTONS) >= 5) {
        return kButtonExtra1;
      }
      break;
    }
    return kButtonNone;

  default:
    return kButtonNone;
  }
}

bool MSWindowsScreen::mapPressFromEvent(WPARAM msg, LPARAM) const
{
  switch (msg) {
  case WM_LBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_XBUTTONDOWN:
  case WM_LBUTTONDBLCLK:
  case WM_MBUTTONDBLCLK:
  case WM_RBUTTONDBLCLK:
  case WM_XBUTTONDBLCLK:
  case WM_NCLBUTTONDOWN:
  case WM_NCMBUTTONDOWN:
  case WM_NCRBUTTONDOWN:
  case WM_NCXBUTTONDOWN:
  case WM_NCLBUTTONDBLCLK:
  case WM_NCMBUTTONDBLCLK:
  case WM_NCRBUTTONDBLCLK:
  case WM_NCXBUTTONDBLCLK:
    return true;

  case WM_LBUTTONUP:
  case WM_MBUTTONUP:
  case WM_RBUTTONUP:
  case WM_XBUTTONUP:
  case WM_NCLBUTTONUP:
  case WM_NCMBUTTONUP:
  case WM_NCRBUTTONUP:
  case WM_NCXBUTTONUP:
    return false;

  default:
    return false;
  }
}

LRESULT CALLBACK MSWindowsScreen::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  assert(s_screen != nullptr);

  LRESULT result = 0;
  if (!s_screen->onEvent(hwnd, msg, wParam, lParam, &result)) {
    result = DefWindowProc(hwnd, msg, wParam, lParam);
  }

  return result;
}

bool MSWindowsScreen::isModifierRepeat(KeyModifierMask oldState, KeyModifierMask state, WPARAM wParam) const
{
  bool result = false;

  if (oldState == state && state != 0) {
    UINT virtKey = ((wParam >> 16) & 0xffu);
    if ((state & KeyModifierShift) != 0 && (virtKey == VK_LSHIFT || virtKey == VK_RSHIFT)) {
      result = true;
    }
    if ((state & KeyModifierControl) != 0 && (virtKey == VK_LCONTROL || virtKey == VK_RCONTROL)) {
      result = true;
    }
    if ((state & KeyModifierAlt) != 0 && (virtKey == VK_LMENU || virtKey == VK_RMENU)) {
      result = true;
    }
    if ((state & KeyModifierSuper) != 0 && (virtKey == VK_LWIN || virtKey == VK_RWIN)) {
      result = true;
    }
  }

  return result;
}
