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
#define COMPILE_MULTIMON_STUBS
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

HINSTANCE MSWindowsScreen::s_windowInstance = nullptr;
MSWindowsScreen *MSWindowsScreen::s_screen = nullptr;

MSWindowsScreen::MSWindowsScreen(bool isPrimary, bool useHooks, IEventQueue *events, bool enableLangSync)
    : PlatformScreen(events),
      m_isPrimary(isPrimary),
      m_useHooks(useHooks),
      m_isOnScreen(m_isPrimary),
      m_hasMouse(GetSystemMetrics(SM_MOUSEPRESENT) != 0),
      m_events(events)
{
  LOG_DEBUG("settting up %s screen", m_isPrimary ? "primary" : "secondary");

  assert(s_windowInstance != nullptr);
  assert(s_screen == nullptr);

  s_screen = this;
  bool oleInitialized = false;
  try {
    if (m_isPrimary && m_useHooks) {
      m_hook.loadLibrary();
    }

    m_screensaver = new MSWindowsScreenSaver();
    m_desks = new MSWindowsDesks(
        m_isPrimary, m_useHooks, m_screensaver, m_events,
        new TMethodJob<MSWindowsScreen>(this, &MSWindowsScreen::updateKeysCB)
    );
    m_keyState = new MSWindowsKeyState(
        m_desks, getEventTarget(), m_events, AppUtil::instance().getKeyboardLayoutList(), enableLangSync
    );

    updateScreenShape();
    m_class = createWindowClass();
    m_window = createWindow(m_class, LPCWSTR(kAppName));
    setupMouseKeys();
    LOG_DEBUG("screen shape: %d,%d %dx%d %s", m_x, m_y, m_w, m_h, m_multimon ? "(multi-monitor)" : "");
    LOG_DEBUG("window is 0x%08x", m_window);

    if (Settings::value(Settings::Core::PreventSleep).toBool()) {
      m_powerManager.disableSleep();
    }

    oleInitialized = SUCCEEDED(OleInitialize(0));

    // Separate drop window: the main window is 1x1 and unsuitable as an OLE
    // target. RegisterDragDrop requires OleInitialize first.
    m_dropWindow = createDropWindow(m_class);
    m_dropTarget = new MSWindowsDropTarget();
    const HRESULT dropHr = RegisterDragDrop(m_dropWindow, m_dropTarget);
    if (FAILED(dropHr)) {
      LOG_ERR("RegisterDragDrop failed: 0x%08lx", dropHr);
      m_dropTarget->Release();
      m_dropTarget = nullptr;
      destroyWindow(m_dropWindow);
      m_dropWindow = nullptr;
    }
  } catch (...) {
    if (m_dropTarget != nullptr) {
      if (m_dropWindow != nullptr) {
        RevokeDragDrop(m_dropWindow);
      }
      m_dropTarget->Release();
      m_dropTarget = nullptr;
    }
    destroyWindow(m_dropWindow);
    m_dropWindow = nullptr;
    if (oleInitialized) {
      OleUninitialize();
    }
    delete m_keyState;
    delete m_desks;
    delete m_screensaver;
    destroyWindow(m_window);
    destroyClass(m_class);
    s_screen = nullptr;
    throw;
  }

  // install event handlers
  m_events->addHandler(EventTypes::System, m_events->getSystemTarget(), [this](const auto &e) {
    handleSystemEvent(e);
  });

  // install the platform event queue
  m_events->adoptBuffer(new MSWindowsEventQueueBuffer(m_events));
}

MSWindowsScreen::~MSWindowsScreen()
{
  assert(s_screen != nullptr);

  disable();
  m_events->adoptBuffer(nullptr);
  m_events->removeHandler(EventTypes::System, m_events->getSystemTarget());
  delete m_keyState;
  delete m_desks;
  delete m_screensaver;

  if (m_dropTarget != nullptr) {
    if (m_dropWindow != nullptr) {
      RevokeDragDrop(m_dropWindow);
    }
    m_dropTarget->Release();
    m_dropTarget = nullptr;
  }
  destroyWindow(m_dropWindow);
  m_dropWindow = nullptr;

  destroyWindow(m_window);
  destroyClass(m_class);

  OleUninitialize();

  s_screen = nullptr;
}

void MSWindowsScreen::init(HINSTANCE windowInstance)
{
  assert(s_windowInstance == nullptr);
  assert(windowInstance != nullptr);

  s_windowInstance = windowInstance;
}

HINSTANCE
MSWindowsScreen::getWindowInstance()
{
  return s_windowInstance;
}

