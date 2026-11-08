/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXScreen.h"

#include "ClientApp.h"
#include "Clipboard.h"
#include "DisplayInvalidException.h"
#include "KeyMap.h"
#include "OSXClipboard.h"
#include "OSXEventQueueBuffer.h"
#include "OSXKeyState.h"
#include "OSXMediaKeySupport.h"
#include "OSXPasteboardPeeker.h"
#include "OSXScreenSaver.h"
#include "arch/Arch.h"
#include "arch/ArchException.h"
#include "base/Event.h"
#include "base/EventQueue.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "base/TMethodJob.h"
#include "client/Client.h"
#include "common/ExitCodes.h"
#include "common/Settings.h"
#include "mt/CondVar.h"
#include "mt/Lock.h"
#include "mt/Mutex.h"
#include "mt/Thread.h"

#include <AppKit/NSEvent.h>
#include <AvailabilityMacros.h>
#include <IOKit/hidsystem/event_status_driver.h>
#include <cstring>
#include <dispatch/dispatch.h>
#include <libproc.h>
#include <mach-o/dyld.h>
#include <math.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

int getSecureInputEventPID();
std::string getProcessName(int pid);

// TODO: upgrade deprecated function usage in these functions.
void setZeroSuppressionInterval();
void avoidSupression();
void logCursorVisibility();
void avoidHesitatingCursor();

//
// OSXScreen
//

uint32_t OSXScreen::registerHotKey(KeyID key, KeyModifierMask mask)
{
  // get mac virtual key and modifier mask matching Synergy key and mask
  uint32_t macKey, macMask;
  if (!m_keyState->mapSynergyHotKeyToMac(key, mask, macKey, macMask)) {
    LOG_DEBUG("could not map hotkey id=%04x mask=%04x", key, mask);
    return 0;
  }

  // choose hotkey id
  uint32_t id;
  if (!m_oldHotKeyIDs.empty()) {
    id = m_oldHotKeyIDs.back();
    m_oldHotKeyIDs.pop_back();
  } else {
    id = m_hotKeys.size() + 1;
  }

  // if this hot key has modifiers only then we'll handle it specially
  EventHotKeyRef ref = nullptr;
  bool okay;
  if (key == kKeyNone) {
    if (m_modifierHotKeys.count(mask) > 0) {
      // already registered
      okay = false;
    } else {
      m_modifierHotKeys[mask] = id;
      okay = true;
    }
  } else {
    EventHotKeyID hkid = {'SNRG', (uint32_t)id};
    OSStatus status = RegisterEventHotKey(macKey, macMask, hkid, GetApplicationEventTarget(), 0, &ref);
    okay = (status == noErr);
    m_hotKeyToIDMap[HotKeyItem(macKey, macMask)] = id;
  }

  if (!okay) {
    m_oldHotKeyIDs.push_back(id);
    m_hotKeyToIDMap.erase(HotKeyItem(macKey, macMask));
    LOG_WARN(
        "failed to register hotkey %s (id=%04x mask=%04x)", synergy::KeyMap::formatKey(key, mask).c_str(), key, mask
    );
    return 0;
  }

  m_hotKeys.insert(std::make_pair(id, HotKeyItem(ref, macKey, macMask)));

  LOG_DEBUG(
      "registered hotkey %s (id=%04x mask=%04x) as id=%d", synergy::KeyMap::formatKey(key, mask).c_str(), key, mask, id
  );
  return id;
}

void OSXScreen::unregisterHotKey(uint32_t id)
{
  // look up hotkey
  HotKeyMap::iterator i = m_hotKeys.find(id);
  if (i == m_hotKeys.end()) {
    return;
  }

  // unregister with OS
  bool okay;
  if (i->second.getRef() != nullptr) {
    okay = (UnregisterEventHotKey(i->second.getRef()) == noErr);
  } else {
    okay = false;
    // XXX -- this is inefficient
    for (ModifierHotKeyMap::iterator j = m_modifierHotKeys.begin(); j != m_modifierHotKeys.end(); ++j) {
      if (j->second == id) {
        m_modifierHotKeys.erase(j);
        okay = true;
        break;
      }
    }
  }
  if (!okay) {
    LOG_WARN("failed to unregister hotkey id=%d", id);
  } else {
    LOG_DEBUG("unregistered hotkey id=%d", id);
  }

  // discard hot key from map and record old id for reuse
  m_hotKeyToIDMap.erase(i->second);
  m_hotKeys.erase(i);
  m_oldHotKeyIDs.push_back(id);
  if (m_activeModifierHotKey == id) {
    m_activeModifierHotKey = 0;
    m_activeModifierHotKeyMask = 0;
  }
}

