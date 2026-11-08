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

static int xi_opcode;

void XWindowsScreen::handleSystemEvent(const Event &event)
{
  auto *xevent = static_cast<XEvent *>(event.getData());
  assert(xevent != nullptr);

  // update key state
  bool isRepeat = false;
  if (m_isPrimary) {
    if (xevent->type == KeyRelease) {
      // check if this is a key repeat by getting the next
      // KeyPress event that has the same key and time as
      // this release event, if any.  first prepare the
      // filter info.
      KeyEventFilter filter;
      filter.m_event = KeyPress;
      filter.m_window = xevent->xkey.window;
      filter.m_time = xevent->xkey.time;
      filter.m_keycode = xevent->xkey.keycode;
      XEvent xevent2;
      isRepeat = (XCheckIfEvent(m_display, &xevent2, &XWindowsScreen::findKeyEvent, (XPointer)&filter) == True);
    }

    if (xevent->type == KeyPress || xevent->type == KeyRelease) {
      if (xevent->xkey.window == m_root) {
        // this is a hot key
        onHotKey(xevent->xkey, isRepeat);
        return;
      } else if (!m_isOnScreen && onHotKey(xevent->xkey, isRepeat)) {
        return;
      }

      bool down = (isRepeat || xevent->type == KeyPress);
      KeyModifierMask state = m_keyState->mapModifiersFromX(xevent->xkey.state);
      m_keyState->onKey(xevent->xkey.keycode, down, state);
    }
  }

  // let input methods try to handle event first
  if (m_ic != nullptr) {
    // XFilterEvent() may eat the event and generate a new KeyPress
    // event with a keycode of 0 because there isn't an actual key
    // associated with the keysym.  but the KeyRelease may pass
    // through XFilterEvent() and keep its keycode.  this means
    // there's a mismatch between KeyPress and KeyRelease keycodes.
    // since we use the keycode on the client to detect when a key
    // is released this won't do.  so we remember the keycode on
    // the most recent KeyPress (and clear it on a matching
    // KeyRelease) so we have a keycode for a synthesized KeyPress.
    if (xevent->type == KeyPress && xevent->xkey.keycode != 0) {
      m_lastKeycode = xevent->xkey.keycode;
    } else if (xevent->type == KeyRelease && xevent->xkey.keycode == m_lastKeycode) {
      m_lastKeycode = 0;
    }

    // now filter the event
    if (XFilterEvent(xevent, DefaultRootWindow(m_display))) {
      if (xevent->type == KeyPress) {
        // add filtered presses to the filtered list
        m_filtered.insert(m_lastKeycode);
      }
      return;
    }

    // discard matching key releases for key presses that were
    // filtered and remove them from our filtered list.
    else if (xevent->type == KeyRelease && m_filtered.contains(xevent->xkey.keycode)) {
      m_filtered.erase(xevent->xkey.keycode);
      return;
    }
  }

  // let screen saver have a go
  if (m_screensaver->handleXEvent(xevent)) {
    // screen saver handled it
    return;
  }

#ifdef HAVE_XI2
  if (m_xi2detected) {
    // Process RawMotion
    auto *cookie = &xevent->xcookie;
    if (XGetEventData(m_display, cookie) && cookie->type == GenericEvent && cookie->extension == xi_opcode) {
      if (cookie->evtype == XI_RawMotion) {
        // Get current pointer's position
        XMotionEvent xmotion;
        xmotion.type = MotionNotify;
        xmotion.send_event = False; // Raw motion
        xmotion.display = m_display;
        xmotion.window = m_window;
        /* xmotion's time, state and is_hint are not used */
        unsigned int msk;
        xmotion.same_screen = XQueryPointer(
            m_display, m_root, &xmotion.root, &xmotion.subwindow, &xmotion.x_root, &xmotion.y_root, &xmotion.x,
            &xmotion.y, &msk
        );
        onMouseMove(xmotion);
        XFreeEventData(m_display, cookie);
        return;
      }
      XFreeEventData(m_display, cookie);
    }
  }
#endif

  // handle the event ourself
  switch (xevent->type) {
  case CreateNotify:
    if (m_isPrimary && !m_xi2detected) {
      // select events on new window
      selectEvents(xevent->xcreatewindow.window);
    }
    break;

  case MappingNotify:
    refreshKeyboard(xevent);
    break;

  case LeaveNotify:
    if (!m_isPrimary) {
      // mouse moved out of hider window somehow.  hide the window.
      XUnmapWindow(m_display, m_window);
    }
    break;

  case SelectionClear: {
    // we just lost the selection.  that means someone else
    // grabbed the selection so this screen is now the
    // selection owner.  report that to the receiver.
    ClipboardID id = getClipboardID(xevent->xselectionclear.selection);
    if (id != kClipboardEnd) {
      m_clipboard[id]->lost(xevent->xselectionclear.time);
      sendClipboardEvent(EventTypes::ClipboardGrabbed, id);
      return;
    }
  } break;

  case SelectionNotify:
    // notification of selection transferred.  we shouldn't
    // get this here because we handle them in the selection
    // retrieval methods.  we'll just delete the property
    // with the data (satisfying the usual ICCCM protocol).
    if (xevent->xselection.property != None) {
      XDeleteProperty(m_display, xevent->xselection.requestor, xevent->xselection.property);
    }
    break;

  case SelectionRequest: {
    // somebody is asking for clipboard data
    ClipboardID id = getClipboardID(xevent->xselectionrequest.selection);
    if (id != kClipboardEnd) {
      m_clipboard[id]->addRequest(
          xevent->xselectionrequest.owner, xevent->xselectionrequest.requestor, xevent->xselectionrequest.target,
          xevent->xselectionrequest.time, xevent->xselectionrequest.property
      );
      return;
    }
  } break;

  case PropertyNotify:
    // property delete may be part of a selection conversion
    if (xevent->xproperty.state == PropertyDelete) {
      processClipboardRequest(xevent->xproperty.window, xevent->xproperty.time, xevent->xproperty.atom);
    }
    break;

  case DestroyNotify:
    // looks like one of the windows that requested a clipboard
    // transfer has gone bye-bye.
    destroyClipboardRequest(xevent->xdestroywindow.window);
    break;

  case KeyPress:
    if (m_isPrimary) {
      onKeyPress(xevent->xkey);
    }
    return;

  case KeyRelease:
    if (m_isPrimary) {
      onKeyRelease(xevent->xkey, isRepeat);
    }
    return;

  case ButtonPress:
    if (m_isPrimary) {
      onMousePress(xevent->xbutton);
    }
    return;

  case ButtonRelease:
    if (m_isPrimary) {
      onMouseRelease(xevent->xbutton);
    }
    return;

  case MotionNotify:
    if (m_isPrimary) {
      onMouseMove(xevent->xmotion);
    }
    return;

  case ConfigureNotify:
    if (!m_isPrimary && xevent->xconfigure.window == m_root) {
      setShape(xevent->xconfigure.width, xevent->xconfigure.height);
      sendEvent(EventTypes::ScreenShapeChanged);
    }
    return;

  default:
#if HAVE_XKB_EXTENSION
    if (m_xkb && xevent->type == m_xkbEventBase) {
      const auto *xkbEvent = reinterpret_cast<XkbEvent *>(xevent);
      switch (xkbEvent->any.xkb_type) {
      case XkbMapNotify:
        refreshKeyboard(xevent);
        return;

      case XkbStateNotify:
        LOG_INFO("group change: %d", xkbEvent->state.group);
        m_keyState->setActiveGroup(xkbEvent->state.group);
        return;

      default:
        return;
      }
    }
#endif

#if HAVE_X11_EXTENSIONS_XRANDR_H
    if (m_xrandr) {
      if (xevent->type == m_xrandrEventBase + RRScreenChangeNotify ||
          (xevent->type == m_xrandrEventBase + RRNotify &&
           reinterpret_cast<XRRNotifyEvent *>(xevent)->subtype == RRNotify_CrtcChange)) {
        LOG_INFO("either XRRScreenChangeNotifyEvent or RRNotify_CrtcChange received");

        // we're required to call back into XLib so XLib can update its internal
        // state
        XRRUpdateConfiguration(xevent);

        // requery/recalculate the screen shape
        saveShape();

        // we need to resize m_window, otherwise we'll get a weird problem where
        // moving off the server onto the client causes the pointer to warp to
        // the center of the server (so you can't move the pointer off the
        // server)
        if (m_isPrimary) {
          XMoveWindow(m_display, m_window, m_x, m_y);
          XResizeWindow(m_display, m_window, m_w, m_h);
        }

        sendEvent(EventTypes::ScreenShapeChanged);
      }
    }
#endif

    break;
  }
}

