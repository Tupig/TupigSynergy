/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsDesks.h"

#include "IScreenSaver.h"
#include "MSWindowsHook.h"
#include "MSWindowsScreen.h"
#include "ScreenException.h"
#include "arch/Arch.h"
#include "base/IEventQueue.h"
#include "base/IJob.h"
#include "base/Log.h"
#include "base/TMethodJob.h"
#include "common/Constants.h"
#include "mt/Lock.h"
#include "mt/Thread.h"
#include "win32/AppUtilWindows.h"

#include <malloc.h>

// these are only defined when WINVER >= 0x0500
#if !defined(SPI_GETMOUSESPEED)
#define SPI_GETMOUSESPEED 112
#endif
#if !defined(SPI_SETMOUSESPEED)
#define SPI_SETMOUSESPEED 113
#endif
#if !defined(SPI_GETSCREENSAVERRUNNING)
#define SPI_GETSCREENSAVERRUNNING 114
#endif

#if !defined(MOUSEEVENTF_HWHEEL)
#define MOUSEEVENTF_HWHEEL 0x1000
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

// <unused>; <unused>
#define SYNERGY_MSG_SWITCH SYNERGY_HOOK_LAST_MSG + 1
// <unused>; <unused>
#define SYNERGY_MSG_ENTER SYNERGY_HOOK_LAST_MSG + 2
// <unused>; <unused>
#define SYNERGY_MSG_LEAVE SYNERGY_HOOK_LAST_MSG + 3
// wParam = flags, HIBYTE(lParam) = virtual key, LOBYTE(lParam) = scan code
#define SYNERGY_MSG_FAKE_KEY SYNERGY_HOOK_LAST_MSG + 4
// flags, XBUTTON id
#define SYNERGY_MSG_FAKE_BUTTON SYNERGY_HOOK_LAST_MSG + 5
// x; y
#define SYNERGY_MSG_FAKE_MOVE SYNERGY_HOOK_LAST_MSG + 6
// xDelta; yDelta
#define SYNERGY_MSG_FAKE_WHEEL SYNERGY_HOOK_LAST_MSG + 7
// POINT*; <unused>
#define SYNERGY_MSG_CURSOR_POS SYNERGY_HOOK_LAST_MSG + 8
// IKeyState*; <unused>
#define SYNERGY_MSG_SYNC_KEYS SYNERGY_HOOK_LAST_MSG + 9
// install; <unused>
#define SYNERGY_MSG_SCREENSAVER SYNERGY_HOOK_LAST_MSG + 10
// dx; dy
#define SYNERGY_MSG_FAKE_REL_MOVE SYNERGY_HOOK_LAST_MSG + 11
// enable; <unused>
#define SYNERGY_MSG_FAKE_INPUT SYNERGY_HOOK_LAST_MSG + 12

static void send_keyboard_input(WORD wVk, WORD wScan, DWORD dwFlags)
{
  INPUT inp;
  inp.type = INPUT_KEYBOARD;
  inp.ki.wVk = (dwFlags & KEYEVENTF_UNICODE) ? 0 : wVk; // 1..254 inclusive otherwise
  inp.ki.wScan = wScan;
  inp.ki.dwFlags = dwFlags & 0xF;
  inp.ki.time = 0;
  inp.ki.dwExtraInfo = 0;
  if (SendInput(1, &inp, sizeof(inp)) == 0) {
    static bool s_loggedBlock = false;
    if (!s_loggedBlock) {
      s_loggedBlock = true;
      LOG_WARN("SendInput blocked, input dropped (target window may run elevated)");
    }
  }
}

void send_mouse_input(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData)
{
  INPUT inp;
  inp.type = INPUT_MOUSE;
  inp.mi.dwFlags = dwFlags;
  inp.mi.dx = dx;
  inp.mi.dy = dy;
  inp.mi.mouseData = dwData;
  inp.mi.time = 0;
  inp.mi.dwExtraInfo = 0;
  if (SendInput(1, &inp, sizeof(inp)) == 0) {
    static bool s_loggedBlock = false;
    if (!s_loggedBlock) {
      s_loggedBlock = true;
      LOG_WARN("SendInput blocked, input dropped (target window may run elevated)");
    }
  }
}

