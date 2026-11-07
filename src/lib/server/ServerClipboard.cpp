/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers.
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/Server.h"

#include "AppUtil.h"
#include "IPlatformScreen.h"
#include "OptionTypes.h"
#include "PacketStreamFilter.h"
#include "ProtocolTypes.h"
#include "Screen.h"
#include "StreamChunker.h"
#include "SynergyException.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "ipc/CoreIpc.h"
#include "net/TCPSocket.h"
#include "server/ClientListener.h"
#include "server/ClientProxy.h"
#include "server/ClientProxyUnknown.h"
#include "server/PrimaryClient.h"
#include "synergy/protocol/FileChunk.h"
#include "synergy/protocol/FileTransferOutbound.h"

#include <QString>
#include <QStringList>

#ifdef _WIN32
#include <algorithm>
#include <array>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <optional>
#include <utility>

using namespace synergy::server;

void Server::sendDragInfo(uint32_t fileCount, const char *info, size_t size)
{
  for (const auto &[name, client] : m_clients) {
    // The primary is where the drag started, so there is nothing to announce to
    // it; its own implementation is a deliberate no-op for the same reason.
    if (client->isPrimary()) {
      continue;
    }

    client->sendDragInfo(fileCount, info, size);
  }
}

void Server::fileChunkSending(uint8_t mark, char *data, size_t dataSize)
{
  for (const auto &[name, client] : m_clients) {
    if (client->isPrimary()) {
      continue;
    }

    client->fileChunkSending(mark, data, dataSize);
  }
}

uint64_t Server::sendLocalFiles(const std::vector<std::string> &paths)
{
  FileTransferOutbound::Options options;
  options.enabled = m_fileTransferSettings.enabled;

  if (m_fileTransferSettings.maxFileSizeMb > 0) {
    options.maxFileSize = bytesFromMegabyteSetting(m_fileTransferSettings.maxFileSizeMb, options.maxFileSize);
  }
  if (m_fileTransferSettings.maxFileCount > 0) {
    constexpr auto kMaxFileCount = static_cast<uint64_t>(4096);
    options.maxFileCount = static_cast<size_t>((std::min)(m_fileTransferSettings.maxFileCount, kMaxFileCount));
  }

  FileTransferSender::Emitter emitter;
  emitter.dragInfo = [this](uint32_t fileCount, const std::string &names) {
    sendDragInfo(fileCount, names.data(), names.size());
  };
  emitter.chunk = [this](uint8_t mark, const std::string &payload) {
    // fileChunkSending takes a mutable pointer; the callee only reads it.
    auto copy = payload;
    fileChunkSending(mark, copy.data(), copy.size());
  };

  return FileTransferOutbound::sendPaths(paths, options, emitter);
}

void Server::handleClipboardGrabbed(const Event &event, BaseClientProxy *grabber)
{
  if (!m_enableClipboard || (m_maximumClipboardSize == 0)) {
    return;
  }

  // ignore events from unknown clients
  if (!m_clientSet.contains(grabber)) {
    return;
  }
  const auto *info = static_cast<const IScreen::ClipboardInfo *>(event.getData());

  // ignore grab if sequence number is old.  always allow primary
  // screen to grab.
  ClipboardInfo &clipboard = m_clipboards[info->m_id];
  if (grabber != m_primaryClient && info->m_sequenceNumber < clipboard.m_clipboardSeqNum) {
    LOG_INFO("ignored screen \"%s\" grab of clipboard %d", getName(grabber).c_str(), info->m_id);
    return;
  }

  // mark screen as owning clipboard
  LOG_INFO(
      "screen \"%s\" grabbed clipboard %d from \"%s\"", getName(grabber).c_str(), info->m_id,
      clipboard.m_clipboardOwner.c_str()
  );
  clipboard.m_clipboardOwner = getName(grabber);
  clipboard.m_clipboardSeqNum = info->m_sequenceNumber;

  // clear the clipboard data (since it's not known at this point)
  if (clipboard.m_clipboard.open(0)) {
    clipboard.m_clipboard.empty();
    clipboard.m_clipboard.close();
  }
  clipboard.m_clipboardData = clipboard.m_clipboard.marshall();

  // tell all other screens to take ownership of clipboard.  tell the
  // grabber that it's clipboard isn't dirty.
  for (auto index = m_clients.begin(); index != m_clients.end(); ++index) {
    BaseClientProxy *client = index->second;
    if (client == grabber) {
      client->setClipboardDirty(info->m_id, false);
    } else {
      client->grabClipboard(info->m_id);
    }
  }

  if (grabber == m_primaryClient && m_active != m_primaryClient) {
    LOG_INFO("clipboard grabbed while active screen was changed, resending clipboard data");
    onClipboardChanged(m_primaryClient, info->m_id, clipboard.m_clipboardSeqNum);
  }
}

