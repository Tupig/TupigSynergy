/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "arch/Arch.h"
#include "base/Log.h"

#include "XWindowsClipboard.h"

#include "XWindowsClipboardBMPConverter.h"
#include "XWindowsClipboardHTMLConverter.h"
#include "XWindowsClipboardTextConverter.h"
#include "XWindowsClipboardUCS2Converter.h"
#include "XWindowsClipboardUTF8Converter.h"
#include "XWindowsUtil.h"
#include "base/Stopwatch.h"

#include <X11/Xatom.h>
#include <algorithm>
#include <cstring>

#if HAVE_FORMAT
#include <format>
#endif

#include <vector>

XWindowsClipboard::CICCCMGetClipboard::CICCCMGetClipboard(Window requestor, Time time, Atom property)
    : m_requestor(requestor),
      m_time(time),
      m_property(property)
{
}

bool XWindowsClipboard::CICCCMGetClipboard::readClipboard(
    Display *display, Atom selection, Atom target, Atom *actualTarget, std::string *data
)
{
  assert(actualTarget != nullptr);
  assert(data != nullptr);

  LOG((
      CLOG_VERBOSE "request selection=%s, target=%s, window=%x", XWindowsUtil::atomToString(display, selection).c_str(),
      XWindowsUtil::atomToString(display, target).c_str(), m_requestor
  ));

  m_atomNone = XInternAtom(display, "NONE", False);
  m_atomIncr = XInternAtom(display, "INCR", False);

  // save output pointers
  m_actualTarget = actualTarget;
  m_data = data;

  // assume failure
  *m_actualTarget = None;
  *m_data = "";

  // delete target property
  XDeleteProperty(display, m_requestor, m_property);

  // select window for property changes
  XWindowAttributes attr;
  XGetWindowAttributes(display, m_requestor, &attr);
  XSelectInput(display, m_requestor, attr.your_event_mask | PropertyChangeMask);

  // request data conversion
  XConvertSelection(display, selection, target, m_property, m_requestor, m_time);

  // synchronize with server before we start following timeout countdown
  XSync(display, False);

  // Xlib inexplicably omits the ability to wait for an event with
  // a timeout.  (it's inexplicable because there's no portable way
  // to do it.)  we'll poll until we have what we're looking for or
  // a timeout expires.  we use a timeout so we don't get locked up
  // by badly behaved selection owners.
  XEvent xevent;
  std::vector<XEvent> events;
  Stopwatch timeout(false);             // timer not stopped, not triggered
  static const double s_timeout = 0.25; // FIXME -- is this too short?
  bool noWait = false;
  while (!m_done && !m_failed) {
    // fail if timeout has expired
    if (timeout.getTime() >= s_timeout) {
      m_failed = true;
      break;
    }

    // process events if any otherwise sleep
    if (noWait || XPending(display) > 0) {
      while (!m_done && !m_failed && (noWait || XPending(display) > 0)) {
        // fail if timeout has expired
        if (timeout.getTime() >= s_timeout) {
          m_failed = true;
          break;
        }

        XNextEvent(display, &xevent);
        if (!processEvent(display, &xevent)) {
          // not processed so save it
          events.push_back(xevent);
        } else {
          // reset timer since we've made some progress
          timeout.reset();

          // don't sleep anymore, just block waiting for events.
          // we're assuming here that the clipboard owner will
          // complete the protocol correctly.  if we continue to
          // sleep we'll get very bad performance.
          noWait = true;
        }
      }
    } else {
      Arch::sleep(0.01);
    }
  }

  // put unprocessed events back
  for (uint32_t i = events.size(); i > 0; --i) {
    XPutBackEvent(display, &events[i - 1]);
  }

  // restore mask
  XSelectInput(display, m_requestor, attr.your_event_mask);

  // return success or failure
  LOG_VERBOSE("request %s after %fs", m_failed ? "failed" : "succeeded", timeout.getTime());
  return !m_failed;
}

bool XWindowsClipboard::CICCCMGetClipboard::processEvent(Display *display, const XEvent *xevent)
{
  // process event
  switch (xevent->type) {
  case DestroyNotify:
    if (xevent->xdestroywindow.window == m_requestor) {
      m_failed = true;
      return true;
    }

    // not interested
    return false;

  case SelectionNotify:
    if (xevent->xselection.requestor == m_requestor) {
      // done if we can't convert
      if (xevent->xselection.property == None || xevent->xselection.property == m_atomNone) {
        m_done = true;
        return true;
      }

      // proceed if conversion successful
      else if (xevent->xselection.property == m_property) {
        m_reading = true;
        break;
      }
    }

    // otherwise not interested
    return false;

  case PropertyNotify:
    // proceed if conversion successful and we're receiving more data
    if (xevent->xproperty.window == m_requestor && xevent->xproperty.atom == m_property &&
        xevent->xproperty.state == PropertyNewValue) {
      if (!m_reading) {
        // we haven't gotten the SelectionNotify yet
        return true;
      }
      break;
    }

    // otherwise not interested
    return false;

  default:
    // not interested
    return false;
  }

  // get the data from the property
  Atom target;
  const std::string::size_type oldSize = m_data->size();
  if (!XWindowsUtil::getWindowProperty(display, m_requestor, m_property, m_data, &target, nullptr, True)) {
    // unable to read property
    m_failed = true;
    return true;
  }

  // note if incremental.  if we're already incremental then the
  // selection owner is busted.  if the INCR property has no size
  // then the selection owner is busted.
  if (target == m_atomIncr) {
    if (m_incr || m_data->size() == oldSize) {
      m_failed = true;
      m_error = true;
    } else {
      m_incr = true;

      // discard INCR data
      *m_data = "";
    }
  }

  // handle incremental chunks
  else if (m_incr) {
    // if first incremental chunk then save target
    if (oldSize == 0) {
      LOG_VERBOSE("  INCR first chunk, target %s", XWindowsUtil::atomToString(display, target).c_str());
      *m_actualTarget = target;
    }

    // secondary chunks must have the same target
    else {
      if (target != *m_actualTarget) {
        LOG_WARN("  INCR target mismatch");
        m_failed = true;
        m_error = true;
      }
    }

    // note if this is the final chunk
    if (m_data->size() == oldSize) {
      LOG_VERBOSE("  INCR final chunk: %d bytes total", m_data->size());
      m_done = true;
    }
  }

  // not incremental;  save the target.
  else {
    LOG_VERBOSE("  target %s", XWindowsUtil::atomToString(display, target).c_str());
    *m_actualTarget = target;
    m_done = true;
  }

  // this event has been processed
  LOGC(!m_incr, (CLOG_VERBOSE "  got data, %d bytes", m_data->size()));
  return true;
}

//
// XWindowsClipboard::Reply
//
