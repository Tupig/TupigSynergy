/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXKeyState.h"
#include "OSXMediaKeySupport.h"
#include "OSXUchrKeyResource.h"
#include "arch/Arch.h"
#include "base/Log.h"

#include "OSXSDKHeaders.h"
#include <IOKit/hidsystem/IOHIDLib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

KeyButton OSXKeyState::mapKeyFromEvent(KeyIDs &ids, KeyModifierMask *maskOut, CGEventRef event) const
{
  ids.clear();

  // map modifier key
  if (maskOut != nullptr) {
    KeyModifierMask activeMask = getActiveModifiers();
    activeMask &= ~KeyModifierAltGr;
    *maskOut = activeMask;
  }

  // get virtual key
  uint32_t vkCode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

  // handle up events
  uint32_t eventKind = CGEventGetType(event);
  if (eventKind == kCGEventKeyUp) {
    // the id isn't used.  we just need the same button we used on
    // the key press.  note that we don't use or reset the dead key
    // state;  up events should not affect the dead key state.
    ids.push_back(kKeyNone);
    return mapVirtualKeyToKeyButton(vkCode);
  }

  // check for special keys
  VirtualKeyMap::const_iterator i = m_virtualKeyMap.find(vkCode);
  if (i != m_virtualKeyMap.end()) {
    m_deadKeyState = 0;
    ids.push_back(i->second);
    return mapVirtualKeyToKeyButton(vkCode);
  }

  // get keyboard info
  AutoTISInputSourceRef currentKeyboardLayout(nullptr, CFRelease);
  CFDataRef ref = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_tisMutex);
    currentKeyboardLayout = AutoTISInputSourceRef(TISCopyCurrentKeyboardLayoutInputSource(), CFRelease);
    if (currentKeyboardLayout)
      ref = (CFDataRef)TISGetInputSourceProperty(currentKeyboardLayout.get(), kTISPropertyUnicodeKeyLayoutData);
  }

  if (!currentKeyboardLayout) {
    return kKeyNone;
  }

  // get the event modifiers and remove the command and control
  // keys.  note if we used them though.
  // UCKeyTranslate expects old-style Carbon modifiers, so convert.
  uint32_t modifiers;
  modifiers = mapModifiersToCarbon(CGEventGetFlags(event));
  static const uint32_t s_commandModifiers = cmdKey | controlKey | rightControlKey;
  bool isCommand = ((modifiers & s_commandModifiers) != 0);
  modifiers &= ~s_commandModifiers;

  // if we've used a command key then we want the glyph produced without
  // the option key (i.e. the base glyph).
  // if (isCommand) {
  modifiers &= ~optionKey;
  //}

  // choose action
  uint16_t action;
  if (eventKind == kCGEventKeyDown) {
    action = kUCKeyActionDown;
  } else if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) == 1) {
    action = kUCKeyActionAutoKey;
  } else {
    return 0;
  }

  // translate via uchr resource
  const UCKeyboardLayout *layout = (const UCKeyboardLayout *)CFDataGetBytePtr(ref);
  const bool layoutValid = (layout != nullptr);

  if (layoutValid) {
    // translate key
    UniCharCount count;
    UniChar chars[2];
    LOG_VERBOSE("modifiers: %08x", modifiers & 0xffu);
    OSStatus status = UCKeyTranslate(
        layout, vkCode & 0xffu, action, (modifiers >> 8) & 0xffu, LMGetKbdType(), 0, &m_deadKeyState,
        sizeof(chars) / sizeof(chars[0]), &count, chars
    );

    // get the characters
    if (status == 0) {
      if (count != 0 || m_deadKeyState == 0) {
        m_deadKeyState = 0;
        for (UniCharCount i = 0; i < count; ++i) {
          ids.push_back(IOSXKeyResource::unicharToKeyID(chars[i]));
        }
        adjustAltGrModifier(ids, maskOut, isCommand);
        return mapVirtualKeyToKeyButton(vkCode);
      }
      return 0;
    }
  }

  return 0;
}

