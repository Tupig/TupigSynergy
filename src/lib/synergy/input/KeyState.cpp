/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "KeyState.h"
#include "base/Log.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <list>

static const KeyButton kButtonMask = (KeyButton)(IKeyState::s_numButtons - 1);

//
// KeyState
//

KeyState::KeyState(IEventQueue *events, std::vector<std::string> layouts, bool isLangSyncEnabled)
    : IKeyState(events),
      m_keyMapPtr(new synergy::KeyMap()),
      m_keyMap(*m_keyMapPtr),
      m_mask(0),
      m_events(events),
      m_isLangSyncEnabled(isLangSyncEnabled)
{
  m_keyMap.setLanguageData(std::move(layouts));
  init();
}

KeyState::KeyState(
    IEventQueue *events, synergy::KeyMap &keyMap, std::vector<std::string> layouts, bool isLangSyncEnabled
)
    : IKeyState(events),
      m_keyMapPtr(nullptr),
      m_keyMap(keyMap),
      m_mask(0),
      m_events(events),
      m_isLangSyncEnabled(isLangSyncEnabled)
{
  m_keyMap.setLanguageData(std::move(layouts));
  init();
}

KeyState::~KeyState()
{
  if (m_keyMapPtr)
    delete m_keyMapPtr;
}

void KeyState::init()
{
  memset(&m_keys, 0, sizeof(m_keys));
  memset(&m_syntheticKeys, 0, sizeof(m_syntheticKeys));
  memset(&m_keyClientData, 0, sizeof(m_keyClientData));
  memset(&m_serverKeys, 0, sizeof(m_serverKeys));
}

void KeyState::onKey(KeyButton button, bool down, KeyModifierMask newState)
{
  // update modifier state
  m_mask = newState;
  LOG_VERBOSE("new mask: 0x%04x", m_mask);

  // ignore bogus buttons
  button &= kButtonMask;
  if (button == 0) {
    return;
  }

  // update key state
  if (down) {
    m_keys[button] = 1;
    m_syntheticKeys[button] = 1;
  } else {
    m_keys[button] = 0;
    m_syntheticKeys[button] = 0;
  }
}

void KeyState::sendKeyEvent(
    void *target, bool press, bool isAutoRepeat, KeyID key, KeyModifierMask mask, int32_t count, KeyButton button
)
{
  using enum EventTypes;
  if (m_keyMap.isHalfDuplex(key, button)) {
    if (isAutoRepeat) {
      // ignore auto-repeat on half-duplex keys
    } else {
      m_events->addEvent(Event(KeyStateKeyDown, target, KeyInfo::alloc(key, mask, button, 1)));
      m_events->addEvent(Event(KeyStateKeyUp, target, KeyInfo::alloc(key, mask, button, 1)));
    }
  } else {
    if (isAutoRepeat) {
      m_events->addEvent(Event(KeyStateKeyRepeat, target, KeyInfo::alloc(key, mask, button, count)));
    } else if (press) {
      m_events->addEvent(Event(KeyStateKeyDown, target, KeyInfo::alloc(key, mask, button, 1)));
    } else {
      m_events->addEvent(Event(KeyStateKeyUp, target, KeyInfo::alloc(key, mask, button, 1)));
    }
  }
}

void KeyState::updateKeyMap(synergy::KeyMap *existing)
{
  if (existing) {
    m_keyMap.swap(*existing);
  } else {
    // get the current keyboard map
    synergy::KeyMap keyMap;
    getKeyMap(keyMap);
    m_keyMap.swap(keyMap);
    m_keyMap.finish();
  }

  // add special keys
  addCombinationEntries();
  addKeypadEntries();
  addAliasEntries();
}

