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

////////////////////////////////////////////////////////////

// This isn't in any Apple SDK that I know of as of yet.
enum
{
  kSynergyEventMouseScroll = 11,
  kSynergyMouseScrollAxisX = 'saxx',
  kSynergyMouseScrollAxisY = 'saxy'
};

void OSXScreen::sendEvent(EventTypes type, void *data) const
{
  m_events->addEvent(Event(type, getEventTarget(), data));
}

void OSXScreen::sendClipboardEvent(EventTypes type, ClipboardID id) const
{
  ClipboardInfo *info = (ClipboardInfo *)malloc(sizeof(ClipboardInfo));
  info->m_id = id;
  info->m_sequenceNumber = m_sequenceNumber;
  sendEvent(type, info);
}

void OSXScreen::handleSystemEvent(const Event &event)
{
  EventRef *carbonEvent = static_cast<EventRef *>(event.getData());
  assert(carbonEvent != nullptr);

  uint32_t eventClass = GetEventClass(*carbonEvent);

  switch (eventClass) {
  case kEventClassMouse:
    switch (GetEventKind(*carbonEvent)) {
    case kSynergyEventMouseScroll: {
      OSStatus r;
      long xScroll;
      long yScroll;

      // get scroll amount
      r = GetEventParameter(
          *carbonEvent, kSynergyMouseScrollAxisX, typeSInt32, nullptr, sizeof(xScroll), nullptr, &xScroll
      );
      if (r != noErr) {
        xScroll = 0;
      }
      r = GetEventParameter(
          *carbonEvent, kSynergyMouseScrollAxisY, typeSInt32, nullptr, sizeof(yScroll), nullptr, &yScroll
      );
      if (r != noErr) {
        yScroll = 0;
      }

      if (xScroll != 0 || yScroll != 0) {
        onMouseWheel(-mapScrollWheelToSynergy(xScroll), mapScrollWheelToSynergy(yScroll));
      }
    }
    }
    break;

  case kEventClassKeyboard:
    switch (GetEventKind(*carbonEvent)) {
    case kEventHotKeyPressed:
    case kEventHotKeyReleased:
      onHotKey(*carbonEvent);
      break;
    }

    break;

  case kEventClassWindow:
    // 2nd param was formerly GetWindowEventTarget(m_userInputWindow) which is 32-bit only,
    // however as m_userInputWindow is never initialized to anything we can take advantage of
    // the fact that GetWindowEventTarget(nullptr) == nullptr
    SendEventToEventTarget(*carbonEvent, nullptr);
    switch (GetEventKind(*carbonEvent)) {
    case kEventWindowActivated:
      LOG_VERBOSE("window activated");
      break;

    case kEventWindowDeactivated:
      LOG_VERBOSE("window deactivated");
      break;

    case kEventWindowFocusAcquired:
      LOG_VERBOSE("focus acquired");
      break;

    case kEventWindowFocusRelinquish:
      LOG_VERBOSE("focus released");
      break;
    }
    break;

  default:
    SendEventToEventTarget(*carbonEvent, GetEventDispatcherTarget());
    break;
  }
}

