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

//
// XWindowsClipboard
//

XWindowsClipboard::XWindowsClipboard(Display *display, Window window, ClipboardID id)
    : m_display(display),
      m_window(window),
      m_id(id)
{
  // get some atoms
  m_atomTargets = XInternAtom(m_display, "TARGETS", False);
  m_atomMultiple = XInternAtom(m_display, "MULTIPLE", False);
  m_atomTimestamp = XInternAtom(m_display, "TIMESTAMP", False);
  m_atomInteger = XInternAtom(m_display, "INTEGER", False);
  m_atomAtom = XInternAtom(m_display, "ATOM", False);
  m_atomAtomPair = XInternAtom(m_display, "ATOM_PAIR", False);
  m_atomData = XInternAtom(m_display, "CLIP_TEMPORARY", False);
  m_atomINCR = XInternAtom(m_display, "INCR", False);
  m_atomMotifClipLock = XInternAtom(m_display, "_MOTIF_CLIP_LOCK", False);
  m_atomMotifClipHeader = XInternAtom(m_display, "_MOTIF_CLIP_HEADER", False);
  m_atomMotifClipAccess = XInternAtom(m_display, "_MOTIF_CLIP_LOCK_ACCESS_VALID", False);
  m_atomGDKSelection = XInternAtom(m_display, "GDK_SELECTION", False);

  // set selection atom based on clipboard id
  if (id == kClipboardClipboard) {
    m_selection = XInternAtom(m_display, "CLIPBOARD", False);
  } else {
    m_selection = XA_PRIMARY;
  }

  // add converters, most desired first
  m_converters.push_back(new XWindowsClipboardHTMLConverter(m_display, "text/html"));
  m_converters.push_back(new XWindowsClipboardHTMLConverter(m_display, "application/x-moz-nativehtml"));
  m_converters.push_back(new XWindowsClipboardBMPConverter(m_display));
  m_converters.push_back(new XWindowsClipboardUTF8Converter(m_display, "text/plain;charset=UTF-8", true));
  m_converters.push_back(new XWindowsClipboardUTF8Converter(m_display, "text/plain;charset=utf-8", true));
  m_converters.push_back(new XWindowsClipboardUTF8Converter(m_display, "UTF8_STRING"));
  m_converters.push_back(new XWindowsClipboardUCS2Converter(m_display, "text/plain;charset=ISO-10646-UCS-2"));
  m_converters.push_back(new XWindowsClipboardUCS2Converter(m_display, "text/unicode"));
  m_converters.push_back(new XWindowsClipboardTextConverter(m_display, "text/plain"));
  m_converters.push_back(new XWindowsClipboardTextConverter(m_display, "STRING"));

  // we have no data
  clearCache();
}

XWindowsClipboard::~XWindowsClipboard()
{
  clearReplies();
  clearConverters();
}

void XWindowsClipboard::lost(Time time)
{
  LOG_DEBUG("lost clipboard %d ownership at %d", m_id, time);
  if (m_owner) {
    m_owner = false;
    m_timeLost = time;
    clearCache();
  }
}

void XWindowsClipboard::addRequest(Window owner, Window requestor, Atom target, ::Time time, Atom property)
{
  // must be for our window and we must have owned the selection
  // at the given time.
  bool success = false;
  if (owner == m_window) {
    LOG_VERBOSE(
        "request for clipboard %d, target %s by 0x%08x (property=%s)", m_selection,
        XWindowsUtil::atomToString(m_display, target).c_str(), requestor,
        XWindowsUtil::atomToString(m_display, property).c_str()
    );
    if (wasOwnedAtTime(time)) {
      if (target == m_atomMultiple && property != None) {
        // add a multiple request.  property may not be None
        // according to ICCCM.
        success = insertMultipleReply(requestor, time, property);
      } else {
        addSimpleRequest(requestor, target, time, property);

        // addSimpleRequest() will have already handled failure
        success = true;
      }
    } else {
      LOG_VERBOSE("clipboard not owned at time %d", time);
    }
  }

  if (!success) {
    // send failure
    LOG_VERBOSE("clipboard request was not added");
    insertReply(new Reply(requestor, target, time));
  }

  // send notifications that are pending
  pushReplies();
}