void XWindowsScreen::onKeyPress(XKeyEvent &xkey)
{
  LOG_VERBOSE("event: KeyPress code=%d, state=0x%04x", xkey.keycode, xkey.state);
  const KeyModifierMask mask = m_keyState->mapModifiersFromX(xkey.state);
  KeyID key = mapKeyFromX(&xkey);
  if (key != kKeyNone) {
    // check for ctrl+alt+del emulation
    if ((key == kKeyPause || key == kKeyBreak) &&
        (mask & (KeyModifierControl | KeyModifierAlt)) == (KeyModifierControl | KeyModifierAlt)) {
      // pretend it's ctrl+alt+del
      LOG_DEBUG("emulate ctrl+alt+del");
      key = kKeyDelete;
    }

    // get which button.  see call to XFilterEvent() in onEvent()
    // for more info.
    bool isFake = false;
    auto keycode = static_cast<KeyButton>(xkey.keycode);
    if (keycode == 0) {
      isFake = true;
      keycode = static_cast<KeyButton>(m_lastKeycode);
      if (keycode == 0) {
        // no keycode
        LOG_VERBOSE("event: KeyPress no keycode");
        return;
      }
    }

    // handle key
    m_keyState->sendKeyEvent(getEventTarget(), true, false, key, mask, 1, keycode);

    // do fake release if this is a fake press
    if (isFake) {
      m_keyState->sendKeyEvent(getEventTarget(), false, false, key, mask, 1, keycode);
    }
  } else {
    LOG_VERBOSE("can't map keycode to key id");
  }
}

