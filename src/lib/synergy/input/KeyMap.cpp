/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2005 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "KeyMap.h"
#include "KeyMapDetail.h"
#include "KeyTypes.h"
#include "base/Log.h"
#include "common/PlatformInfo.h"

#include <algorithm>
#include <assert.h>
#include <cctype>
#include <cstdlib>

namespace synergy {

KeyMap::KeyMap() : m_numGroups(0), m_composeAcrossGroups(false)
{
  m_modifierKeyItem.m_id = kKeyNone;
  m_modifierKeyItem.m_group = 0;
  m_modifierKeyItem.m_button = 0;
  m_modifierKeyItem.m_required = 0;
  m_modifierKeyItem.m_sensitive = 0;
  m_modifierKeyItem.m_generates = 0;
  m_modifierKeyItem.m_dead = false;
  m_modifierKeyItem.m_lock = false;
  m_modifierKeyItem.m_client = 0;
}

void KeyMap::swap(KeyMap &x) noexcept
{
  m_keyIDMap.swap(x.m_keyIDMap);
  m_modifierKeys.swap(x.m_modifierKeys);
  m_halfDuplex.swap(x.m_halfDuplex);
  m_halfDuplexMods.swap(x.m_halfDuplexMods);
  auto tmp1 = m_numGroups;
  m_numGroups = x.m_numGroups;
  x.m_numGroups = tmp1;
  bool tmp2 = m_composeAcrossGroups;
  m_composeAcrossGroups = x.m_composeAcrossGroups;
  x.m_composeAcrossGroups = tmp2;
}

void KeyMap::addKeyEntry(const KeyItem &item)
{
  // ignore kKeyNone
  if (item.m_id == kKeyNone) {
    return;
  }

  // resize number of groups for key
  auto numGroups = item.m_group + 1;
  if (getNumGroups() > numGroups) {
    numGroups = getNumGroups();
  }
  KeyGroupTable &groupTable = m_keyIDMap[item.m_id];
  if (groupTable.size() < static_cast<size_t>(numGroups)) {
    groupTable.resize(numGroups);
  }

  // make a list from the item
  KeyItemList items;
  items.push_back(item);

  // set group and dead key flag on the item
  KeyItem &newItem = items.back();
  newItem.m_dead = isDeadKey(item.m_id);

  // mask the required bits with the sensitive bits
  newItem.m_required &= newItem.m_sensitive;

  // see if we already have this item;  just return if so
  KeyEntryList &entries = groupTable[item.m_group];
  for (const auto &entry : entries) {
    if (entry.size() == 1 && newItem == entry.at(0)) {
      return;
    }
  }

  // add item list
  entries.push_back(items);
  LOG(
      (CLOG_VERBOSE "add key: %04x %d %03x %04x (%04x %04x %04x)%s", newItem.m_id, newItem.m_group, newItem.m_button,
       newItem.m_client, newItem.m_required, newItem.m_sensitive, newItem.m_generates, newItem.m_dead ? " dead" : "")
  );
}

void KeyMap::addKeyAliasEntry(
    KeyID targetID, int32_t group, KeyModifierMask targetRequired, KeyModifierMask targetSensitive, KeyID sourceID,
    KeyModifierMask sourceRequired, KeyModifierMask sourceSensitive
)
{
  // if we can already generate the target as desired then we're done.
  if (findCompatibleKey(targetID, group, targetRequired, targetSensitive) != nullptr) {
    return;
  }

  // find a compatible source, preferably in the same group
  for (int32_t gd = 0, n = getNumGroups(); gd < n; ++gd) {
    auto eg = getEffectiveGroup(group, gd);
    const KeyItemList *sourceEntry = findCompatibleKey(sourceID, eg, sourceRequired, sourceSensitive);
    if (sourceEntry != nullptr && sourceEntry->size() == 1) {
      KeyMap::KeyItem targetItem = sourceEntry->back();
      targetItem.m_id = targetID;
      targetItem.m_group = eg;
      addKeyEntry(targetItem);
      break;
    }
  }
}

bool KeyMap::addKeyCombinationEntry(KeyID id, int32_t group, const KeyID *keys, uint32_t numKeys)
{
  // disallow kKeyNone
  if (id == kKeyNone) {
    return false;
  }

  int32_t numGroups = group + 1;
  if (getNumGroups() > numGroups) {
    numGroups = getNumGroups();
  }
  KeyGroupTable &groupTable = m_keyIDMap[id];
  if (groupTable.size() < static_cast<size_t>(numGroups)) {
    groupTable.resize(numGroups);
  }
  if (!groupTable[group].empty()) {
    // key is already in the table
    return false;
  }

  // convert to buttons
  KeyItemList items;
  for (uint32_t i = 0; i < numKeys; ++i) {
    KeyIDMap::const_iterator gtIndex = m_keyIDMap.find(keys[i]);
    if (gtIndex == m_keyIDMap.end()) {
      return false;
    }
    const KeyGroupTable &curTable = gtIndex->second;

    // if we allow group switching during composition then search all
    // groups for keys, otherwise search just the given group.
    int32_t n = 1;
    if (m_composeAcrossGroups) {
      n = (int32_t)curTable.size();
    }

    bool found = false;
    for (int32_t gd = 0; gd < n && !found; ++gd) {
      const auto eg = (group + gd) % getNumGroups();
      const KeyEntryList &entries = curTable[eg];
      for (const auto &entry : entries) {
        if (entry.size() == 1) {
          found = true;
          items.push_back(entry.at(0));
          break;
        }
      }
    }
    if (!found) {
      // required key is not in keyboard group
      return false;
    }
  }

  // add key
  groupTable[group].push_back(items);
  return true;
}

void KeyMap::allowGroupSwitchDuringCompose()
{
  m_composeAcrossGroups = true;
}

void KeyMap::addHalfDuplexButton(KeyButton button)
{
  m_halfDuplex.insert(button);
}

void KeyMap::clearHalfDuplexModifiers()
{
  m_halfDuplexMods.clear();
}

void KeyMap::addHalfDuplexModifier(KeyID key)
{
  m_halfDuplexMods.insert(key);
}

void KeyMap::finish()
{
  m_numGroups = findNumGroups();

  // make sure every key has the same number of groups
  for (auto i = m_keyIDMap.begin(); i != m_keyIDMap.end(); ++i) {
    i->second.resize(m_numGroups);
  }

  // compute keys that generate each modifier
  setModifierKeys();
}

void KeyMap::foreachKey(ForeachKeyCallback cb, void *userData)
{
  for (auto &[keyId, keyGroup] : m_keyIDMap) {
    for (size_t group = 0; group < keyGroup.size(); ++group) {
      KeyEntryList &entryList = keyGroup.at(group);
      for (auto &entry : entryList) {
        KeyItemList &itemList = entry;
        for (size_t k = 0; k < itemList.size(); ++k) {
          (*cb)(keyId, static_cast<int32_t>(group), itemList.at(k), userData);
        }
      }
    }
  }
}

const KeyMap::KeyItem *KeyMap::mapKey(
    Keystrokes &keys, KeyID id, int32_t group, ModifierToKeys &activeModifiers, KeyModifierMask &currentState,
    KeyModifierMask desiredMask, bool isAutoRepeat, const std::string &lang
) const
{
  LOG(
      (CLOG_VERBOSE "mapKey %04x (%d) with mask %04x, start state: %04x, group: %d", id, id, desiredMask, currentState,
       group)
  );

  // handle group change
  if (id == kKeyNextGroup) {
    keys.push_back(Keystroke(1, false, false));
    return nullptr;
  } else if (id == kKeyPrevGroup) {
    keys.push_back(Keystroke(-1, false, false));
    return nullptr;
  }

  const KeyItem *item;
  switch (id) {
  case kKeyShift_L:
  case kKeyShift_R:
  case kKeyControl_L:
  case kKeyControl_R:
  case kKeyAlt_L:
  case kKeyAlt_R:
  case kKeyMeta_L:
  case kKeyMeta_R:
  case kKeySuper_L:
  case kKeySuper_R:
  case kKeyAltGr:
  case kKeyCapsLock:
  case kKeyNumLock:
  case kKeyScrollLock:
    item = mapModifierKey(keys, id, group, activeModifiers, currentState, desiredMask, isAutoRepeat, lang);
    break;

  case kKeySetModifiers:
    if (!keysForModifierState(0, group, activeModifiers, currentState, desiredMask, desiredMask, 0, keys)) {
      LOG_VERBOSE("unable to set modifiers %04x", desiredMask);
      return nullptr;
    }
    return &m_modifierKeyItem;

  case kKeyClearModifiers:
    if (!keysForModifierState(
            0, group, activeModifiers, currentState, currentState & ~desiredMask, desiredMask, 0, keys
        )) {
      LOG_VERBOSE("unable to clear modifiers %04x", desiredMask);
      return nullptr;
    }
    return &m_modifierKeyItem;

  default:
    if (isCommand(desiredMask)) {
      item = mapCommandKey(keys, id, group, activeModifiers, currentState, desiredMask, isAutoRepeat, lang);
    } else {
      item = mapCharacterKey(keys, id, group, activeModifiers, currentState, desiredMask, isAutoRepeat, lang);
    }
    break;
  }

  if (item != nullptr) {
    LOG_VERBOSE("mapped to %03x, new state %04x", item->m_button, currentState);
  }
  return item;
}

void KeyMap::setLanguageData(std::vector<std::string> layouts)
{
  m_keyboardLayouts = std::move(layouts);
}

int32_t KeyMap::getLanguageGroupID(int32_t group, const std::string &lang) const
{
  auto id = group;

  if (auto it = std::find(m_keyboardLayouts.begin(), m_keyboardLayouts.end(), lang); it != m_keyboardLayouts.end()) {
    id = static_cast<int>(std::distance(m_keyboardLayouts.begin(), it));
    LOG_VERBOSE("language %s has group id %d", lang.c_str(), id);
  } else {
    LOG_VERBOSE("could not found requested language");
  }

  return id;
}

int32_t KeyMap::getNumGroups() const
{
  return m_numGroups;
}

int32_t KeyMap::getEffectiveGroup(int32_t group, int32_t offset) const
{
  return (group + offset + getNumGroups()) % getNumGroups();
}

const KeyMap::KeyItemList *
KeyMap::findCompatibleKey(KeyID id, int32_t group, KeyModifierMask required, KeyModifierMask sensitive) const
{
  assert(group >= 0 && group < getNumGroups());

  KeyIDMap::const_iterator i = m_keyIDMap.find(id);
  if (i == m_keyIDMap.end()) {
    return nullptr;
  }

  const KeyEntryList &entries = i->second[group];
  for (const auto &entry : entries) {
    if ((entry.back().m_sensitive & sensitive) == 0 ||
        (entry.back().m_required & sensitive) == (required & sensitive)) {
      return &entry;
    }
  }

  return nullptr;
}

bool KeyMap::isHalfDuplex(KeyID key, KeyButton button) const
{
  return (m_halfDuplex.count(button) + m_halfDuplexMods.count(key) > 0);
}

bool KeyMap::isCommand(KeyModifierMask mask) const
{
  return ((mask & getCommandModifiers()) != 0);
}

KeyModifierMask KeyMap::getCommandModifiers() const
{
  // we currently treat ctrl, alt, meta and super as command modifiers.
  // some platforms may have a more limited set (OS X only needs Alt)
  // but this works anyway.
  return KeyModifierControl | KeyModifierAlt | KeyModifierAltGr | KeyModifierMeta | KeyModifierSuper;
}

void KeyMap::initModifierKey(KeyItem &item)
{
  item.m_generates = 0;
  item.m_lock = false;
  switch (item.m_id) {
  case kKeyShift_L:
  case kKeyShift_R:
    item.m_generates = KeyModifierShift;
    break;

  case kKeyControl_L:
  case kKeyControl_R:
    item.m_generates = KeyModifierControl;
    break;

  case kKeyAlt_L:
  case kKeyAlt_R:
    item.m_generates = KeyModifierAlt;
    break;

  case kKeyMeta_L:
  case kKeyMeta_R:
    item.m_generates = KeyModifierMeta;
    break;

  case kKeySuper_L:
  case kKeySuper_R:
    item.m_generates = KeyModifierSuper;
    break;

  case kKeyAltGr:
    item.m_generates = KeyModifierAltGr;
    break;

  case kKeyCapsLock:
    item.m_generates = KeyModifierCapsLock;
    item.m_lock = true;
    break;

  case kKeyNumLock:
    item.m_generates = KeyModifierNumLock;
    item.m_lock = true;
    break;

  case kKeyScrollLock:
    item.m_generates = KeyModifierScrollLock;
    item.m_lock = true;
    break;

  default:
    // not a modifier
    break;
  }
}

int32_t KeyMap::findNumGroups() const
{
  size_t max = 0;
  for (auto i = m_keyIDMap.begin(); i != m_keyIDMap.end(); ++i) {
    if (i->second.size() > max) {
      max = i->second.size();
    }
  }
  return static_cast<int32_t>(max);
}

void KeyMap::setModifierKeys()
{
  m_modifierKeys.clear();
  m_modifierKeys.resize(kKeyModifierNumBits * getNumGroups());
  for (const auto &[keyId, keyGroup] : m_keyIDMap) {
    const KeyGroupTable &groupTable = keyGroup;
    int32_t g = -1;
    for (const auto &group : groupTable) {
      g++;
      const KeyEntryList &entries = group;
      for (const auto &entry : entries) {
        // skip multi-key sequences
        if (entry.size() != 1) {
          continue;
        }

        // skip keys that don't generate a modifier
        const KeyItem &item = entry.back();
        if (item.m_generates == 0) {
          continue;
        }

        // add key to each indicated modifier in this group
        for (int32_t b = 0; b < kKeyModifierNumBits; ++b) {
          // skip if item doesn't generate bit b
          if (((1u << b) & item.m_generates) != 0) {
            int32_t mIndex = g * kKeyModifierNumBits + b;
            m_modifierKeys[mIndex].push_back(&item);
          }
        }
      }
    }
  }
}

const KeyMap::KeyItem *KeyMap::mapCommandKey(
    Keystrokes &keys, KeyID id, int32_t group, ModifierToKeys &activeModifiers, KeyModifierMask &currentState,
    KeyModifierMask desiredMask, bool isAutoRepeat, const std::string &lang
) const
{
  static const KeyModifierMask s_overrideModifiers = 0xffffu;

  // find KeySym in table
  KeyIDMap::const_iterator i = m_keyIDMap.find(id);
  if (i == m_keyIDMap.end()) {
    // unknown key
    LOG_VERBOSE("key %04x is not on keyboard", id);
    return nullptr;
  }
  const KeyGroupTable &keyGroupTable = i->second;

  // find the first key that generates this KeyID
  const KeyItem *keyItem = nullptr;
  const auto numGroups = getNumGroups();
  for (int32_t groupOffset = 0; groupOffset < numGroups; ++groupOffset) {
    const auto effectiveGroup = getEffectiveGroup(group, groupOffset);
    const KeyEntryList &entryList = keyGroupTable[effectiveGroup];
    for (const auto &entry : entryList) {
      if (entry.size() != 1) {
        continue;
      }
      // match based on shift and make sure all required modifiers,
      // except shift, are already in the desired mask;  we're
      // after the right button not the right character.
      // we'll use desiredMask as-is, overriding the key's required
      // modifiers, when synthesizing this button.
      const auto &item = entry.back();
      KeyModifierMask desiredShiftMask = KeyModifierShift & desiredMask;
      KeyModifierMask requiredIgnoreShiftMask = item.m_required & ~KeyModifierShift;
      if ((item.m_required & desiredShiftMask) == (item.m_sensitive & desiredShiftMask) &&
          ((requiredIgnoreShiftMask & desiredMask) == requiredIgnoreShiftMask)) {
        LOG_VERBOSE("found key in group %d", effectiveGroup);
        keyItem = &item;
        break;
      }
    }
    if (keyItem) {
      break;
    }
  }
  if (!keyItem) {
    // no mapping for this keysym
    LOG_VERBOSE("no mapping for key %04x", id);
    return nullptr;
  }

  // make working copy of modifiers
  ModifierToKeys newModifiers = activeModifiers;
  KeyModifierMask newState = currentState;
  auto newGroup = group;

  // don't try to change CapsLock
  desiredMask = (desiredMask & ~KeyModifierCapsLock) | (currentState & KeyModifierCapsLock);

  // add the key
  if (!keysForKeyItem(
          *keyItem, newGroup, newModifiers, newState, desiredMask, s_overrideModifiers, isAutoRepeat, keys, lang
      )) {
    LOG_VERBOSE("can't map key");
    keys.clear();
    return nullptr;
  }

  // add keystrokes to restore modifier keys
  if (!keysToRestoreModifiers(*keyItem, group, newModifiers, newState, activeModifiers, keys)) {
    LOG_VERBOSE("modifiers were not restored");
    keys.clear();
    return nullptr;
  }

  // save new modifiers
  activeModifiers = newModifiers;
  currentState = newState;

  return keyItem;
}

const KeyMap::KeyItemList *
KeyMap::getKeyItemList(const KeyMap::KeyGroupTable &keyGroupTable, int32_t group, KeyModifierMask desiredMask) const
{
  const KeyItemList *itemList = nullptr;

  // find best key in any group, starting with the active group
  for (int32_t groupOffset = 0; groupOffset < getNumGroups(); ++groupOffset) {
    const auto effectiveGroup = getEffectiveGroup(group, groupOffset);
    auto keyIndex = input::detail::findBestKeyIndex(keyGroupTable[effectiveGroup], desiredMask);
    if (keyIndex != -1) {
      LOG_VERBOSE("found key in group %d", effectiveGroup);
      itemList = &keyGroupTable[effectiveGroup][keyIndex];
      break;
    }
  }

  return itemList;
}

const KeyMap::KeyItem *KeyMap::mapCharacterKey(
    Keystrokes &keys, KeyID id, int32_t group, ModifierToKeys &activeModifiers, KeyModifierMask &currentState,
    KeyModifierMask desiredMask, bool isAutoRepeat, const std::string &lang
) const
{
  // find KeySym in table
  KeyIDMap::const_iterator i = m_keyIDMap.find(id);
  if (i == m_keyIDMap.end()) {
    // unknown key
    LOG_VERBOSE("key %04x is not on keyboard", id);

    return nullptr;
  }

  // get keys to press for key
  const auto itemList = getKeyItemList(i->second, getLanguageGroupID(group, lang), desiredMask);
  if (!itemList || itemList->empty()) {
    // no mapping for this keysym
    LOG_VERBOSE("no mapping for key %04x", id);
    return nullptr;
  }

  const KeyItem &keyItem = itemList->back();

  // make working copy of modifiers
  ModifierToKeys newModifiers = activeModifiers;
  KeyModifierMask newState = currentState;
  int32_t newGroup = group;

  // add each key
  for (auto &item : *itemList) {
    if (!keysForKeyItem(item, newGroup, newModifiers, newState, desiredMask, 0, isAutoRepeat, keys, lang)) {
      LOG_VERBOSE("can't map key");
      keys.clear();
      return nullptr;
    }
  }

  // add keystrokes to restore modifier keys
  if (!keysToRestoreModifiers(keyItem, group, newModifiers, newState, activeModifiers, keys)) {
    LOG_VERBOSE("modifiers were not restored");
    keys.clear();
    return nullptr;
  }

  // save new modifiers
  activeModifiers = newModifiers;
  currentState = newState;

  return &keyItem;
}

} // namespace synergy