bool XWindowsClipboard::addSimpleRequest(Window requestor, Atom target, ::Time time, Atom property)
{
  // obsolete requestors may supply a None property.  in
  // that case we use the target as the property to store
  // the conversion.
  if (property == None) {
    property = target;
  }

  // handle targets
  std::string data;
  Atom type = None;
  int format = 0;
  if (target == m_atomTargets) {
    type = getTargetsData(data, &format);
  } else if (target == m_atomTimestamp) {
    type = getTimestampData(data, &format);
  } else {
    const IXWindowsClipboardConverter *converter = getConverter(target);
    if (converter != nullptr) {
      const auto clipboardFormat = static_cast<int>(converter->getFormat());
      if (m_added[clipboardFormat]) {
        try {
          data = converter->fromIClipboard(m_data[clipboardFormat]);
          format = converter->getDataSize();
          type = converter->getAtom();
        } catch (...) {
          // ignore -- cannot convert
          LOG_WARN("error while converting clipboard data");
        }
      }
    }
  }

  if (type != None) {
    // success
    LOG_VERBOSE("clipboard request added");
    insertReply(new Reply(requestor, target, time, property, data, type, format));
    return true;
  } else {
    // failure
    LOG_VERBOSE("clipboard request not added");
    insertReply(new Reply(requestor, target, time));
    return false;
  }
}

bool XWindowsClipboard::processRequest(Window requestor, ::Time /*time*/, Atom property)
{
  ReplyMap::iterator index = m_replies.find(requestor);
  if (index == m_replies.end()) {
    // unknown requestor window
    return false;
  }
  LOG_VERBOSE(
      "received property %s delete from 0x08%x", XWindowsUtil::atomToString(m_display, property).c_str(), requestor
  );

  // find the property in the known requests.  it should be the
  // first property but we'll check 'em all if we have to.
  ReplyList &replies = index->second;
  for (auto index2 = replies.begin(); index2 != replies.end(); ++index2) {
    const Reply *reply = *index2;
    if (reply->m_replied && reply->m_property == property) {
      // if reply is complete then remove it and start the
      // next one.
      pushReplies(index, replies, index2);
      return true;
    }
  }

  return false;
}

bool XWindowsClipboard::destroyRequest(Window requestor)
{
  ReplyMap::iterator index = m_replies.find(requestor);
  if (index == m_replies.end()) {
    // unknown requestor window
    return false;
  }

  // destroy all replies for this window
  clearReplies(index->second);
  m_replies.erase(index);

  // note -- we don't stop watching the window for events because
  // we're called in response to the window being destroyed.

  return true;
}

Window XWindowsClipboard::getWindow() const
{
  return m_window;
}

Atom XWindowsClipboard::getSelection() const
{
  return m_selection;
}

bool XWindowsClipboard::empty()
{
  assert(m_open);

  LOG_DEBUG("empty clipboard %d", m_id);

  // assert ownership of clipboard
  XSetSelectionOwner(m_display, m_selection, m_window, m_time);
  if (XGetSelectionOwner(m_display, m_selection) != m_window) {
    LOG_WARN("failed to grab clipboard %d", m_id);
    return false;
  }

  // clear all data.  since we own the data now, the cache is up
  // to date.
  clearCache();
  m_cached = true;

  // FIXME -- actually delete motif clipboard items?
  // FIXME -- do anything to motif clipboard properties?

  // save time
  m_timeOwned = m_time;
  m_timeLost = 0;

  // we're the owner now
  m_owner = true;
  LOG_DEBUG("grabbed clipboard %d", m_id);

  return true;
}

void XWindowsClipboard::add(Format format, const std::string &data)
{
  assert(m_open);
  assert(m_owner);

  LOG_DEBUG("add %d bytes to clipboard %d format: %d", data.size(), m_id, format);

  const auto formatID = static_cast<int>(format);
  m_data[formatID] = data;
  m_added[formatID] = true;

  // FIXME -- set motif clipboard item?
}

bool XWindowsClipboard::open(Time time) const
{
  if (m_open) {
    LOG_WARN("failed to open clipboard: already opened");
    return false;
  }

  LOG_DEBUG("open clipboard %d", m_id);

  // assume not motif
  m_motif = false;

  // lock clipboard
  if (m_id == kClipboardClipboard) {
    if (!motifLockClipboard()) {
      return false;
    }

    // check if motif owns the selection.  unlock motif clipboard
    // if it does not.
    m_motif = motifOwnsClipboard();
    LOG_VERBOSE("motif does %sown clipboard", m_motif ? "" : "not ");
    if (!m_motif) {
      motifUnlockClipboard();
    }
  }

  // now open
  m_open = true;
  m_time = time;

  // be sure to flush the cache later if it's dirty
  m_checkCache = true;

  return true;
}

