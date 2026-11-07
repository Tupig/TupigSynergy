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

Display *XWindowsScreen::openDisplay(const char *displayName)
{
  // get the DISPLAY
  if (displayName == nullptr) {
    displayName = std::getenv("DISPLAY");
    if (displayName == nullptr) {
      displayName = ":0.0";
    }
  }

  // open the display
  LOG_VERBOSE("calling XOpenDisplay(\"%s\")", displayName);
  Display *display = XOpenDisplay(displayName);
  if (display == nullptr) {
    throw ScreenUnavailableException();
  }

  // verify the availability of the XTest extension
  if (!m_isPrimary) {
    int majorOpcode;
    int firstEvent;
    int firstError;
    if (!XQueryExtension(display, XTestExtensionName, &majorOpcode, &firstEvent, &firstError)) {
      LOG_ERR("the XTest extension is not available");
      XCloseDisplay(display);
      throw ScreenOpenFailureException();
    }
  }

#if HAVE_XKB_EXTENSION
  {
    m_xkb = false;
    int major = XkbMajorVersion;
    int minor = XkbMinorVersion;
    if (XkbLibraryVersion(&major, &minor)) {
      int opcode;
      int firstError;
      if (XkbQueryExtension(display, &opcode, &m_xkbEventBase, &firstError, &major, &minor)) {
        m_xkb = true;
        XkbSelectEvents(display, XkbUseCoreKbd, XkbMapNotifyMask, XkbMapNotifyMask);
        XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotifyMask, XkbGroupStateMask, XkbGroupStateMask);
      }
    }
  }
#endif

#if HAVE_X11_EXTENSIONS_XRANDR_H
  // query for XRandR extension
  int dummyError;
  m_xrandr = XRRQueryExtension(display, &m_xrandrEventBase, &dummyError);
  if (m_xrandr) {
    // enable XRRScreenChangeNotifyEvent
    XRRSelectInput(display, DefaultRootWindow(display), RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask);
  }
#endif

  return display;
}

void XWindowsScreen::saveShape()
{
  // get shape of default screen
  setShape(WidthOfScreen(DefaultScreenOfDisplay(m_display)), HeightOfScreen(DefaultScreenOfDisplay(m_display)));
}

void XWindowsScreen::setShape(int32_t width, int32_t height)
{
  // set shape
  m_x = 0;
  m_y = 0;

  m_w = width;
  m_h = height;

  // get center of screen
  m_xCenter = m_x + (m_w >> 1);
  m_yCenter = m_y + (m_h >> 1);

  // check if xinerama is enabled and there is more than one screen.
  // get center of first Xinerama screen.  Xinerama appears to have
  // a bug when XWarpPointer() is used in combination with
  // XGrabPointer().  in that case, the warp is successful but the
  // next pointer motion warps the pointer again, apparently to
  // constrain it to some unknown region, possibly the region from
  // 0,0 to Wm,Hm where Wm (Hm) is the minimum width (height) over
  // all physical screens.  this warp only seems to happen if the
  // pointer wasn't in that region before the XWarpPointer().  the
  // second (unexpected) warp causes Synergy to think the pointer
  // has been moved when it hasn't.  to work around the problem,
  // we warp the pointer to the center of the first physical
  // screen instead of the logical screen.
  m_xinerama = false;
#if HAVE_X11_EXTENSIONS_XINERAMA_H
  int eventBase;
  int errorBase;
  if ((XineramaQueryExtension(m_display, &eventBase, &errorBase) != 0) && (XineramaIsActive(m_display) != 0)) {
    int numScreens;
    XineramaScreenInfo *screens;
    screens = XineramaQueryScreens(m_display, &numScreens);
    if (screens != nullptr) {
      if (numScreens > 1) {
        m_xinerama = true;
        for (int n = 0; n < numScreens; n++) {
          LOG_DEBUG(
              "xinerama screen: %d origin: %d,%d size: %dx%d", n, screens[n].x_org, screens[n].y_org, screens[n].width,
              screens[n].height
          );
          m_xCenter = std::max(m_xCenter, (screens[n].x_org + screens[n].width) >> 1);
          m_yCenter = std::max(m_yCenter, (screens[n].y_org + screens[n].height) >> 1);
        }
      }
      XFree(screens);
    }
  }
#endif

  LOG_DEBUG("center: %d,%d", m_xCenter, m_yCenter);
}

