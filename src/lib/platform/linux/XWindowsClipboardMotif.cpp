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

bool XWindowsClipboard::motifLockClipboard() const
{
  // fail if anybody owns the lock (even us, so this is non-recursive)
  Window lockOwner = XGetSelectionOwner(m_display, m_atomMotifClipLock);
  if (lockOwner != None) {
    LOG_VERBOSE("motif lock owner 0x%08x", lockOwner);
    return false;
  }

  // try to grab the lock
  // FIXME -- is this right?  there's a race condition here --
  // A grabs successfully, B grabs successfully, A thinks it
  // still has the grab until it gets a SelectionClear.
  Time time = XWindowsUtil::getCurrentTime(m_display, m_window);
  XSetSelectionOwner(m_display, m_atomMotifClipLock, m_window, time);
  lockOwner = XGetSelectionOwner(m_display, m_atomMotifClipLock);
  if (lockOwner != m_window) {
    LOG_VERBOSE("motif lock owner 0x%08x", lockOwner);
    return false;
  }

  LOG_VERBOSE("locked motif clipboard");
  return true;
}

void XWindowsClipboard::motifUnlockClipboard() const
{
  LOG_VERBOSE("unlocked motif clipboard");

  // fail if we don't own the lock
  if (Window lockOwner = XGetSelectionOwner(m_display, m_atomMotifClipLock); lockOwner != m_window) {
    return;
  }

  // release lock
  Time time = XWindowsUtil::getCurrentTime(m_display, m_window);
  XSetSelectionOwner(m_display, m_atomMotifClipLock, None, time);
}

bool XWindowsClipboard::motifOwnsClipboard() const
{
  // get the current selection owner
  // FIXME -- this can't be right.  even if the window is destroyed
  // Motif will still have a valid clipboard.  how can we tell if
  // some other client owns CLIPBOARD?
  Window owner = XGetSelectionOwner(m_display, m_selection);
  if (owner == None) {
    return false;
  }

  // get the Motif clipboard header property from the root window
  Atom target;
  int32_t format;
  std::string data;
  if (Window root = RootWindow(m_display, DefaultScreen(m_display));
      !XWindowsUtil::getWindowProperty(m_display, root, m_atomMotifClipHeader, &data, &target, &format, False)) {
    return false;
  }

  // check the owner window against the current clipboard owner
  if (data.size() >= sizeof(MotifClipHeader)) {
    MotifClipHeader header;
    std::memcpy(&header, data.data(), sizeof(header));
    if ((header.m_id == MotifClip::Header) && (static_cast<Window>(header.m_selectionOwner) == owner)) {
      return true;
    }
  }

  return false;
}

