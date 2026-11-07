/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsDesks.h"
#include "MSWindowsPolicies.h"

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

// seconds to wait for the desk thread to acknowledge an injected input
// message; bounds the caller so a stalled desk thread cannot hang the
// core event thread forever
constexpr double kDeskReadyTimeout = 2.0;

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

//
// MSWindowsDesks
//

MSWindowsDesks::MSWindowsDesks(
    bool isPrimary, bool useHooks, const IScreenSaver *screensaver, IEventQueue *events, IJob *updateKeys
)
    : m_isPrimary(isPrimary),
      m_useHooks(useHooks),
      m_isOnScreen(m_isPrimary),
      m_screensaver(screensaver),
      m_deskReady(&m_mutex, false),
      m_updateKeys(updateKeys),
      m_events(events)
{

  m_cursor = createBlankCursor();
  m_deskClass = createDeskWindowClass(m_isPrimary);
  m_keyLayout = AppUtilWindows::instance().getCurrentKeyboardLayout();
  resetOptions();
}

MSWindowsDesks::~MSWindowsDesks()
{
  disable();
  destroyClass(m_deskClass);
  destroyCursor(m_cursor);
  delete m_updateKeys;
}

void MSWindowsDesks::enable()
{
  m_threadID = GetCurrentThreadId();

  // set the active desk and (re)install the hooks
  checkDesk();

  // install the desk timer.  this timer periodically checks
  // which desk is active and reinstalls the hooks as necessary.
  // we wouldn't need this if windows notified us of a desktop
  // change but as far as i can tell it doesn't.
  m_timer = m_events->newTimer(0.2, nullptr);
  m_events->addHandler(EventTypes::Timer, m_timer, [this](const auto &) { handleCheckDesk(); });

  updateKeys();
}

void MSWindowsDesks::disable()
{
  // remove timer
  if (m_timer != nullptr) {
    m_events->removeHandler(EventTypes::Timer, m_timer);
    m_events->deleteTimer(m_timer);
    m_timer = nullptr;
  }

  // destroy desks
  removeDesks();

  m_isOnScreen = m_isPrimary;
}

void MSWindowsDesks::enter()
{
  sendMessage(SYNERGY_MSG_ENTER, 0, 0);
}

void MSWindowsDesks::leave(HKL keyLayout)
{
  sendMessage(SYNERGY_MSG_LEAVE, (WPARAM)keyLayout, 0);
}

void MSWindowsDesks::resetOptions()
{
  m_leaveForegroundOption = false;
}

void MSWindowsDesks::setOptions(const OptionsList &options)
{
  for (uint32_t i = 0, n = (uint32_t)options.size(); i < n; i += 2) {
    if (options[i] == kOptionWin32KeepForeground) {
      m_leaveForegroundOption = (options[i + 1] != 0);
      LOG_VERBOSE("%s the foreground window", m_leaveForegroundOption ? "don\'t grab" : "grab");
    }
  }
}

void MSWindowsDesks::updateKeys()
{
  sendMessage(SYNERGY_MSG_SYNC_KEYS, 0, 0);
}

void MSWindowsDesks::setShape(
    int32_t x, int32_t y, int32_t width, int32_t height, int32_t xCenter, int32_t yCenter, bool isMultimon
)
{
  m_x = x;
  m_y = y;
  m_w = width;
  m_h = height;
  m_xCenter = xCenter;
  m_yCenter = yCenter;
  m_multimon = isMultimon;
}

void MSWindowsDesks::installScreensaverHooks(bool install)
{
  if (m_isPrimary && m_screensaverNotify != install) {
    m_screensaverNotify = install;
    sendMessage(SYNERGY_MSG_SCREENSAVER, install, 0);
  }
}

void MSWindowsDesks::fakeInputBegin()
{
  sendMessage(SYNERGY_MSG_FAKE_INPUT, 1, 0);
}

