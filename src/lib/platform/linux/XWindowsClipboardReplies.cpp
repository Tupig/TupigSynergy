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

bool XWindowsClipboard::insertMultipleReply(Window requestor, ::Time time, Atom property)
{
  // get the requested targets
  Atom target;
  int32_t format;
  std::string data;
  if (!XWindowsUtil::getWindowProperty(m_display, requestor, property, &data, &target, &format, False)) {
    // can't get the requested targets
    return false;
  }

  // fail if the requested targets isn't of the correct form
  if (format != 32 || target != m_atomAtomPair) {
    return false;
  }

  // data is a list of atom pairs:  target, property
  XWindowsUtil::convertAtomProperty(data);
  auto targets = static_cast<const Atom *>(static_cast<const void *>(data.data()));
  const uint32_t numTargets = data.size() / sizeof(Atom);

  // add replies for each target
  bool changed = false;
  for (uint32_t i = 0; i < numTargets; i += 2) {
    const Atom target = targets[i + 0];
    const Atom property = targets[i + 1];
    if (!addSimpleRequest(requestor, target, time, property)) {
      // note that we can't perform the requested conversion
      XWindowsUtil::replaceAtomData(data, i, None);
      changed = true;
    }
  }

  // update the targets property if we changed it
  if (changed) {
    XWindowsUtil::setWindowProperty(m_display, requestor, property, data.data(), data.size(), target, format);
  }

  // add reply for MULTIPLE request
  insertReply(new Reply(requestor, m_atomMultiple, time, property, std::string(), None, 32));

  return true;
}

void XWindowsClipboard::insertReply(Reply *reply)
{
  assert(reply != nullptr);

  // note -- we must respond to requests in order if requestor,target,time
  // are the same, otherwise we can use whatever order we like with one
  // exception:  each reply in a MULTIPLE reply must be handled in order
  // as well.  those replies will almost certainly not share targets so
  // we can't simply use requestor,target,time as map index.
  //
  // instead we'll use just the requestor.  that's more restrictive than
  // necessary but we're guaranteed to do things in the right order.
  // note that we could also include the time in the map index and still
  // ensure the right order.  but since that'll just make it harder to
  // find the right reply when handling property notify events we stick
  // to just the requestor.

  const bool newWindow = !m_replies.contains(reply->m_requestor);
  m_replies[reply->m_requestor].push_back(reply);

  // adjust requestor's event mask if we haven't done so already.  we
  // want events in case the window is destroyed or any of its
  // properties change.
  if (newWindow) {
    // note errors while we adjust event masks
    bool error = false;
    {
      XWindowsUtil::ErrorLock lock(m_display, &error);

      // get and save the current event mask
      XWindowAttributes attr;
      XGetWindowAttributes(m_display, reply->m_requestor, &attr);
      m_eventMasks[reply->m_requestor] = attr.your_event_mask;

      // add the events we want
      XSelectInput(m_display, reply->m_requestor, attr.your_event_mask | StructureNotifyMask | PropertyChangeMask);
    }

    // if we failed then the window has already been destroyed
    if (error) {
      m_replies.erase(reply->m_requestor);
      delete reply;
    }
  }
}

void XWindowsClipboard::pushReplies()
{
  // send the first reply for each window if that reply hasn't
  // been sent yet.
  for (auto index = m_replies.begin(); index != m_replies.end();) {
    assert(!index->second.empty());
    auto listit = index->second.begin();
    while (listit != index->second.end()) {
      if (!(*listit)->m_replied)
        break;
      ++listit;
    }
    if (listit != index->second.end() && !(*listit)->m_replied) {
      pushReplies(index, index->second, listit);
    } else {
      ++index;
    }
  }
}

void XWindowsClipboard::pushReplies(ReplyMap::iterator &mapIndex, ReplyList &replies, ReplyList::iterator index)
{
  Reply *reply = *index;
  while (sendReply(reply)) {
    // reply is complete.  discard it and send the next reply,
    // if any.
    index = replies.erase(index);
    delete reply;
    if (index == replies.end()) {
      break;
    }
    reply = *index;
  }

  // if there are no more replies in the list then remove the list
  // and stop watching the requestor for events.
  if (replies.empty()) {
    XWindowsUtil::ErrorLock lock(m_display);
    Window requestor = mapIndex->first;
    XSelectInput(m_display, requestor, m_eventMasks[requestor]);
    m_replies.erase(mapIndex++);
    m_eventMasks.erase(requestor);
  } else {
    ++mapIndex;
  }
}