void MSWindowsDesks::deskMouseMove(int32_t x, int32_t y) const
{
  // when using absolute positioning with mouse_event(),
  // the normalized device coordinates range over only
  // the primary screen.
  int32_t w = GetSystemMetrics(SM_CXSCREEN);
  int32_t h = GetSystemMetrics(SM_CYSCREEN);
  send_mouse_input(
      MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, (DWORD)((65535.0f * x) / (w - 1) + 0.5f),
      (DWORD)((65535.0f * y) / (h - 1) + 0.5f), 0
  );
}

void MSWindowsDesks::deskMouseRelativeMove(int32_t dx, int32_t dy) const
{
  // relative moves are subject to cursor acceleration which we don't
  // want.so we disable acceleration, do the relative move, then
  // restore acceleration.  there's a slight chance we'll end up in
  // the wrong place if the user moves the cursor using this system's
  // mouse while simultaneously moving the mouse on the server
  // system.  that defeats the purpose of Synergy so we'll assume
  // that won't happen.  even if it does, the next mouse move will
  // correct the position.

  // save mouse speed & acceleration
  int oldSpeed[4];
  bool accelChanged =
      SystemParametersInfo(SPI_GETMOUSE, 0, oldSpeed, 0) && SystemParametersInfo(SPI_GETMOUSESPEED, 0, oldSpeed + 3, 0);

  // use 1:1 motion
  if (accelChanged) {
    int newSpeed[4] = {0, 0, 0, 1};
    accelChanged = SystemParametersInfo(SPI_SETMOUSE, 0, newSpeed, 0) ||
                   SystemParametersInfo(SPI_SETMOUSESPEED, 0, newSpeed + 3, 0);
  }

  // move relative to mouse position
  send_mouse_input(MOUSEEVENTF_MOVE, dx, dy, 0);

  // restore mouse speed & acceleration
  if (accelChanged) {
    SystemParametersInfo(SPI_SETMOUSE, 0, oldSpeed, 0);
    SystemParametersInfo(SPI_SETMOUSESPEED, 0, oldSpeed + 3, 0);
  }
}

/*!
 * Wraps the `ShowCursor` function and calls it repeatedly until the cursor visibility is at
 * the desired state. Windows maintains an internal counter for cursor visibility, and only
 * shows or hides the cursor when it reaches a certain threshold.
 */
void setCursorVisibility(bool visible)
{
  LOG_DEBUG("%s cursor", visible ? "showing" : "hiding");

  const int max = 10;
  int attempts = 0;
  while (attempts++ < max) {
    const auto displayCounter = ShowCursor(visible ? TRUE : FALSE);
    LOG_VERBOSE("cursor display counter: %d", displayCounter);

    if (visible) {
      if (displayCounter < 0) {
        LOG_VERBOSE("cursor still hidden, retrying, attempt: %d", attempts);
      } else {
        LOG_VERBOSE("cursor is now visible, attempts: %d", attempts);
        return;
      }
    } else {
      if (displayCounter >= 0) {
        LOG_VERBOSE("cursor still visible, retrying, attempt: %d", attempts);
      } else {
        LOG_VERBOSE("cursor is now hidden, attempts: %d", attempts);
        return;
      }
    }
  }

  LOG_ERR("unable to set cursor visibility after %d attempts", attempts);
}

void MSWindowsDesks::deskEnter(Desk *desk)
{
  if (!m_isPrimary) {
    ReleaseCapture();
  }

  setCursorVisibility(true);

  SetWindowPos(desk->m_window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_HIDEWINDOW);

  // restore the foreground window
  // XXX -- this raises the window to the top of the Z-order.  we
  // want it to stay wherever it was to properly support X-mouse
  // (mouse over activation) but i've no idea how to do that.
  // the obvious workaround of using SetWindowPos() to move it back
  // after being raised doesn't work.
  DWORD thisThread = GetWindowThreadProcessId(desk->m_window, nullptr);
  DWORD thatThread = GetWindowThreadProcessId(desk->m_foregroundWindow, nullptr);
  AttachThreadInput(thatThread, thisThread, TRUE);
  SetForegroundWindow(desk->m_foregroundWindow);
  AttachThreadInput(thatThread, thisThread, FALSE);
  EnableWindow(desk->m_window, desk->m_lowLevel ? FALSE : TRUE);
  desk->m_foregroundWindow = nullptr;
}

