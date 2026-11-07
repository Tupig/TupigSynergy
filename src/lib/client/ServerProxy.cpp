/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy.h"

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

ServerProxy::ServerProxy(
    IServerProxyClient *client, synergy::IStream *stream, IEventQueue *events, const ServerProxyConfig &config
)
    : m_client(client),
      m_stream(stream),
      m_events(events)
{
  assert(m_client != nullptr);
  assert(m_stream != nullptr);

  // initialize modifier translation table
  for (KeyModifierID id = 0; id < kKeyModifierIDLast; ++id)
    m_modifierTranslationTable[id] = id;

  // handle data on stream
  m_events->addHandler(EventTypes::StreamInputReady, m_stream->getEventTarget(), [this](const auto &) {
    handleData();
  });
  m_events->addHandler(EventTypes::ClipboardSending, this, [this](const auto &e) {
    ClipboardChunk::send(m_stream, e.getDataObject());
  });

  // Default list blocks Ctrl+Alt+Delete. An explicit empty list blocks nothing.
  if (!config.blockedKeyCombos.empty()) {
    m_inputValidator.setBlockedCombinations(
        InputValidator::parseBlockedCombinations(config.blockedKeyCombos), config.blockedKeyCombosConfigured
    );
    LOG_INFO("blocked key combinations configured: %zu", m_inputValidator.blockedCombinationCount());
  }

  // File transfer is off unless the user turns it on and names a drop directory:
  // it means writing bytes chosen by the peer onto this machine.
  {
    FileTransferReceiver::Options options;
    options.enabled = config.fileTransfer.enabled;
    options.dropDirectory = config.fileTransfer.dropDirectory;

    if (config.fileTransfer.maxFileSizeMb > 0) {
      options.maxFileSize = bytesFromMegabyteSetting(config.fileTransfer.maxFileSizeMb, options.maxFileSize);
    }
    if (config.fileTransfer.maxFileCount > 0) {
      constexpr auto kMaxFileCount = static_cast<uint64_t>(4096);
      options.maxFileCount = static_cast<size_t>((std::min)(config.fileTransfer.maxFileCount, kMaxFileCount));
    }

    m_fileTransferReceiver = std::make_unique<FileTransferReceiver>(options);
    LOG_DEBUG(
        "file transfer: enabled=%d, drop directory \"%s\"", options.enabled ? 1 : 0, options.dropDirectory.c_str()
    );
  }

  // send heartbeat
  setKeepAliveRate(kKeepAliveRate);
}

ServerProxy::~ServerProxy()
{
  setKeepAliveRate(-1.0);
  m_events->removeHandler(EventTypes::StreamInputReady, m_stream->getEventTarget());
}

synergy::KeyboardLayoutManager &ServerProxy::layoutManager()
{
  if (m_layoutManager == nullptr) {
    // the default constructor reads the local keyboard layouts from AppUtil,
    // which only exists once the application has started
    m_layoutManager = std::make_unique<synergy::KeyboardLayoutManager>();
  }
  return *m_layoutManager;
}

template <typename... Args> bool ServerProxy::readOrDisconnect(const char *format, Args &&...args)
{
  if (ProtocolUtil::readf(m_stream, format, args...)) {
    return true;
  }
  // readf returns false when the stream ends mid-message, leaving the output
  // arguments partially written; never forward those values
  m_client->disconnect("truncated message");
  return false;
}

void ServerProxy::resetKeepAliveAlarm()
{
  if (m_keepAliveAlarmTimer != nullptr) {
    m_events->removeHandler(EventTypes::Timer, m_keepAliveAlarmTimer);
    m_events->deleteTimer(m_keepAliveAlarmTimer);
    m_keepAliveAlarmTimer = nullptr;
  }
  if (m_keepAliveAlarm > 0.0) {
    m_keepAliveAlarmTimer = m_events->newOneShotTimer(m_keepAliveAlarm, nullptr);
    m_events->addHandler(EventTypes::Timer, m_keepAliveAlarmTimer, [this](const auto &) { handleKeepAliveAlarm(); });
  }
}

void ServerProxy::setKeepAliveRate(double rate)
{
  m_keepAliveAlarm = rate * kKeepAlivesUntilDeath;
  resetKeepAliveAlarm();
}

