/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2024, 2026 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2022, 2026 Red Hat, Inc.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "EiScreen.h"

#include "App.h"
#include "EiClipboard.h"
#include "EiEventQueueBuffer.h"
#include "EiKeyState.h"
#include "IScreen.h"
#include "OptionTypes.h"
#ifdef WINAPI_LIBPORTAL
#include "PortalInputCapture.h"
#include "PortalRemoteDesktop.h"
#endif
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "common/Constants.h"
#include "common/Settings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

// Values are in pixels
struct ScrollRemainder
{
  double x;
  double y;
};

namespace synergy {

ButtonID EiScreen::mapButtonFromEvdev(ei_event *event) const
{
  switch (ei_event_button_get_button(event)) {
  case 0x110:
    return kButtonLeft;
  case 0x111:
    return kButtonRight;
  case 0x112:
    return kButtonMiddle;
  case 0x113:
    return kButtonExtra0;
  case 0x114:
    return kButtonExtra1;
  default:
    return kButtonNone;
  }

  return kButtonNone;
}

bool EiScreen::onHotkey(KeyID keyid, bool isPressed, KeyModifierMask mask)
{
  auto it = m_hotkeys.find(keyid);

  if (it == m_hotkeys.end()) {
    return false;
  }

  // Note: our mask (see on_key_event) only contains some modifiers
  // but we don't put a limitation on modifiers in the hotkeys. So some
  // key combinations may not work correctly, more effort is needed here.
  if (auto id = it->second.findByMask(mask); id != 0) {
    EventTypes type = isPressed ? EventTypes::PrimaryScreenHotkeyDown : EventTypes::PrimaryScreenHotkeyUp;
    sendEvent(type, HotKeyInfo::alloc(id));
    return true;
  }

  return false;
}

void EiScreen::onKeyEvent(ei_event *event)
{
  auto keycode = ei_event_keyboard_get_key(event);
  uint32_t keyval = keycode + 8;
  bool pressed = ei_event_keyboard_get_key_is_press(event);
  KeyID keyid = m_keyState->mapKeyFromKeyval(keyval);
  auto keybutton = static_cast<KeyButton>(keyval);
  bool repeat;

  m_keyState->updateXkbState(keyval, pressed);
  KeyModifierMask mask = m_keyState->pollActiveModifiers();

  repeat = pressed && m_lastPressed == keyid && keyid != kKeyNone;

  m_lastPressed = pressed ? keyid : kKeyNone;

  LOG_VERBOSE(
      "event: key %s%s keycode=%d keyid=%d mask=0x%x", pressed ? "press" : "release", repeat ? " (repeat)" : "",
      keycode, keyid, mask
  );

  if (m_isPrimary && onHotkey(keyid, pressed, mask)) {
    return;
  }

  if (keyid != kKeyNone) {
    m_keyState->sendKeyEvent(getEventTarget(), pressed, repeat, keyid, mask, 1, keybutton);
  }
}

void EiScreen::onButtonEvent(ei_event *event)
{
  assert(m_isPrimary);

  auto buttonID = mapButtonFromEvdev(event);
  bool pressed = ei_event_button_get_is_press(event);
  KeyModifierMask mask = m_keyState->pollActiveModifiers();

  LOG_VERBOSE("event: button %s button=%d mask=0x%x", pressed ? "press" : "release", buttonID, mask);

  if (buttonID == kButtonNone) {
    LOG_DEBUG("event: button not recognized");
    return;
  }

  auto eventType = pressed ? EventTypes::PrimaryScreenButtonDown : EventTypes::PrimaryScreenButtonUp;

  sendEvent(eventType, ButtonInfo::alloc(buttonID, mask));
}

void EiScreen::onPointerScrollEvent(ei_event *event)
{
  // Ratio of 10 pixels == one wheel click because that's what mutter/gtk
  // use (for historical reasons).
  static const int s_pixelsPerWheelClick = 10;
  // Our logical wheel clicks are multiples 120, so we
  // convert between the two and keep the remainders because
  // we will very likely get subpixel scroll events.
  // This means a single pixel is 120/s_pixelToWheelRation in wheel values.
  const int s_pixelToWheelRatio = s_scrollDelta / s_pixelsPerWheelClick;

  assert(m_isPrimary);

  auto dx = ei_event_scroll_get_dx(event);
  auto dy = ei_event_scroll_get_dy(event);
  struct ei_device *device = ei_event_get_device(event);

  LOG_VERBOSE("event: scroll (%.2f, %.2f)", dx, dy);

  auto *remainder = static_cast<struct ScrollRemainder *>(ei_device_get_user_data(device));
  if (!remainder) {
    remainder = new ScrollRemainder();
    ei_device_set_user_data(device, remainder);
  }

  dx += remainder->x;
  dy += remainder->y;

  double x;
  double y;
  double rx = modf(dx, &x);
  double ry = modf(dy, &y);

  assert(!std::isnan(x) && !std::isinf(x));
  assert(!std::isnan(y) && !std::isinf(y));

  // libei and Synergy seem to use opposite directions, so we have
  // to send the opposite of the value reported by EI if we want to
  // remain compatible with other platforms (including X11).
  if (x != 0 || y != 0)
    sendEvent(
        EventTypes::PrimaryScreenWheel,
        WheelInfo::alloc((int32_t)-x * s_pixelToWheelRatio, (int32_t)-y * s_pixelToWheelRatio)
    );

  remainder->x = rx;
  remainder->y = ry;
}

void EiScreen::onPointerScrollDiscreteEvent(ei_event *event)
{
  // both libei and Synergy use multiples of 120 to represent
  // one scroll wheel click event so we can just forward things
  // as-is.

  assert(m_isPrimary);

  auto dx = ei_event_scroll_get_discrete_dx(event);
  auto dy = ei_event_scroll_get_discrete_dy(event);

  LOG_VERBOSE("event: scroll discrete (%d, %d)", dx, dy);

  // libei and Synergy seem to use opposite directions, so we have
  // to send the opposite of the value reported by EI if we want to
  // remain compatible with other platforms (including X11).
  sendEvent(EventTypes::PrimaryScreenWheel, WheelInfo::alloc(-dx, -dy));
}

void EiScreen::onMotionEvent(ei_event *event)
{
  assert(m_isPrimary);

  auto dx = ei_event_pointer_get_dx(event);
  auto dy = ei_event_pointer_get_dy(event);

  if (m_isOnScreen) {
    LOG_DEBUG("event: motion on primary x=%i y=%i)", m_cursorX, m_cursorY);
    sendEvent(EventTypes::PrimaryScreenMotionOnPrimary, MotionInfo::alloc(m_cursorX, m_cursorY));
#ifdef WINAPI_LIBPORTAL
    if (m_portalInputCapture && m_portalInputCapture->isActive()) {
      m_portalInputCapture->release();
    }
#endif
  } else {
    m_bufferDX += dx;
    m_bufferDY += dy;
    auto pixelDx = static_cast<std::int32_t>(m_bufferDX);
    auto pixelDy = static_cast<std::int32_t>(m_bufferDY);
    if (pixelDx || pixelDy) {
      LOG_VERBOSE("event: motion on secondary x=%d y=%d", pixelDx, pixelDy);
      sendEvent(EventTypes::PrimaryScreenMotionOnSecondary, MotionInfo::alloc(pixelDx, pixelDy));
      m_bufferDX -= pixelDx;
      m_bufferDY -= pixelDy;
    }
  }
}

void EiScreen::onAbsMotionEvent(const ei_event *) const
{
  assert(m_isPrimary);
}

void EiScreen::handleConnectedToEisEvent(const Event &event)
{
  int fd = static_cast<EiConnectInfo *>(event.getData())->m_fd;
  LOG_DEBUG("eis connection established, fd=%d", fd);

  auto rc = ei_setup_backend_fd(m_ei, fd);
  if (rc != 0) {
    LOG_WARN("failed to set up ei: %s", strerror(-rc));
  }
}

void EiScreen::handlePortalSessionClosed()
{
  // Portal may or may not EI_EVENT_DISCONNECT us before sending the DBus Closed
  // signal. Let's clean up either way.
  LOG_DEBUG("eis screen handling portal session closed");
  cleanupEi();
  initEi();
}

void EiScreen::handleSystemEvent(const Event &)
{
  std::scoped_lock lock{m_mutex};

  // Only one ei_dispatch per system event, see the comment in
  // EiEventQueueBuffer::addEvent
  ei_dispatch(m_ei);
  struct ei_event *event;

  while ((event = ei_get_event(m_ei)) != nullptr) {
    auto type = ei_event_get_type(event);
    auto seat = ei_event_get_seat(event);
    auto device = ei_event_get_device(event);

    switch (type) {
    case EI_EVENT_CONNECT:
      LOG_DEBUG("connected to eis");
      break;
    case EI_EVENT_SEAT_ADDED:
      if (!m_eiSeat) {
        m_eiSeat = ei_seat_ref(seat);
        ei_seat_bind_capabilities(
            m_eiSeat, EI_DEVICE_CAP_POINTER, EI_DEVICE_CAP_POINTER_ABSOLUTE, EI_DEVICE_CAP_KEYBOARD,
            EI_DEVICE_CAP_BUTTON, EI_DEVICE_CAP_SCROLL, nullptr
        );
        LOG_DEBUG("ei: using seat %s", ei_seat_get_name(m_eiSeat));
        // we don't care about touch
      }
      break;
    case EI_EVENT_DEVICE_ADDED:
      if (seat == m_eiSeat) {
        addDevice(device);
      } else {
        LOG_INFO("seat %s is ignored", ei_seat_get_name(m_eiSeat));
      }
      break;
    case EI_EVENT_DEVICE_REMOVED:
      removeDevice(device);
      break;
    case EI_EVENT_SEAT_REMOVED:
      if (seat == m_eiSeat) {
        m_eiSeat = ei_seat_unref(m_eiSeat);
      }
      break;
    case EI_EVENT_DISCONNECT:
      // We're using libei which emulates the various seat/device remove events
      // so by the time we get here our EiScreen should be in a neutral state.
      //
      // We must release the xdg-portal InputCapture in case it is still active
      // so that the cursor is usable and not stuck on the Synergy server.
      LOG_WARN("disconnected from eis, will afterwards commence attempt to reconnect");
      if (m_isPrimary) {
        LOG_DEBUG("re-allocating portal input capture connection and releasing active captures");
#ifdef WINAPI_LIBPORTAL
        if (m_portalInputCapture) {
          if (m_portalInputCapture->isActive()) {
            m_portalInputCapture->release();
          }
          delete m_portalInputCapture;
          m_portalInputCapture = new PortalInputCapture(this, this->m_events);
        }
#endif
      }
      this->handlePortalSessionClosed();
      break;
    case EI_EVENT_DEVICE_PAUSED:
      LOG_DEBUG("device %s is paused", ei_device_get_name(device));
      break;
    case EI_EVENT_DEVICE_RESUMED:
      LOG_DEBUG("device %s is resumed", ei_device_get_name(device));
      if (!m_isPrimary && m_isOnScreen) {
        ei_device_start_emulating(device, ++m_sequenceNumber);
      }
      break;
    case EI_EVENT_KEYBOARD_MODIFIERS:
      // FIXME
      break;

    // events below are for a receiver context (barriers)
    case EI_EVENT_FRAME:
      break;
    case EI_EVENT_DEVICE_START_EMULATING:
      LOG_DEBUG("device %s started emulating", ei_device_get_name(device));
      break;
    case EI_EVENT_DEVICE_STOP_EMULATING:
      LOG_DEBUG("device %s stopped emulating", ei_device_get_name(device));
      break;
    case EI_EVENT_KEYBOARD_KEY:
      onKeyEvent(event);
      break;
    case EI_EVENT_BUTTON_BUTTON:
      onButtonEvent(event);
      break;
    case EI_EVENT_POINTER_MOTION:
      onMotionEvent(event);
      break;
    case EI_EVENT_POINTER_MOTION_ABSOLUTE:
      onAbsMotionEvent(event);
      break;
    case EI_EVENT_TOUCH_UP:
      break;
    case EI_EVENT_TOUCH_MOTION:
      break;
    case EI_EVENT_TOUCH_DOWN:
      break;
    case EI_EVENT_SCROLL_DELTA:
      onPointerScrollEvent(event);
      break;
    case EI_EVENT_SCROLL_DISCRETE:
      onPointerScrollDiscreteEvent(event);
      break;
    case EI_EVENT_SCROLL_STOP:
    case EI_EVENT_SCROLL_CANCEL:
      break;
    default:
      break;
    }
    ei_event_unref(event);
  }
}

} // namespace synergy
