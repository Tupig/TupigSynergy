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

void KeyMap::collectButtons(const ModifierToKeys &mods, ButtonToKeyMap &keys)
{
  keys.clear();
  for (const auto &[modifierMask, keyItem] : mods) {
    keys.insert(std::make_pair(keyItem.m_button, &keyItem));
  }
}

void KeyMap::addGroupToKeystroke(Keystrokes &keys, int32_t &group, const std::string &lang) const
{
  group = getLanguageGroupID(group, lang);
  keys.push_back(Keystroke(group, true, false));
}

const KeyMap::KeyItem *KeyMap::mapModifierKey(
    Keystrokes &keys, KeyID id, int32_t group, ModifierToKeys &activeModifiers, KeyModifierMask &currentState,
    KeyModifierMask desiredMask, bool isAutoRepeat, const std::string &lang
) const
{
  return mapCharacterKey(keys, id, group, activeModifiers, currentState, desiredMask, isAutoRepeat, lang);
}

namespace input::detail {

int32_t findBestKeyIndex(const std::vector<KeyMap::KeyItemList> &entryList, KeyModifierMask desiredState)
{
  // check for an item that can accommodate the desiredState exactly
  for (int32_t i = 0; i < (int32_t)entryList.size(); ++i) {
    const KeyMap::KeyItem &item = entryList[i].back();
    if ((item.m_required & desiredState) == item.m_required &&
        (item.m_required & desiredState) == (item.m_sensitive & desiredState)) {
      LOG_VERBOSE("best key index %d of %d (exact)", i + 1, entryList.size());
      return i;
    }
  }

  // choose the item that requires the fewest modifier changes
  int32_t bestCount = 32;
  int32_t bestIndex = -1;
  for (int32_t i = 0; i < (int32_t)entryList.size(); ++i) {
    const KeyMap::KeyItem &item = entryList[i].back();
    KeyModifierMask change = ((item.m_required ^ desiredState) & item.m_sensitive);
    int32_t n = 0;
    for (KeyModifierMask bits = change; bits != 0; bits >>= 1) {
      if ((bits & 1) != 0) {
        ++n;
      }
    }
    if (n < bestCount) {
      bestCount = n;
      bestIndex = i;
    }
  }
  if (bestIndex != -1) {
    LOG_VERBOSE("best key index %d of %d (%d modifiers)", bestIndex + 1, entryList.size(), bestCount);
  }

  return bestIndex;
}

} // namespace input::detail

const KeyMap::KeyItem *KeyMap::keyForModifier(KeyButton button, int32_t group, int32_t modifierBit) const
{
  assert(modifierBit >= 0 && modifierBit < kKeyModifierNumBits);
  assert(group >= 0 && group < getNumGroups());

  // find a key that generates the given modifier in the given group
  // but doesn't use the given button, presumably because we're trying
  // to generate a KeyID that's only bound the the given button.
  // this is important when a shift button is modified by shift;  we
  // must use the other shift button to do the shifting.
  const ModifierKeyItemList &items = m_modifierKeys[group * kKeyModifierNumBits + modifierBit];
  for (const auto &item : items) {
    if (item->m_button != button) {
      return item;
    }
  }
  return nullptr;
}