void KeyState::updateKeyState()
{
  // reset our state
  memset(&m_keys, 0, sizeof(m_keys));
  memset(&m_syntheticKeys, 0, sizeof(m_syntheticKeys));
  memset(&m_keyClientData, 0, sizeof(m_keyClientData));
  memset(&m_serverKeys, 0, sizeof(m_serverKeys));
  m_activeModifiers.clear();

  // get the current keyboard state
  KeyButtonSet keysDown;
  pollPressedKeys(keysDown);
  for (const auto &key : keysDown) {
    m_keys[key] = 1;
  }

  // get the current modifier state
  clearStaleModifiers();
  m_mask = pollActiveModifiers();

  // set active modifiers
  AddActiveModifierContext addModifierContext(pollActiveGroup(), m_mask, m_activeModifiers);
  m_keyMap.foreachKey(&KeyState::addActiveModifierCB, &addModifierContext);

  LOG_VERBOSE("modifiers on update: 0x%04x", m_mask);
}

void KeyState::addActiveModifierCB(KeyID, int32_t group, synergy::KeyMap::KeyItem &keyItem, void *vcontext)
{
  auto *context = static_cast<AddActiveModifierContext *>(vcontext);
  if (group == context->m_activeGroup && (keyItem.m_generates & context->m_mask) != 0) {
    context->m_activeModifiers.insert(std::make_pair(keyItem.m_generates, keyItem));
  }
}

void KeyState::setHalfDuplexMask(KeyModifierMask mask)
{
  m_keyMap.clearHalfDuplexModifiers();
  if ((mask & KeyModifierCapsLock) != 0) {
    m_keyMap.addHalfDuplexModifier(kKeyCapsLock);
  }
  if ((mask & KeyModifierNumLock) != 0) {
    m_keyMap.addHalfDuplexModifier(kKeyNumLock);
  }
  if ((mask & KeyModifierScrollLock) != 0) {
    m_keyMap.addHalfDuplexModifier(kKeyScrollLock);
  }
}

void KeyState::fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton serverID, const std::string &lang)
{
  // if this server key is already down then this is probably a
  // mis-reported autorepeat.
  serverID &= kButtonMask;
  if (m_serverKeys[serverID] != 0) {
    fakeKeyRepeat(id, mask, 1, serverID, lang);
    return;
  }

  // ignore certain keys
  if (isIgnoredKey(id, mask)) {
    LOG_VERBOSE("ignored key %04x %04x", id, mask);
    return;
  }

  Keystrokes keys;
  ModifierToKeys oldActiveModifiers = m_activeModifiers;
  const synergy::KeyMap::KeyItem *keyItem =
      m_keyMap.mapKey(keys, id, pollActiveGroup(), m_activeModifiers, getActiveModifiersRValue(), mask, false, lang);

  if (keyItem == nullptr) {
    // a media key won't be mapped on mac, so we need to fake it in a
    // special way
    if (id == kKeyAudioDown || id == kKeyAudioUp || id == kKeyAudioMute || id == kKeyAudioPlay || id == kKeyAudioPrev ||
        id == kKeyAudioNext || id == kKeyBrightnessDown || id == kKeyBrightnessUp) {
      LOG_VERBOSE("emulating media key");
      fakeMediaKey(id);
    }

    return;
  }

  auto localID = (KeyButton)(keyItem->m_button & kButtonMask);
  updateModifierKeyState(localID, oldActiveModifiers, m_activeModifiers);
  if (localID != 0) {
    // note keys down
    ++m_keys[localID];
    ++m_syntheticKeys[localID];
    m_keyClientData[localID] = keyItem->m_client;
    m_serverKeys[serverID] = localID;
  }

  // generate key events
  fakeKeys(keys, 1);
}

