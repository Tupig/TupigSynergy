/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2022, 2025 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2003 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsKeyState.h"

#include "MSWindowsDesks.h"
#include "MSWindowsHandle.h"
#include "arch/win32/XArchWindows.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "common/Constants.h"

// extended mouse buttons
#if !defined(VK_XBUTTON1)
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#endif

void MSWindowsKeyState::getKeyMap(synergy::KeyMap &keyMap)
{
  // update keyboard groups
  if (getGroups(m_groups)) {
    m_groupMap.clear();
    int32_t numGroups = (int32_t)m_groups.size();
    for (int32_t g = 0; g < numGroups; ++g) {
      m_groupMap[m_groups[g]] = g;
    }
  }
  HKL activeLayout = GetKeyboardLayout(0);

  // clear table
  memset(m_virtualKeyToButton, 0, sizeof(m_virtualKeyToButton));
  m_keyToVKMap.clear();

  synergy::KeyMap::KeyItem item;
  int32_t numGroups = (int32_t)m_groups.size();
  for (int32_t g = 0; g < numGroups; ++g) {
    item.m_group = g;
    ActivateKeyboardLayout(m_groups[g], 0);

    // clear tables
    memset(m_buttonToVK, 0, sizeof(m_buttonToVK));
    memset(m_buttonToNumpadVK, 0, sizeof(m_buttonToNumpadVK));

    // map buttons (scancodes) to virtual keys
    for (KeyButton i = 1; i < 256; ++i) {
      UINT vk = MapVirtualKey(i, 1);
      if (vk == 0) {
        // unmapped
        continue;
      }

      // deal with certain virtual keys specially
      switch (vk) {
      case VK_SHIFT:
        // this is important for sending the correct modifier to the
        // client, a patch from bug #242 (right shift broken for ms
        // remote desktop) removed this to just use left shift, which
        // caused bug #2799 (right shift broken for osx).
        // we must not repeat this same mistake and must fix platform
        // specific bugs in code that only affects that platform.
        if (MapVirtualKey(VK_RSHIFT, 0) == i) {
          vk = VK_RSHIFT;
        } else {
          vk = VK_LSHIFT;
        }
        break;

      case VK_CONTROL:
        vk = VK_LCONTROL;
        break;

      case VK_MENU:
        vk = VK_LMENU;
        break;

      case VK_NUMLOCK:
        vk = VK_PAUSE;
        break;

      case VK_NUMPAD0:
      case VK_NUMPAD1:
      case VK_NUMPAD2:
      case VK_NUMPAD3:
      case VK_NUMPAD4:
      case VK_NUMPAD5:
      case VK_NUMPAD6:
      case VK_NUMPAD7:
      case VK_NUMPAD8:
      case VK_NUMPAD9:
      case VK_DECIMAL:
        // numpad keys are saved in their own table
        m_buttonToNumpadVK[i] = vk;
        continue;

      case VK_LWIN:
      case VK_RWIN:
        break;

      case VK_RETURN:
      case VK_PRIOR:
      case VK_NEXT:
      case VK_END:
      case VK_HOME:
      case VK_LEFT:
      case VK_UP:
      case VK_RIGHT:
      case VK_DOWN:
      case VK_INSERT:
      case VK_DELETE:
        // also add extended key for these
        m_buttonToVK[i | 0x100u] = vk;
        break;
      }

      if (m_buttonToVK[i] == 0) {
        m_buttonToVK[i] = vk;
      }
    }

    // now map virtual keys to buttons.  multiple virtual keys may map
    // to a single button.  if the virtual key matches the one in
    // m_buttonToVK then we use the button as is.  if not then it's
    // either a numpad key and we use the button as is or it's an
    // extended button.
    for (UINT i = 1; i < 255; ++i) {
      // skip virtual keys we don't want
      switch (i) {
      case VK_LBUTTON:
      case VK_RBUTTON:
      case VK_MBUTTON:
      case VK_XBUTTON1:
      case VK_XBUTTON2:
      case VK_SHIFT:
      case VK_CONTROL:
      case VK_MENU:
        continue;
      }

      // get the button
      KeyButton button = static_cast<KeyButton>(MapVirtualKey(i, 0));
      if (button == 0) {
        continue;
      }

      // deal with certain virtual keys specially
      switch (i) {
      case VK_NUMPAD0:
      case VK_NUMPAD1:
      case VK_NUMPAD2:
      case VK_NUMPAD3:
      case VK_NUMPAD4:
      case VK_NUMPAD5:
      case VK_NUMPAD6:
      case VK_NUMPAD7:
      case VK_NUMPAD8:
      case VK_NUMPAD9:
      case VK_DECIMAL:
        m_buttonToNumpadVK[button] = i;
        break;

      default:
        // add extended key if virtual keys don't match
        if (m_buttonToVK[button] != i) {
          m_buttonToVK[button | 0x100u] = i;
        }
        break;
      }
    }

    // add the extended printscreen (alt+printscreen)
    // or maybe it's the non-extended printscreen?
    // scancodes and keyboard inputs are complicated
    if (m_buttonToVK[0x54u] == 0) {
      m_buttonToVK[0x54u] = VK_SNAPSHOT;
    }

    // set virtual key to button table
    if (activeLayout == m_groups[g]) {
      for (KeyButton i = 0; i < 512; ++i) {
        if (m_buttonToVK[i] != 0) {
          if (m_virtualKeyToButton[m_buttonToVK[i]] == 0) {
            m_virtualKeyToButton[m_buttonToVK[i]] = i;
          }
        }
        if (m_buttonToNumpadVK[i] != 0) {
          if (m_virtualKeyToButton[m_buttonToNumpadVK[i]] == 0) {
            m_virtualKeyToButton[m_buttonToNumpadVK[i]] = i;
          }
        }
      }
    }

    // add numpad keys
    for (KeyButton i = 0; i < 512; ++i) {
      if (m_buttonToNumpadVK[i] != 0) {
        item.m_id = getKeyID(m_buttonToNumpadVK[i], i);
        item.m_button = i;
        item.m_required = KeyModifierNumLock;
        item.m_sensitive = KeyModifierNumLock | KeyModifierShift;
        item.m_generates = 0;
        item.m_client = m_buttonToNumpadVK[i];
        addKeyEntry(keyMap, item);
      }
    }

    // add other keys
    BYTE keys[256];
    memset(keys, 0, sizeof(keys));
    for (KeyButton i = 0; i < 512; ++i) {
      if (m_buttonToVK[i] != 0) {
        // initialize item
        item.m_id = getKeyID(m_buttonToVK[i], i);
        item.m_button = i;
        item.m_required = 0;
        item.m_sensitive = 0;
        item.m_client = m_buttonToVK[i];

        // get flags for modifier keys
        synergy::KeyMap::initModifierKey(item);

        if (item.m_id == 0) {
          // translate virtual key to a character with and without
          // shift, caps lock, and AltGr.
          struct Modifier
          {
            UINT m_vk1;
            UINT m_vk2;
            BYTE m_state;
            KeyModifierMask m_mask;
          };
          static const Modifier modifiers[] = {
              {VK_SHIFT, VK_SHIFT, 0x80u, KeyModifierShift},
              {VK_CAPITAL, VK_CAPITAL, 0x01u, KeyModifierCapsLock},
              {VK_CONTROL, VK_MENU, 0x80u, KeyModifierAltGr}
          };
          static const size_t s_numModifiers = sizeof(modifiers) / sizeof(modifiers[0]);
          static const size_t s_numCombinations = 1 << s_numModifiers;
          KeyID id[s_numCombinations];

          bool anyFound = false;
          KeyButton button = static_cast<KeyButton>(i & 0xffu);
          for (size_t j = 0; j < s_numCombinations; ++j) {
            for (size_t k = 0; k < s_numModifiers; ++k) {
              // if ((j & (1 << k)) != 0) {
              //  http://msdn.microsoft.com/en-us/library/ke55d167.aspx
              if ((j & (1i64 << k)) != 0) {
                keys[modifiers[k].m_vk1] = modifiers[k].m_state;
                keys[modifiers[k].m_vk2] = modifiers[k].m_state;
              } else {
                keys[modifiers[k].m_vk1] = 0;
                keys[modifiers[k].m_vk2] = 0;
              }
            }
            id[j] = getIDForKey(item, button, m_buttonToVK[i], keys, m_groups[g]);
            if (id[j] != 0) {
              anyFound = true;
            }
          }

          if (anyFound) {
            // determine what modifiers we're sensitive to.
            // we're sensitive if the KeyID changes when the
            // modifier does.
            item.m_sensitive = 0;
            for (size_t k = 0; k < s_numModifiers; ++k) {
              for (size_t j = 0; j < s_numCombinations; ++j) {
                // if (id[j] != id[j ^ (1u << k)]) {
                //  http://msdn.microsoft.com/en-us/library/ke55d167.aspx
                if (id[j] != id[j ^ (1ui64 << k)]) {
                  item.m_sensitive |= modifiers[k].m_mask;
                  break;
                }
              }
            }

            // save each key.  the map will automatically discard
            // duplicates, like an unshift and shifted version of
            // a key that's insensitive to shift.
            for (size_t j = 0; j < s_numCombinations; ++j) {
              item.m_id = id[j];
              item.m_required = 0;
              for (size_t k = 0; k < s_numModifiers; ++k) {
                if ((j & (1i64 << k)) != 0) {
                  item.m_required |= modifiers[k].m_mask;
                }
              }
              addKeyEntry(keyMap, item);
            }
          }
        } else {
          // found in table
          switch (m_buttonToVK[i]) {
          case VK_TAB:
            // add kKeyLeftTab, too
            item.m_id = kKeyLeftTab;
            item.m_required |= KeyModifierShift;
            item.m_sensitive |= KeyModifierShift;
            addKeyEntry(keyMap, item);
            item.m_id = kKeyTab;
            item.m_required &= ~KeyModifierShift;
            break;

          case VK_CANCEL:
            item.m_required |= KeyModifierControl;
            item.m_sensitive |= KeyModifierControl;
            break;
          }

          addKeyEntry(keyMap, item);
        }
      }
    }
  }

  // restore keyboard layout
  ActivateKeyboardLayout(activeLayout, 0);
}