void XWindowsScreen::onKeyRelease(XKeyEvent &xkey, bool isRepeat)
{
  const KeyModifierMask mask = m_keyState->mapModifiersFromX(xkey.state);
  KeyID key = mapKeyFromX(&xkey);
  if (key != kKeyNone) {
    // check for ctrl+alt+del emulation
    if ((key == kKeyPause || key == kKeyBreak) &&
        (mask & (KeyModifierControl | KeyModifierAlt)) == (KeyModifierControl | KeyModifierAlt)) {
      // pretend it's ctrl+alt+del and ignore autorepeat
      LOG_DEBUG("emulate ctrl+alt+del");
      key = kKeyDelete;
      isRepeat = false;
    }

    auto keycode = static_cast<KeyButton>(xkey.keycode);
    if (!isRepeat) {
      // no press event follows so it's a plain release
      LOG_VERBOSE("event: KeyRelease code=%d, state=0x%04x", keycode, xkey.state);
      m_keyState->sendKeyEvent(getEventTarget(), false, false, key, mask, 1, keycode);
    } else {
      // found a press event following so it's a repeat.
      // we could attempt to count the already queued
      // repeats but we'll just send a repeat of 1.
      // note that we discard the press event.
      LOG_VERBOSE("event: repeat code=%d, state=0x%04x", keycode, xkey.state);
      m_keyState->sendKeyEvent(getEventTarget(), false, true, key, mask, 1, keycode);
    }
  }
}

