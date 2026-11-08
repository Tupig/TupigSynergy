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

//
// XWindowsScreen
//

// NOTE -- the X display is shared among several objects but is owned
// by the XWindowsScreen.  Xlib is not reentrant so we must ensure
// that no two objects can simultaneously call Xlib with the display.
// this is easy since we only make X11 calls from the main thread.
// we must also ensure that these objects do not use the display in
// their destructors or, if they do, we can tell them not to.  This
// is to handle unexpected disconnection of the X display, when any
// call on the display is invalid.  In that situation we discard the
// display and the X11 event queue buffer, ignore any calls that try
// to use the display, and wait to be destroyed.

XWindowsScreen *XWindowsScreen::s_screen = nullptr;

XWindowsScreen::XWindowsScreen(const char *displayName, bool isPrimary, IEventQueue *events)
    : PlatformScreen(events),
      m_isPrimary(isPrimary),
      m_isOnScreen(m_isPrimary),
      m_events(events)
{
  assert(s_screen == nullptr);
  s_screen = this;

  if (XInitThreads() == 0) {
    throw std::runtime_error("XInitThreads() returned zero");
  }

  // set the X I/O error handler so we catch the display disconnecting
  XSetIOErrorHandler(&XWindowsScreen::ioErrorHandler);

  try {
    m_display = openDisplay(displayName);
    m_root = DefaultRootWindow(m_display);
    saveShape();
    m_window = openWindow();
    m_screensaver = new XWindowsScreenSaver(m_display, m_window, getEventTarget(), events);
    m_keyState = new XWindowsKeyState(m_display, m_xkb, events, m_keyMap);
    LOG_DEBUG("screen shape: %d,%d %dx%d %s", m_x, m_y, m_w, m_h, m_xinerama ? "(xinerama)" : "");
    LOG_DEBUG("window is 0x%08x", m_window);
  } catch (...) {
    if (m_display != nullptr) {
      XCloseDisplay(m_display);
    }
    throw;
  }

  // primary/secondary screen only initialization
  if (m_isPrimary) {
#ifdef HAVE_XI2
    m_xi2detected = detectXI2();
    if (m_xi2detected) {
      selectXIRawMotion();
    } else
#endif
    {
      // start watching for events on other windows
      selectEvents(m_root);
    }

    // prepare to use input methods
    openIM();
  } else {
    // become impervious to server grabs
    XTestGrabControl(m_display, True);
  }

  // disable sleep if the flag is set
  if (Settings::value(Settings::Core::PreventSleep).toBool()) {
    m_powerManager.disableSleep();
  }

  // initialize the clipboards
  for (ClipboardID id = 0; id < kClipboardEnd; ++id) {
    m_clipboard[id] = new XWindowsClipboard(m_display, m_window, id);
  }

  // install event handlers
  m_events->addHandler(EventTypes::System, m_events->getSystemTarget(), [this](const auto &e) {
    handleSystemEvent(e);
  });

  // install the platform event queue
  m_events->adoptBuffer(new XWindowsEventQueueBuffer(m_display, m_window, m_events));
}

XWindowsScreen::~XWindowsScreen()
{
  assert(s_screen != nullptr);

  m_events->adoptBuffer(nullptr);
  m_events->removeHandler(EventTypes::System, m_events->getSystemTarget());
  for (auto clipboard : m_clipboard) {
    delete clipboard;
  }
  delete m_keyState;
  delete m_screensaver;
  m_keyState = nullptr;
  m_screensaver = nullptr;
  if (m_display != nullptr) {
    // FIXME -- is it safe to clean up the IC and IM without a display?
    if (m_ic != nullptr) {
      XDestroyIC(m_ic);
    }
    if (m_im != nullptr) {
      XCloseIM(m_im);
    }
    XDestroyWindow(m_display, m_window);
    XCloseDisplay(m_display);
  }
  XSetIOErrorHandler(nullptr);

  s_screen = nullptr;
}

void XWindowsScreen::enable()
{
  if (m_displayLost) {
    return;
  }

  if (!m_isPrimary) {
    // get the keyboard control state
    XKeyboardState keyControl;
    XGetKeyboardControl(m_display, &keyControl);
    m_autoRepeat = (keyControl.global_auto_repeat == AutoRepeatModeOn);
    m_keyState->setAutoRepeat(keyControl);

    // move hider window under the cursor center
    XMoveWindow(m_display, m_window, m_xCenter, m_yCenter);

    // raise and show the window
    // FIXME -- take focus?
    XMapRaised(m_display, m_window);

    // warp the mouse to the cursor center
    fakeMouseMove(m_xCenter, m_yCenter);
  }
}