bool KeyState::fakeKeyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton serverID, const std::string &lang)
{
  LOG_VERBOSE("fakeKeyRepeat");
  serverID &= kButtonMask;

  // if we haven't seen this button go down then ignore it
  KeyButton oldLocalID = m_serverKeys[serverID];
  if (oldLocalID == 0) {
    return false;
  }

  // get keys for key repeat
  Keystrokes keys;
  ModifierToKeys oldActiveModifiers = m_activeModifiers;
  const synergy::KeyMap::KeyItem *keyItem =
      m_keyMap.mapKey(keys, id, pollActiveGroup(), m_activeModifiers, getActiveModifiersRValue(), mask, true, lang);
  if (keyItem == nullptr) {
    return false;
  }
  auto localID = (KeyButton)(keyItem->m_button & kButtonMask);
  if (localID == 0) {
    return false;
  }

  // if the KeyButton for the auto-repeat is not the same as for the
  // initial press then mark the initial key as released and the new
  // key as pressed.  this can happen when we auto-repeat after a
  // dead key.  for example, a dead accent followed by 'a' will
  // generate an 'a with accent' followed by a repeating 'a'.  the
  // KeyButtons for the two KeyIDs might be different.
  if (localID != oldLocalID) {
    // replace key up with previous KeyButton but leave key down
    // alone so it uses the new KeyButton.
    for (auto &key : keys) {
      if (key.m_type == Keystroke::KeyType::Button && key.m_data.m_button.m_button == localID) {
        key.m_data.m_button.m_button = oldLocalID;
        break;
      }
    }

    // note that old key is now up
    --m_keys[oldLocalID];
    --m_syntheticKeys[oldLocalID];

    // note keys down
    updateModifierKeyState(localID, oldActiveModifiers, m_activeModifiers);
    ++m_keys[localID];
    ++m_syntheticKeys[localID];
    m_keyClientData[localID] = keyItem->m_client;
    m_serverKeys[serverID] = localID;
  }

  // generate key events
  fakeKeys(keys, count);
  return true;
}

bool KeyState::fakeKeyUp(KeyButton serverID)
{
  // if we haven't seen this button go down then ignore it
  KeyButton localID = m_serverKeys[serverID & kButtonMask];
  if (localID == 0) {
    return false;
  }

  // get the sequence of keys to simulate key release
  Keystrokes keys;
  keys.push_back(Keystroke(localID, false, false, m_keyClientData[localID]));

  // note keys down
  --m_keys[localID];
  --m_syntheticKeys[localID];
  m_serverKeys[serverID] = 0;

  // check if this is a modifier
  auto i = m_activeModifiers.begin();
  while (i != m_activeModifiers.end()) {
    if (i->second.m_button == localID && !i->second.m_lock) {
      // modifier is no longer down
      KeyModifierMask mask = i->first;

      ModifierToKeys::iterator tmp = i;
      ++i;
      m_activeModifiers.erase(tmp);

      if (!m_activeModifiers.contains(mask)) {
        // no key for modifier is down so deactivate modifier
        m_mask &= ~mask;
        LOG_VERBOSE("new state %04x", m_mask);
      }
    } else {
      ++i;
    }
  }

  // generate key events
  fakeKeys(keys, 1);
  return true;
}

void KeyState::fakeAllKeysUp()
{
  Keystrokes keys;
  for (KeyButton i = 0; i < IKeyState::s_numButtons; ++i) {
    if (m_syntheticKeys[i] > 0) {
      keys.push_back(Keystroke(i, false, false, m_keyClientData[i]));
      m_keys[i] = 0;
      m_syntheticKeys[i] = 0;
    }
  }
  fakeKeys(keys, 1);
  memset(&m_serverKeys, 0, sizeof(m_serverKeys));
  m_activeModifiers.clear();
  m_mask = pollActiveModifiers();
}

bool KeyState::fakeMediaKey(KeyID)
{
  return false;
}

bool KeyState::isKeyDown(KeyButton button) const
{
  return (m_keys[button & kButtonMask] > 0);
}

KeyModifierMask KeyState::getActiveModifiers() const
{
  return m_mask;
}

KeyModifierMask &KeyState::getActiveModifiersRValue()
{
  return m_mask;
}