bool KeyMap::keysForKeyItem(
    const KeyItem &keyItem, int32_t &group, ModifierToKeys &activeModifiers, KeyModifierMask &currentState,
    KeyModifierMask desiredState, KeyModifierMask overrideModifiers, bool isAutoRepeat, Keystrokes &keystrokes,
    const std::string &lang
) const
{
  static const KeyModifierMask s_notRequiredMask = KeyModifierAltGr | KeyModifierNumLock | KeyModifierScrollLock;

  // add keystrokes to adjust the group
  if (group != keyItem.m_group) {
    group = keyItem.m_group;
    addGroupToKeystroke(keystrokes, group, lang);
  }

  EKeystroke type;
  if (keyItem.m_dead) {
    // adjust modifiers for dead key
    if (!keysForModifierState(
            keyItem.m_button, group, activeModifiers, currentState, keyItem.m_required, keyItem.m_sensitive, 0,
            keystrokes
        )) {
      LOG_VERBOSE("unable to match modifier state for dead key %d", keyItem.m_button);
      return false;
    }

    // press and release the dead key
    type = kKeystrokeClick;
  } else {
    // if this a command key then we don't have to match some of the
    // key's required modifiers.
    KeyModifierMask sensitive = keyItem.m_sensitive & ~overrideModifiers;

    // XXX -- must handle pressing a modifier.  in particular, if we want
    // to synthesize a KeyID on level 1 of a KeyButton that has Shift_L
    // mapped to level 0 then we must release that button if it's down
    // (in order to satisfy a shift modifier) then press a different
    // button (any other button) mapped to the shift modifier and then
    // the Shift_L button.
    // match key's required state
    LOG_VERBOSE("state: %04x,%04x,%04x", currentState, keyItem.m_required, sensitive);
    if (!keysForModifierState(
            keyItem.m_button, group, activeModifiers, currentState, keyItem.m_required, sensitive, 0, keystrokes
        )) {
      LOG(
          (CLOG_VERBOSE "unable to match modifier state (%04x,%04x) for key %d", keyItem.m_required,
           keyItem.m_sensitive, keyItem.m_button)
      );
      return false;
    }

    // match desiredState as closely as possible.  we must not
    // change any modifiers in keyItem.m_sensitive.  and if the key
    // is a modifier, we don't want to change that modifier.
    LOG(
        (CLOG_VERBOSE "desired state: %04x %04x,%04x,%04x", desiredState, currentState, keyItem.m_required,
         keyItem.m_sensitive)
    );
    if (!keysForModifierState(
            keyItem.m_button, group, activeModifiers, currentState, desiredState, ~(sensitive | keyItem.m_generates),
            s_notRequiredMask, keystrokes
        )) {
      LOG(
          (CLOG_VERBOSE "unable to match desired modifier state (%04x,%04x) for key %d", desiredState,
           ~keyItem.m_sensitive & 0xffffu, keyItem.m_button)
      );
      return false;
    }

    // repeat or press of key
    type = isAutoRepeat ? kKeystrokeRepeat : kKeystrokePress;
  }
  addKeystrokes(type, keyItem, activeModifiers, currentState, keystrokes);

  return true;
}

bool KeyMap::keysToRestoreModifiers(
    const KeyItem &keyItem, int32_t, ModifierToKeys &activeModifiers, KeyModifierMask &currentState,
    const ModifierToKeys &desiredModifiers, Keystrokes &keystrokes
) const
{
  // XXX -- we're not considering modified modifiers here

  ModifierToKeys oldModifiers = activeModifiers;

  // get the pressed modifier buttons before and after
  ButtonToKeyMap oldKeys;
  ButtonToKeyMap newKeys;
  collectButtons(oldModifiers, oldKeys);
  collectButtons(desiredModifiers, newKeys);

  // release unwanted keys
  for (const auto &[_mask, _keyItem] : oldModifiers) {
    KeyButton button = _keyItem.m_button;
    if (button != keyItem.m_button && !newKeys.contains(button)) {
      EKeystroke type = kKeystrokeRelease;
      if (_keyItem.m_lock) {
        type = kKeystrokeUnmodify;
      }
      addKeystrokes(type, _keyItem, activeModifiers, currentState, keystrokes);
    }
  }

  // press wanted keys
  for (const auto &[_mask, _keyItem] : desiredModifiers) {
    const KeyButton button = _keyItem.m_button;
    if (button != keyItem.m_button && !oldKeys.contains(button)) {
      EKeystroke type = kKeystrokePress;
      if (_keyItem.m_lock) {
        type = kKeystrokeModify;
      }
      addKeystrokes(type, _keyItem, activeModifiers, currentState, keystrokes);
    }
  }
  return true;
}