Window XWindowsScreen::openWindow() const
{
  // default window attributes.  we don't want the window manager
  // messing with our window and we don't want the cursor to be
  // visible inside the window.
  XSetWindowAttributes attr;
  attr.do_not_propagate_mask = 0;
  attr.override_redirect = True;
  attr.cursor = createBlankCursor();

  // adjust attributes and get size and shape
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  if (m_isPrimary) {
    // grab window attributes.  this window is used to capture user
    // input when the user is focused on another client.  it covers
    // the whole screen.
    attr.event_mask = PointerMotionMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
                      KeymapStateMask | PropertyChangeMask;
    x = m_x;
    y = m_y;
    w = m_w;
    h = m_h;
  } else {
    // cursor hider window attributes.  this window is used to hide the
    // cursor when it's not on the screen.  the window is hidden as soon
    // as the cursor enters the screen or the display's real mouse is
    // moved.  we'll reposition the window as necessary so its
    // position here doesn't matter.  it only needs to be 1x1 because
    // it only needs to contain the cursor's hotspot.
    attr.event_mask = LeaveWindowMask;
    x = 0;
    y = 0;
    w = 1;
    h = 1;
  }

  // create and return the window
  Window window = XCreateWindow(
      m_display, m_root, x, y, w, h, 0, 0, InputOnly, CopyFromParent,
      CWDontPropagate | CWEventMask | CWOverrideRedirect | CWCursor, &attr
  );
  if (window == None) {
    throw ScreenOpenFailureException();
  }
  return window;
}

void XWindowsScreen::openIM()
{
  // open the input methods
  XIM im = XOpenIM(m_display, nullptr, nullptr, nullptr);
  if (im == nullptr) {
    LOG_INFO("no support for IM");
    return;
  }

  // find the appropriate style.  Synergy supports XIMPreeditNothing
  // only at the moment.
  XIMStyles *styles;
  if (XGetIMValues(im, XNQueryInputStyle, &styles, nullptr) != nullptr || styles == nullptr) {
    LOG_WARN("cannot get IM styles");
    XCloseIM(im);
    return;
  }
  XIMStyle style = 0;
  for (unsigned short i = 0; i < styles->count_styles; ++i) {
    style = styles->supported_styles[i];
    if (((style & XIMPreeditNothing) != 0) && ((style & (XIMStatusNothing | XIMStatusNone)) != 0)) {
      break;
    }
  }
  XFree(styles);
  if (style == 0) {
    LOG_INFO("no supported IM styles");
    XCloseIM(im);
    return;
  }

  // create an input context for the style and tell it about our window
  XIC ic = XCreateIC(im, XNInputStyle, style, XNClientWindow, m_window, nullptr);
  if (ic == nullptr) {
    LOG_WARN("cannot create IC");
    XCloseIM(im);
    return;
  }

  // find out the events we must select for and do so
  unsigned long mask;
  if (XGetICValues(ic, XNFilterEvents, &mask, nullptr) != nullptr) {
    LOG_WARN("cannot get IC filter events");
    XDestroyIC(ic);
    XCloseIM(im);
    return;
  }

  // we have IM
  m_im = im;
  m_ic = ic;
  m_lastKeycode = 0;

  // select events on our window that IM requires
  XWindowAttributes attr;
  XGetWindowAttributes(m_display, m_window, &attr);
  XSelectInput(m_display, m_window, attr.your_event_mask | mask);

  // listen for screen-resize messages
  XSelectInput(m_display, m_root, StructureNotifyMask);
}