int32_t KeyState::getEffectiveGroup(int32_t group, int32_t offset) const
{
  return m_keyMap.getEffectiveGroup(group, offset);
}

bool KeyState::isIgnoredKey(KeyID key, KeyModifierMask) const
{
  switch (key) {
  case kKeyCapsLock:
  case kKeyNumLock:
  case kKeyScrollLock:
    return true;

  default:
    return false;
  }
}

KeyButton KeyState::getButton(KeyID id, int32_t group) const
{
  const synergy::KeyMap::KeyItemList *items = m_keyMap.findCompatibleKey(id, group, 0, 0);
  if (items == nullptr) {
    return 0;
  } else {
    return items->back().m_button;
  }
}

void KeyState::fakeKeys(const Keystrokes &keys, uint32_t count)
{
  // do nothing if no keys or no repeats
  if (count == 0 || keys.empty()) {
    return;
  }

  // generate key events
  for (auto k = keys.begin(); k != keys.end();) {
    if (k->m_type == Keystroke::KeyType::Button && k->m_data.m_button.m_repeat) {
      // repeat from here up to but not including the next key
      // with m_repeat == false count times.
      Keystrokes::const_iterator start = k;
      while (count-- > 0) {
        // send repeating events
        for (k = start; k != keys.end() && k->m_type == Keystroke::KeyType::Button && k->m_data.m_button.m_repeat;
             ++k) {
          fakeKey(*k);
        }
      }

      // note -- k is now on the first non-repeat key after the
      // repeat keys, exactly where we'd like to continue from.
    } else if (k->m_type != Keystroke::KeyType::Group || (!k->m_data.m_group.m_restore && m_isLangSyncEnabled)) {
      // send event
      fakeKey(*k);

      // next key
      ++k;
    } else {
      LOG_VERBOSE("skipping keystroke, language sync is disabled");
      ++k;
    }
  }
}

void KeyState::updateModifierKeyState(
    KeyButton button, const ModifierToKeys &oldModifiers, const ModifierToKeys &newModifiers
)
{
  // get the pressed modifier buttons before and after
  synergy::KeyMap::ButtonToKeyMap oldKeys;
  synergy::KeyMap::ButtonToKeyMap newKeys;
  for (const auto &[modifier, keyItem] : oldModifiers) {
    oldKeys.insert(std::make_pair(keyItem.m_button, &keyItem));
  }
  for (const auto &[modifier, keyItem] : newModifiers) {
    newKeys.insert(std::make_pair(keyItem.m_button, &keyItem));
  }

  // get the modifier buttons that were pressed or released
  synergy::KeyMap::ButtonToKeyMap pressed;
  synergy::KeyMap::ButtonToKeyMap released;
  std::ranges::set_difference(oldKeys, newKeys, std::inserter(released, released.end()), ButtonToKeyLess());
  std::ranges::set_difference(newKeys, oldKeys, std::inserter(pressed, pressed.end()), ButtonToKeyLess());

  // update state
  for (synergy::KeyMap::ButtonToKeyMap::const_iterator i = released.begin(); i != released.end(); ++i) {
    if (i->first != button) {
      m_keys[i->first] = 0;
      m_syntheticKeys[i->first] = 0;
    }
  }
  for (synergy::KeyMap::ButtonToKeyMap::const_iterator i = pressed.begin(); i != pressed.end(); ++i) {
    if (i->first != button) {
      m_keys[i->first] = 1;
      m_syntheticKeys[i->first] = 1;
      m_keyClientData[i->first] = i->second->m_client;
    }
  }
}

//
// KeyState::AddActiveModifierContext
//

KeyState::AddActiveModifierContext::AddActiveModifierContext(
    int32_t group, KeyModifierMask mask, ModifierToKeys &activeModifiers
)
    : m_activeGroup(group),
      m_mask(mask),
      m_activeModifiers(activeModifiers)
{
  // do nothing
}