void MSWindowsScreen::enable()
{
  LOG_DEBUG("enabling %s screen", m_isPrimary ? "primary" : "secondary");
  m_isEnabled = true;

  assert(m_isOnScreen == m_isPrimary);

  // we need to poll some things to fix them
  m_fixTimer = m_events->newTimer(1.0, nullptr);
  m_events->addHandler(EventTypes::Timer, m_fixTimer, [this](const auto &) { handleFixes(); });

  // install our clipboard snooper
  if (!AddClipboardFormatListener(m_window)) {
    LOG_WARN("failed to add the clipboard format listener: %d", GetLastError());
  }

  // track the active desk and (re)install the hooks
  m_desks->enable();

  if (m_isPrimary) {
    // set jump zones
    m_hook.setZone(m_x, m_y, m_w, m_h, getJumpZoneSize());

    // watch jump zones
    m_hook.setMode(kHOOK_WATCH_JUMP_ZONE);
  }
}

void MSWindowsScreen::disable()
{
  // Idempotent: the matching enable() may already have been undone.
  //
  // Screen::~Screen() disables the platform screen (guarded by its own m_enabled)
  // and then deletes it, and ~MSWindowsScreen() calls disable() unconditionally.
  // Without this guard a normal shutdown therefore reaches the body below twice,
  // which re-runs every teardown step: desks->disable(), the hook mode, key state,
  // the fix timer, and RemoveClipboardFormatListener(). The repeated clipboard
  // removal is the visible symptom - removing a listener that is no longer
  // registered fails with ERROR_INVALID_PARAMETER (87), logging a warning that
  // suggests a clipboard problem where there is none. The other repeated calls
  // happen to be tolerant, but they are just as unintended.
  //
  // Every resource released below is also released by the destructor or is
  // already null-safe, so returning early loses no cleanup.
  if (!m_isEnabled) {
    LOG_DEBUG("screen already disabled, skipping");
    return;
  }

  LOG_DEBUG("disabling %s screen", m_isPrimary ? "primary" : "secondary");
  m_isEnabled = false;

  // stop tracking the active desk
  m_desks->disable();

  if (m_isPrimary) {
    // disable hooks
    m_hook.setMode(kHOOK_DISABLE);

    // enable special key sequences on win95 family
    enableSpecialKeys(true);
  }

  // tell key state
  m_keyState->disable();

  // stop snooping the clipboard
  if (!RemoveClipboardFormatListener(m_window)) {
    LOG_WARN("failed to remove the clipboard format listener: %d", GetLastError());
  }

  // uninstall fix timer
  if (m_fixTimer != nullptr) {
    m_events->removeHandler(EventTypes::Timer, m_fixTimer);
    m_events->deleteTimer(m_fixTimer);
    m_fixTimer = nullptr;
  }

  m_isOnScreen = m_isPrimary;
}

void MSWindowsScreen::enter()
{
  m_desks->enter();
  if (m_isPrimary) {
    // enable special key sequences on win95 family
    enableSpecialKeys(true);

    // watch jump zones
    m_hook.setMode(kHOOK_WATCH_JUMP_ZONE);

    // all messages prior to now are invalid
    nextMark();

    m_primaryKeyDownList.clear();
  } else {
    // Entering a secondary screen. Ensure that no screensaver is active
    // and that the screen is not in powersave mode.
    ArchMiscWindows::wakeupDisplay();

    if (m_screensaver != nullptr && m_screensaverActive) {
      m_screensaver->deactivate();
      m_screensaverActive = 0;
    }
  }

  // now on screen
  m_isOnScreen = true;
  setupMouseKeys();
}

bool MSWindowsScreen::canLeave()
{
  POINT pos;
  if (!getThisCursorPos(&pos)) {
    // prevent screen leave when cursor position is not available; if unable to get cursor position,
    // screen will become inaccessible if the cursor leaves the screen.
    LOG_DEBUG("unable to leave screen, cursor position not available");
    return false;
  }

  return true;
}

void MSWindowsScreen::leave()
{
  // get keyboard layout of foreground window.  we'll use this
  // keyboard layout for translating keys sent to clients.
  m_keyLayout = AppUtilWindows::instance().getCurrentKeyboardLayout();

  // tell the key mapper about the keyboard layout
  m_keyState->setKeyLayout(m_keyLayout);

  // tell desk that we're leaving and tell it the keyboard layout
  m_desks->leave(m_keyLayout);

  if (m_isPrimary) {
    LOG_VERBOSE("centering cursor on leave: %+d, %+d", m_xCenter, m_yCenter);
    warpCursor(m_xCenter, m_yCenter);

    // disable special key sequences on win95 family
    enableSpecialKeys(false);

    // all messages prior to now are invalid
    nextMark();

    // remember the modifier state.  this is the modifier state
    // reflected in the internal keyboard state.
    m_keyState->saveModifiers();

    m_hook.setMode(kHOOK_RELAY_EVENTS);

    m_primaryKeyDownList.clear();
    for (KeyButton i = 0; i < IKeyState::s_numButtons; ++i) {
      if (m_keyState->isKeyDown(i)) {
        m_primaryKeyDownList.push_back(i);
        LOG_VERBOSE("key button %d is down before leaving to another screen", i);
      }
    }
  }

  // now off screen
  m_isOnScreen = false;
}