void ServerProxy::handleData()
{
  // handle messages until there are no more.  first read message code.
  uint8_t code[4];
  uint32_t n = m_stream->read(code, 4);
  while (n != 0) {
    // verify we got an entire code
    if (n != 4) {
      LOG_ERR("incomplete message from server: %d bytes", n);
      m_client->disconnect("incomplete message from server");
      return;
    }

    // parse message
    LOG_VERBOSE("msg from server: %c%c%c%c", code[0], code[1], code[2], code[3]);
    try {
      switch ((this->*m_parser)(code)) {
        using enum ConnectionResult;
      case Okay:
        break;

      case Unknown:
        LOG_ERR("invalid message from server: %c%c%c%c", code[0], code[1], code[2], code[3]);
        // not possible to determine message boundaries
        // read the whole stream to discard unkonwn data
        while (m_stream->read(nullptr, 4))
          ;
        break;

      case Disconnect:
        return;
      }
    } catch (const BadClientException &e) {
      LOG_ERR("protocol error from server: %s", e.what());
      ProtocolUtil::writef(m_stream, kMsgEBad);
      m_client->disconnect("invalid message from server");
      return;
    }

    // next message
    n = m_stream->read(code, 4);
  }

  // reply once for the whole batch.  this is intended to work around a delay
  // when running a linux server and an OS X (any BSD?) client.  the client
  // waits to send an ACK (if net.inet.tcp.delayed_ack is 1) in hopes of
  // piggybacking it on a data packet; we provide that packet here.
  if (m_needNoopReply) {
    m_needNoopReply = false;
    ProtocolUtil::writef(m_stream, kMsgCNoop);
  }

  flushCompressedMouse();
}

ServerProxy::ConnectionResult ServerProxy::parseHandshakeMessage(const uint8_t *code)
{
  using enum ConnectionResult;
  using enum synergy::core::ConnectionRefusal;

  if (memcmp(code, kMsgQInfo, 4) == 0) {
    queryInfo();
  }

  else if (memcmp(code, kMsgCInfoAck, 4) == 0) {
    infoAcknowledgment();
  }

  else if (memcmp(code, kMsgDSetOptions, 4) == 0) {
    setOptions();

    // handshake is complete
    m_parser = &ServerProxy::parseMessage;

    if (const auto missedKeyboardLayouts = layoutManager().getMissedLayouts(); !missedKeyboardLayouts.empty()) {
      LOG_WARN("server layouts missing on this computer: %s", missedKeyboardLayouts.c_str());
      ipcSendToClient("missingKeyboardLayouts", QString::fromStdString(missedKeyboardLayouts));
    }

    m_client->handshakeComplete();
  }

  else if (memcmp(code, kMsgCResetOptions, 4) == 0) {
    resetOptions();
  }

  else if (memcmp(code, kMsgCKeepAlive, 4) == 0) {
    // echo keep alives and reset alarm
    ProtocolUtil::writef(m_stream, kMsgCKeepAlive);
    resetKeepAliveAlarm();
  }

  else if (memcmp(code, kMsgCNoop, 4) == 0) {
    // accept and discard no-op
  }

  else if (memcmp(code, kMsgCClose, 4) == 0) {
    // server wants us to hangup
    LOG_VERBOSE("recv close");
    m_client->disconnect(nullptr);
    return Disconnect;
  }

  else if (memcmp(code, kMsgEIncompatible, 4) == 0) {
    int32_t major = 0;
    int32_t minor = 0;
    if (!readOrDisconnect(kMsgEIncompatible + 4, &major, &minor)) {
      return Disconnect;
    }
    LOG_ERR("server has incompatible version %d.%d", major, minor);
    m_client->refuseConnection(IncompatibleVersion, "server has incompatible version");
    return Disconnect;
  }

  else if (memcmp(code, kMsgEBusy, 4) == 0) {
    LOG_ERR("server already has a connected client with name \"%s\"", m_client->getName().c_str());
    m_client->refuseConnection(AlreadyConnected, "server already has a connected client with our name");
    return Disconnect;
  }

  else if (memcmp(code, kMsgEUnknown, 4) == 0) {
    LOG_ERR("server refused client with name \"%s\"", m_client->getName().c_str());
    m_client->refuseConnection(UnknownClient, "server refused client with our name");
    return Disconnect;
  }

  else if (memcmp(code, kMsgEBad, 4) == 0) {
    LOG_ERR("server disconnected due to a protocol error");
    m_client->refuseConnection(ProtocolError, "server reported a protocol error");
    return Disconnect;
  } else if (memcmp(code, kMsgDLanguageSynchronisation, 4) == 0) {
    setServerLanguages();
  } else {
    return Unknown;
  }

  return Okay;
}