void XWindowsClipboard::close() const
{
  assert(m_open);

  LOG_DEBUG("close clipboard %d", m_id);

  // unlock clipboard
  if (m_motif) {
    motifUnlockClipboard();
  }

  m_motif = false;
  m_open = false;
}

IClipboard::Time XWindowsClipboard::getTime() const
{
  checkCache();
  return m_timeOwned;
}

bool XWindowsClipboard::has(Format format) const
{
  assert(m_open);

  fillCache();
  return m_added[static_cast<int>(format)];
}

std::string XWindowsClipboard::get(Format format) const
{
  assert(m_open);

  fillCache();
  return m_data[static_cast<int>(format)];
}

void XWindowsClipboard::clearConverters()
{
  for (auto index = m_converters.begin(); index != m_converters.end(); ++index) {
    delete *index;
  }
  m_converters.clear();
}

IXWindowsClipboardConverter *XWindowsClipboard::getConverter(Atom target, bool onlyIfNotAdded) const
{
  IXWindowsClipboardConverter *converter = nullptr;
  for (auto index = m_converters.begin(); index != m_converters.end(); ++index) {
    converter = *index;
    if (converter->getAtom() == target) {
      break;
    }
  }
  if (converter == nullptr) {
    LOG_VERBOSE("  no converter for target %s", XWindowsUtil::atomToString(m_display, target).c_str());
    return nullptr;
  }

  // optionally skip already handled targets
  if (const auto formatID = static_cast<int>(converter->getFormat()); onlyIfNotAdded && m_added[formatID]) {
    LOG_VERBOSE("  skipping handled format %d", formatID);
    return nullptr;
  }

  return converter;
}

void XWindowsClipboard::checkCache() const
{
  if (!m_checkCache) {
    return;
  }
  m_checkCache = false;

  // get the time the clipboard ownership was taken by the current
  // owner.
  if (m_motif) {
    m_timeOwned = motifGetTime();
  } else {
    m_timeOwned = icccmGetTime();
  }

  // if we can't get the time then use the time passed to us
  if (m_timeOwned == 0) {
    m_timeOwned = m_time;
  }

  // if the cache is dirty then flush it
  if (m_timeOwned != m_cacheTime) {
    clearCache();
  }
}

void XWindowsClipboard::clearCache() const
{
  const_cast<XWindowsClipboard *>(this)->doClearCache();
}

void XWindowsClipboard::doClearCache()
{
  m_checkCache = false;
  m_cached = false;
  for (int32_t index = 0; index < static_cast<int>(Format::TotalFormats); ++index) {
    m_data[index] = "";
    m_added[index] = false;
  }
}

void XWindowsClipboard::fillCache() const
{
  // get the selection data if not already cached
  checkCache();
  if (!m_cached) {
    const_cast<XWindowsClipboard *>(this)->doFillCache();
  }
}

void XWindowsClipboard::doFillCache()
{
  if (m_motif) {
    motifFillCache();
  } else {
    icccmFillCache();
  }
  m_checkCache = false;
  m_cached = true;
  m_cacheTime = m_timeOwned;
}

void XWindowsClipboard::icccmFillCache()
{
  LOG_DEBUG("icccm fill clipboard %d", m_id);

  // see if we can get the list of available formats from the selection.
  // if not then use a default list of formats.  note that some clipboard
  // owners are broken and report TARGETS as the type of the TARGETS data
  // instead of the correct type ATOM;  allow either.
  const Atom atomTargets = m_atomTargets;
  Atom target;
  std::string data;
  if (!icccmGetSelection(atomTargets, &target, &data) || (target != m_atomAtom && target != m_atomTargets)) {
    LOG_VERBOSE("selection doesn't support TARGETS");
    data = "";
    XWindowsUtil::appendAtomData(data, XA_STRING);
  }

  XWindowsUtil::convertAtomProperty(data);
  auto targets = static_cast<const Atom *>(static_cast<const void *>(data.data()));
  const uint32_t numTargets = data.size() / sizeof(Atom);
  LOG_DEBUG("  available targets: %s", XWindowsUtil::atomsToString(m_display, targets, numTargets).c_str());

  // try each converter in order (because they're in order of
  // preference).
  for (ConverterList::const_iterator index = m_converters.begin(); index != m_converters.end(); ++index) {
    const IXWindowsClipboardConverter *converter = *index;
    const auto formatID = static_cast<int>(converter->getFormat());

    // skip already handled targets
    if (m_added[formatID]) {
      continue;
    }

    // see if atom is in target list
    Atom target = None;
    // XXX -- just ask for the converter's target to see if it's
    // available rather than checking TARGETS.  i've seen clipboard
    // owners that don't report all the targets they support.
    target = converter->getAtom();
    /*
    for (uint32_t i = 0; i < numTargets; ++i) {
        if (converter->getAtom() == targets[i]) {
            target = targets[i];
            break;
        }
    }
    */
    if (target == None) {
      continue;
    }

    // get the data
    Atom actualTarget;
    std::string targetData;
    if (!icccmGetSelection(target, &actualTarget, &targetData)) {
      LOG_VERBOSE("  no data for target %s", XWindowsUtil::atomToString(m_display, target).c_str());
      continue;
    }

    // add to clipboard and note we've done it
    m_data[formatID] = converter->toIClipboard(targetData);
    m_added[formatID] = true;
    LOG(
        (CLOG_DEBUG "added format %d for target %s (%u %s)", formatID,
         XWindowsUtil::atomToString(m_display, target).c_str(), targetData.size(),
         targetData.size() == 1 ? "byte" : "bytes")
    );
  }
}

