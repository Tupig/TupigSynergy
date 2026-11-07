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

bool KeyMap::isDeadKey(KeyID key)
{
  return (key == kKeyCompose || (key >= 0x0300 && key <= 0x036f));
}

KeyID KeyMap::getDeadKey(KeyID key)
{
  if (isDeadKey(key)) {
    // already dead
    return key;
  }

  switch (key) {
  case '`':
    return kKeyDeadGrave;

  case '\'':
  case 0xb4u:
    return kKeyDeadAcute;

  case '^':
  case 0x2c6:
    return kKeyDeadCircumflex;

  case '~':
  case 0x2dcu:
    return kKeyDeadTilde;

  case 0xafu:
    return kKeyDeadMacron;

  case 0x2d8u:
    return kKeyDeadBreve;

  case 0x2d9u:
    return kKeyDeadAbovedot;

  case 0xa8u:
    return kKeyDeadDiaeresis;

  case 0xb0u:
  case 0x2dau:
    return kKeyDeadAbovering;

  case '\"':
  case 0x2ddu:
    return kKeyDeadDoubleacute;

  case 0x2c7u:
    return kKeyDeadCaron;

  case 0xb8u:
    return kKeyDeadCedilla;

  case 0x2dbu:
    return kKeyDeadOgonek;

  default:
    // unknown
    return kKeyNone;
  }
}

std::string KeyMap::formatKey(KeyID key, KeyModifierMask mask)
{

  std::string x;
  for (int32_t i = 0; i < kKeyModifierNumBits; ++i) {
    KeyModifierMask mod = (1u << i);
    if ((mask & mod) != 0 && modifierToNameMap().contains(mod)) {
      x += modifierToNameMap().find(mod)->second;
      x += "+";
    }
  }
  if (key != kKeyNone) {
    if (keyToNameMap().contains(key)) {
      x += keyToNameMap().find(key)->second;
    }
    // XXX -- we're assuming ASCII here
    else if (key >= 33 && key < 127) {
      x += (char)key;
    } else {
      x += synergy::string::sprintf("\\u%04x", key);
    }
  } else if (!x.empty()) {
    // remove trailing '+'
    x.erase(x.size() - 1);
  }
  return x;
}

bool KeyMap::parseKey(const std::string &x, KeyID &key)
{

  // parse the key
  key = kKeyNone;
  if (nameToKeyMap().contains(x)) {
    key = nameToKeyMap().find(x)->second;
  }
  // XXX -- we're assuming ASCII encoding here
  else if (x.size() == 1) {
    if (!isgraph(x[0])) {
      // unknown key
      return false;
    }
    key = (KeyID)x[0];
  } else if (x.size() == 6 && x[0] == '\\' && x[1] == 'u') {
    // escaped unicode (\uXXXX where XXXX is a hex number)
    char *end;
    key = (KeyID)strtol(x.c_str() + 2, &end, 16);
    if (*end != '\0') {
      return false;
    }
  } else if (!x.empty()) {
    // unknown key
    return false;
  }

  return true;
}

bool KeyMap::parseModifiers(std::string &x, KeyModifierMask &mask)
{

  mask = 0;
  std::string::size_type tb = x.find_first_not_of(" \t", 0);
  while (tb != std::string::npos) {
    // get next component
    std::string::size_type te = x.find_first_of(" \t+)", tb);
    if (te == std::string::npos) {
      te = x.size();
    }
    std::string c = x.substr(tb, te - tb);
    if (c.empty()) {
      // allow '+' to be parsed as the key after modifiers (e.g. Control+Shift++)
      if (x[tb] == '+') {
        x.erase(0, tb);
        return true;
      }

      // missing component
      return false;
    }

    if (nameToModifierMap().contains(c)) {
      KeyModifierMask mod = nameToModifierMap().find(c)->second;
      if ((mask & mod) != 0) {
        // modifier appears twice
        return false;
      }
      mask |= mod;
    } else {
      // unknown string
      x.erase(0, tb);
      std::string::size_type trimmedStart = x.find_first_not_of(" \t");
      std::string::size_type trimmedEnd = x.find_last_not_of(" \t");
      if (trimmedStart == std::string::npos) {
        x = "";
      } else {
        x = x.substr(trimmedStart, trimmedEnd - trimmedStart + 1);
      }
      return true;
    }

    // check for '+' or end of string
    tb = x.find_first_not_of(" \t", te);
    if (tb != std::string::npos) {
      if (x[tb] != '+') {
        // expected '+'
        return false;
      }
      tb = x.find_first_not_of(" \t", tb + 1);
    }
  }

  // parsed the whole thing
  x = "";
  return true;
}

const KeyMap::NameToKeyMap &KeyMap::nameToKeyMap()
{
  static const NameToKeyMap map = [] {
    NameToKeyMap m;
    for (const KeyNameMapEntry *i = kKeyNameMap; i->m_name != nullptr; ++i) {
      m[i->m_name] = i->m_id;
    }
    return m;
  }();
  return map;
}

const KeyMap::KeyToNameMap &KeyMap::keyToNameMap()
{
  static const KeyToNameMap map = [] {
    KeyToNameMap m;
    for (const KeyNameMapEntry *i = kKeyNameMap; i->m_name != nullptr; ++i) {
      m[i->m_id] = i->m_name;
    }
    return m;
  }();
  return map;
}

const KeyMap::NameToModifierMap &KeyMap::nameToModifierMap()
{
  static const NameToModifierMap map = [] {
    NameToModifierMap m;
    for (const KeyModifierNameMapEntry *i = kModifierNameMap; i->m_name != nullptr; ++i) {
      m[i->m_name] = i->m_mask;
    }
    return m;
  }();
  return map;
}

const KeyMap::ModifierToNameMap &KeyMap::modifierToNameMap()
{
  static const ModifierToNameMap map = [] {
    ModifierToNameMap m;
    for (const KeyModifierNameMapEntry *i = kModifierNameMap; i->m_name != nullptr; ++i) {
      m[i->m_mask] = i->m_name;
    }
    return m;
  }();
  return map;
}

//
// KeyMap::Keystroke
//

} // namespace synergy