ServerProxy::ConnectionResult ServerProxy::parseMessage(const uint8_t *code)
{
  using enum ConnectionResult;

  if (memcmp(code, kMsgDMouseMove, 4) == 0) {
    mouseMove();
  }

  else if (memcmp(code, kMsgDDragInfo, 4) == 0) {
    dragInfo();
  }

  else if (memcmp(code, kMsgDFileTransfer, 4) == 0) {
    fileChunk();
  }

  else if (memcmp(code, kMsgDMouseRelMove, 4) == 0) {
    mouseRelativeMove();
  }

  else if (memcmp(code, kMsgDMouseWheel, 4) == 0) {
    mouseWheel();
  }

  else if (memcmp(code, kMsgDKeyDown, 4) == 0) {
    uint16_t id = 0;
    uint16_t mask = 0;
    uint16_t button = 0;
    if (!readOrDisconnect(kMsgDKeyDown + 4, &id, &mask, &button)) {
      return Disconnect;
    }
    LOG_VERBOSE("recv key down id=0x%08x, mask=0x%04x, button=0x%04x", id, mask, button);

    keyDown(id, mask, button, "");
  }

  else if (memcmp(code, kMsgDKeyDownLang, 4) == 0) {
    std::string lang;
    uint16_t id = 0;
    uint16_t mask = 0;
    uint16_t button = 0;

    if (!readOrDisconnect(kMsgDKeyDownLang + 4, &id, &mask, &button, &lang)) {
      return Disconnect;
    }
    LOG_VERBOSE("recv key down id=0x%08x, mask=0x%04x, button=0x%04x, lang=\"%s\"", id, mask, button, lang.c_str());

    keyDown(id, mask, button, lang);
  }

  else if (memcmp(code, kMsgDKeyUp, 4) == 0) {
    keyUp();
  }

  else if (memcmp(code, kMsgDMouseDown, 4) == 0) {
    mouseDown();
  }

  else if (memcmp(code, kMsgDMouseUp, 4) == 0) {
    mouseUp();
  }

  else if (memcmp(code, kMsgDKeyRepeat, 4) == 0) {
    keyRepeat();
  }

  else if (memcmp(code, kMsgCKeepAlive, 4) == 0) {
    // echo keep alives and reset alarm
    ProtocolUtil::writef(m_stream, kMsgCKeepAlive);
    resetKeepAliveAlarm();
  }

  else if (memcmp(code, kMsgCNoop, 4) == 0) {
    // accept and discard no-op
  }

  else if (memcmp(code, kMsgCEnter, 4) == 0) {
    enter();
  }

  else if (memcmp(code, kMsgCLeave, 4) == 0) {
    leave();
  }

  else if (memcmp(code, kMsgCClipboard, 4) == 0) {
    grabClipboard();
  }

  else if (memcmp(code, kMsgCScreenSaver, 4) == 0) {
    screensaver();
  }

  else if (memcmp(code, kMsgQInfo, 4) == 0) {
    queryInfo();
  }

  else if (memcmp(code, kMsgCInfoAck, 4) == 0) {
    infoAcknowledgment();
  }

  else if (memcmp(code, kMsgDClipboard, 4) == 0) {
    setClipboard();
  }

  else if (memcmp(code, kMsgCResetOptions, 4) == 0) {
    resetOptions();
  }

  else if (memcmp(code, kMsgDSetOptions, 4) == 0) {
    setOptions();
  }

  else if (memcmp(code, kMsgDSecureInputNotification, 4) == 0) {
    secureInputNotification();
  }

  else if (memcmp(code, kMsgCClose, 4) == 0) {
    // server wants us to hangup
    LOG_VERBOSE("recv close");
    m_client->disconnect(nullptr);
    return Disconnect;
  } else if (memcmp(code, kMsgEBad, 4) == 0) {
    LOG_ERR("server disconnected due to a protocol error");
    m_client->disconnect("server reported a protocol error");
    return Disconnect;
  } else {
    return Unknown;
  }

  // remember that a reply is due; it is sent once for the whole incoming
  // batch by handleData instead of once per message
  m_needNoopReply = true;

  return Okay;
}

void ServerProxy::handleKeepAliveAlarm()
{
  LOG_INFO("server is dead");
  m_client->disconnect("server is not responding");
}

void ServerProxy::onInfoChanged()
{
  // ignore mouse motion until we receive acknowledgment of our info
  // change message.
  m_ignoreMouse = true;

  // send info update
  queryInfo();
}

bool ServerProxy::onGrabClipboard(ClipboardID id)
{
  LOG_VERBOSE("sending clipboard %d changed", id);
  ProtocolUtil::writef(m_stream, kMsgCClipboard, id, m_seqNum);
  return true;
}

