/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy.h"
#include "client/ServerProxyRead.h"

#include "Clipboard.h"
#include "ClipboardChunk.h"
#include "OptionTypes.h"
#include "ProtocolTypes.h"
#include "ProtocolUtil.h"
#include "StreamChunker.h"
#include "SynergyException.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "client/Client.h"
#include "io/IStream.h"
#include "ipc/CoreIpc.h"

#include <QString>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

//
// ServerProxy
//

KeyID ServerProxy::translateKey(KeyID id) const
{
  static const KeyID s_translationTable[kKeyModifierIDLast][2] = {
      {kKeyNone, kKeyNone},     {kKeyShift_L, kKeyShift_R}, {kKeyControl_L, kKeyControl_R}, {kKeyAlt_L, kKeyAlt_R},
      {kKeyMeta_L, kKeyMeta_R}, {kKeySuper_L, kKeySuper_R}, {kKeyAltGr, kKeyAltGr}
  };

  KeyModifierID id2 = kKeyModifierIDNull;
  uint32_t side = 0;
  switch (id) {
  case kKeyShift_L:
    id2 = kKeyModifierIDShift;
    side = 0;
    break;

  case kKeyShift_R:
    id2 = kKeyModifierIDShift;
    side = 1;
    break;

  case kKeyControl_L:
    id2 = kKeyModifierIDControl;
    side = 0;
    break;

  case kKeyControl_R:
    id2 = kKeyModifierIDControl;
    side = 1;
    break;

  case kKeyAlt_L:
    id2 = kKeyModifierIDAlt;
    side = 0;
    break;

  case kKeyAlt_R:
    id2 = kKeyModifierIDAlt;
    side = 1;
    break;

  case kKeyAltGr:
    id2 = kKeyModifierIDAltGr;
    side = 1; // there is only one alt gr key on the right side
    break;

  case kKeyMeta_L:
    id2 = kKeyModifierIDMeta;
    side = 0;
    break;

  case kKeyMeta_R:
    id2 = kKeyModifierIDMeta;
    side = 1;
    break;

  case kKeySuper_L:
    id2 = kKeyModifierIDSuper;
    side = 0;
    break;

  case kKeySuper_R:
    id2 = kKeyModifierIDSuper;
    side = 1;
    break;

  default:
    break;
  }

  if (id2 != kKeyModifierIDNull) {
    return s_translationTable[m_modifierTranslationTable[id2]][side];
  } else {
    return id;
  }
}

KeyModifierMask ServerProxy::translateModifierMask(KeyModifierMask mask) const
{
  static const KeyModifierMask s_masks[kKeyModifierIDLast] = {0x0000,          KeyModifierShift, KeyModifierControl,
                                                              KeyModifierAlt,  KeyModifierMeta,  KeyModifierSuper,
                                                              KeyModifierAltGr};

  KeyModifierMask newMask = mask & ~(KeyModifierShift | KeyModifierControl | KeyModifierAlt | KeyModifierMeta |
                                     KeyModifierSuper | KeyModifierAltGr);
  if ((mask & KeyModifierShift) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDShift]];
  }
  if ((mask & KeyModifierControl) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDControl]];
  }
  if ((mask & KeyModifierAlt) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDAlt]];
  }
  if ((mask & KeyModifierAltGr) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDAltGr]];
  }
  if ((mask & KeyModifierMeta) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDMeta]];
  }
  if ((mask & KeyModifierSuper) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDSuper]];
  }
  return newMask;
}