void MSWindowsDesks::fakeInputEnd()
{
  sendMessage(SYNERGY_MSG_FAKE_INPUT, 0, 0);
}

void MSWindowsDesks::getCursorPos(int32_t &x, int32_t &y) const
{
  POINT pos{0, 0};
  sendMessage(SYNERGY_MSG_CURSOR_POS, reinterpret_cast<WPARAM>(&pos), 0);
  x = pos.x;
  y = pos.y;
}

void MSWindowsDesks::fakeKeyEvent(WORD virtualKey, WORD scanCode, DWORD flags, bool /*isAutoRepeat*/) const
{
  sendMessage(SYNERGY_MSG_FAKE_KEY, flags, MAKELPARAM(scanCode, virtualKey));
}

void MSWindowsDesks::fakeMouseButton(ButtonID button, bool press)
{
  // the system will swap the meaning of left/right for us if
  // the user has configured a left-handed mouse but we don't
  // want it to swap since we want the handedness of the
  // server's mouse.  so pre-swap for a left-handed mouse.
  if (GetSystemMetrics(SM_SWAPBUTTON)) {
    switch (button) {
    case kButtonLeft:
      button = kButtonRight;
      break;

    case kButtonRight:
      button = kButtonLeft;
      break;
    }
  }

  // map button id to button flag and button data
  DWORD data = 0;
  DWORD flags;
  switch (button) {
  case kButtonLeft:
    flags = press ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    break;

  case kButtonMiddle:
    flags = press ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    break;

  case kButtonRight:
    flags = press ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    break;

  case kButtonExtra0:
    data = XBUTTON1;
    flags = press ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    break;

  case kButtonExtra1:
    data = XBUTTON2;
    flags = press ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    break;

  default:
    return;
  }

  // do it
  sendMessage(SYNERGY_MSG_FAKE_BUTTON, flags, data);
}

void MSWindowsDesks::fakeMouseMove(int32_t x, int32_t y) const
{
  sendMessage(SYNERGY_MSG_FAKE_MOVE, static_cast<WPARAM>(x), static_cast<LPARAM>(y));
}

void MSWindowsDesks::fakeMouseRelativeMove(int32_t dx, int32_t dy) const
{
  sendMessage(SYNERGY_MSG_FAKE_REL_MOVE, static_cast<WPARAM>(dx), static_cast<LPARAM>(dy));
}

void MSWindowsDesks::fakeMouseWheel(int32_t xDelta, int32_t yDelta) const
{
  sendMessage(SYNERGY_MSG_FAKE_WHEEL, xDelta, yDelta);
}

void MSWindowsDesks::sendMessage(UINT msg, WPARAM wParam, LPARAM lParam) const
{
  if (m_activeDesk != nullptr && m_activeDesk->m_window != nullptr) {
    if (!PostThreadMessage(m_activeDesk->m_threadID, msg, wParam, lParam)) {
      LOG_WARN("could not post message %i to desk thread (error: %i)", msg, GetLastError());
      return;
    }
    waitForDesk();
  }
}

HCURSOR
MSWindowsDesks::createBlankCursor() const
{
  // create a transparent cursor
  int cw = GetSystemMetrics(SM_CXCURSOR);
  int ch = GetSystemMetrics(SM_CYCURSOR);
  uint8_t *cursorAND = new uint8_t[ch * ((cw + 31) >> 2)];
  uint8_t *cursorXOR = new uint8_t[ch * ((cw + 31) >> 2)];
  memset(cursorAND, 0xff, ch * ((cw + 31) >> 2));
  memset(cursorXOR, 0x00, ch * ((cw + 31) >> 2));
  HCURSOR c = CreateCursor(MSWindowsScreen::getWindowInstance(), 0, 0, cw, ch, cursorAND, cursorXOR);
  delete[] cursorXOR;
  delete[] cursorAND;
  return c;
}

void MSWindowsDesks::destroyCursor(HCURSOR cursor) const
{
  if (cursor != nullptr) {
    DestroyCursor(cursor);
  }
}