bool MSWindowsScreen::setClipboard(ClipboardID, const IClipboard *src)
{
  MSWindowsClipboard dst(m_window);
  if (src != nullptr) {
    // save clipboard data
    return Clipboard::copy(&dst, src);
  } else {
    // assert clipboard ownership
    if (!dst.open(0)) {
      return false;
    }
    dst.empty();
    dst.close();
    return true;
  }
}

void MSWindowsScreen::checkClipboards()
{
  // if we think we own the clipboard but we don't then somebody
  // grabbed the clipboard on this screen without us knowing.
  // tell the server that this screen grabbed the clipboard.
  //
  // this works around bugs in the clipboard viewer chain.
  // sometimes NT will simply never send WM_DRAWCLIPBOARD
  // messages for no apparent reason and rebooting fixes the
  // problem.  since we don't want a broken clipboard until the
  // next reboot we do this double check.  clipboard ownership
  // won't be reflected on other screens until we leave but at
  // least the clipboard itself will work.
  if (m_ownClipboard && !MSWindowsClipboard::isOwnedBySynergy()) {
    LOG_DEBUG("clipboard changed: lost ownership and no notification received");
    m_ownClipboard = false;
    sendClipboardEvent(EventTypes::ClipboardGrabbed, kClipboardClipboard);
    sendClipboardEvent(EventTypes::ClipboardGrabbed, kClipboardSelection);
  }
}

void MSWindowsScreen::openScreensaver(bool notify)
{
  assert(m_screensaver != nullptr);

  m_screensaverNotify = notify;
  if (m_screensaverNotify) {
    m_desks->installScreensaverHooks(true);
  } else if (m_screensaver) {
    m_screensaver->disable();
  }
}

void MSWindowsScreen::closeScreensaver()
{
  if (m_screensaver != nullptr) {
    if (m_screensaverNotify) {
      m_desks->installScreensaverHooks(false);
    } else {
      m_screensaver->enable();
    }
  }
  m_screensaverNotify = false;
}

void MSWindowsScreen::screensaver(bool activate)
{
  assert(m_screensaver != nullptr);
  if (m_screensaver == nullptr)
    return;

  if (activate) {
    m_screensaver->activate();
  } else {
    m_screensaver->deactivate();
  }
}

void MSWindowsScreen::resetOptions()
{
  m_desks->resetOptions();
}

void MSWindowsScreen::setOptions(const OptionsList &options)
{
  m_desks->setOptions(options);
}

void MSWindowsScreen::setSequenceNumber(uint32_t seqNum)
{
  m_sequenceNumber = seqNum;
}

bool MSWindowsScreen::isPrimary() const
{
  return m_isPrimary;
}

void *MSWindowsScreen::getEventTarget() const
{
  return const_cast<MSWindowsScreen *>(this);
}

bool MSWindowsScreen::getClipboard(ClipboardID, IClipboard *dst) const
{
  MSWindowsClipboard src(m_window);
  Clipboard::copy(dst, &src);
  return true;
}

void MSWindowsScreen::getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const
{
  assert(m_class != 0);

  x = m_x;
  y = m_y;
  w = m_w;
  h = m_h;
}

void MSWindowsScreen::getCursorPos(int32_t &x, int32_t &y) const
{
  m_desks->getCursorPos(x, y);
}

/*
 * getThisCursorPos and setThisCursorPos will attempt to negotiate with the
 * system to try get the and set the mouse position, however on the logon screen
 * due to hooks this process has it may unable to work around the problem.
 * Although these functions did not fix the issue at hand (#5294) its worth
 * keeping them here anyway.
 */
bool MSWindowsScreen::getThisCursorPos(LPPOINT pos)
{
  auto result = GetCursorPos(pos);
  if (!result) {
    LOG_DEBUG("could not get cursor pos, error: %s", windowsErrorToString(GetLastError()).c_str());

    LOG_DEBUG("retrying get cursor pos");
    result = GetCursorPos(pos);
    if (!result) {
      LOG_DEBUG("could not get cursor pos, error: %s", windowsErrorToString(GetLastError()).c_str());

      updateDesktopThread();
    }
  }

  return result;
}