void ServerProxy::onClipboardChanged(ClipboardID id, const IClipboard *clipboard)
{
  std::string data = IClipboard::marshall(clipboard);
  LOG_DEBUG("sending clipboard %d seqnum=%d", id, m_seqNum);

  StreamChunker::sendClipboard(data, data.size(), id, m_seqNum, m_events, this);
}

void ServerProxy::flushCompressedMouse()
{
  if (m_compressMouse) {
    m_compressMouse = false;
    m_client->mouseMove(m_xMouse, m_yMouse);
  }
  if (m_compressMouseRelative) {
    m_compressMouseRelative = false;
    m_client->mouseRelativeMove(m_dxMouse, m_dyMouse);
    m_dxMouse = 0;
    m_dyMouse = 0;
  }
}

void ServerProxy::sendInfo(const ClientInfo &info)
{
  LOG_VERBOSE("sending info shape=%d,%d %dx%d", info.m_x, info.m_y, info.m_w, info.m_h);
  ProtocolUtil::writef(m_stream, kMsgDInfo, info.m_x, info.m_y, info.m_w, info.m_h, 0, info.m_mx, info.m_my);
}

void ServerProxy::enter()
{
  // parse
  int16_t x = 0;
  int16_t y = 0;
  uint16_t mask = 0;
  uint32_t seqNum = 0;
  if (!readOrDisconnect(kMsgCEnter + 4, &x, &y, &seqNum, &mask)) {
    return;
  }
  LOG_VERBOSE("recv enter, %d,%d %d %04x", x, y, seqNum, mask);

  // discard old compressed mouse motion, if any
  m_compressMouse = false;
  m_compressMouseRelative = false;
  m_dxMouse = 0;
  m_dyMouse = 0;
  m_seqNum = seqNum;
  m_serverLayout = "";
  m_isUserNotifiedAboutLayoutSyncError = false;

  // forward
  m_client->enter(x, y, seqNum, static_cast<KeyModifierMask>(mask), false);
}

void ServerProxy::leave()
{
  // parse
  LOG_VERBOSE("recv leave");

  // send last mouse motion
  flushCompressedMouse();

  // forward
  m_client->leave();
}

void ServerProxy::setClipboard()
{
  // parse
  ClipboardID id;
  uint32_t seq;

  auto r = ClipboardChunk::assemble(
      m_stream, m_clipboardDataCached, id, seq, m_clipboardChunkState, m_client->getMaximumClipboardReceiveSizeBytes()
  );

  if (r == TransferState::Started) {
    size_t size = ClipboardChunk::getExpectedSize(m_clipboardChunkState);
    LOG_DEBUG("receiving clipboard %d size=%zu", id, size);
  } else if (r == TransferState::Finished) {
    LOG_DEBUG("received clipboard %d size=%zu", id, m_clipboardDataCached.size());

    // forward
    Clipboard clipboard;
    clipboard.unmarshall(m_clipboardDataCached, 0);
    m_client->setClipboard(id, &clipboard);
    m_clipboardDataCached.clear();
    m_clipboardDataCached.shrink_to_fit();

    LOG_INFO("clipboard was updated");
  } else if (r == TransferState::Error) {
    m_client->disconnect("invalid clipboard data from server");
  }
}

void ServerProxy::grabClipboard()
{
  // parse
  ClipboardID id = 0;
  uint32_t seqNum = 0;
  if (!readOrDisconnect(kMsgCClipboard + 4, &id, &seqNum)) {
    return;
  }
  LOG_DEBUG("recv grab clipboard %d", id);

  // validate
  if (id >= kClipboardEnd) {
    return;
  }

  // forward
  m_client->grabClipboard(id);
}

void ServerProxy::screensaver()
{
  // parse
  int8_t on = 0;
  if (!readOrDisconnect(kMsgCScreenSaver + 4, &on)) {
    return;
  }
  LOG_VERBOSE("recv screen saver on=%d", on);

  // forward
  m_client->screensaver(on != 0);
}

void ServerProxy::resetOptions()
{
  // parse
  LOG_VERBOSE("recv reset options");

  // forward
  m_client->resetOptions();

  // reset keep alive
  setKeepAliveRate(kKeepAliveRate);

  // reset modifier translation table
  for (KeyModifierID id = 0; id < kKeyModifierIDLast; ++id) {
    m_modifierTranslationTable[id] = id;
  }
}