ATOM MSWindowsDesks::createDeskWindowClass(bool isPrimary) const
{
  WNDCLASSEX classInfo;
  classInfo.cbSize = sizeof(classInfo);
  classInfo.style = CS_DBLCLKS | CS_NOCLOSE;
  classInfo.lpfnWndProc = isPrimary ? &MSWindowsDesks::primaryDeskProc : &MSWindowsDesks::secondaryDeskProc;
  classInfo.cbClsExtra = 0;
  classInfo.cbWndExtra = 0;
  classInfo.hInstance = MSWindowsScreen::getWindowInstance();
  classInfo.hIcon = nullptr;
  classInfo.hCursor = m_cursor;
  classInfo.hbrBackground = nullptr;
  classInfo.lpszMenuName = nullptr;
  classInfo.lpszClassName = kDeskClassNameW;
  classInfo.hIconSm = nullptr;
  return RegisterClassEx(&classInfo);
}

void MSWindowsDesks::destroyClass(ATOM windowClass) const
{
  if (windowClass != 0) {
    UnregisterClass(MAKEINTATOM(windowClass), MSWindowsScreen::getWindowInstance());
  }
}

HWND MSWindowsDesks::createWindow(ATOM windowClass, const wchar_t *name) const
{
  HWND window = CreateWindowEx(
      WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, MAKEINTATOM(windowClass), name, WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
      MSWindowsScreen::getWindowInstance(), nullptr
  );
  if (window == nullptr) {
    LOG_ERR("failed to create window: %d", GetLastError());
    throw ScreenOpenFailureException();
  }
  return window;
}

void MSWindowsDesks::destroyWindow(HWND hwnd) const
{
  if (hwnd != nullptr) {
    DestroyWindow(hwnd);
  }
}

LRESULT CALLBACK MSWindowsDesks::primaryDeskProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MSWindowsDesks::secondaryDeskProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  // would like to detect any local user input and hide the hider
  // window but for now we just detect mouse motion.
  bool hide = false;
  switch (msg) {
  case WM_MOUSEMOVE:
    if (LOWORD(lParam) != 0 || HIWORD(lParam) != 0) {
      hide = true;
    }
    break;
  }

  if (hide && IsWindowVisible(hwnd)) {
    ReleaseCapture();
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_HIDEWINDOW);
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

MSWindowsDesks::Desk *MSWindowsDesks::addDesk(const std::wstring &name, HDESK hdesk)
{
  Desk *desk = new Desk;
  desk->m_name = name;
  desk->m_desk = hdesk;
  desk->m_targetID = GetCurrentThreadId();
  desk->m_thread = new Thread(new TMethodJob<MSWindowsDesks>(this, &MSWindowsDesks::deskThread, desk));
  waitForDesk();
  m_desks.insert(std::make_pair(name, desk));
  return desk;
}

void MSWindowsDesks::removeDesks()
{
  for (Desks::iterator index = m_desks.begin(); index != m_desks.end(); ++index) {
    Desk *desk = index->second;
    PostThreadMessage(desk->m_threadID, WM_QUIT, 0, 0);
    desk->m_thread->wait();
    delete desk->m_thread;
    delete desk;
  }
  m_desks.clear();
  m_activeDesk = nullptr;
  m_activeDeskName = L"";
}