void OSXKeyState::getKeyMap(synergy::KeyMap &keyMap)
{
  // update keyboard groups
  int32_t numGroups{0};
  if (getGroups(m_groups)) {
    m_groupMap.clear();
    numGroups = CFArrayGetCount(m_groups.get());
    for (int32_t g = 0; g < numGroups; ++g) {
      TISInputSourceRef keyboardLayout = (TISInputSourceRef)CFArrayGetValueAtIndex(m_groups.get(), g);
      CFDataRef id = nullptr;
      {
        std::lock_guard<std::mutex> lock(g_tisMutex);
        id = (CFDataRef)TISGetInputSourceProperty(keyboardLayout, kTISPropertyInputSourceID);
      }
      m_groupMap[id] = g;
    }
  }

  uint32_t keyboardType = LMGetKbdType();
  for (int32_t g = 0; g < numGroups; ++g) {
    // add special keys
    getKeyMapForSpecialKeys(keyMap, g);

    const void *resource;
    bool layoutValid = false;

    // add regular keys
    // try uchr resource first
    TISInputSourceRef keyboardLayout = (TISInputSourceRef)CFArrayGetValueAtIndex(m_groups.get(), g);
    CFDataRef resourceRef = nullptr;
    {
      std::lock_guard<std::mutex> lock(g_tisMutex);
      resourceRef = (CFDataRef)TISGetInputSourceProperty(keyboardLayout, kTISPropertyUnicodeKeyLayoutData);
    }

    layoutValid = resourceRef != nullptr;
    if (layoutValid)
      resource = CFDataGetBytePtr(resourceRef);

    if (layoutValid) {
      OSXUchrKeyResource uchr(resource, keyboardType);
      if (uchr.isValid()) {
        LOG_VERBOSE("using uchr resource for group %d", g);
        getKeyMap(keyMap, g, uchr);
        continue;
      }
    }

    LOG_VERBOSE("no keyboard resource for group %d", g);
  }
}

bool OSXKeyState::getKeyMap(synergy::KeyMap &keyMap, int32_t group, const IOSXKeyResource &r) const
{
  if (!r.isValid()) {
    return false;
  }

  // space for all possible modifier combinations
  std::vector<bool> modifiers(r.getNumModifierCombinations());

  // make space for the keys that any single button can synthesize
  std::vector<std::pair<KeyID, bool>> buttonKeys(r.getNumTables());

  // iterate over each button
  synergy::KeyMap::KeyItem item;
  for (uint32_t i = 0; i < r.getNumButtons(); ++i) {
    item.m_button = mapVirtualKeyToKeyButton(i);

    // the KeyIDs we've already handled
    std::set<KeyID> keys;

    // convert the entry in each table for this button to a KeyID
    for (uint32_t j = 0; j < r.getNumTables(); ++j) {
      buttonKeys[j].first = r.getKey(j, i);
      buttonKeys[j].second = synergy::KeyMap::isDeadKey(buttonKeys[j].first);
    }

    // iterate over each character table
    for (uint32_t j = 0; j < r.getNumTables(); ++j) {
      // get the KeyID for the button/table
      KeyID id = buttonKeys[j].first;
      if (id == kKeyNone) {
        continue;
      }

      // if we've already handled the KeyID in the table then
      // move on to the next table
      if (keys.count(id) > 0) {
        continue;
      }
      keys.insert(id);

      // prepare item.  the client state is 1 for dead keys.
      item.m_id = id;
      item.m_group = group;
      item.m_dead = buttonKeys[j].second;
      item.m_client = buttonKeys[j].second ? 1 : 0;
      synergy::KeyMap::initModifierKey(item);
      if (item.m_lock) {
        // all locking keys are half duplex on OS X
        keyMap.addHalfDuplexButton(i);
      }

      // collect the tables that map to the same KeyID.  we know it
      // can't be any earlier tables because of the check above.
      std::set<uint8_t> tables;
      tables.insert(static_cast<uint8_t>(j));
      for (uint32_t k = j + 1; k < r.getNumTables(); ++k) {
        if (buttonKeys[k].first == id) {
          tables.insert(static_cast<uint8_t>(k));
        }
      }

      // collect the modifier combinations that map to any of the
      // tables we just collected
      for (uint32_t k = 0; k < r.getNumModifierCombinations(); ++k) {
        modifiers[k] = (tables.count(r.getTableForModifier(k)) > 0);
      }

      // figure out which modifiers the key is sensitive to.  the
      // key is insensitive to a modifier if for every modifier mask
      // with the modifier bit unset in the modifiers we also find
      // the same mask with the bit set.
      //
      // we ignore a few modifiers that we know aren't important
      // for generating characters.  in fact, we want to ignore any
      // characters generated by the control key.  we don't map
      // those and instead expect the control modifier plus a key.
      uint32_t sensitive = 0;
      for (uint32_t k = 0; (1u << k) < r.getNumModifierCombinations(); ++k) {
        uint32_t bit = (1u << k);
        if ((bit << 8) == cmdKey || (bit << 8) == controlKey || (bit << 8) == rightControlKey) {
          continue;
        }
        for (uint32_t m = 0; m < r.getNumModifierCombinations(); ++m) {
          if (modifiers[m] != modifiers[m ^ bit]) {
            sensitive |= bit;
            break;
          }
        }
      }

      // find each required modifier mask.  the key can be synthesized
      // using any of the masks.
      std::set<uint32_t> required;
      for (uint32_t k = 0; k < r.getNumModifierCombinations(); ++k) {
        if ((k & sensitive) == k && modifiers[k & sensitive]) {
          required.insert(k);
        }
      }

      // now add a key entry for each key/required modifier pair.
      item.m_sensitive = mapModifiersFromOSX(sensitive << 16);
      for (std::set<uint32_t>::iterator k = required.begin(); k != required.end(); ++k) {
        item.m_required = mapModifiersFromOSX(*k << 16);
        keyMap.addKeyEntry(item);
      }
    }
  }

  return true;
}

