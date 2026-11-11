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
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace synergy {

EiScreen::EiScreen(bool isPrimary, IEventQueue *events, bool usePortal)
    : PlatformScreen{events},
      m_isPrimary{isPrimary},
      m_events{events},
      m_w{1},
      m_h{1},
      m_isOnScreen{isPrimary}
{
  initEi();
  m_keyState = new EiKeyState(this, events);
  // install event handlers
  m_events->addHandler(EventTypes::System, m_events->getSystemTarget(), [this](const auto &e) {
    handleSystemEvent(e);
  });

#ifdef WINAPI_LIBPORTAL
  if (usePortal) {
    m_events->addHandler(EventTypes::EIConnected, getEventTarget(), [this](const auto &e) {
      handleConnectedToEisEvent(e);
    });
    if (isPrimary) {
      m_portalInputCapture = new PortalInputCapture(this, m_events);
      // Portal input capture manages its own clipboard
    } else {
      m_events->addHandler(EventTypes::EISessionClosed, getEventTarget(), [this](const auto &) {
        handlePortalSessionClosed();
      });
      m_portalRemoteDesktop = new PortalRemoteDesktop(this, m_events);
      // Create clipboard for remote desktop (secondary screen)
      m_clipboard = new EiClipboard(kClipboardClipboard);
    }
  } else {
#else
  (void)usePortal;
  {
#endif
    // Note: socket backend does not support reconnections
    auto rc = ei_setup_backend_socket(m_ei, nullptr);
    if (rc != 0) {
      LOG_ERR("ei init error: %s", strerror(-rc));
      throw std::runtime_error("failed to init ei context");
    }
    // Create clipboard for socket backend
    m_clipboard = new EiClipboard(kClipboardClipboard);
  }

  // disable sleep if the flag is set
  if (Settings::value(Settings::Core::PreventSleep).toBool()) {
    m_powerManager.disableSleep();
  }
}

EiScreen::~EiScreen()
{
  m_events->adoptBuffer(nullptr);
  m_events->removeHandler(EventTypes::System, m_events->getSystemTarget());

  cleanupEi();

  delete m_keyState;
  delete m_clipboard;

#ifdef WINAPI_LIBPORTAL
  delete m_portalRemoteDesktop;
#endif
}

void EiScreen::eiLogEvent(ei_log_priority priority, const char *message) const
{
  switch (priority) {
  case EI_LOG_PRIORITY_DEBUG:
    LOG_VERBOSE("ei: %s", message);
    break;
  case EI_LOG_PRIORITY_INFO:
    LOG_INFO("ei: %s", message);
    break;
  case EI_LOG_PRIORITY_WARNING:
    LOG_WARN("ei: %s", message);
    break;
  case EI_LOG_PRIORITY_ERROR:
    LOG_ERR("ei: %s", message);
    break;
  default:
    LOG_PRINT("ei: %s", message);
    break;
  }
}

void EiScreen::initEi()
{
  if (m_isPrimary) {
    m_ei = ei_new_receiver(nullptr); // we receive from the display server
  } else {
    m_ei = ei_new_sender(nullptr); // we send to the display server
  }
  ei_set_user_data(m_ei, this);
  ei_log_set_priority(m_ei, EI_LOG_PRIORITY_DEBUG);
  ei_log_set_handler(m_ei, handleEiLogEvent);
  std::string configName = kAppId;
  ei_configure_name(m_ei, configName.append(" client").c_str());

  // install the platform event queue
  m_events->adoptBuffer(nullptr);
  m_events->adoptBuffer(new EiEventQueueBuffer(m_ei, m_events));
}

void EiScreen::cleanupEi()
{
  if (m_eiPointer) {
    free(ei_device_get_user_data(m_eiPointer));
    ei_device_set_user_data(m_eiPointer, nullptr);
    m_eiPointer = ei_device_unref(m_eiPointer);
  }
  if (m_eiKeyboard) {
    free(ei_device_get_user_data(m_eiKeyboard));
    ei_device_set_user_data(m_eiKeyboard, nullptr);
    m_eiKeyboard = ei_device_unref(m_eiKeyboard);
  }
  if (m_eiAbs) {
    free(ei_device_get_user_data(m_eiAbs));
    ei_device_set_user_data(m_eiAbs, nullptr);
    m_eiAbs = ei_device_unref(m_eiAbs);
  }
  m_eiSeat = ei_seat_unref(m_eiSeat);
  for (auto it = m_eiDevices.begin(); it != m_eiDevices.end(); it++) {
    free(ei_device_get_user_data(*it));
    ei_device_set_user_data(*it, nullptr);
    ei_device_unref(*it);
  }
  m_eiDevices.clear();
  m_ei = ei_unref(m_ei);
}

void *EiScreen::getEventTarget() const
{
  return const_cast<void *>(static_cast<const void *>(this));
}

bool EiScreen::getClipboard(ClipboardID id, IClipboard *clipboard) const
{
#ifdef WINAPI_LIBPORTAL
  // If using portal input capture, get clipboard from there
  if (m_portalInputCapture) {
    const auto sourceClipboard = m_portalInputCapture->getClipboard(id);
    if (!sourceClipboard) {
      return false;
    }
    return IClipboard::copy(clipboard, sourceClipboard);
  }
#endif

  // Otherwise use our own clipboard
  if (!m_clipboard) {
    return false;
  }

  return IClipboard::copy(clipboard, m_clipboard);
}

void EiScreen::getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const
{
  x = m_x;
  y = m_y;
  w = m_w;
  h = m_h;
}

void EiScreen::getCursorPos(int32_t &x, int32_t &y) const
{
  x = m_cursorX;
  y = m_cursorY;
}

void EiScreen::reconfigure(uint32_t activeSides)
{
  const static auto sidesText = sidesMaskToString(activeSides);
  LOG_DEBUG("active sides: %s (0x%02x)", sidesText.c_str(), activeSides);
  m_activeSides = activeSides;
}

std::uint32_t EiScreen::activeSides()
{
  return m_activeSides;
}

void EiScreen::warpCursor(int32_t x, int32_t y)
{
  m_cursorX = x;
  m_cursorY = y;
}

std::uint32_t EiScreen::registerHotKey(KeyID key, KeyModifierMask mask)
{
  static std::uint32_t next_id;
  std::uint32_t id = std::min(++next_id, 1u);

  // Bug: id rollover means duplicate hotkey ids. Oh well.

  auto set = m_hotkeys.find(key);
  if (set == m_hotkeys.end()) {
    m_hotkeys.try_emplace(key, HotKeySet{key});
    set = m_hotkeys.find(key);
  }
  set->second.addItem(HotKeyItem(mask, id));

  return id;
}

void EiScreen::unregisterHotKey(uint32_t id)
{
  for (auto &[key, set] : m_hotkeys) {
    (void)key;
    if (set.removeById(id)) {
      break;
    }
  }
}

void EiScreen::fakeInputBegin()
{
  // FIXME -- not implemented
}

void EiScreen::fakeInputEnd()
{
  // FIXME -- not implemented
}

std::int32_t EiScreen::getJumpZoneSize() const
{
  return 1;
}

bool EiScreen::isAnyMouseButtonDown(uint32_t &) const
{
  return false;
}

void EiScreen::getCursorCenter(int32_t &x, int32_t &y) const
{
  x = m_x + m_w / 2;
  y = m_y + m_h / 2;
}

void EiScreen::fakeMouseButton(ButtonID button, bool press)
{
  uint32_t code;

  if (!m_eiPointer)
    return;

  switch (button) {
  case kButtonLeft:
    code = 0x110; // BTN_LEFT
    break;
  case kButtonMiddle:
    code = 0x112; // BTN_MIDDLE
    break;
  case kButtonRight:
    code = 0x111; // BTN_RIGHT
    break;
  default:
    code = 0x110 + (button - 1);
    break;
  }

  ei_device_button_button(m_eiPointer, code, press);
  ei_device_frame(m_eiPointer, ei_now(m_ei));
}

void EiScreen::fakeMouseMove(int32_t x, int32_t y)
{
  // We get one motion event before enter() with the target position
  if (!m_isOnScreen) {
    m_cursorX = x;
    m_cursorY = y;
    return;
  }

  if (!m_eiAbs)
    return;

  ei_device_pointer_motion_absolute(m_eiAbs, x, y);
  ei_device_frame(m_eiAbs, ei_now(m_ei));
}

void EiScreen::fakeMouseRelativeMove(int32_t dx, int32_t dy) const
{
  if (!m_eiPointer)
    return;

  ei_device_pointer_motion(m_eiPointer, dx, dy);
  ei_device_frame(m_eiPointer, ei_now(m_ei));
}

void EiScreen::fakeMouseWheel(ScrollDelta delta) const
{
  if (!m_eiPointer)
    return;

  delta = applyScrollModifier(delta);
  // libei and Synergy seem to use opposite directions, so we have
  // to send EI the opposite of the value received if we want to remain
  // compatible with other platforms (including X11).
  ei_device_scroll_discrete(m_eiPointer, -delta.x, -delta.y);
  ei_device_frame(m_eiPointer, ei_now(m_ei));
}

void EiScreen::fakeKey(uint32_t keycode, bool isDown) const
{
  if (!m_eiKeyboard)
    return;

  auto xkbKeycode = keycode + 8;
  m_keyState->updateXkbState(xkbKeycode, isDown);
  ei_device_keyboard_key(m_eiKeyboard, keycode, isDown);
  ei_device_frame(m_eiKeyboard, ei_now(m_ei));
}

void EiScreen::enable()
{
  // Nothing really to be done here
  // Portal-based clipboard gets notifications via events
  // Socket-based clipboard is passive (no monitoring needed)
}

void EiScreen::disable()
{
  // Nothing to do here
  // Portal-based clipboard gets notifications via events
  // Socket-based clipboard is passive (no monitoring needed)
}

void EiScreen::enter()
{
  m_isOnScreen = true;
  if (!m_isPrimary) {
    ++m_sequenceNumber;
    if (m_eiPointer) {
      ei_device_start_emulating(m_eiPointer, m_sequenceNumber);
    }
    if (m_eiKeyboard) {
      ei_device_start_emulating(m_eiKeyboard, m_sequenceNumber);
    }
    if (m_eiAbs) {
      ei_device_start_emulating(m_eiAbs, m_sequenceNumber);
      fakeMouseMove(m_cursorX, m_cursorY);
    }
  } else {
    LOG_DEBUG("releasing input capture at x=%i y=%i", m_cursorX, m_cursorY);
#ifdef WINAPI_LIBPORTAL
    if (m_portalInputCapture) {
      m_portalInputCapture->release(m_cursorX, m_cursorY);
    }
#endif
  }
}

bool EiScreen::canLeave()
{
  return true;
}

void EiScreen::leave()
{
  if (!m_isPrimary) {
    if (m_eiPointer) {
      ei_device_stop_emulating(m_eiPointer);
    }
    if (m_eiKeyboard) {
      ei_device_stop_emulating(m_eiKeyboard);
    }
    if (m_eiAbs) {
      ei_device_stop_emulating(m_eiAbs);
    }
  }

  m_isOnScreen = false;
}

bool EiScreen::setClipboard(ClipboardID id, const IClipboard *clipboard)
{
  if (!clipboard) {
    return false;
  }

#ifdef WINAPI_LIBPORTAL
  // If using portal input capture, set clipboard there
  if (m_portalInputCapture) {
    IClipboard *targetClipboard = m_portalInputCapture->getClipboard(id);
    if (!targetClipboard) {
      return false;
    }
    return IClipboard::copy(targetClipboard, clipboard);
  }
#endif

  // Otherwise use our own clipboard
  if (!m_clipboard) {
    return false;
  }

  bool ok = IClipboard::copy(m_clipboard, clipboard);

#ifdef WINAPI_LIBPORTAL
  if (ok && m_portalRemoteDesktop && id == kClipboardClipboard) {
    m_portalRemoteDesktop->claimClipboard();
  }
#endif

  return ok;
}

void EiScreen::checkClipboards()
{
  // For portal-based input capture, clipboard changes come via portal events
  // For socket-based, clipboard is passive and changes are sent explicitly
  // Nothing to do here
}

void EiScreen::openScreensaver(bool /*notify*/)
{
  // FIXME
}

void EiScreen::closeScreensaver()
{
  // FIXME
}

void EiScreen::screensaver(bool /*activate*/)
{
  // FIXME
}

void EiScreen::resetOptions()
{
  // Should reset options to neutral, see setOptions().
  // We don't have ei-specific options, nothing to do here
}

void EiScreen::setOptions(const OptionsList &options)
{
  for (auto it = options.begin(); it != options.end(); ++it) {
    if (*it == kOptionClipboardSharingSize) {
      ++it;
      if (it == options.end())
        break;
      m_maximumClipboardSize = *it;
      LOG_DEBUG("ei screen received clipboard size limit: %zu KB", m_maximumClipboardSize);
    }
  }
}

bool EiScreen::isPrimary() const
{
  return m_isPrimary;
}

void EiScreen::updateShape()
{
  m_w = 1;
  m_h = 1;
  m_x = std::numeric_limits<uint32_t>::max();
  m_y = std::numeric_limits<uint32_t>::max();
  for (auto it = m_eiDevices.begin(); it != m_eiDevices.end(); it++) {
    auto idx = 0;
    struct ei_region *r;
    while ((r = ei_device_get_region(*it, idx++)) != nullptr) {
      m_x = std::min(ei_region_get_x(r), m_x);
      m_y = std::min(ei_region_get_y(r), m_y);
      m_w = std::max(ei_region_get_x(r) + ei_region_get_width(r), m_w);
      m_h = std::max(ei_region_get_y(r) + ei_region_get_height(r), m_h);
    }
  }

  LOG_DEBUG("logical output size: %dx%d@%d.%d", m_w, m_h, m_x, m_y);
  m_cursorX = m_x + m_w / 2;
  m_cursorY = m_y + m_h / 2;

  sendEvent(EventTypes::ScreenShapeChanged, nullptr);
}

void EiScreen::addDevice(struct ei_device *device)
{
  LOG_DEBUG("adding device %s", ei_device_get_name(device));

  // Noteworthy: EI in principle supports multiple devices with multiple
  // capabilities, so there may be more than one logical pointer (or even
  // multiple seats). Supporting this is ... tricky so for now we go the easy
  // route: one device for each capability. Note this may be the same device
  // if the first device comes with multiple capabilities.

  if (!m_eiPointer && ei_device_has_capability(device, EI_DEVICE_CAP_POINTER) &&
      ei_device_has_capability(device, EI_DEVICE_CAP_BUTTON) &&
      ei_device_has_capability(device, EI_DEVICE_CAP_SCROLL)) {
    m_eiPointer = ei_device_ref(device);
  }

  if (!m_eiKeyboard && ei_device_has_capability(device, EI_DEVICE_CAP_KEYBOARD)) {
    m_eiKeyboard = ei_device_ref(device);

    if (auto keymap = ei_device_keyboard_get_keymap(device);
        keymap && ei_keymap_get_type(keymap) == EI_KEYMAP_TYPE_XKB) {
      int fd = ei_keymap_get_fd(keymap);
      size_t len = ei_keymap_get_size(keymap);
      m_keyState->init(fd, len);
    } else {
      // We rely on the EIS implementation to give us a keymap, otherwise we
      // really have no idea what a keycode means (other than it's linux/input.h
      // code) Where the EIS implementation does not tell us, we just default to
      // whatever libxkbcommon thinks is default. At least this way we can
      // influence with env vars what we get
      LOG_WARN("keyboard device %s does not have a keymap, we are guessing", ei_device_get_name(device));
      m_keyState->initDefaultKeymap();
    }
    m_keyState->updateKeyMap();
  }

  if (!m_eiAbs && ei_device_has_capability(device, EI_DEVICE_CAP_POINTER_ABSOLUTE) &&
      ei_device_has_capability(device, EI_DEVICE_CAP_BUTTON) &&
      ei_device_has_capability(device, EI_DEVICE_CAP_SCROLL)) {
    m_eiAbs = ei_device_ref(device);
  }

  m_eiDevices.emplace_back(ei_device_ref(device));

  updateShape();
}

void EiScreen::removeDevice(struct ei_device *device)
{
  LOG_DEBUG("removing device %s", ei_device_get_name(device));

  if (device == m_eiPointer)
    m_eiPointer = ei_device_unref(m_eiPointer);
  if (device == m_eiKeyboard)
    m_eiKeyboard = ei_device_unref(m_eiKeyboard);
  if (device == m_eiAbs)
    m_eiAbs = ei_device_unref(m_eiAbs);

  for (auto it = m_eiDevices.begin(); it != m_eiDevices.end(); it++) {
    if (*it == device) {
      m_eiDevices.erase(it);
      ei_device_unref(device);
      break;
    }
  }

  updateShape();
}

void EiScreen::sendEvent(EventTypes type, void *data)
{
  m_events->addEvent(Event(type, getEventTarget(), data));
}

void EiScreen::sendClipboardEvent(EventTypes type, ClipboardID id) const
{
  auto *info = static_cast<ClipboardInfo *>(malloc(sizeof(ClipboardInfo)));
  if (info == nullptr) {
    LOG_ERR("malloc failed for ClipboardInfo");
    return;
  }
  info->m_id = id;
  info->m_sequenceNumber = m_sequenceNumber;

  // Use const_cast to call non-const sendEvent from const method
  const_cast<EiScreen *>(this)->sendEvent(type, info);
}

void EiScreen::setSequenceNumber(uint32_t seqNum)
{
  m_sequenceNumber = seqNum;
}

void EiScreen::updateButtons()
{
  // libei relies on the EIS implementation to keep our button count correct,
  // so there's not much we need to/can do here.
}

IKeyState *EiScreen::getKeyState() const
{
  return m_keyState;
}

std::string EiScreen::getSecureInputApp() const
{
  // Secure input is a macOS-only concept; like X11 and Windows, report none.
  return "";
}

EiScreen::HotKeyItem::HotKeyItem(std::uint32_t mask, std::uint32_t id) : mask(mask), id(id)
{
  // Todo: Implement
}

EiScreen::HotKeySet::HotKeySet(KeyID key) : m_id(key)
{
  // Todo: Implement
}

bool EiScreen::HotKeySet::removeById(std::uint32_t id)
{
  for (auto it = m_set.begin(); it != m_set.end(); ++it) {
    if (it->id == id) {
      m_set.erase(it);
      return true;
    }
  }
  return false;
}

void EiScreen::HotKeySet::addItem(HotKeyItem item)
{
  m_set.push_back(item);
}

std::uint32_t EiScreen::HotKeySet::findByMask(std::uint32_t mask) const
{
  for (const auto &item : m_set) {
    if (item.mask == mask) {
      return item.id;
    }
  }
  return 0;
}

} // namespace synergy