void MSWindowsDesks::checkDesk()
{
  // get current desktop.  if we already know about it then return.
  Desk *desk;
  HDESK hdesk = openInputDesktop();
  std::wstring name = getDesktopName(hdesk);
  Desks::const_iterator index = m_desks.find(name);
  if (index == m_desks.end()) {
    desk = addDesk(name, hdesk);
    // hold on to hdesk until thread exits so the desk can't
    // be removed by the system
  } else {
    closeDesktop(hdesk);
    desk = index->second;
  }

  // if active desktop changed then tell the old and new desk threads
  // about the change.  don't switch desktops when the screensaver is
  // active becaue we'd most likely switch to the screensaver desktop
  // which would have the side effect of forcing the screensaver to
  // stop.
  const bool changed = name != m_activeDeskName;
  const auto action = synergy::windows::desktopAction(changed, changed && m_screensaver->isActive());
  if (action == synergy::windows::DesktopAction::Switch) {
    // show cursor on previous desk
    bool wasOnScreen = m_isOnScreen;
    if (!wasOnScreen) {
      sendMessage(SYNERGY_MSG_ENTER, 0, 0);
    }

    // always sync keys when switching desks to ensure keyboard modifier
    // states are correct.
    LOG_DEBUG("switched to desk \"%ls\"", name.c_str());
    bool syncKeys = false;
    if (synergy::windows::shouldSyncDesktopKeys(action, isDeskAccessible(desk))) {
      LOG_DEBUG("desktop is accessible, syncing keyboard state after desk switch");
      syncKeys = true;
    } else {
      LOG_DEBUG("desktop is inaccessible");
    }

    // switch desk
    m_activeDesk = desk;
    m_activeDeskName = name;
    sendMessage(SYNERGY_MSG_SWITCH, 0, 0);

    // hide cursor on new desk
    if (!wasOnScreen) {
      sendMessage(SYNERGY_MSG_LEAVE, (WPARAM)m_keyLayout, 0);
    }

    // update keys if necessary
    if (syncKeys) {
      updateKeys();
    }
  } else if (action == synergy::windows::DesktopAction::NotifyScreensaver) {
    // screen saver might have started
    PostThreadMessage(m_threadID, SYNERGY_MSG_SCREEN_SAVER, TRUE, 0);
  }
}

bool MSWindowsDesks::isDeskAccessible(const Desk *desk) const
{
  return (desk != nullptr && desk->m_desk != nullptr);
}

void MSWindowsDesks::waitForDesk() const
{
  MSWindowsDesks *self = const_cast<MSWindowsDesks *>(this);

  Lock lock(&m_mutex);
  // bound the wait so a stalled desk thread cannot hang the caller forever
  while (!(bool)m_deskReady) {
    if (!m_deskReady.wait(kDeskReadyTimeout)) {
      LOG_ERR("timed out waiting for desk thread; desk may be stalled");
      break;
    }
  }
  self->m_deskReady = false;
}

void MSWindowsDesks::handleCheckDesk()
{
  checkDesk();

  // also check if screen saver is running if on a modern OS and
  // this is the primary screen.
  if (m_isPrimary) {
    BOOL running;
    SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &running, FALSE);
    PostThreadMessage(m_threadID, SYNERGY_MSG_SCREEN_SAVER, running, 0);
  }
}

HDESK
MSWindowsDesks::openInputDesktop()
{
  return OpenInputDesktop(DF_ALLOWOTHERACCOUNTHOOK, TRUE, DESKTOP_CREATEWINDOW | DESKTOP_HOOKCONTROL | GENERIC_WRITE);
}

void MSWindowsDesks::closeDesktop(HDESK desk)
{
  if (desk != nullptr) {
    CloseDesktop(desk);
  }
}

std::wstring MSWindowsDesks::getDesktopName(HDESK desk)
{
  if (desk == nullptr) {
    return std::wstring();
  } else {
    DWORD size;
    GetUserObjectInformation(desk, UOI_NAME, nullptr, 0, &size);
    TCHAR *name = (TCHAR *)alloca(size + sizeof(TCHAR));
    GetUserObjectInformation(desk, UOI_NAME, name, size, &size);
    std::wstring result(name);
    return result;
  }
}

HWND MSWindowsDesks::getForegroundWindow() const
{
  // Ideally we'd return nullptr as much as possible, only returning
  // the actual foreground window when we know it's going to mess
  // up our keyboard input.  For now we'll just let the user
  // decide.
  if (m_leaveForegroundOption) {
    return nullptr;
  }
  return GetForegroundWindow();
}