void XWindowsScreen::disable()
{
  if (m_displayLost) {
    return;
  }

  // release input context focus
  if (m_ic != nullptr) {
    XUnsetICFocus(m_ic);
  }

  // unmap the hider/grab window.  this also ungrabs the mouse and
  // keyboard if they're grabbed.
  XUnmapWindow(m_display, m_window);

  // restore auto-repeat state
  if (!m_isPrimary && m_autoRepeat) {
    // XAutoRepeatOn(m_display);
  }
}

void XWindowsScreen::enter()
{
  if (m_displayLost) {
    return;
  }

  screensaver(false);

  // release input context focus
  if (m_ic != nullptr) {
    XUnsetICFocus(m_ic);
  }

  // set the input focus to what it had been when we took it
  if (m_lastFocus != None) {
    // the window may not exist anymore so ignore errors
    XWindowsUtil::ErrorLock lock(m_display);
    XSetInputFocus(m_display, m_lastFocus, m_lastFocusRevert, CurrentTime);
  }

#if HAVE_X11_EXTENSIONS_DPMS_H
  // Force the DPMS to turn screen back on since we don't
  // actually cause physical hardware input to trigger it
  int dummy;
  CARD16 powerlevel;
  if (BOOL enabled; DPMSQueryExtension(m_display, &dummy, &dummy) && DPMSCapable(m_display) &&
                    DPMSInfo(m_display, &powerlevel, &enabled)) {
    if (enabled && powerlevel != DPMSModeOn)
      DPMSForceLevel(m_display, DPMSModeOn);
  }
#endif

  // unmap the hider/grab window.  this also ungrabs the mouse and
  // keyboard if they're grabbed.
  XUnmapWindow(m_display, m_window);

  // maybe call this if entering for the screensaver
  // set keyboard focus to root window.  the screensaver should then
  // pick up key events for when the user enters a password to unlock.
  // XSetInputFocus(m_display, PointerRoot, PointerRoot, CurrentTime);

  if (!m_isPrimary) {
    // get the keyboard control state
    XKeyboardState keyControl;
    XGetKeyboardControl(m_display, &keyControl);
    m_autoRepeat = (keyControl.global_auto_repeat == AutoRepeatModeOn);
    m_keyState->setAutoRepeat(keyControl);

    // turn off auto-repeat.  we do this so fake key press events don't
    // cause the local server to generate their own auto-repeats of
    // those keys.
    // XAutoRepeatOff(m_display);
  }

  // now on screen
  m_isOnScreen = true;
}

bool XWindowsScreen::canLeave()
{
  // raise and show the window, required to grab mouse and keyboard
  XMapRaised(m_display, m_window);

  // see if grabbing the mouse and keyboard, if primary, is possible
  return !(m_isPrimary && !grabMouseAndKeyboard());
}

void XWindowsScreen::leave()
{
  if (!m_isPrimary) {
    // restore the previous keyboard auto-repeat state.  if the user
    // changed the auto-repeat configuration while on the client then
    // that state is lost.  that's because we can't get notified by
    // the X server when the auto-repeat configuration is changed so
    // we can't track the desired configuration.
    if (m_autoRepeat) {
      // XAutoRepeatOn(m_display);
    }

    // move hider window under the cursor center
    XMoveWindow(m_display, m_window, m_xCenter, m_yCenter);
  }

  // raise and show the window
  XMapRaised(m_display, m_window);

  // grab the mouse and keyboard, if primary and possible
  if (m_isPrimary && !grabMouseAndKeyboard()) {
    XUnmapWindow(m_display, m_window);
  }

  // save current focus
  XGetInputFocus(m_display, &m_lastFocus, &m_lastFocusRevert);

  // take focus
  if (m_isPrimary || !m_preserveFocus) {
    XSetInputFocus(m_display, m_window, RevertToPointerRoot, CurrentTime);
  }

  // now warp the mouse.  we warp after showing the window so we're
  // guaranteed to get the mouse leave event and to prevent the
  // keyboard focus from changing under point-to-focus policies.
  if (m_isPrimary) {
    warpCursor(m_xCenter, m_yCenter);
  } else {
    // WARN: not using fakeMouseMove() intentionally
    // forcibly ignore any xinerama quirks, so that `xrandr --panning ... --tracking ...` works
    XTestFakeMotionEvent(m_display, DefaultScreen(m_display), m_xCenter, m_yCenter, CurrentTime);
    XFlush(m_display);
  }

  // set input context focus to our window
  if (m_ic != nullptr) {
    XmbResetIC(m_ic);
    XSetICFocus(m_ic);
    m_filtered.clear();
  }

  // now off screen
  m_isOnScreen = false;
}