void ServerProxy::setOptions()
{
  // parse
  OptionsList options;
  if (!readOrDisconnect(kMsgDSetOptions + 4, &options)) {
    return;
  }
  LOG_VERBOSE("recv set options size=%d", options.size());

  if (options.size() % 2 != 0) {
    LOG_ERR("options are the incorrect size, can not process them");
    return;
  }

  // forward
  m_client->setOptions(options);

  // update modifier table
  for (uint32_t i = 0, n = (uint32_t)options.size(); i < n; i += 2) {
    KeyModifierID id = kKeyModifierIDNull;
    if (options[i] == kOptionModifierMapForShift) {
      id = kKeyModifierIDShift;
    } else if (options[i] == kOptionModifierMapForControl) {
      id = kKeyModifierIDControl;
    } else if (options[i] == kOptionModifierMapForAlt) {
      id = kKeyModifierIDAlt;
    } else if (options[i] == kOptionModifierMapForAltGr) {
      id = kKeyModifierIDAltGr;
    } else if (options[i] == kOptionModifierMapForMeta) {
      id = kKeyModifierIDMeta;
    } else if (options[i] == kOptionModifierMapForSuper) {
      id = kKeyModifierIDSuper;
    } else if (options[i] == kOptionHeartbeat) {
      // update keep alive
      setKeepAliveRate(1.0e-3 * static_cast<double>(options[i + 1]));
    }

    if (id != kKeyModifierIDNull) {
      m_modifierTranslationTable[id] = std::clamp<KeyModifierMask>(options[i + 1], 0, kKeyModifierIDLast - 1);
      LOG_VERBOSE("modifier %d mapped to %d", id, m_modifierTranslationTable[id]);
    }
  }
}

void ServerProxy::queryInfo()
{
  ClientInfo info;
  m_client->getShape(info.m_x, info.m_y, info.m_w, info.m_h);
  m_client->getCursorPos(info.m_mx, info.m_my);
  sendInfo(info);
}

void ServerProxy::infoAcknowledgment()
{
  LOG_VERBOSE("recv info acknowledgment");
  m_ignoreMouse = false;
}

void ServerProxy::secureInputNotification()
{
  std::string app;
  if (!readOrDisconnect(kMsgDSecureInputNotification + 4, &app)) {
    return;
  }
  LOG_INFO("application \"%s\" is blocking the keyboard", app.c_str());
}

void ServerProxy::dragInfo()
{
  if (m_fileTransferReceiver != nullptr && m_fileTransferReceiver->onDragInfo(m_stream)) {
    LOG_INFO("file transfer: server is offering %zu file(s)", m_fileTransferReceiver->acceptedNames().size());
  }
}

void ServerProxy::fileChunk()
{
  if (m_fileTransferReceiver == nullptr) {
    // Should not happen: the receiver is also what drains the message body, so
    // without it the stream would go out of step.
    LOG_ERR("file transfer: no receiver to hand the chunk to");
    return;
  }

  switch (m_fileTransferReceiver->onFileChunk(m_stream)) {
  case TransferState::Finished:
    LOG_INFO("file transfer: received a file");
    if (m_fileTransferReceiver->writtenFiles().size() == m_fileTransferReceiver->acceptedNames().size() &&
        !m_fileTransferReceiver->writtenFiles().empty()) {
      m_client->offerReceivedFiles(m_fileTransferReceiver->writtenFiles());
    }
    break;
  case TransferState::Error:
    LOG_WARN("file transfer: the server sent a chunk this client rejected");
    break;
  case TransferState::Started:
  case TransferState::InProgress:
    break;
  }
}

void ServerProxy::setServerLanguages()
{
  std::string serverLayout;
  if (!readOrDisconnect(kMsgDLanguageSynchronisation + 4, &serverLayout)) {
    return;
  }
  layoutManager().setRemoteLayouts(serverLayout);
}

void ServerProxy::setActiveServerLanguage(const std::string_view &language)
{
  if (!language.empty() && (language.size() > 0)) {
    if (m_serverLayout != language) {
      m_isUserNotifiedAboutLayoutSyncError = false;
      m_serverLayout = language;
    }

    if (!layoutManager().isLayoutInstalled(m_serverLayout)) {
      if (!m_isUserNotifiedAboutLayoutSyncError) {
        LOG_WARN("current server layout is not installed on client");
        m_isUserNotifiedAboutLayoutSyncError = true;
      }
    } else {
      m_isUserNotifiedAboutLayoutSyncError = false;
    }
  } else {
    LOG_VERBOSE("active server layout is empty");
  }
}