bool XWindowsScreen::onHotKey(const XKeyEvent &xkey, bool isRepeat)
{
  // find the hot key id
  HotKeyToIDMap::const_iterator i = m_hotKeyToIDMap.find(HotKeyItem(xkey.keycode, xkey.state));
  if (i == m_hotKeyToIDMap.end()) {
    return false;
  }

  // find what kind of event
  EventTypes type;
  if (xkey.type == KeyPress) {
    type = EventTypes::PrimaryScreenHotkeyDown;
  } else if (xkey.type == KeyRelease) {
    type = EventTypes::PrimaryScreenHotkeyUp;
  } else {
    return false;
  }

  // generate event (ignore key repeats)
  if (!isRepeat) {
    m_events->addEvent(Event(type, getEventTarget(), HotKeyInfo::alloc(i->second)));
  }
  return true;
}

void XWindowsScreen::onMousePress(const XButtonEvent &xbutton)
{
  LOG_VERBOSE("event: ButtonPress button=%d", xbutton.button);
  ButtonID button = mapButtonFromX(&xbutton);
  KeyModifierMask mask = m_keyState->mapModifiersFromX(xbutton.state);
  if (button != kButtonNone) {
    sendEvent(EventTypes::PrimaryScreenButtonDown, ButtonInfo::alloc(button, mask));
  }
}

void XWindowsScreen::onMouseRelease(const XButtonEvent &xbutton)
{
  using enum EventTypes;
  LOG_VERBOSE("event: ButtonRelease button=%d", xbutton.button);
  ButtonID button = mapButtonFromX(&xbutton);
  KeyModifierMask mask = m_keyState->mapModifiersFromX(xbutton.state);
  if (button != kButtonNone) {
    sendEvent(PrimaryScreenButtonUp, ButtonInfo::alloc(button, mask));
  } else if (xbutton.button == 4) {
    // wheel forward (away from user)
    sendEvent(PrimaryScreenWheel, WheelInfo::alloc(0, s_scrollDelta));
  } else if (xbutton.button == 5) {
    // wheel backward (toward user)
    sendEvent(PrimaryScreenWheel, WheelInfo::alloc(0, -s_scrollDelta));
  } else if (xbutton.button == 6) {
    // wheel tilt left
    sendEvent(PrimaryScreenWheel, WheelInfo::alloc(-s_scrollDelta, 0));
  } else if (xbutton.button == 7) {
    // wheel tilt right
    sendEvent(PrimaryScreenWheel, WheelInfo::alloc(s_scrollDelta, 0));
  }
}

void XWindowsScreen::onMouseMove(const XMotionEvent &xmotion)
{
  LOG_VERBOSE("event: MotionNotify %d,%d", xmotion.x_root, xmotion.y_root);

  // compute motion delta (relative to the last known
  // mouse position)
  int32_t x = xmotion.x_root - m_xCursor;
  int32_t y = xmotion.y_root - m_yCursor;

  // save position to compute delta of next motion
  m_xCursor = xmotion.x_root;
  m_yCursor = xmotion.y_root;

  if (xmotion.send_event) {
    // we warped the mouse.  discard events until we
    // find the matching sent event.  see
    // warpCursorNoFlush() for where the events are
    // sent.  we discard the matching sent event and
    // can be sure we've skipped the warp event.
    XEvent xevent;
    char cntr = 0;
    do {
      XMaskEvent(m_display, PointerMotionMask, &xevent);
      if (cntr++ > 10) {
        LOG_WARN("too many discarded events! %d", cntr);
        break;
      }
    } while (!xevent.xany.send_event);
    cntr = 0;
  } else if (m_isOnScreen) {
    // motion on primary screen
    sendEvent(EventTypes::PrimaryScreenMotionOnPrimary, MotionInfo::alloc(m_xCursor, m_yCursor));
  } else {
    // motion on secondary screen.  warp mouse back to
    // center.
    //
    // my lombard (powerbook g3) running linux and
    // using the adbmouse driver has two problems:
    // first, the driver only sends motions of +/-2
    // pixels and, second, it seems to discard some
    // physical input after a warp.  the former isn't a
    // big deal (we're just limited to every other
    // pixel) but the latter is a PITA.  to work around
    // it we only warp when the mouse has moved more
    // than s_size pixels from the center.
    static const int32_t s_size = 32;
    if (xmotion.x_root - m_xCenter < -s_size || xmotion.x_root - m_xCenter > s_size ||
        xmotion.y_root - m_yCenter < -s_size || xmotion.y_root - m_yCenter > s_size) {
      warpCursorNoFlush(m_xCenter, m_yCenter);
    }

    // send event if mouse moved.  do this after warping
    // back to center in case the motion takes us onto
    // the primary screen.  if we sent the event first
    // in that case then the warp would happen after
    // warping to the primary screen's enter position,
    // effectively overriding it.
    if (x != 0 || y != 0) {
      sendEvent(EventTypes::PrimaryScreenMotionOnSecondary, MotionInfo::alloc(x, y));
    }
  }
}