bool XWindowsScreen::setClipboard(ClipboardID id, const IClipboard *clipboard)
{
  // fail if we don't have the requested clipboard
  if (m_clipboard[id] == nullptr) {
    return false;
  }

  // get the actual time.  ICCCM does not allow CurrentTime.
  Time timestamp = XWindowsUtil::getCurrentTime(m_display, m_clipboard[id]->getWindow());

  if (clipboard != nullptr) {
    // save clipboard data
    return Clipboard::copy(m_clipboard[id], clipboard, timestamp);
  } else {
    // assert clipboard ownership
    if (!m_clipboard[id]->open(timestamp)) {
      return false;
    }
    m_clipboard[id]->empty();
    m_clipboard[id]->close();
    return true;
  }
}

void XWindowsScreen::checkClipboards()
{
  // do nothing, we're always up to date
}

void XWindowsScreen::openScreensaver(bool notify)
{
  m_screensaverNotify = notify;
  if (!m_screensaverNotify) {
    m_screensaver->disable();
  }
}

void XWindowsScreen::closeScreensaver()
{
  if (!m_screensaverNotify) {
    m_screensaver->enable();
  }
}

void XWindowsScreen::screensaver(bool activate)
{
  if (activate) {
    m_screensaver->activate();
  } else {
    m_screensaver->deactivate();
  }
}

void XWindowsScreen::resetOptions()
{
  m_xtestIsXineramaUnaware = true;
  m_preserveFocus = false;
}

void XWindowsScreen::setOptions(const OptionsList &options)
{
  if (options.size() % 2 != 0) {
    LOG_ERR("options are the incorrect size, can not process them");
    return;
  }
  for (uint32_t i = 0, n = options.size(); i < n; i += 2) {
    if (options[i] == kOptionXTestXineramaUnaware) {
      m_xtestIsXineramaUnaware = (options[i + 1] != 0);
      LOG_VERBOSE("library, XTest is Xinerama unaware %s", m_xtestIsXineramaUnaware ? "true" : "false");
    } else if (options[i] == kOptionScreenPreserveFocus) {
      m_preserveFocus = (options[i + 1] != 0);
      LOG_VERBOSE("preserve focus: %s", m_preserveFocus ? "true" : "false");
    }
  }
}

void XWindowsScreen::setSequenceNumber(uint32_t seqNum)
{
  m_sequenceNumber = seqNum;
}

bool XWindowsScreen::isPrimary() const
{
  return m_isPrimary;
}

std::string XWindowsScreen::getSecureInputApp() const
{
  // ignore on Linux
  return "";
}

void *XWindowsScreen::getEventTarget() const
{
  return const_cast<XWindowsScreen *>(this);
}

bool XWindowsScreen::getClipboard(ClipboardID id, IClipboard *clipboard) const
{
  assert(clipboard != nullptr);

  // fail if we don't have the requested clipboard
  if (m_clipboard[id] == nullptr) {
    return false;
  }

  // get the actual time.  ICCCM does not allow CurrentTime.
  Time timestamp = XWindowsUtil::getCurrentTime(m_display, m_clipboard[id]->getWindow());

  // copy the clipboard
  return Clipboard::copy(clipboard, m_clipboard[id], timestamp);
}

void XWindowsScreen::getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const
{
  x = m_x;
  y = m_y;
  w = m_w;
  h = m_h;
}

void XWindowsScreen::getCursorPos(int32_t &x, int32_t &y) const
{
  Window root;
  Window window;
  int mx;
  int my;
  int xWindow;
  int yWindow;
  unsigned int mask;
  if (XQueryPointer(m_display, m_root, &root, &window, &mx, &my, &xWindow, &yWindow, &mask)) {
    x = mx;
    y = my;
  } else {
    x = m_xCenter;
    y = m_yCenter;
  }
}

void XWindowsScreen::reconfigure(uint32_t activeSides)
{
  const static auto sidesText = sidesMaskToString(activeSides);
  LOG_DEBUG("active sides: %s (0x%02x)", sidesText.c_str(), activeSides);
  m_activeSides = activeSides;
}

uint32_t XWindowsScreen::activeSides()
{
  return m_activeSides;
}

void XWindowsScreen::warpCursor(int32_t x, int32_t y)
{
  // warp mouse
  warpCursorNoFlush(x, y);

  // remove all input events before and including warp
  XEvent event;
  while (XCheckMaskEvent(
      m_display,
      PointerMotionMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | KeymapStateMask, &event
  )) {
    // do nothing
  }

  // save position as last position
  m_xCursor = x;
  m_yCursor = y;
}

void XWindowsScreen::sendEvent(EventTypes type, void *data)
{
  m_events->addEvent(Event(type, getEventTarget(), data));
}