bool MSWindowsKeyState::getGroups(GroupList &groups) const
{
  // get keyboard layouts
  uint32_t newNumLayouts = GetKeyboardLayoutList(0, nullptr);
  if (newNumLayouts == 0) {
    LOG_VERBOSE("can't get keyboard layouts");
    return false;
  }
  HKL *newLayouts = new HKL[newNumLayouts];
  newNumLayouts = GetKeyboardLayoutList(newNumLayouts, newLayouts);
  if (newNumLayouts == 0) {
    LOG_VERBOSE("can't get keyboard layouts");
    delete[] newLayouts;
    return false;
  }

  groups.clear();
  groups.insert(groups.end(), newLayouts, newLayouts + newNumLayouts);
  delete[] newLayouts;
  return true;
}

void MSWindowsKeyState::setWindowGroup(int32_t group)
{
  HWND targetWindow = GetForegroundWindow();

  bool sysCharSet = true;
  // XXX -- determine if m_groups[group] can be used with the system
  // character set.

  if (!PostMessage(targetWindow, WM_INPUTLANGCHANGEREQUEST, sysCharSet ? 1 : 0, (LPARAM)m_groups[group])) {
    LOG_WARN("failed to post change language message");
  }

  // XXX -- use a short delay to let the target window process the message
  // before it sees the keyboard events.  i'm not sure why this is
  // necessary since the messages should arrive in order.  if we don't
  // delay, though, some of our keyboard events may disappear.
  Sleep(100);
}

