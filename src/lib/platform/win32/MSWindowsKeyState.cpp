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

//
// MSWindowsKeyState
//

struct Win32Modifiers
{
public:
  UINT m_vk;
  KeyModifierMask m_mask;
};

static const Win32Modifiers s_modifiers[] = {{VK_SHIFT, KeyModifierShift},      {VK_LSHIFT, KeyModifierShift},
                                             {VK_RSHIFT, KeyModifierShift},     {VK_CONTROL, KeyModifierControl},
                                             {VK_LCONTROL, KeyModifierControl}, {VK_RCONTROL, KeyModifierControl},
                                             {VK_MENU, KeyModifierAlt},         {VK_LMENU, KeyModifierAlt},
                                             {VK_RMENU, KeyModifierAlt},        {VK_LWIN, KeyModifierSuper},
                                             {VK_RWIN, KeyModifierSuper}};

MSWindowsKeyState::MSWindowsKeyState(
    MSWindowsDesks *desks, void *eventTarget, IEventQueue *events, std::vector<std::string> layouts,
    bool isLangSyncEnabled
)
    : KeyState(events, std::move(layouts), isLangSyncEnabled),
      m_eventTarget(eventTarget),
      m_desks(desks),
      m_keyLayout(GetKeyboardLayout(0)),
      m_fixTimer(nullptr),
      m_lastDown(0),
      m_useSavedModifiers(false),
      m_savedModifiers(0),
      m_originalSavedModifiers(0),
      m_events(events)
{
  init();
}

MSWindowsKeyState::MSWindowsKeyState(
    MSWindowsDesks *desks, void *eventTarget, IEventQueue *events, synergy::KeyMap &keyMap,
    std::vector<std::string> layouts, bool isLangSyncEnabled
)
    : KeyState(events, keyMap, std::move(layouts), isLangSyncEnabled),
      m_eventTarget(eventTarget),
      m_desks(desks),
      m_keyLayout(GetKeyboardLayout(0)),
      m_fixTimer(nullptr),
      m_lastDown(0),
      m_useSavedModifiers(false),
      m_savedModifiers(0),
      m_originalSavedModifiers(0),
      m_events(events)
{
  init();
}

MSWindowsKeyState::~MSWindowsKeyState()
{
  disable();
}

void MSWindowsKeyState::init()
{
  // look up symbol that's available on winNT family but not win95
  HMODULE userModule = GetModuleHandle(L"user32.dll");
  m_ToUnicodeEx = (ToUnicodeEx_t)GetProcAddress(userModule, "ToUnicodeEx");
}

void MSWindowsKeyState::disable()
{
  if (m_fixTimer != nullptr) {
    m_events->removeHandler(EventTypes::Timer, m_fixTimer);
    m_events->deleteTimer(m_fixTimer);
    m_fixTimer = nullptr;
  }
  m_lastDown = 0;
}

KeyButton MSWindowsKeyState::virtualKeyToButton(UINT virtualKey) const
{
  return m_virtualKeyToButton[virtualKey & 0xffu];
}

void MSWindowsKeyState::setKeyLayout(HKL keyLayout)
{
  m_keyLayout = keyLayout;
}

bool MSWindowsKeyState::testAutoRepeat(bool press, bool isRepeat, KeyButton button)
{
  if (!isRepeat) {
    isRepeat = (press && m_lastDown != 0 && button == m_lastDown);
  }
  if (press) {
    m_lastDown = button;
  } else {
    m_lastDown = 0;
  }
  return isRepeat;
}

void MSWindowsKeyState::saveModifiers()
{
  m_savedModifiers = getActiveModifiers();
  m_originalSavedModifiers = m_savedModifiers;
}

void MSWindowsKeyState::useSavedModifiers(bool enable)
{
  if (enable != m_useSavedModifiers) {
    m_useSavedModifiers = enable;
    if (!m_useSavedModifiers) {
      // transfer any modifier state changes to KeyState's state
      KeyModifierMask mask = m_originalSavedModifiers ^ m_savedModifiers;
      getActiveModifiersRValue() = (getActiveModifiers() & ~mask) | (m_savedModifiers & mask);
    }
  }
}