bool OSXScreen::onMouseMove()
{
  // when we receive a mouse-move event, it is possible it was queued for a period
  // and that the mouse has already moved again since then.  to handle this, we need
  // to query the current mouse position rather than using the position in the event.
  CGEventRef event = CGEventCreate(NULL);
  CGPoint pos = CGEventGetLocation(event);
  CFRelease(event);
  CGFloat mx = pos.x;
  CGFloat my = pos.y;

  LOG_VERBOSE("mouse move %+f,%+f", mx, my);

  CGFloat x = mx - m_xCursor;
  CGFloat y = my - m_yCursor;

  if ((x == 0 && y == 0) || (mx == m_xCenter && mx == m_yCenter)) {
    return true;
  }

  // save position to compute delta of next motion
  m_xCursor = (int32_t)mx;
  m_yCursor = (int32_t)my;

  if (m_isOnScreen) {
    // motion on primary screen
    sendEvent(EventTypes::PrimaryScreenMotionOnPrimary, MotionInfo::alloc(m_xCursor, m_yCursor));
  } else {
    // motion on secondary screen.  warp mouse back to
    // center.
    warpCursor(m_xCenter, m_yCenter);

    // examine the motion.  if it's about the distance
    // from the center of the screen to an edge then
    // it's probably a bogus motion that we want to
    // ignore (see warpCursorNoFlush() for a further
    // description).
    static int32_t bogusZoneSize = 10;
    if (-x + bogusZoneSize > m_xCenter - m_x || x + bogusZoneSize > m_x + m_w - m_xCenter ||
        -y + bogusZoneSize > m_yCenter - m_y || y + bogusZoneSize > m_y + m_h - m_yCenter) {
      LOG_DEBUG("dropped bogus motion %+d,%+d", x, y);
    } else {
      // send motion
      // Accumulate together the move into the running total
      static CGFloat m_xFractionalMove = 0;
      static CGFloat m_yFractionalMove = 0;

      m_xFractionalMove += x;
      m_yFractionalMove += y;

      // Return the integer part
      int32_t intX = (int32_t)m_xFractionalMove;
      int32_t intY = (int32_t)m_yFractionalMove;

      // And keep only the fractional part
      m_xFractionalMove -= intX;
      m_yFractionalMove -= intY;
      sendEvent(EventTypes::PrimaryScreenMotionOnSecondary, MotionInfo::alloc(intX, intY));
    }
  }

  return true;
}

bool OSXScreen::onMouseButton(bool pressed, uint16_t macButton)
{
  // Buttons 2 and 3 are inverted on the mac
  ButtonID button = mapMacButtonToSynergy(macButton);

  if (pressed) {
    LOG_VERBOSE("event: button press button=%d", button);
    if (button != kButtonNone) {
      KeyModifierMask mask = m_keyState->getActiveModifiers();
      sendEvent(EventTypes::PrimaryScreenButtonDown, ButtonInfo::alloc(button, mask));
    }
  } else {
    LOG_VERBOSE("event: button release button=%d", button);
    if (button != kButtonNone) {
      KeyModifierMask mask = m_keyState->getActiveModifiers();
      sendEvent(EventTypes::PrimaryScreenButtonUp, ButtonInfo::alloc(button, mask));
    }
  }

  return true;
}

bool OSXScreen::onMouseWheel(int32_t xDelta, int32_t yDelta) const
{
  LOG_VERBOSE("event: button wheel delta=%+d,%+d", xDelta, yDelta);
  sendEvent(EventTypes::PrimaryScreenWheel, WheelInfo::alloc(xDelta, yDelta));
  return true;
}

void OSXScreen::displayReconfigurationCallback(
    CGDirectDisplayID displayID, CGDisplayChangeSummaryFlags flags, void *inUserData
)
{
  OSXScreen *screen = (OSXScreen *)inUserData;

  // Closing or opening the lid when an external monitor is
  // connected causes an kCGDisplayBeginConfigurationFlag event
  CGDisplayChangeSummaryFlags mask = kCGDisplayBeginConfigurationFlag | kCGDisplayMovedFlag | kCGDisplaySetModeFlag |
                                     kCGDisplayAddFlag | kCGDisplayRemoveFlag | kCGDisplayEnabledFlag |
                                     kCGDisplayDisabledFlag | kCGDisplayMirrorFlag | kCGDisplayUnMirrorFlag |
                                     kCGDisplayDesktopShapeChangedFlag;

  LOG_VERBOSE("event: display was reconfigured: %x %x %x", flags, mask, flags & mask);

  if (flags & mask) { /* Something actually did change */
    LOG_VERBOSE("event: screen changed shape; refreshing dimensions");
    if (!screen->updateScreenShape(displayID, flags)) {
      LOG_ERR("failed to update screen shape during display reconfiguration");
    }
  }
}