bool KeyMap::keysForModifierState(
    KeyButton button, int32_t group, ModifierToKeys &activeModifiers, KeyModifierMask &currentState,
    KeyModifierMask requiredState, KeyModifierMask sensitiveMask, KeyModifierMask notRequiredMask,
    Keystrokes &keystrokes
) const
{
  // compute which modifiers need changing
  KeyModifierMask flipMask = ((currentState ^ requiredState) & sensitiveMask);
  // if a modifier is not required then don't even try to match it.  if
  // we don't mask out notRequiredMask then we'll try to match those
  // modifiers but succeed if we can't.  however, this is known not
  // to work if the key itself is a modifier (the numlock toggle can
  // interfere) so we don't try to match at all.
  flipMask &= ~notRequiredMask;
  LOG(
      (CLOG_VERBOSE "flip: %04x (%04x vs %04x in %04x - %04x)", flipMask, currentState, requiredState,
       sensitiveMask & 0xffffu, notRequiredMask & 0xffffu)
  );
  if (flipMask == 0) {
    return true;
  }

  // fix modifiers.  this is complicated by the fact that a modifier may
  // be sensitive to other modifiers!  (who thought that up?)
  //
  // we'll assume that modifiers with higher bits are affected by modifiers
  // with lower bits.  there's not much basis for that assumption except
  // that we're pretty sure shift isn't changed by other modifiers.
  int32_t bit = kKeyModifierNumBits;
  while (bit-- > 0) {
    KeyModifierMask mask = (1u << bit);
    if ((flipMask & mask) == 0) {
      // modifier is already correct
      continue;
    }

    // do we want the modifier active or inactive?
    bool active = ((requiredState & mask) != 0);

    // get the KeyItem for the modifier in the group
    const KeyItem *keyItem = keyForModifier(button, group, bit);
    if (keyItem == nullptr) {
      if ((mask & notRequiredMask) == 0) {
        LOG_VERBOSE("no key for modifier %04x", mask);
        return false;
      } else {
        continue;
      }
    }

    // if this modifier is sensitive to modifiers then adjust those
    // modifiers.  also check if our assumption was correct.  note
    // that we only need to adjust the modifiers on key down.
    KeyModifierMask sensitive = keyItem->m_sensitive;
    if ((sensitive & mask) != 0) {
      // modifier is sensitive to itself.  that makes no sense
      // so ignore it.
      LOG_VERBOSE("modifier %04x modified by itself", mask);
      sensitive &= ~mask;
    }
    if (sensitive != 0) {
      if (sensitive > mask) {
        // our assumption is incorrect
        LOG_VERBOSE("modifier %04x modified by %04x", mask, sensitive);
        return false;
      }
      if (active &&
          !keysForModifierState(
              button, group, activeModifiers, currentState, keyItem->m_required, sensitive, notRequiredMask, keystrokes
          )) {
        return false;
      } else if (!active) {
        // release the modifier
        // XXX -- this doesn't work!  if Alt and Meta are mapped
        // to one key and we want to release Meta we can't do
        // that without also releasing Alt.
        // need to think about support for modified modifiers.
      }
    }

    // current state should match required state
    if ((currentState & sensitive) != (keyItem->m_required & sensitive)) {
      LOG(
          (CLOG_VERBOSE "unable to match modifier state for modifier %04x (%04x "
                        "vs %04x in %04x)",
           mask, currentState, keyItem->m_required, sensitive)
      );
      return false;
    }

    // add keystrokes
    EKeystroke type = active ? kKeystrokeModify : kKeystrokeUnmodify;
    addKeystrokes(type, *keyItem, activeModifiers, currentState, keystrokes);
  }

  return true;
}