KeyID MSWindowsKeyState::mapKeyFromEvent(WPARAM charAndVirtKey, LPARAM info, KeyModifierMask *maskOut) const
{
  static const KeyModifierMask s_controlAlt = KeyModifierControl | KeyModifierAlt;

  // extract character, virtual key, and if we didn't use AltGr
  auto wc = static_cast<WCHAR>((charAndVirtKey & 0xffffu));
  UINT vkCode = ((charAndVirtKey >> 16) & 0xffu);
  bool noAltGr = ((charAndVirtKey & 0xff000000u) != 0);

  // handle some keys via table lookup
  KeyID id = getKeyID(vkCode, (KeyButton)((info >> 16) & 0x1ffu));

  // check if not in table;  map character to key id
  if (id == kKeyNone && wc != 0) {
    // UTF16
    id = static_cast<KeyID>(wc) & 0xffffu;
  }

  // set modifier mask
  if (maskOut != nullptr) {
    KeyModifierMask active = getActiveModifiers();
    if (!noAltGr && (active & s_controlAlt) == s_controlAlt) {
      // if !noAltGr then we're only interested in matching the
      // key, not the AltGr.  AltGr is down (i.e. control and alt
      // are down) but we don't want the client to have to match
      // that so we clear it.
      active &= ~s_controlAlt;
    }
    if (id == kKeyHangul) {
      // If shift-space is used to change input mode, clear shift modifier.
      active &= ~KeyModifierShift;
    }
    *maskOut = active;
  }

  return id;
}

bool MSWindowsKeyState::didGroupsChange() const
{
  GroupList groups;
  return (getGroups(groups) && groups != m_groups);
}

UINT MSWindowsKeyState::mapKeyToVirtualKey(KeyID key) const
{
  if (key == kKeyNone) {
    return 0;
  }
  KeyToVKMap::const_iterator i = m_keyToVKMap.find(key);
  if (i == m_keyToVKMap.end()) {
    return 0;
  } else {
    return i->second;
  }
}

void MSWindowsKeyState::onKey(KeyButton button, bool down, KeyModifierMask newState)
{
  KeyState::onKey(button, down, newState);
}

void MSWindowsKeyState::sendKeyEvent(
    void *target, bool press, bool isAutoRepeat, KeyID key, KeyModifierMask mask, int32_t count, KeyButton button
)
{
  if (press || isAutoRepeat) {
    // send key
    if (press && !isAutoRepeat) {
      KeyState::sendKeyEvent(target, true, false, key, mask, 1, button);
      if (count > 0) {
        --count;
      }
    }
    if (count >= 1) {
      KeyState::sendKeyEvent(target, true, true, key, mask, count, button);
    }
  } else {
    // do key up
    KeyState::sendKeyEvent(target, false, false, key, mask, 1, button);
  }
}

void MSWindowsKeyState::fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang)
{
  KeyState::fakeKeyDown(id, mask, button, lang);
}

bool MSWindowsKeyState::fakeKeyRepeat(
    KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang
)
{
  return KeyState::fakeKeyRepeat(id, mask, count, button, lang);
}

// We must use SendSAS (Secure Attention Sequence) to simulate Ctrl+Alt+Del (since Windows Vista).
//
// According to the MS docs:
// > To successfully call the SendSAS function, an application must either be running as a
// > service or have the uiAccess attribute of the requestedExecutionLevel element set to "true"
// > in its application manifest.
//
// Since the daemon is running as a service, we can use it to send the event.
// We do this by calling `SetEvent` on the `SendSAS` event (created by the daemon).
//
// History: It used to be possible to use `PostMessage` to send `MOD_CONTROL | MOD_ALT, VK_DELETE`
// as a backup but this ability was removed by Microsoft for security in favor of requiring the
// `SendSAS` event to be used, which makes DoS and social engineering attacks more difficult.
bool MSWindowsKeyState::fakeCtrlAltDel()
{
  // The daemon creates this event with default permissions, which means this process must be
  // elevated to be able to open it. If not elevated, an access denied error will be returned.
  MSWindowsHandle sendSasEvent(OpenEvent(EVENT_MODIFY_STATE, FALSE, LPCWSTR(kSendSasEventName)));
  if (!sendSasEvent.get()) {
    LOG_ERR(
        "couldn't open SAS event, unable to simulate ctrl+alt+del, error: %s",
        windowsErrorToString(GetLastError()).c_str()
    );
    return false;
  }

  // Note: We don't directly call SendSAS, but rather we tell the daemon to do it by setting the event.
  LOG_DEBUG("setting SAS event to simulate ctrl+alt+del");
  if (!SetEvent(sendSasEvent.get())) {
    LOG_ERR(
        "failed to set SAS event, unable to simulate ctrl+alt+del, error: %s",
        windowsErrorToString(GetLastError()).c_str()
    );
    return false;
  }

  return true;
}