bool OSXKeyState::getGroups(AutoCFArray &groups) const
{
  // get number of layouts
  CFStringRef keys[] = {kTISPropertyInputSourceCategory};
  CFStringRef values[] = {kTISCategoryKeyboardInputSource};
  AutoCFDictionary dict(
      CFDictionaryCreate(nullptr, (const void **)keys, (const void **)values, 1, nullptr, nullptr), CFRelease
  );
  AutoCFArray kbds(nullptr, CFRelease);
  {
    std::lock_guard<std::mutex> lock(g_tisMutex);
    kbds = AutoCFArray(TISCreateInputSourceList(dict.get(), false), CFRelease);
  }

  if (CFArrayGetCount(kbds.get()) > 0) {
    groups = std::move(kbds);
  } else {
    LOG_VERBOSE("can't get keyboard layouts");
    return false;
  }

  return true;
}

void OSXKeyState::setGroup(int32_t group)
{
  TISInputSourceRef keyboardLayout = (TISInputSourceRef)CFArrayGetValueAtIndex(m_groups.get(), group);
  if (!keyboardLayout) {
    LOG_WARN("needed keyboard layout is null");
    return;
  }
  CFBooleanRef canBeSetted = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_tisMutex);
    AutoTISInputSourceRef source(TISCopyCurrentKeyboardInputSource(), CFRelease);
    if (source)
      canBeSetted = (CFBooleanRef)TISGetInputSourceProperty(source.get(), kTISPropertyInputSourceIsEnableCapable);
  }
  if (!canBeSetted) {
    LOG_WARN("needed keyboard layout is disabled for programmatically selection");
    return;
  }

  {
    std::lock_guard<std::mutex> lock(g_tisMutex);
    if (TISSelectInputSource(keyboardLayout) != noErr) {
      LOG_WARN("failed to set needed keyboard layout");
    }
  }

  LOG_VERBOSE("keyboard layout change to %d", group);

  // A minimal delay is needed after a group change because the
  // keyboard key event often happens immediately after.
  // Language (TIS) and event (CG) systems are not in the mutual
  // event queue and without a delay the subsequent key press
  // event could be applied before the keyboard layout would
  // actually be changed.
  Arch::sleep(.01);
}

void OSXKeyState::adjustAltGrModifier(const KeyIDs &ids, KeyModifierMask *mask, bool isCommand) const
{
  if (!isCommand) {
    for (KeyIDs::const_iterator i = ids.begin(); i != ids.end(); ++i) {
      KeyID id = *i;
      if (id != kKeyNone && ((id < 0xe000u || id > 0xefffu) || (id >= kKeyKP_Equal && id <= kKeyKP_9))) {
        *mask |= KeyModifierAltGr;
        return;
      }
    }
  }
}

#pragma clang diagnostic pop