bool OSXScreen::onKey(CGEventRef event)
{
  CGEventType eventKind = CGEventGetType(event);

  // get the key and active modifiers
  uint32_t virtualKey = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
  CGEventFlags macMask = CGEventGetFlags(event);
  LOG_VERBOSE("event: Key event kind: %d, keycode=%d", eventKind, virtualKey);

  // Special handling to track state of modifiers
  if (eventKind == kCGEventFlagsChanged) {
    // get old and new modifier state
    KeyModifierMask oldMask = getActiveModifiers();
    KeyModifierMask newMask = m_keyState->mapModifiersFromOSX(macMask);
    m_keyState->handleModifierKeys(getEventTarget(), oldMask, newMask);

    // if the current set of modifiers exactly matches a modifiers-only
    // hot key then generate a hot key down event.
    if (m_activeModifierHotKey == 0) {
      if (m_modifierHotKeys.count(newMask) > 0) {
        m_activeModifierHotKey = m_modifierHotKeys[newMask];
        m_activeModifierHotKeyMask = newMask;
        m_events->addEvent(
            Event(EventTypes::PrimaryScreenHotkeyDown, getEventTarget(), HotKeyInfo::alloc(m_activeModifierHotKey))
        );
      }
    }

    // if a modifiers-only hot key is active and should no longer be
    // then generate a hot key up event.
    else if (m_activeModifierHotKey != 0) {
      KeyModifierMask mask = (newMask & m_activeModifierHotKeyMask);
      if (mask != m_activeModifierHotKeyMask) {
        m_events->addEvent(
            Event(EventTypes::PrimaryScreenHotkeyUp, getEventTarget(), HotKeyInfo::alloc(m_activeModifierHotKey))
        );
        m_activeModifierHotKey = 0;
        m_activeModifierHotKeyMask = 0;
      }
    }

    return true;
  }

  // check for hot key
  HotKeyToIDMap::const_iterator i =
      m_hotKeyToIDMap.find(HotKeyItem(virtualKey, m_keyState->mapModifiersToCarbon(macMask) & 0xff00u));
  if (i != m_hotKeyToIDMap.end()) {
    uint32_t id = i->second;

    // determine event type
    EventTypes type;
    if (eventKind == kCGEventKeyDown) {
      type = EventTypes::PrimaryScreenHotkeyDown;
    } else if (eventKind == kCGEventKeyUp) {
      type = EventTypes::PrimaryScreenHotkeyUp;
    } else {
      return false;
    }

    m_events->addEvent(Event(type, getEventTarget(), HotKeyInfo::alloc(id)));

    return true;
  }

  // decode event type
  bool down = (eventKind == kCGEventKeyDown);
  bool up = (eventKind == kCGEventKeyUp);
  bool isRepeat = (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) == 1);

  // map event to keys
  KeyModifierMask mask;
  OSXKeyState::KeyIDs keys;
  KeyButton button = m_keyState->mapKeyFromEvent(keys, &mask, event);
  if (button == 0) {
    return false;
  }

  // check for AltGr in mask.  if set we send neither the AltGr nor
  // the super modifiers to clients then remove AltGr before passing
  // the modifiers to onKey.
  KeyModifierMask sendMask = (mask & ~KeyModifierAltGr);
  if ((mask & KeyModifierAltGr) != 0) {
    sendMask &= ~KeyModifierSuper;
  }
  mask &= ~KeyModifierAltGr;

  // update button state
  if (down) {
    m_keyState->onKey(button, true, mask);
  } else if (up) {
    if (!m_keyState->isKeyDown(button)) {
      // up event for a dead key.  throw it away.
      return false;
    }
    m_keyState->onKey(button, false, mask);
  }

  // send key events
  for (OSXKeyState::KeyIDs::const_iterator i = keys.begin(); i != keys.end(); ++i) {
    m_keyState->sendKeyEvent(getEventTarget(), down, isRepeat, *i, sendMask, 1, button);
  }

  return true;
}

void OSXScreen::onMediaKey(CGEventRef event)
{
  KeyID keyID;
  bool down;
  bool isRepeat;

  if (!getMediaKeyEventInfo(event, &keyID, &down, &isRepeat)) {
    LOG_ERR("Failed to decode media key event");
    return;
  }

  LOG_VERBOSE("Media key event: keyID=0x%02x, %s, repeat=%s", keyID, (down ? "down" : "up"), (isRepeat ? "yes" : "no"));

  KeyButton button = 0;
  KeyModifierMask mask = m_keyState->getActiveModifiers();
  m_keyState->sendKeyEvent(getEventTarget(), down, isRepeat, keyID, mask, 1, button);
}