bool XWindowsClipboard::icccmGetSelection(Atom target, Atom *actualTarget, std::string *data) const
{
  assert(actualTarget != nullptr);
  assert(data != nullptr);

  // request data conversion
  if (CICCCMGetClipboard getter(m_window, m_time, m_atomData);
      !getter.readClipboard(m_display, m_selection, target, actualTarget, data)) {
    LOG_VERBOSE("can't get data for selection target %s", XWindowsUtil::atomToString(m_display, target).c_str());
    LOGC(getter.error(), (CLOG_WARN "icccm violation by clipboard owner"));
    return false;
  } else if (*actualTarget == None) {
    LOG_VERBOSE("selection conversion failed for target %s", XWindowsUtil::atomToString(m_display, target).c_str());
    return false;
  }
  return true;
}

IClipboard::Time XWindowsClipboard::icccmGetTime() const
{
  Atom actualTarget;
  std::string data;
  if (icccmGetSelection(m_atomTimestamp, &actualTarget, &data) && actualTarget == m_atomInteger) {
    Time time = *static_cast<const Time *>(static_cast<const void *>(data.data()));
    LOG_VERBOSE("got ICCCM time %d", time);
    return time;
  } else {
    // no timestamp
    LOG_VERBOSE("can't get ICCCM time");
    return 0;
  }
}

bool XWindowsClipboard::wasOwnedAtTime(::Time time) const
{
  // not owned if we've never owned the selection
  checkCache();
  if (m_timeOwned == 0) {
    return false;
  }

  // if time is CurrentTime then return true if we still own the
  // selection and false if we do not.  else if we still own the
  // selection then get the current time, otherwise use
  // m_timeLost as the end time.
  Time lost = m_timeLost;
  if (m_timeLost == 0) {
    if (time == CurrentTime) {
      return true;
    } else {
      lost = XWindowsUtil::getCurrentTime(m_display, m_window);
    }
  } else {
    if (time == CurrentTime) {
      return false;
    }
  }

  // compare time to range
  Time duration = lost - m_timeOwned;
  Time when = time - m_timeOwned;
  return (/*when >= 0 &&*/ when <= duration);
}

Atom XWindowsClipboard::getTargetsData(std::string &data, int *format) const
{
  assert(format != nullptr);

  // add standard targets
  XWindowsUtil::appendAtomData(data, m_atomTargets);
  XWindowsUtil::appendAtomData(data, m_atomMultiple);
  XWindowsUtil::appendAtomData(data, m_atomTimestamp);

  // add targets we can convert to
  for (auto index = m_converters.begin(); index != m_converters.end(); ++index) {
    const IXWindowsClipboardConverter *converter = *index;

    // skip formats we don't have
    if (m_added[static_cast<int>(converter->getFormat())]) {
      XWindowsUtil::appendAtomData(data, converter->getAtom());
    }
  }

  *format = 32;
  return m_atomAtom;
}

Atom XWindowsClipboard::getTimestampData(std::string &data, int *format) const
{
  assert(format != nullptr);

  checkCache();
  XWindowsUtil::appendTimeData(data, m_timeOwned);
  *format = 32;
  return m_atomInteger;
}

//
// XWindowsClipboard::CICCCMGetClipboard
//