Cursor XWindowsScreen::createBlankCursor() const
{
  // this seems just a bit more complicated than really necessary

  // get the closet cursor size to 1x1
  unsigned int w = 0;
  unsigned int h = 0;
  XQueryBestCursor(m_display, m_root, 1, 1, &w, &h);
  w = std::max(1u, w);
  h = std::max(1u, h);

  // make bitmap data for cursor of closet size.  since the cursor
  // is blank we can use the same bitmap for shape and mask:  all
  // zeros.
  const int size = ((w + 7) >> 3) * h;
  auto *data = new char[size];
  memset(data, 0, size);

  // make bitmap
  Pixmap bitmap = XCreateBitmapFromData(m_display, m_root, data, w, h);

  // need an arbitrary color for the cursor
  XColor color;
  color.pixel = 0;
  color.red = color.green = color.blue = 0;
  color.flags = DoRed | DoGreen | DoBlue;

  // make cursor from bitmap
  Cursor cursor = XCreatePixmapCursor(m_display, bitmap, bitmap, &color, &color, 0, 0);

  // don't need bitmap or the data anymore
  delete[] data;
  XFreePixmap(m_display, bitmap);

  return cursor;
}

void XWindowsScreen::selectEvents(Window w) const
{
  // ignore errors while we adjust event masks.  windows could be
  // destroyed at any time after the XQueryTree() in doSelectEvents()
  // so we must ignore BadWindow errors.
  XWindowsUtil::ErrorLock lock(m_display);

  // adjust event masks
  doSelectEvents(w);
}

void XWindowsScreen::doSelectEvents(Window w) const
{
  // we want to track the mouse everywhere on the display.  to achieve
  // that we select PointerMotionMask on every window.  we also select
  // SubstructureNotifyMask in order to get CreateNotify events so we
  // select events on new windows too.

  // we don't want to adjust our grab window
  if (w == m_window) {
    return;
  }

  // X11 has a design flaw. If *no* client selected PointerMotionMask for
  // a window, motion events will be delivered to that window's parent.
  // If *any* client, not necessarily the owner, selects PointerMotionMask
  // on such a window, X will stop propagating motion events to its
  // parent. This breaks applications that rely on event propagation
  // behavior.
  //
  // Avoid selecting PointerMotionMask unless some other client selected
  // it already.
  long mask = SubstructureNotifyMask;
  XWindowAttributes attr;
  XGetWindowAttributes(m_display, w, &attr);
  if ((attr.all_event_masks & PointerMotionMask) == PointerMotionMask) {
    mask |= PointerMotionMask;
  }

  // select events of interest.  do this before querying the tree so
  // we'll get notifications of children created after the XQueryTree()
  // so we won't miss them.
  XSelectInput(m_display, w, mask);

  // recurse on child windows
  Window rw;
  Window pw;
  Window *cw;
  unsigned int nc;
  if (XQueryTree(m_display, w, &rw, &pw, &cw, &nc)) {
    for (unsigned int i = 0; i < nc; ++i) {
      doSelectEvents(cw[i]);
    }
    XFree(cw);
  }
}

void XWindowsScreen::refreshKeyboard(XEvent *event)
{
  if (XPending(m_display) > 0) {
    XEvent tmpEvent;
    XPeekEvent(m_display, &tmpEvent);
    if (tmpEvent.type == MappingNotify) {
      // discard this event since another follows.
      // we tend to get a bunch of these in a row.
      return;
    }
  }

  // keyboard mapping changed
#if HAVE_XKB_EXTENSION
  if (m_xkb && event->type == m_xkbEventBase) {
    XkbRefreshKeyboardMapping((XkbMapNotifyEvent *)event);
  } else
#else
  {
    XRefreshKeyboardMapping(&event->xmapping);
  }
#endif
    m_keyState->updateKeyMap();
  m_keyState->updateKeyState();
}

//
// XWindowsScreen::HotKeyItem
//