KeyID MSWindowsKeyState::getKeyID(UINT virtualKey, KeyButton button) const
{
  // Some virtual keycodes have same values.
  // VK_HANGUL == VK_KANA, VK_HANJA == NK_KANJI
  // which are used to change the input mode of IME.
  // But they have different X11 keysym. So we should distinguish them.
  if ((LOWORD(m_keyLayout) & 0xffffu) == 0x0412u) { // 0x0412 : Korean Locale ID
    if (virtualKey == VK_HANGUL || virtualKey == VK_HANJA) {
      // If shift-space is used to change the input mode,
      // the extented bit is not set. So add it to get right key id.
      button |= 0x100u;
    }
  }

  if ((button & 0x100u) != 0) {
    virtualKey += 0x100u;
  }
  return s_virtualKey[virtualKey];
}

UINT MSWindowsKeyState::mapButtonToVirtualKey(KeyButton button) const
{
  return m_buttonToVK[button];
}

KeyID MSWindowsKeyState::getIDForKey(
    synergy::KeyMap::KeyItem &, KeyButton button, UINT virtualKey, PBYTE keyState, HKL hkl
) const
{
  WCHAR unicode[2];
  int n = m_ToUnicodeEx(virtualKey, button, keyState, unicode, sizeof(unicode) / sizeof(unicode[0]), 0, hkl);
  KeyID id = static_cast<KeyID>(unicode[0]);

  switch (n) {
  case -1: {
    // dead key. add an space to the keyboard so we exit
    // the dead key mode and future calls to this function
    // with different modifiers are not affected.

    BYTE emptyState[256] = {};
    n = m_ToUnicodeEx(VK_SPACE, 0, emptyState, unicode, sizeof(unicode) / sizeof(unicode[0]), 0, hkl);

    // as an alternative, we could use the returned
    // buffer in unicode to look at the dead key character
    // and not rely on getDeadKey to provide the mapping
    return synergy::KeyMap::getDeadKey(id);
  }
  default:
  case 0:
    // unmapped
    return kKeyNone;

  case 1:
    return id;

  case 2:
    // left over dead key in buffer. this used to recurse,
    // but apparently this causes a stack overflow, so just
    // return no key instead.
    return kKeyNone;
  }
}

void MSWindowsKeyState::addKeyEntry(synergy::KeyMap &keyMap, synergy::KeyMap::KeyItem &item)
{
  keyMap.addKeyEntry(item);
  if (item.m_group == 0) {
    m_keyToVKMap[item.m_id] = static_cast<UINT>(item.m_client);
  }
}