void OSXScreen::constructMouseButtonEventMap()
{
  const CGEventType source[NumButtonIDs][3] = {
      {kCGEventLeftMouseUp, kCGEventLeftMouseDragged, kCGEventLeftMouseDown},
      {kCGEventRightMouseUp, kCGEventRightMouseDragged, kCGEventRightMouseDown},
      {kCGEventOtherMouseUp, kCGEventOtherMouseDragged, kCGEventOtherMouseDown},
      {kCGEventOtherMouseUp, kCGEventOtherMouseDragged, kCGEventOtherMouseDown},
      {kCGEventOtherMouseUp, kCGEventOtherMouseDragged, kCGEventOtherMouseDown},
      {kCGEventOtherMouseUp, kCGEventOtherMouseDragged, kCGEventOtherMouseDown}
  };

  for (uint16_t button = 0; button < NumButtonIDs; button++) {
    MouseButtonEventMapType new_map;
    for (uint16_t state = (uint32_t)kMouseButtonUp; state < kMouseButtonStateMax; state++) {
      CGEventType curEvent = source[button][state];
      new_map[state] = curEvent;
    }
    MouseButtonEventMap[button] = new_map;
  }
}

void OSXScreen::postMouseEvent(CGPoint &pos) const
{
  // check if cursor position is valid on the client display configuration
  CGDisplayCount displayCount = 0;
  CGGetDisplaysWithPoint(pos, 0, nullptr, &displayCount);
  if (displayCount == 0) {
    // cursor position invalid - clamp to bounds of last valid display.
    // find the last valid display using the last cursor position.
    displayCount = 0;
    CGDirectDisplayID displayID;
    CGGetDisplaysWithPoint(CGPointMake(m_xCursor, m_yCursor), 1, &displayID, &displayCount);
    if (displayCount != 0) {
      CGRect displayRect = CGDisplayBounds(displayID);
      if (pos.x < displayRect.origin.x) {
        pos.x = displayRect.origin.x;
      } else if (pos.x > displayRect.origin.x + displayRect.size.width - 1) {
        pos.x = displayRect.origin.x + displayRect.size.width - 1;
      }
      if (pos.y < displayRect.origin.y) {
        pos.y = displayRect.origin.y;
      } else if (pos.y > displayRect.origin.y + displayRect.size.height - 1) {
        pos.y = displayRect.origin.y + displayRect.size.height - 1;
      }
    }
  }

  CGEventType type = kCGEventMouseMoved;

  int8_t button = m_buttonState.getFirstButtonDown();
  if (button != -1) {
    MouseButtonEventMapType thisButtonType = MouseButtonEventMap[button];
    type = thisButtonType[kMouseButtonDragged];
  }

  CGEventRef event = CGEventCreateMouseEvent(nullptr, type, pos, static_cast<CGMouseButton>(button));

  // Dragging events also need the click state
  CGEventSetIntegerValueField(event, kCGMouseEventClickState, m_clickState);

  // Fix for sticky keys
  CGEventFlags modifiers = m_keyState->getModifierStateAsOSXFlags();
  CGEventSetFlags(event, modifiers);

  // Set movement deltas to fix issues with certain 3D programs
  SInt64 deltaX = pos.x;
  deltaX -= m_xCursor;

  SInt64 deltaY = pos.y;
  deltaY -= m_yCursor;

  CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, deltaX);
  CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, deltaY);

  double deltaFX = deltaX;
  double deltaFY = deltaY;

  CGEventSetDoubleValueField(event, kCGMouseEventDeltaX, deltaFX);
  CGEventSetDoubleValueField(event, kCGMouseEventDeltaY, deltaFY);

  CGEventPost(kCGHIDEventTap, event);

  CFRelease(event);
}