bool MSWindowsScreen::setThisCursorPos(int x, int y)
{
  auto result = SetCursorPos(x, y);
  if (!result) {
    LOG_DEBUG("could not set cursor pos, error: %s", windowsErrorToString(GetLastError()).c_str());

    LOG_DEBUG("retrying to set cursor pos");
    result = SetCursorPos(x, y);
    if (!result) {
      LOG_DEBUG("could not set cursor pos, error: %s", windowsErrorToString(GetLastError()).c_str());

      updateDesktopThread();
    }
  }

  return result;
}

void MSWindowsScreen::updateDesktopThread()
{
  LOG_DEBUG("updating desktop thread");

  HDESK hDesk = OpenInputDesktop(0, true, GENERIC_ALL);
  if (hDesk == nullptr) {
    LOG_DEBUG("could not open input desktop, error: %s", windowsErrorToString(GetLastError()).c_str());
    return;
  }

  if (!SetThreadDesktop(hDesk)) {
    LOG_DEBUG("could not set thread desktop, error: %s", windowsErrorToString(GetLastError()).c_str());
  }

  if (!CloseDesktop(hDesk)) {
    LOG_DEBUG("could not close desktop, error: %s", windowsErrorToString(GetLastError()).c_str());
  }
}

void MSWindowsScreen::reconfigure(uint32_t activeSides)
{
  assert(m_isPrimary);
  const static auto sidesText = sidesMaskToString(activeSides);
  LOG_DEBUG("active sides: %s (0x%02x)", sidesText.c_str(), activeSides);
  m_hook.setSides(activeSides);
}

uint32_t MSWindowsScreen::activeSides()
{
  return m_hook.getSides();
}

void MSWindowsScreen::nextMark()
{
  // next mark
  ++m_mark;

  // mark point in message queue where the mark was changed
  PostThreadMessage(GetCurrentThreadId(), SYNERGY_MSG_MARK, m_mark, 0);
}

bool MSWindowsScreen::ignore() const
{
  return (m_mark != m_markReceived);
}

void MSWindowsScreen::handleFixes()
{
  // fix clipboard chain
  fixClipboardViewer();

  // update keys if keyboard layouts have changed
  if (m_keyState->didGroupsChange()) {
    updateKeys();
  }
}

void MSWindowsScreen::fixClipboardViewer()
{
  // XXX -- disable this code for now.  somehow it can cause an infinite
  // recursion in the WM_DRAWCLIPBOARD handler.  either we're sending
  // the message to our own window or some window farther down the chain
  // forwards the message to our window or a window farther up the chain.
  // i'm not sure how that could happen.  the m_nextClipboardWindow = nullptr
  // was not in the code that infinite loops and may fix the bug but i
  // doubt it.
  /*
      ChangeClipboardChain(m_window, m_nextClipboardWindow);
      m_nextClipboardWindow = nullptr;
      m_nextClipboardWindow = SetClipboardViewer(m_window);
  */
}

void MSWindowsScreen::enableSpecialKeys(bool) const
{
}

void MSWindowsScreen::updateKeysCB(const void *)
{
  // record which keys we think are down
  bool down[IKeyState::s_numButtons];
  bool sendFixes = (isPrimary() && !m_isOnScreen);
  if (sendFixes) {
    for (KeyButton i = 0; i < IKeyState::s_numButtons; ++i) {
      down[i] = m_keyState->isKeyDown(i);
    }
  }

  // update layouts if necessary
  if (m_keyState->didGroupsChange()) {
    PlatformScreen::updateKeyMap();
  }

  // now update the keyboard state
  PlatformScreen::updateKeyState();

  // now see which keys we thought were down but now think are up.
  // send key releases for these keys to the active client.
  if (sendFixes) {
    KeyModifierMask mask = pollActiveModifiers();
    for (KeyButton i = 0; i < IKeyState::s_numButtons; ++i) {
      if (down[i] && !m_keyState->isKeyDown(i)) {
        m_keyState->sendKeyEvent(getEventTarget(), false, false, kKeyNone, mask, 1, i);
      }
    }
  }
}

MSWindowsScreen::HotKeyItem::HotKeyItem(UINT keycode, UINT mask) : m_keycode(keycode), m_mask(mask)
{
  // do nothing
}

UINT MSWindowsScreen::HotKeyItem::getVirtualKey() const
{
  return m_keycode;
}

bool MSWindowsScreen::HotKeyItem::operator<(const HotKeyItem &x) const
{
  return (m_keycode < x.m_keycode || (m_keycode == x.m_keycode && m_mask < x.m_mask));
}

std::string MSWindowsScreen::getSecureInputApp() const
{
  // ignore on Windows
  return "";
}