void ServerProxy::keyDown(uint16_t id, uint16_t mask, uint16_t button, const std::string &lang)
{
  // get mouse up to date
  flushCompressedMouse();
  setActiveServerLanguage(lang);

  // Validate before the event reaches the platform layer: the server is a trust
  // boundary, so treat what it sends as untrusted. A malformed mask is cleared
  // rather than dropped - dropping would lose input. Rate limiting does drop,
  // because accepting unbounded events lets a hostile peer flood the local
  // machine; key releases are deliberately exempt (see keyUp).
  auto cleanMask = static_cast<uint16_t>(InputValidator::sanitizeModifierMask(mask));
  if (cleanMask != mask) {
    LOG_WARN("cleared undefined modifier bits in key down mask 0x%04x -> 0x%04x", mask, cleanMask);
  }

  if (m_inputValidator.isRateLimited(static_cast<KeyID>(id), std::chrono::steady_clock::now())) {
    return;
  }

  // Opt-in interception, checked after sanitizing so the mask tested is the one
  // that would actually be applied. Nothing is configured by default.
  if (m_inputValidator.isBlockedCombination(static_cast<KeyID>(id), static_cast<KeyModifierMask>(cleanMask))) {
    LOG_INFO("key down id=0x%04x mask=0x%04x blocked by configuration", id, cleanMask);
    return;
  }

  // translate
  KeyID id2 = translateKey(static_cast<KeyID>(id));
  KeyModifierMask mask2 = translateModifierMask(static_cast<KeyModifierMask>(cleanMask));
  if (id2 != static_cast<KeyID>(id) || mask2 != static_cast<KeyModifierMask>(cleanMask))
    LOG_VERBOSE("key down translated to id=0x%08x, mask=0x%04x", id2, mask2);

  // forward
  m_client->keyDown(id2, mask2, button, lang);
}

bool ServerProxy::keyRepeat()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  uint16_t id = 0;
  uint16_t mask = 0;
  uint16_t count = 0;
  uint16_t button = 0;
  std::string lang;
  if (!readOrDisconnect(kMsgDKeyRepeat + 4, &id, &mask, &count, &button, &lang)) {
    return false;
  }
  LOG(
      (CLOG_VERBOSE "recv key repeat id=0x%08x, mask=0x%04x, count=%d, "
                    "button=0x%04x, lang=\"%s\"",
       id, mask, count, button, lang.c_str())
  );

  // Sanitize and rate limit as for key down.
  auto cleanMask = static_cast<uint16_t>(InputValidator::sanitizeModifierMask(mask));
  if (cleanMask != mask) {
    LOG_WARN("cleared undefined modifier bits in key repeat mask 0x%04x -> 0x%04x", mask, cleanMask);
  }

  if (m_inputValidator.isRateLimited(static_cast<KeyID>(id), std::chrono::steady_clock::now())) {
    return true;
  }

  // Blocked repeats are dropped for the same reason as blocked presses: the key
  // never reached the local machine, so repeating it would be inconsistent.
  if (m_inputValidator.isBlockedCombination(static_cast<KeyID>(id), static_cast<KeyModifierMask>(cleanMask))) {
    LOG_INFO("key repeat id=0x%04x mask=0x%04x blocked by configuration", id, cleanMask);
    return true;
  }

  // translate
  KeyID id2 = translateKey(static_cast<KeyID>(id));
  KeyModifierMask mask2 = translateModifierMask(static_cast<KeyModifierMask>(cleanMask));
  if (id2 != static_cast<KeyID>(id) || mask2 != static_cast<KeyModifierMask>(cleanMask))
    LOG_VERBOSE("key repeat translated to id=0x%08x, mask=0x%04x", id2, mask2);

  // forward
  m_client->keyRepeat(id2, mask2, count, button, lang);
  return true;
}

bool ServerProxy::keyUp()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  uint16_t id = 0;
  uint16_t mask = 0;
  uint16_t button = 0;
  if (!readOrDisconnect(kMsgDKeyUp + 4, &id, &mask, &button)) {
    return false;
  }
  LOG_VERBOSE("recv key up id=0x%08x, mask=0x%04x, button=0x%04x", id, mask, button);

  // Sanitize, but never rate limit a key release: swallowing it would leave the
  // local machine believing the key is still held down.
  auto cleanMask = static_cast<uint16_t>(InputValidator::sanitizeModifierMask(mask));
  if (cleanMask != mask) {
    LOG_WARN("cleared undefined modifier bits in key up mask 0x%04x -> 0x%04x", mask, cleanMask);
  }

  // translate
  KeyID id2 = translateKey(static_cast<KeyID>(id));
  KeyModifierMask mask2 = translateModifierMask(static_cast<KeyModifierMask>(cleanMask));
  if (id2 != static_cast<KeyID>(id) || mask2 != static_cast<KeyModifierMask>(cleanMask))
    LOG_VERBOSE("key up translated to id=0x%08x, mask=0x%04x", id2, mask2);

  // forward
  m_client->keyUp(id2, mask2, button);
  return true;
}