void MSWindowsDesks::deskLeave(Desk *desk, HKL keyLayout)
{
  setCursorVisibility(false);

  if (m_isPrimary) {
    // map a window to hide the cursor and to use whatever keyboard
    // layout we choose rather than the keyboard layout of the last
    // active window.
    int x, y, w, h;
    if (desk->m_lowLevel) {
      // with a low level hook the cursor will never budge so
      // just a 1x1 window is sufficient.
      x = m_xCenter;
      y = m_yCenter;
      w = 1;
      h = 1;
    } else {
      // with regular hooks the cursor will jitter as it's moved
      // by the user then back to the center by us.  to be sure
      // we never lose it, cover all the monitors with the window.
      x = m_x;
      y = m_y;
      w = m_w;
      h = m_h;
    }
    SetWindowPos(desk->m_window, HWND_TOP, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // switch to requested keyboard layout
    ActivateKeyboardLayout(keyLayout, 0);

    // if not using low-level hooks we have to also activate the
    // window to ensure we don't lose keyboard focus.
    // FIXME -- see if this can be avoided.  if so then always
    // disable the window (see handling of SYNERGY_MSG_SWITCH).
    if (!desk->m_lowLevel) {
      SetActiveWindow(desk->m_window);
    }

    // if using low-level hooks then disable the foreground window
    // so it can't mess up any of our keyboard events.  the console
    // program, for example, will cause characters to be reported as
    // unshifted, regardless of the shift key state.  interestingly
    // we do see the shift key go down and up.
    //
    // note that we must enable the window to activate it and we
    // need to disable the window on deskEnter.
    else {
      desk->m_foregroundWindow = getForegroundWindow();
      if (desk->m_foregroundWindow != nullptr) {
        EnableWindow(desk->m_window, TRUE);
        SetActiveWindow(desk->m_window);
        DWORD thisThread = GetWindowThreadProcessId(desk->m_window, nullptr);
        DWORD thatThread = GetWindowThreadProcessId(desk->m_foregroundWindow, nullptr);

        AttachThreadInput(thatThread, thisThread, TRUE);
        SetForegroundWindow(desk->m_window);
        AttachThreadInput(thatThread, thisThread, FALSE);
      }
    }
  } else {
    // move hider window under the cursor center, raise, and show it
    SetWindowPos(desk->m_window, HWND_TOP, m_xCenter, m_yCenter, 1, 1, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // watch for mouse motion.  if we see any then we hide the
    // hider window so the user can use the physically attached
    // mouse if desired.  we'd rather not capture the mouse but
    // we aren't notified when the mouse leaves our window.
    SetCapture(desk->m_window);

    // windows can take a while to hide the cursor, so wait a few milliseconds to ensure the cursor
    // is hidden before centering. this doesn't seem to affect the fluidity of the transition.
    // without this, the cursor appears to flicker in the center of the screen which is annoying.
    // a slightly more elegant but complex solution could be to use a timed event.
    // 30 ms seems to work well enough without making the transition feel janky; a lower number
    // would be better but 10 ms doesn't seem to be quite long enough, as we get noticeable flicker.
    // this is largely a balance and out of our control, since windows can be unpredictable...
    // maybe another approach would be to repeatedly check the cursor visibility until it is hidden.
    LOG_VERBOSE("centering cursor on leave: %+d,%+d", m_xCenter, m_yCenter);
    ARCH->sleep(0.03);
    deskMouseMove(m_xCenter, m_yCenter);
  }
}

void MSWindowsDesks::deskThread(const void *vdesk)
{
  MSG msg;

  // use given desktop for this thread
  Desk *desk = const_cast<Desk *>(static_cast<const Desk *>(vdesk));
  desk->m_threadID = GetCurrentThreadId();
  desk->m_window = nullptr;
  desk->m_foregroundWindow = nullptr;
  if (desk->m_desk != nullptr && SetThreadDesktop(desk->m_desk) != 0) {
    // create a message queue
    PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE);

    // create a window.  we use this window to hide the cursor.
    try {
      desk->m_window = createWindow(m_deskClass, kDeskClassNameW);
      LOG_DEBUG("desk %ls window is 0x%08x", desk->m_name.c_str(), desk->m_window);
    } catch (...) {
      // ignore
      LOG_DEBUG("can't create desk window for %ls", desk->m_name.c_str());
    }
  }

  // tell main thread that we're ready
  {
    Lock lock(&m_mutex);
    m_deskReady = true;
    m_deskReady.broadcast();
  }

  while (GetMessage(&msg, nullptr, 0, 0)) {
    switch (msg.message) {
    default:
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      continue;

    case SYNERGY_MSG_SWITCH:
      if (m_useHooks) {
        MSWindowsHook::uninstall();
        if (m_screensaverNotify) {
          MSWindowsHook::uninstallScreenSaver();
          MSWindowsHook::installScreenSaver();
        }
        switch (MSWindowsHook::install()) {
        case kHOOK_FAILED:
          // we won't work on this desk
          desk->m_lowLevel = false;
          break;

        case kHOOK_OKAY:
          desk->m_lowLevel = false;
          break;

        case kHOOK_OKAY_LL:
          desk->m_lowLevel = true;
          break;
        }

        // a window on the primary screen with low-level hooks
        // should never activate.
        if (desk->m_window)
          EnableWindow(desk->m_window, desk->m_lowLevel ? FALSE : TRUE);
      }
      break;

    case SYNERGY_MSG_ENTER:
      m_isOnScreen = true;
      deskEnter(desk);
      break;

    case SYNERGY_MSG_LEAVE:
      m_isOnScreen = false;
      m_keyLayout = (HKL)msg.wParam;
      deskLeave(desk, m_keyLayout);
      break;

    case SYNERGY_MSG_FAKE_KEY:
      // Note, this is intended to be HI/LOWORD and not HI/LOBYTE
      send_keyboard_input(HIWORD(msg.lParam), LOWORD(msg.lParam), (DWORD)msg.wParam);
      break;

    case SYNERGY_MSG_FAKE_BUTTON:
      if (msg.wParam != 0) {
        send_mouse_input((DWORD)msg.wParam, 0, 0, (DWORD)msg.lParam);
      }
      break;

    case SYNERGY_MSG_FAKE_MOVE:
      deskMouseMove(static_cast<int32_t>(msg.wParam), static_cast<int32_t>(msg.lParam));
      break;

    case SYNERGY_MSG_FAKE_REL_MOVE:
      deskMouseRelativeMove(static_cast<int32_t>(msg.wParam), static_cast<int32_t>(msg.lParam));
      break;

    case SYNERGY_MSG_FAKE_WHEEL:
      // XXX -- add support for x-axis scrolling
      if (msg.lParam != 0) {
        send_mouse_input(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)msg.lParam);
      }
      if (msg.wParam != 0) {
        send_mouse_input(MOUSEEVENTF_HWHEEL, 0, 0, (DWORD)msg.wParam);
      }
      break;

    case SYNERGY_MSG_CURSOR_POS: {
      POINT *pos = reinterpret_cast<POINT *>(msg.wParam);
      if (!GetCursorPos(pos)) {
        pos->x = m_xCenter;
        pos->y = m_yCenter;
      }
      break;
    }

    case SYNERGY_MSG_SYNC_KEYS:
      m_updateKeys->run();
      break;

    case SYNERGY_MSG_SCREENSAVER:
      if (m_useHooks) {
        if (msg.wParam != 0) {
          MSWindowsHook::installScreenSaver();
        } else {
          MSWindowsHook::uninstallScreenSaver();
        }
      }
      break;

    case SYNERGY_MSG_FAKE_INPUT:
      send_keyboard_input(
          SYNERGY_HOOK_FAKE_INPUT_VIRTUAL_KEY, SYNERGY_HOOK_FAKE_INPUT_SCANCODE, msg.wParam ? 0 : KEYEVENTF_KEYUP
      );
      break;
    }

    // notify that message was processed
    Lock lock(&m_mutex);
    m_deskReady = true;
    m_deskReady.broadcast();
  }

  // clean up
  deskEnter(desk);
  if (desk->m_window != nullptr) {
    DestroyWindow(desk->m_window);
  }
  if (desk->m_desk != nullptr) {
    closeDesktop(desk->m_desk);
  }
}
