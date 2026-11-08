/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "XDGKeyUtil.h"

#include "KeyTypes.h"

#include <xkbcommon/xkbcommon-keysyms.h>

KeyID XDGKeyUtil::mapKeySymToKeyID(KeySym k)
{
  switch (k & 0xffffff00) {
  case 0x0000:
    // Latin-1
    return static_cast<KeyID>(k);

  case 0xfe00:
    // ISO 9995 Function and Modifier Keys
    switch (k) {
    case XKB_KEY_ISO_Left_Tab:
      return kKeyLeftTab;

    case XKB_KEY_ISO_Level3_Shift:
      return kKeyAltGr;

#ifdef XKB_KEY_ISO_Level5_Shift
    case XKB_KEY_ISO_Level5_Shift:
      return XKB_KEY_ISO_Level5_Shift; // there is no "usual" key for this...
#endif

    case XKB_KEY_ISO_Next_Group:
      return kKeyNextGroup;

    case XKB_KEY_ISO_Prev_Group:
      return kKeyPrevGroup;

    case XKB_KEY_dead_grave:
      return kKeyDeadGrave;

    case XKB_KEY_dead_acute:
      return kKeyDeadAcute;

    case XKB_KEY_dead_circumflex:
      return kKeyDeadCircumflex;

    case XKB_KEY_dead_tilde:
      return kKeyDeadTilde;

    case XKB_KEY_dead_macron:
      return kKeyDeadMacron;

    case XKB_KEY_dead_breve:
      return kKeyDeadBreve;

    case XKB_KEY_dead_abovedot:
      return kKeyDeadAbovedot;

    case XKB_KEY_dead_diaeresis:
      return kKeyDeadDiaeresis;

    case XKB_KEY_dead_abovering:
      return kKeyDeadAbovering;

    case XKB_KEY_dead_doubleacute:
      return kKeyDeadDoubleacute;

    case XKB_KEY_dead_caron:
      return kKeyDeadCaron;

    case XKB_KEY_dead_cedilla:
      return kKeyDeadCedilla;

    case XKB_KEY_dead_ogonek:
      return kKeyDeadOgonek;

    default:
      return kKeyNone;
    }

  case 0xff00:
    // MISCELLANY
    return static_cast<KeyID>(k - 0xff00 + 0xef00);

  case 0x1008ff00:
    // "Internet" keys
    return static_cast<KeyID>(s_map1008FF.at(k & 0xff));

  default: {
    // lookup character in table
    if (const auto i = s_keySymToUCS4.find(k); i != s_keySymToUCS4.end()) {
      return i->second;
    }

    // unknown character
    return kKeyNone;
  }
  }
}

uint32_t XDGKeyUtil::getModifierBitForKeySym(KeySym keysym)
{
  switch (keysym) {
  case XKB_KEY_Shift_L:
  case XKB_KEY_Shift_R:
    return kKeyModifierBitShift;

  case XKB_KEY_Control_L:
  case XKB_KEY_Control_R:
    return kKeyModifierBitControl;

  case XKB_KEY_Alt_L:
  case XKB_KEY_Alt_R:
    return kKeyModifierBitAlt;

  case XKB_KEY_Meta_L:
  case XKB_KEY_Meta_R:
    return kKeyModifierBitMeta;

  case XKB_KEY_Super_L:
  case XKB_KEY_Super_R:
  case XKB_KEY_Hyper_L:
  case XKB_KEY_Hyper_R:
    return kKeyModifierBitSuper;

  case XKB_KEY_Mode_switch:
  case XKB_KEY_ISO_Level3_Shift:
    return kKeyModifierBitAltGr;

#ifdef XKB_KEY_ISO_Level5_Shift
  case XKB_KEY_ISO_Level5_Shift:
    return kKeyModifierBitLevel5Lock;
#endif

  case XKB_KEY_Caps_Lock:
    return kKeyModifierBitCapsLock;

  case XKB_KEY_Num_Lock:
    return kKeyModifierBitNumLock;

  case XKB_KEY_Scroll_Lock:
    return kKeyModifierBitScrollLock;

  default:
    return kKeyModifierBitNone;
  }
}