bool OSXScreen::onHotKey(EventRef event) const
{
  // get the hotkey id
  EventHotKeyID hkid;
  GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(EventHotKeyID), nullptr, &hkid);
  uint32_t id = hkid.id;

  // determine event type
  EventTypes type;
  uint32_t eventKind = GetEventKind(event);
  if (eventKind == kEventHotKeyPressed) {
    type = EventTypes::PrimaryScreenHotkeyDown;
  } else if (eventKind == kEventHotKeyReleased) {
    type = EventTypes::PrimaryScreenHotkeyUp;
  } else {
    return false;
  }

  m_events->addEvent(Event(type, getEventTarget(), HotKeyInfo::alloc(id)));

  return true;
}

// Quartz event tap support for the secondary display. This makes sure that we
// will show the cursor if a local event comes in while Synergy has the cursor
// off the screen.
CGEventRef OSXScreen::handleCGInputEventSecondary(CGEventTapProxy, CGEventType type, CGEventRef event, void *refcon)
{
  // this fix is really screwing with the correct show/hide behavior. it
  // should be tested better before reintroducing.
  return event;

  OSXScreen *screen = (OSXScreen *)refcon;
  if (screen->m_cursorHidden && type == kCGEventMouseMoved) {

    CGPoint pos = CGEventGetLocation(event);
    if (pos.x != screen->m_xCenter || pos.y != screen->m_yCenter) {

      LOG_DEBUG("show cursor on secondary, type=%d pos=%d,%d", type, pos.x, pos.y);
      screen->showCursor();
    }
  }
  return event;
}

// Quartz event tap support
CGEventRef OSXScreen::handleCGInputEvent(CGEventTapProxy, CGEventType type, CGEventRef event, void *refcon)
{
  OSXScreen *screen = (OSXScreen *)refcon;

  switch (type) {
  case kCGEventLeftMouseDown:
  case kCGEventRightMouseDown:
  case kCGEventOtherMouseDown:
    screen->onMouseButton(true, CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber) + 1);
    break;
  case kCGEventLeftMouseUp:
  case kCGEventRightMouseUp:
  case kCGEventOtherMouseUp:
    screen->onMouseButton(false, CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber) + 1);
    break;
  case kCGEventLeftMouseDragged:
  case kCGEventRightMouseDragged:
  case kCGEventOtherMouseDragged:
  case kCGEventMouseMoved:
    // we intentionally ignore the position in the event here as the events are
    // queued and will no longer be accurate when we process them.
    screen->onMouseMove();

    // The system ignores our cursor-centering calls if
    // we don't return the event. This should be harmless,
    // but might register as slight movement to other apps
    // on the system. It hasn't been a problem before, though.
    return event;
    break;
  case kCGEventScrollWheel:
    screen->onMouseWheel(
        screen->mapScrollWheelToSynergy(CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis2)),
        screen->mapScrollWheelToSynergy(CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1))
    );
    break;
  case kCGEventKeyDown:
  case kCGEventKeyUp:
  case kCGEventFlagsChanged:
    screen->onKey(event);
    break;
  case kCGEventTapDisabledByTimeout:
    // Re-enable our event-tap if we still have accessibility permissions
    if (screen->checkAXPermissions()) {
      CGEventTapEnable(screen->m_eventTapPort, true);
      LOG_INFO("quartz event tap was disabled by timeout, re-enabling");
    }
    break;
  case kCGEventTapDisabledByUserInput:
    LOG_ERR("quartz event tap was disabled by user input");
    break;
  case NX_NULLEVENT:
    break;
  default:
    if (type == NX_SYSDEFINED) {
      if (isMediaKeyEvent(event)) {
        LOG_VERBOSE("detected media key event");
        screen->onMediaKey(event);
      } else {
        LOG_VERBOSE("ignoring unknown system defined event");
        return event;
      }
      break;
    }

    LOG_VERBOSE("unknown quartz event type: 0x%02x", type);
  }

  if (screen->m_isOnScreen) {
    return event;
  } else {
    return nullptr;
  }
}

#pragma clang diagnostic pop