void XWindowsScreen::sendClipboardEvent(EventTypes type, ClipboardID id)
{
  auto *info = (ClipboardInfo *)malloc(sizeof(ClipboardInfo));
  info->m_id = id;
  info->m_sequenceNumber = m_sequenceNumber;
  sendEvent(type, info);
}

IKeyState *XWindowsScreen::getKeyState() const
{
  return m_keyState;
}

Bool XWindowsScreen::findKeyEvent(Display *, XEvent *xevent, XPointer arg)
{
  const auto *filter = reinterpret_cast<KeyEventFilter *>(arg);
  return (xevent->type == filter->m_event && xevent->xkey.window == filter->m_window &&
          xevent->xkey.time == filter->m_time && xevent->xkey.keycode == filter->m_keycode)
             ? True
             : False;
}

ClipboardID XWindowsScreen::getClipboardID(Atom selection) const
{
  for (ClipboardID id = 0; id < kClipboardEnd; ++id) {
    if (m_clipboard[id] != nullptr && m_clipboard[id]->getSelection() == selection) {
      return id;
    }
  }
  return kClipboardEnd;
}

void XWindowsScreen::processClipboardRequest(Window requestor, Time time, Atom property) const
{
  // check every clipboard until one returns success
  for (const auto &clipboard : m_clipboard) {
    if (clipboard != nullptr && clipboard->processRequest(requestor, time, property)) {
      break;
    }
  }
}

void XWindowsScreen::destroyClipboardRequest(Window requestor) const
{
  // check every clipboard until one returns success
  for (const auto &clipboard : m_clipboard) {
    if (clipboard != nullptr && clipboard->destroyRequest(requestor)) {
      break;
    }
  }
}

void XWindowsScreen::onError()
{
  // prevent further access to the X display
  m_events->adoptBuffer(nullptr);
  m_screensaver->destroy();
  m_screensaver = nullptr;
  m_display = nullptr;
  m_displayLost = true;

  // notify of failure
  sendEvent(EventTypes::ScreenError, nullptr);
}

int XWindowsScreen::ioErrorHandler(Display *)
{
  // The X11 display has disconnected unexpectedly.  Instead of exiting,
  // mark the display as lost and notify the application so it can attempt
  // recovery or gracefully shut down.
  if (s_screen != nullptr) {
    LOG_ERR("X11 display connection lost (ioErrorHandler fired)");
    s_screen->onError();
  }
  // Return non-zero — some Xlib versions exit on 0 return anyway,
  // but returning non-zero signals the error was "handled".
  return 1;
}

unsigned int XWindowsScreen::mapButtonToX(ButtonID id) const
{
  switch (id) {
  case kButtonLeft:
  case kButtonRight:
  case kButtonMiddle:
    return static_cast<uint>(id);
  case kX11ScrollWheelUp:
    return 4;
  case kX11ScrollWheelDown:
    return 5;
  case kX11ScrollWheelLeft:
    return 6;
  case kX11ScrollWheelRight:
    return 7;
  case kButtonExtra0:
    return 8;
  case kButtonExtra1:
    return 9;
  default:
    return 0;
  }
}

void XWindowsScreen::updateButtons()
{
  // query the button mapping
  uint32_t numButtons = XGetPointerMapping(m_display, nullptr, 0);
  auto *tmpButtons = new unsigned char[numButtons];
  XGetPointerMapping(m_display, tmpButtons, numButtons);

  // find the largest logical button id
  unsigned char maxButton = 0;
  for (uint32_t i = 0; i < numButtons; ++i) {
    if (tmpButtons[i] > maxButton) {
      maxButton = tmpButtons[i];
    }
  }

  // allocate button array
  m_buttons.resize(std::max<size_t>(maxButton, numButtons));

  // fill in button array values.  m_buttons[i] is the physical
  // button number for logical button i+1.
  for (uint32_t i = 0; i < numButtons; ++i) {
    m_buttons[i] = 0;
  }
  for (uint32_t i = 0; i < numButtons; ++i) {
    if (tmpButtons[i] > 0) {
      m_buttons[tmpButtons[i] - 1] = i + 1;
    }
  }

  // clean up
  delete[] tmpButtons;
}

//
// XWindowsScreen::HotKeyItem
//

XWindowsScreen::HotKeyItem::HotKeyItem(int keycode, unsigned int mask) : m_keycode(keycode), m_mask(mask)
{
  // do nothing
}

bool XWindowsScreen::HotKeyItem::operator<(const HotKeyItem &x) const
{
  return (m_keycode < x.m_keycode || (m_keycode == x.m_keycode && m_mask < x.m_mask));
}