void OSXScreen::fakeMouseButton(ButtonID id, bool press)
{
  // Buttons are indexed from one, but the button down array is indexed from zero
  uint32_t index = mapSynergyButtonToMac(id) - kButtonLeft;
  if (index >= NumButtonIDs) {
    return;
  }

  CGPoint pos;
  if (!m_cursorPosValid) {
    int32_t x, y;
    getCursorPos(x, y);
  }
  pos.x = m_xCursor;
  pos.y = m_yCursor;

  // variable used to detect mouse coordinate differences between
  // old & new mouse clicks. Used in double click detection.
  int32_t xDiff = m_xCursor - m_lastSingleClickXCursor;
  int32_t yDiff = m_yCursor - m_lastSingleClickYCursor;
  double diff = sqrt(xDiff * xDiff + yDiff * yDiff);
  // max sqrt(x^2 + y^2) difference allowed to double click
  // since we don't have double click distance in NX APIs
  // we define our own defaults.
  const double maxDiff = sqrt(2) + 0.0001;

  double clickTime = [NSEvent doubleClickInterval];

  // As long as the click is within the time window and distance window
  // increase clickState (double click, triple click, etc)
  // This will allow for higher than triple click but the quartz documenation
  // does not specify that this should be limited to triple click
  if (press) {
    if ((Arch::time() - m_lastClickTime) <= clickTime && diff <= maxDiff) {
      m_clickState++;
    } else {
      m_clickState = 1;
    }

    m_lastClickTime = Arch::time();
  }

  if (m_clickState == 1) {
    m_lastSingleClickXCursor = m_xCursor;
    m_lastSingleClickYCursor = m_yCursor;
  }

  EMouseButtonState state = press ? kMouseButtonDown : kMouseButtonUp;

  LOG_VERBOSE("faking mouse button id: %d press: %s", index, press ? "pressed" : "released");

  MouseButtonEventMapType thisButtonMap = MouseButtonEventMap[index];
  CGEventType type = thisButtonMap[state];

  CGEventRef event = CGEventCreateMouseEvent(nullptr, type, pos, static_cast<CGMouseButton>(index));

  CGEventSetIntegerValueField(event, kCGMouseEventClickState, m_clickState);

  // Fix for sticky keys
  CGEventFlags modifiers = m_keyState->getModifierStateAsOSXFlags();
  CGEventSetFlags(event, modifiers);

  m_buttonState.set(index, state);
  CGEventPost(kCGHIDEventTap, event);

  CFRelease(event);
}

void OSXScreen::fakeMouseMove(int32_t x, int32_t y)
{
  // synthesize event
  CGPoint pos;
  pos.x = x;
  pos.y = y;
  postMouseEvent(pos);

  // save new cursor position
  m_xCursor = static_cast<int32_t>(pos.x);
  m_yCursor = static_cast<int32_t>(pos.y);
  m_cursorPosValid = true;
}

void OSXScreen::fakeMouseRelativeMove(int32_t dx, int32_t dy) const
{
  // OS X does not appear to have a fake relative mouse move function.
  // simulate it by getting the current mouse position and adding to
  // that.  this can yield the wrong answer but there's not much else
  // we can do.

  // get current position
  CGEventRef event = CGEventCreate(nullptr);
  CGPoint oldPos = CGEventGetLocation(event);
  CFRelease(event);

  // synthesize event
  CGPoint pos;
  m_xCursor = static_cast<int32_t>(oldPos.x);
  m_yCursor = static_cast<int32_t>(oldPos.y);
  pos.x = oldPos.x + dx;
  pos.y = oldPos.y + dy;
  postMouseEvent(pos);

  // we now assume we don't know the current cursor position
  m_cursorPosValid = false;
}

void OSXScreen::fakeMouseWheel(ScrollDelta delta) const
{
  if (delta.x != 0 || delta.y != 0) {
    // use server's acceleration with a little boost since other platforms
    // take one wheel step as a larger step than the mac does.
    delta = applyScrollModifier(
        {static_cast<int32_t>(3.0 * delta.x / s_scrollDelta), static_cast<int32_t>(3.0 * delta.y / s_scrollDelta)}
    );
    // create a scroll event, post it and release it.  not sure if kCGScrollEventUnitLine
    // is the right choice here over kCGScrollEventUnitPixel
    CGEventRef scrollEvent = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 2, delta.y, delta.x);

    // Fix for sticky keys
    CGEventFlags modifiers = m_keyState->getModifierStateAsOSXFlags();
    CGEventSetFlags(scrollEvent, modifiers);

    CGEventPost(kCGHIDEventTap, scrollEvent);
    CFRelease(scrollEvent);
  }
}

void OSXScreen::showCursor()
{
  LOG_DEBUG("showing cursor");

  CFStringRef propertyString = CFStringCreateWithCString(nullptr, "SetsCursorInBackground", kCFStringEncodingMacRoman);

  CGSSetConnectionProperty(_CGSDefaultConnection(), _CGSDefaultConnection(), propertyString, kCFBooleanTrue);

  CFRelease(propertyString);

  CGError error = CGDisplayShowCursor(m_displayID);
  if (error != kCGErrorSuccess) {
    LOG_ERR("failed to show cursor, error=%d", error);
  }

  // appears to fix "mouse randomly not showing" bug
  CGAssociateMouseAndMouseCursorPosition(true);

  logCursorVisibility();

  m_cursorHidden = false;
}