void Server::handleClipboardChanged(const Event &event, BaseClientProxy *client)
{
  // ignore events from unknown clients
  if (!m_clientSet.contains(client)) {
    return;
  }
  const auto *info = static_cast<const IScreen::ClipboardInfo *>(event.getData());
  onClipboardChanged(client, info->m_id, info->m_sequenceNumber);
}

void Server::onClipboardChanged(const BaseClientProxy *sender, ClipboardID id, uint32_t seqNum)
{
  ClipboardInfo &clipboard = m_clipboards[id];

  // ignore update if sequence number is old
  if (seqNum < clipboard.m_clipboardSeqNum) {
    LOG_INFO("ignored screen \"%s\" update of clipboard %d (mis-sequenced)", getName(sender).c_str(), id);
    return;
  }

  // should be the expected client; tolerate a late update from a
  // screen that has since disconnected and released clipboard ownership
  const auto ownerIt = m_clients.find(clipboard.m_clipboardOwner);
  if (ownerIt == m_clients.end() || sender != ownerIt->second) {
    LOG_WARN("ignored screen \"%s\" update of clipboard %d (not the clipboard owner)", getName(sender).c_str(), id);
    return;
  }

  // get data
  sender->getClipboard(id, &clipboard.m_clipboard);

  std::string data = clipboard.m_clipboard.marshall();
  if (IClipboard::exceedsSizeLimit(data.size(), m_maximumClipboardSize)) {
    LOG_WARN("not sending clipboard data, exceeds limit: %i KB", m_maximumClipboardSize);
    return;
  }

  // ignore if data hasn't changed
  if (data == clipboard.m_clipboardData) {
    LOG_DEBUG("ignored screen \"%s\" update of clipboard %d (unchanged)", clipboard.m_clipboardOwner.c_str(), id);
    return;
  }

  // got new data
  LOG_INFO("screen \"%s\" updated clipboard %d", clipboard.m_clipboardOwner.c_str(), id);
  clipboard.m_clipboardData = data;

  // tell all clients except the sender that the clipboard is dirty
  for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
    BaseClientProxy *client = index->second;
    client->setClipboardDirty(id, client != sender);
  }

  // send the new clipboard to the active screen
  m_active->setClipboard(id, &clipboard.m_clipboard);
}

void Server::onScreensaver(bool activated)
{
  LOG_DEBUG("onScreenSaver %s", activated ? "activated" : "deactivated");

  if (activated) {
    // save current screen and position
    m_activeSaver = m_active;
    m_xSaver = m_x;
    m_ySaver = m_y;

    // jump to primary screen
    if (m_active != m_primaryClient) {
      switchScreen(m_primaryClient, 0, 0, true);
    }
  } else {
    // jump back to previous screen and position.  we must check
    // that the position is still valid since the screen may have
    // changed resolutions while the screen saver was running.
    if (m_activeSaver != nullptr && m_activeSaver != m_primaryClient) {
      // check position
      BaseClientProxy *screen = m_activeSaver;
      int32_t x;
      int32_t y;
      int32_t w;
      int32_t h;
      screen->getShape(x, y, w, h);
      int32_t zoneSize = getJumpZoneSize(screen);
      if (m_xSaver < x + zoneSize) {
        m_xSaver = x + zoneSize;
      } else if (m_xSaver >= x + w - zoneSize) {
        m_xSaver = x + w - zoneSize - 1;
      }
      if (m_ySaver < y + zoneSize) {
        m_ySaver = y + zoneSize;
      } else if (m_ySaver >= y + h - zoneSize) {
        m_ySaver = y + h - zoneSize - 1;
      }

      // jump
      switchScreen(screen, m_xSaver, m_ySaver, false);
    }

    // reset state
    m_activeSaver = nullptr;
  }

  // send message to all clients
  for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
    BaseClientProxy *client = index->second;
    client->screensaver(activated);
  }
}