void XWindowsClipboard::motifFillCache()
{
  LOG_DEBUG("motif fill clipboard %d", m_id);

  // get the Motif clipboard header property from the root window
  Atom target;
  int32_t format;
  std::string data;
  Window root = RootWindow(m_display, DefaultScreen(m_display));
  if (!XWindowsUtil::getWindowProperty(m_display, root, m_atomMotifClipHeader, &data, &target, &format, False)) {
    return;
  }

  MotifClipHeader header;
  if (data.size() < sizeof(header)) { // check that the header is okay
    return;
  }
  std::memcpy(&header, data.data(), sizeof(header));
  if (header.m_id != MotifClip::Header || header.m_numItems < 1) {
    return;
  }

  // get the Motif item property from the root window
  static const int buffer_size = 18 + 20;
  char name[buffer_size];
#if HAVE_FORMAT
  std::format_to_n(name, buffer_size, "_MOTIF_CLIP_ITEM_{}", header.m_item);
#else
  snprintf(name, buffer_size, "_MOTIF_CLIP_ITEM_%d", header.m_item);
#endif
  Atom atomItem = XInternAtom(m_display, name, False);
  data = "";
  if (!XWindowsUtil::getWindowProperty(m_display, root, atomItem, &data, &target, &format, False)) {
    return;
  }

  MotifClipItem item;
  if (data.size() < sizeof(item)) { // check that the item is okay
    return;
  }
  std::memcpy(&item, data.data(), sizeof(item));
  if (item.m_id != MotifClip::Item || item.m_numFormats - item.m_numDeletedFormats < 1) {
    return;
  }

  // format list is after static item structure elements
  const int32_t numFormats = item.m_numFormats - item.m_numDeletedFormats;
  auto formats = static_cast<const int32_t *>(static_cast<const void *>(item.m_size + data.data()));

  // get the available formats
  using MotifFormatMap = std::map<Atom, std::string>;
  MotifFormatMap motifFormats;
  for (int32_t i = 0; i < numFormats; ++i) {
    // get Motif format property from the root window
#if HAVE_FORMAT
    std::format_to_n(name, buffer_size, "_MOTIF_CLIP_ITEM_{}", formats[i]);
#else
    snprintf(name, buffer_size, "_MOTIF_CLIP_ITEM_%d", formats[i]);
#endif
    Atom atomFormat = XInternAtom(m_display, name, False);
    std::string data;
    if (!XWindowsUtil::getWindowProperty(m_display, root, atomFormat, &data, &target, &format, False)) {
      continue;
    }

    // check that the format is okay
    MotifClipFormat motifFormat;
    if (data.size() < sizeof(motifFormat)) {
      continue;
    }
    std::memcpy(&motifFormat, data.data(), sizeof(motifFormat));
    if (motifFormat.m_id != MotifClip::Format || motifFormat.m_length < 0 || motifFormat.m_type == None ||
        motifFormat.m_deleted != 0) {
      continue;
    }

    // save it
    motifFormats.insert(std::make_pair(motifFormat.m_type, data));
  }

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
    MotifFormatMap::const_iterator index2 = motifFormats.find(converter->getAtom());
    if (index2 == motifFormats.end()) {
      continue;
    }

    // get format
    MotifClipFormat motifFormat;
    std::memcpy(&motifFormat, index2->second.data(), sizeof(motifFormat));
    const Atom target = motifFormat.m_type;

    // get the data (finally)
    Atom actualTarget;
    std::string targetData;
    if (!motifGetSelection(&motifFormat, &actualTarget, &targetData)) {
      LOG_VERBOSE("  no data for target %s", XWindowsUtil::atomToString(m_display, target).c_str());
      continue;
    }

    // add to clipboard and note we've done it
    m_data[formatID] = converter->toIClipboard(targetData);
    m_added[formatID] = true;
    LOG_DEBUG("added format %d for target %s", format, XWindowsUtil::atomToString(m_display, target).c_str());
  }
}

bool XWindowsClipboard::motifGetSelection(const MotifClipFormat *format, Atom *actualTarget, std::string *data) const
{
  // if the current clipboard owner and the owner indicated by the
  // motif clip header are the same then transfer via a property on
  // the root window, otherwise transfer as a normal ICCCM client.
  if (!motifOwnsClipboard()) {
    return icccmGetSelection(format->m_type, actualTarget, data);
  }

  // use motif way
  // FIXME -- this isn't right.  it'll only work if the data is
  // already stored on the root window and only if it fits in a
  // property.  motif has some scheme for transferring part by
  // part that i don't know.
  static const int buffer_size = 18 + 20;
  char name[buffer_size];
#if HAVE_FORMAT
  std::format_to_n(name, buffer_size, "_MOTIF_CLIP_ITEM_{}", format->m_data);
#else
  snprintf(name, buffer_size, "_MOTIF_CLIP_ITEM_%d", format->m_data);
#endif
  Atom target = XInternAtom(m_display, name, False);
  Window root = RootWindow(m_display, DefaultScreen(m_display));
  return XWindowsUtil::getWindowProperty(m_display, root, target, data, actualTarget, nullptr, False);
}

IClipboard::Time XWindowsClipboard::motifGetTime() const
{
  return icccmGetTime();
}