void KeyMap::addKeystrokes(
    EKeystroke type, const KeyItem &keyItem, ModifierToKeys &activeModifiers, KeyModifierMask &currentState,
    Keystrokes &keystrokes
) const
{
  const auto button = keyItem.m_button;
  const auto data = keyItem.m_client;
  switch (type) {
  case kKeystrokePress:
    keystrokes.push_back(Keystroke(button, true, false, data));
    if (keyItem.m_generates != 0) {
      if (!keyItem.m_lock || (currentState & keyItem.m_generates) == 0) {
        // add modifier key and activate modifier
        activeModifiers.insert(std::make_pair(keyItem.m_generates, keyItem));
        currentState |= keyItem.m_generates;
      } else {
        // deactivate locking modifier
        activeModifiers.erase(keyItem.m_generates);
        currentState &= ~keyItem.m_generates;
      }
    }
    break;

  case kKeystrokeRelease:
    keystrokes.push_back(Keystroke(button, false, false, data));
    if (keyItem.m_generates != 0 && !keyItem.m_lock) {
      // remove key from active modifiers
      std::pair<ModifierToKeys::iterator, ModifierToKeys::iterator> range =
          activeModifiers.equal_range(keyItem.m_generates);
      for (ModifierToKeys::iterator i = range.first; i != range.second; ++i) {
        if (i->second.m_button == button) {
          activeModifiers.erase(i);
          break;
        }
      }

      // if no more keys for this modifier then deactivate modifier
      if (!activeModifiers.contains(keyItem.m_generates)) {
        currentState &= ~keyItem.m_generates;
      }
    }
    break;

  case kKeystrokeRepeat:
    if (synergy::platform::isWindows()) {
      keystrokes.push_back(Keystroke(button, false, true, data));
      keystrokes.push_back(Keystroke(button, true, true, data));
    } else if (synergy::platform::isMac()) {
      keystrokes.push_back(Keystroke(button, true, true, data));
    }
    break;

  case kKeystrokeClick:
    keystrokes.push_back(Keystroke(button, true, false, data));
    keystrokes.push_back(Keystroke(button, false, false, data));
    // no modifier changes on key click
    break;

  case kKeystrokeModify:
  case kKeystrokeUnmodify:
    if (keyItem.m_lock) {
      // we assume there's just one button for this modifier
      if (m_halfDuplex.contains(button)) {
        if (type == kKeystrokeModify) {
          // turn half-duplex toggle on (press)
          keystrokes.push_back(Keystroke(button, true, false, data));
        } else {
          // turn half-duplex toggle off (release)
          keystrokes.push_back(Keystroke(button, false, false, data));
        }
      } else {
        // toggle (click)
        keystrokes.push_back(Keystroke(button, true, false, data));
        keystrokes.push_back(Keystroke(button, false, false, data));
      }
    } else if (type == kKeystrokeModify) {
      // press modifier
      keystrokes.push_back(Keystroke(button, true, false, data));
    } else {
      // release all the keys that generate the modifier that are
      // currently down
      std::pair<ModifierToKeys::const_iterator, ModifierToKeys::const_iterator> range =
          activeModifiers.equal_range(keyItem.m_generates);
      for (ModifierToKeys::const_iterator i = range.first; i != range.second; ++i) {
        keystrokes.push_back(Keystroke(i->second.m_button, false, false, i->second.m_client));
      }
    }

    if (type == kKeystrokeModify) {
      activeModifiers.insert(std::make_pair(keyItem.m_generates, keyItem));
      currentState |= keyItem.m_generates;
    } else {
      activeModifiers.erase(keyItem.m_generates);
      currentState &= ~keyItem.m_generates;
    }
    break;
  }
}

KeyMap::Keystroke::Keystroke(KeyButton button, bool press, bool repeat, uint32_t data) : m_type(KeyType::Button)
{
  m_data.m_button.m_button = button;
  m_data.m_button.m_press = press;
  m_data.m_button.m_repeat = repeat;
  m_data.m_button.m_client = data;
}

KeyMap::Keystroke::Keystroke(int32_t group, bool absolute, bool restore) : m_type(KeyType::Group)
{
  m_data.m_group.m_group = group;
  m_data.m_group.m_absolute = absolute;
  m_data.m_group.m_restore = restore;
}

} // namespace synergy