void OSXScreen::hideCursor()
{
  LOG_DEBUG("hiding cursor");

  CFStringRef propertyString = CFStringCreateWithCString(nullptr, "SetsCursorInBackground", kCFStringEncodingMacRoman);

  CGSSetConnectionProperty(_CGSDefaultConnection(), _CGSDefaultConnection(), propertyString, kCFBooleanTrue);

  CFRelease(propertyString);

  CGError error = CGDisplayHideCursor(m_displayID);
  if (error != kCGErrorSuccess) {
    LOG_ERR("failed to hide cursor, error=%d", error);
  }

  // appears to fix "mouse randomly not hiding" bug
  CGAssociateMouseAndMouseCursorPosition(true);

  logCursorVisibility();

  m_cursorHidden = true;
}

ButtonID OSXScreen::mapSynergyButtonToMac(uint16_t button) const
{
  switch (button) {
  case 1:
    return kButtonLeft;
  case 2:
    return kMacButtonMiddle;
  case 3:
    return kMacButtonRight;
  case 4:
    return kButtonExtra0;
  case 5:
    return kButtonExtra1;
  default:
    return kButtonNone;
  }
}

ButtonID OSXScreen::mapMacButtonToSynergy(uint16_t macButton) const
{
  switch (macButton) {
  case 1:
    return kButtonLeft;
  case 2:
    return kButtonRight;
  case 3:
    return kButtonMiddle;
  case 4:
    return kButtonExtra0;
  case 5:
    return kButtonExtra1;
  default:
    return kButtonNone;
  }
}

int32_t OSXScreen::mapScrollWheelToSynergy(int32_t x) const
{
  // return accelerated scrolling
  double d = (1.0 + getScrollSpeed()) * x;
  return static_cast<int32_t>(120.0 * d);
}

double OSXScreen::getScrollSpeed() const
{
  double scaling = 0.0;

  CFPropertyListRef pref = ::CFPreferencesCopyValue(
      CFSTR("com.apple.scrollwheel.scaling"), kCFPreferencesAnyApplication, kCFPreferencesCurrentUser,
      kCFPreferencesAnyHost
  );
  if (pref != nullptr) {
    CFTypeID id = CFGetTypeID(pref);
    if (id == CFNumberGetTypeID()) {
      CFNumberRef value = static_cast<CFNumberRef>(pref);
      if (CFNumberGetValue(value, kCFNumberDoubleType, &scaling)) {
        if (scaling < 0.0) {
          scaling = 0.0;
        }
      }
    }
    CFRelease(pref);
  }

  return scaling;
}

#pragma mark -

//
// OSXScreen::HotKeyItem
//

OSXScreen::HotKeyItem::HotKeyItem(uint32_t keycode, uint32_t mask) : m_ref(nullptr), m_keycode(keycode), m_mask(mask)
{
  // do nothing
}

OSXScreen::HotKeyItem::HotKeyItem(EventHotKeyRef ref, uint32_t keycode, uint32_t mask)
    : m_ref(ref),
      m_keycode(keycode),
      m_mask(mask)
{
  // do nothing
}

EventHotKeyRef OSXScreen::HotKeyItem::getRef() const
{
  return m_ref;
}

bool OSXScreen::HotKeyItem::operator<(const HotKeyItem &x) const
{
  return (m_keycode < x.m_keycode || (m_keycode == x.m_keycode && m_mask < x.m_mask));
}

void OSXScreen::MouseButtonState::set(uint32_t button, EMouseButtonState state)
{
  bool newState = (state == kMouseButtonDown);
  m_buttons.set(button, newState);
}

bool OSXScreen::MouseButtonState::any()
{
  return m_buttons.any();
}

void OSXScreen::MouseButtonState::reset()
{
  m_buttons.reset();
}

void OSXScreen::MouseButtonState::overwrite(uint32_t buttons)
{
  m_buttons = std::bitset<NumButtonIDs>(buttons);
}

bool OSXScreen::MouseButtonState::test(uint32_t button) const
{
  return m_buttons.test(button);
}

int8_t OSXScreen::MouseButtonState::getFirstButtonDown() const
{
  if (m_buttons.any()) {
    for (unsigned short button = 0; button < m_buttons.size(); button++) {
      if (m_buttons.test(button)) {
        return button;
      }
    }
  }
  return -1;
}

#pragma clang diagnostic pop