KeyID XWindowsScreen::mapKeyFromX(XKeyEvent *event) const
{
  // convert to a keysym
  KeySym keysym;
  if (event->type == KeyPress && m_ic != nullptr) {
    // do multibyte lookup.  can only call XmbLookupString with a
    // key press event and a valid XIC so we checked those above.
    char scratch[32];
    auto n = static_cast<int>(std::size(scratch));
    char *buffer = scratch;
    int status;
    n = XmbLookupString(m_ic, event, buffer, n, &keysym, &status);
    if (status == XBufferOverflow) {
      // not enough space.  grow buffer and try again.
      buffer = new char[n];
      n = XmbLookupString(m_ic, event, buffer, n, &keysym, &status);
      delete[] buffer;
    }

    // see what we got.  since we don't care about the string
    // we'll just look for a keysym.
    switch (status) {
    default:
    case XLookupNone:
    case XLookupChars:
      keysym = 0;
      break;

    case XLookupKeySym:
    case XLookupBoth:
      break;
    }
  } else {
    // plain old lookup
    char dummy[1];
    XLookupString(event, dummy, 0, &keysym, nullptr);
  }

  LOG_VERBOSE("mapped code=%d to keysym=0x%04x", event->keycode, keysym);

  // convert key
  KeyID result = XDGKeyUtil::mapKeySymToKeyID(keysym);
  LOG_VERBOSE("mapped keysym=0x%04x to keyID=%d", keysym, result);
  return result;
}

ButtonID XWindowsScreen::mapButtonFromX(const XButtonEvent *event) const
{
  switch (unsigned int button = event->button; button) {
  case 1:
  case 2:
  case 3:
  case 6:
  case 7:
    return static_cast<ButtonID>(button); // Handle Left, Middle and Right buttons
  case 8:
    return kButtonExtra0; // Mouse 4
  case 9:
    return kButtonExtra1; // Mouse 5
  default:
    return kButtonNone;
  }
}

bool XWindowsScreen::detectXI2()
{
  int event;
  int error;
  return XQueryExtension(m_display, "XInputExtension", &xi_opcode, &event, &error);
}

#ifdef HAVE_XI2
void XWindowsScreen::selectXIRawMotion()
{
  XIEventMask mask;

  mask.deviceid = XIAllDevices;
  mask.mask_len = XIMaskLen(XI_RawMotion);
  mask.mask = (unsigned char *)calloc(mask.mask_len, sizeof(char));
  mask.deviceid = XIAllMasterDevices;
  memset(mask.mask, 0, 2);
  XISetMask(mask.mask, XI_RawKeyRelease);
  XISetMask(mask.mask, XI_RawMotion);
  XISelectEvents(m_display, DefaultRootWindow(m_display), &mask, 1);
  free(mask.mask);
}
#endif