bool ServerProxy::mouseDown()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  int8_t id = 0;
  if (!readOrDisconnect(kMsgDMouseDown + 4, &id)) {
    return false;
  }
  LOG_VERBOSE("recv mouse down id=%d", id);

  // Logged, not rejected: the wire format allows ids from devices with more
  // buttons than we enumerate, and MouseTypes.h shows the legal set is not a
  // range (kButtonNone is 0; the X11 scroll wheel uses 254 and 255).
  const auto button = static_cast<ButtonID>(id);
  if (!InputValidator::isKnownButtonId(button)) {
    LOG_VERBOSE("mouse down with unrecognised button id=%d", id);
  }

  // forward
  m_client->mouseDown(button);
  return true;
}

bool ServerProxy::mouseUp()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  int8_t id = 0;
  if (!readOrDisconnect(kMsgDMouseUp + 4, &id)) {
    return false;
  }
  LOG_VERBOSE("recv mouse up id=%d", id);

  // Logged, not rejected - see mouseDown.
  const auto button = static_cast<ButtonID>(id);
  if (!InputValidator::isKnownButtonId(button)) {
    LOG_VERBOSE("mouse up with unrecognised button id=%d", id);
  }

  // forward
  m_client->mouseUp(button);
  return true;
}

bool ServerProxy::mouseMove()
{
  // parse
  bool ignore;
  int16_t x = 0;
  int16_t y = 0;
  if (!readOrDisconnect(kMsgDMouseMove + 4, &x, &y)) {
    return false;
  }

  // note if we should ignore the move
  ignore = m_ignoreMouse;

  // compress mouse motion events if more input follows
  if (!ignore && !m_compressMouse && m_stream->isReady()) {
    m_compressMouse = true;
  }

  // if compressing then ignore the motion but record it
  if (m_compressMouse) {
    m_compressMouseRelative = false;
    ignore = true;
    m_xMouse = x;
    m_yMouse = y;
    m_dxMouse = 0;
    m_dyMouse = 0;
  }
  LOG_VERBOSE("recv mouse move %d,%d", x, y);

  // forward
  if (!ignore) {
    m_client->mouseMove(x, y);
  }
  return true;
}

bool ServerProxy::mouseRelativeMove()
{
  // parse
  bool ignore;
  int16_t dx = 0;
  int16_t dy = 0;
  if (!readOrDisconnect(kMsgDMouseRelMove + 4, &dx, &dy)) {
    return false;
  }

  // note if we should ignore the move
  ignore = m_ignoreMouse;

  // compress mouse motion events if more input follows
  if (!ignore && !m_compressMouseRelative && m_stream->isReady()) {
    m_compressMouseRelative = true;
  }

  // if compressing then ignore the motion but record it
  if (m_compressMouseRelative) {
    ignore = true;
    m_dxMouse += dx;
    m_dyMouse += dy;
  }
  LOG_VERBOSE("recv mouse relative move %d,%d", dx, dy);

  // forward
  if (!ignore) {
    m_client->mouseRelativeMove(dx, dy);
  }
  return true;
}

bool ServerProxy::mouseWheel()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  int16_t xDelta = 0;
  int16_t yDelta = 0;
  if (!readOrDisconnect(kMsgDMouseWheel + 4, &xDelta, &yDelta)) {
    return false;
  }
  LOG_VERBOSE("recv mouse wheel %+d,%+d", xDelta, yDelta);

  // forward
  m_client->mouseWheel(xDelta, yDelta);
  return true;
}