KeyModifierMask MSWindowsKeyState::pollActiveModifiers() const
{
  KeyModifierMask state = 0;

  // get non-toggle modifiers from our own shadow key state
  for (size_t i = 0; i < sizeof(s_modifiers) / sizeof(s_modifiers[0]); ++i) {
    KeyButton button = virtualKeyToButton(s_modifiers[i].m_vk);
    if (button != 0 && isKeyDown(button)) {
      state |= s_modifiers[i].m_mask;
    }
  }

  // we can get toggle modifiers from the system
  if ((GetKeyState(VK_CAPITAL) & 0x01) != 0) {
    state |= KeyModifierCapsLock;
  }
  if ((GetKeyState(VK_NUMLOCK) & 0x01) != 0) {
    state |= KeyModifierNumLock;
  }
  if ((GetKeyState(VK_SCROLL) & 0x01) != 0) {
    state |= KeyModifierScrollLock;
  }

  return state;
}

int32_t MSWindowsKeyState::pollActiveGroup() const
{
  // determine the thread that'll receive this event
  HWND targetWindow = GetForegroundWindow();
  DWORD targetThread = GetWindowThreadProcessId(targetWindow, nullptr);

  // get keyboard layout for the thread
  HKL hkl = GetKeyboardLayout(targetThread);

  if (!hkl) {
    // GetKeyboardLayout failed. Maybe targetWindow is a console window.
    // We're getting the keyboard layout of the desktop instead.
    targetWindow = GetDesktopWindow();
    targetThread = GetWindowThreadProcessId(targetWindow, nullptr);
    hkl = GetKeyboardLayout(targetThread);
  }

  // get group
  GroupMap::const_iterator i = m_groupMap.find(hkl);
  if (i == m_groupMap.end()) {
    LOG_VERBOSE("can't find keyboard layout %08x", hkl);
    return 0;
  }

  return i->second;
}

void MSWindowsKeyState::pollPressedKeys(KeyButtonSet &pressedKeys) const
{
  BYTE keyState[256];
  if (!GetKeyboardState(keyState)) {
    LOG_WARN("keyboard state is unexpected");
    LOG_DEBUG("function 'GetKeyboardState' returned false on 'pollPressedKeys'");
    return;
  }
  for (KeyButton i = 1; i < 256; ++i) {
    if ((keyState[i] & 0x80) != 0) {
      KeyButton keyButton = virtualKeyToButton(i);
      if (keyButton != 0) {
        pressedKeys.insert(keyButton);
      }
    }
  }
}

void MSWindowsKeyState::fakeKey(const Keystroke &keystroke)
{
  switch (keystroke.m_type) {
  case Keystroke::KeyType::Button: {
    LOG(
        (CLOG_VERBOSE "  %03x (%08x) %s", keystroke.m_data.m_button.m_button, keystroke.m_data.m_button.m_client,
         keystroke.m_data.m_button.m_press ? "down" : "up")
    );
    KeyButton scanCode = keystroke.m_data.m_button.m_button;

    // windows doesn't send key ups for key repeats
    if (keystroke.m_data.m_button.m_repeat && !keystroke.m_data.m_button.m_press) {
      LOG_VERBOSE("  discard key repeat release");
      break;
    }

    // get the virtual key for the button
    WORD vk = (WORD)keystroke.m_data.m_button.m_client;
    DWORD flags = 0;

    if (keystroke.m_data.m_button.m_press == false)
      flags |= KEYEVENTF_KEYUP;

    // special handling of VK_SNAPSHOT
    if (vk == VK_SNAPSHOT)
      scanCode = (getActiveModifiers() & KeyModifierAlt) ? 0x54 : 0x137;

    if (scanCode & 0x100)
      flags |= KEYEVENTF_EXTENDEDKEY;

    // vk,sc,flags,keystroke.m_data.m_button.m_repeat

    m_desks->fakeKeyEvent(vk, scanCode, flags, keystroke.m_data.m_button.m_repeat);

    // synthesize event
    // m_desks->fakeKeyEvent(button, vk,
    //						keystroke.m_data.m_button.m_press,
    //						keystroke.m_data.m_button.m_repeat);
    break;
  }

  case Keystroke::KeyType::Group:
    // we don't restore the group.  we'd like to but we can't be
    // sure the restoring group change will be processed after the
    // key events.
    if (!keystroke.m_data.m_group.m_restore) {
      if (keystroke.m_data.m_group.m_absolute) {
        LOG_VERBOSE("  group %d", keystroke.m_data.m_group.m_group);
        setWindowGroup(keystroke.m_data.m_group.m_group);
      } else {
        LOG_VERBOSE("  group %+d", keystroke.m_data.m_group.m_group);
        setWindowGroup(getEffectiveGroup(pollActiveGroup(), keystroke.m_data.m_group.m_group));
      }
    }
    break;
  }
}

KeyModifierMask &MSWindowsKeyState::getActiveModifiersRValue()
{
  if (m_useSavedModifiers) {
    return m_savedModifiers;
  } else {
    return KeyState::getActiveModifiersRValue();
  }
}