bool XWindowsClipboard::sendReply(Reply *reply)
{
  assert(reply != nullptr);

  // bail out immediately if reply is done
  if (reply->m_done) {
    LOG((
        CLOG_VERBOSE "clipboard: finished reply to 0x%08x,%d,%d", reply->m_requestor, reply->m_target, reply->m_property
    ));
    return true;
  }

  // start in failed state if property is None
  bool failed = (reply->m_property == None);
  if (!failed) {
    LOG(
        (CLOG_VERBOSE "clipboard: setting property on 0x%08x,%d,%d", reply->m_requestor, reply->m_target,
         reply->m_property)
    );

    // send using INCR if already sending incrementally or if reply
    // is too large, otherwise just send it.
    const uint32_t maxRequestSize = 3 * XMaxRequestSize(m_display);
    const bool useINCR = (reply->m_data.size() > maxRequestSize);

    // send INCR reply if incremental and we haven't replied yet
    if (useINCR && !reply->m_replied) {
      uint32_t size = reply->m_data.size();
      if (!XWindowsUtil::setWindowProperty(
              m_display, reply->m_requestor, reply->m_property, &size, 4, m_atomINCR, 32
          )) {
        failed = true;
      }
    }

    // send more INCR reply or entire non-incremental reply
    else {
      // how much more data should we send?
      uint32_t size = reply->m_data.size() - reply->m_ptr;
      if (size > maxRequestSize)
        size = maxRequestSize;

      // send it
      if (!XWindowsUtil::setWindowProperty(
              m_display, reply->m_requestor, reply->m_property, reply->m_data.data() + reply->m_ptr, size,
              reply->m_type, reply->m_format
          )) {
        failed = true;
      } else {
        reply->m_ptr += size;

        // we've finished the reply if we just sent the zero
        // size incremental chunk or if we're not incremental.
        reply->m_done = (size == 0 || !useINCR);
      }
    }
  }

  // if we've failed then delete the property and say we're done.
  // if we haven't replied yet then we can send a failure notify,
  // otherwise we've failed in the middle of an incremental
  // transfer;  i don't know how to cancel that so i'll just send
  // the final zero-length property.
  // FIXME -- how do you gracefully cancel an incremental transfer?
  if (failed) {
    LOG(
        (CLOG_VERBOSE "clipboard: sending failure to 0x%08x,%d,%d", reply->m_requestor, reply->m_target,
         reply->m_property)
    );
    reply->m_done = true;
    if (reply->m_property != None) {
      XWindowsUtil::ErrorLock lock(m_display);
      XDeleteProperty(m_display, reply->m_requestor, reply->m_property);
    }

    if (!reply->m_replied) {
      sendNotify(reply->m_requestor, m_selection, reply->m_target, None, reply->m_time);

      // don't wait for any reply (because we're not expecting one)
      return true;
    } else {
      static const char dummy = 0;
      XWindowsUtil::setWindowProperty(
          m_display, reply->m_requestor, reply->m_property, &dummy, 0, reply->m_type, reply->m_format
      );

      // wait for delete notify
      return false;
    }
  }

  // notification already sended
  if (reply->m_replied) {
    return false;
  }

  LOG_VERBOSE("clipboard: sending notify to 0x%08x,%d,%d", reply->m_requestor, reply->m_target, reply->m_property);
  reply->m_replied = true;

  // nothing to log
  if (CLOG->getFilter() < LogLevel::Level::Verbose) {
    sendNotify(
        reply->m_requestor, m_selection, reply->m_target, reply->m_property, static_cast<unsigned int>(reply->m_time)
    );

    // wait for delete notify
    return false;
  }

  // dump every property on the requestor window to the Trace
  // log.  we've seen what appears to be a bug in lesstif and
  // knowing the properties may help design a workaround, if
  // it becomes necessary.
  XWindowsUtil::ErrorLock lock(m_display);
  int n;
  Atom *props = XListProperties(m_display, reply->m_requestor, &n);
  LOG_VERBOSE("properties of 0x%08x:", reply->m_requestor);
  for (int i = 0; i < n; ++i) {
    Atom target;
    std::string data;
    char *name = XGetAtomName(m_display, props[i]);
    if (!XWindowsUtil::getWindowProperty(m_display, reply->m_requestor, props[i], &data, &target, nullptr, False)) {
      LOG_VERBOSE("  %s: <can't read property>", name);
    } else {
      // convert to hex if contains non ascii symbols
      if (std::ranges::find_if(data, [](const unsigned char &c) { return c < 32 || c > 126; }) != data.end()) {
        const std::string hex_digits = "0123456789abcdef";
        std::string tmp;
        tmp.reserve(data.length() * 3);
        std::ranges::for_each(data, [&hex_digits, &tmp](const unsigned char &c) {
          tmp += hex_digits[c >> 16];
          tmp += hex_digits[c & 15];
          tmp += ' ';
        });
        data = tmp;
      }
      char *type = XGetAtomName(m_display, target);
      LOG_VERBOSE("  %s (%s): %s", name, type, data.c_str());
      if (type != nullptr) {
        XFree(type);
      }
    }
    if (name != nullptr) {
      XFree(name);
    }
  }
  if (props != nullptr) {
    XFree(props);
  }

  sendNotify(reply->m_requestor, m_selection, reply->m_target, reply->m_property, reply->m_time);

  // wait for delete notify
  return false;
}

void XWindowsClipboard::clearReplies()
{
  for (auto index = m_replies.begin(); index != m_replies.end(); ++index) {
    clearReplies(index->second);
  }
  m_replies.clear();
  m_eventMasks.clear();
}

void XWindowsClipboard::clearReplies(ReplyList &replies)
{
  for (auto reply : replies) {
    delete reply;
  }
  replies.clear();
}

void XWindowsClipboard::sendNotify(Window requestor, Atom selection, Atom target, Atom property, Time time)
{
  XEvent event;
  event.xselection.type = SelectionNotify;
  event.xselection.display = m_display;
  event.xselection.requestor = requestor;
  event.xselection.selection = selection;
  event.xselection.target = target;
  event.xselection.property = property;
  event.xselection.time = time;
  XWindowsUtil::ErrorLock lock(m_display);
  XSendEvent(m_display, requestor, False, 0, &event);
}

XWindowsClipboard::Reply::Reply(Window requestor, Atom target, ::Time time)
    : m_requestor(requestor),
      m_target(target),
      m_time(time),
      m_property(None),
      m_type(None),
      m_format(32)
{
  // do nothing
}

XWindowsClipboard::Reply::Reply(
    Window requestor, Atom target, ::Time time, Atom property, const std::string &data, Atom type, int format
)
    : m_requestor(requestor),
      m_target(target),
      m_time(time),
      m_property(property),
      m_data(data),
      m_type(type),
      m_format(format)
{
  // do nothing
}
