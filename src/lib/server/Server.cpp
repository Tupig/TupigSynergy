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

//
// Server
//

Server::Server(
    ServerConfig &config, PrimaryClient *primaryClient, synergy::Screen *screen, IEventQueue *events,
    NetworkProtocol protocol, FileTransferSettings fileTransfer
)
    : m_primaryClient(primaryClient),
      m_active(primaryClient),
      m_config(&config),
      m_inputFilter(config.getInputFilter()),
      m_screen(screen),
      m_events(events),
      m_protocol(protocol),
      m_fileTransferSettings(std::move(fileTransfer))
{
  // must have a primary client and it must have a canonical name
  assert(m_primaryClient != nullptr);
  assert(config.isScreen(primaryClient->getName()));
  assert(m_screen != nullptr);

  std::string primaryName = getName(primaryClient);

  // clear clipboards
  for (auto &clipboard : m_clipboards) {
    clipboard.m_clipboardOwner = primaryName;
    clipboard.m_clipboardSeqNum = m_seqNum;
    if (clipboard.m_clipboard.open(0)) {
      clipboard.m_clipboard.empty();
      clipboard.m_clipboard.close();
    }
    clipboard.m_clipboardData = clipboard.m_clipboard.marshall();
  }

  // install event handlers
  m_events->addHandler(EventTypes::Timer, this, [this](const auto &) { handleSwitchWaitTimeout(); });
  m_events->addHandler(EventTypes::KeyStateKeyDown, m_inputFilter, [this](const auto &e) { handleKeyDownEvent(e); });
  m_events->addHandler(EventTypes::KeyStateKeyUp, m_inputFilter, [this](const auto &e) { handleKeyUpEvent(e); });
  m_events->addHandler(EventTypes::KeyStateKeyRepeat, m_inputFilter, [this](const auto &e) {
    handleKeyRepeatEvent(e);
  });
  m_events->addHandler(EventTypes::PrimaryScreenButtonDown, m_inputFilter, [this](const auto &e) {
    handleButtonDownEvent(e);
  });
  m_events->addHandler(EventTypes::PrimaryScreenButtonUp, m_inputFilter, [this](const auto &e) {
    handleButtonUpEvent(e);
  });
  m_events->addHandler(
      EventTypes::PrimaryScreenMotionOnPrimary, m_primaryClient->getEventTarget(),
      [this](const auto &e) { handleMotionPrimaryEvent(e); }
  );
  m_events->addHandler(
      EventTypes::PrimaryScreenMotionOnSecondary, m_primaryClient->getEventTarget(),
      [this](const auto &e) { handleMotionSecondaryEvent(e); }
  );
  m_events->addHandler(EventTypes::PrimaryScreenWheel, m_primaryClient->getEventTarget(), [this](const auto &e) {
    handleWheelEvent(e);
  });
  m_events->addHandler(
      EventTypes::PrimaryScreenSaverActivated, m_primaryClient->getEventTarget(),
      [this](const auto &) { onScreensaver(true); }
  );
  m_events->addHandler(
      EventTypes::PrimaryScreenSaverDeactivated, m_primaryClient->getEventTarget(),
      [this](const auto &) { onScreensaver(false); }
  );
  m_events->addHandler(EventTypes::ServerSwitchToScreen, m_inputFilter, [this](const auto &e) {
    handleSwitchToScreenEvent(e);
  });
  m_events->addHandler(EventTypes::ServerSwitchInDirection, m_inputFilter, [this](const auto &e) {
    handleSwitchInDirectionEvent(e);
  });
  m_events->addHandler(EventTypes::ServerToggleScreen, m_inputFilter, [this](const auto &e) {
    handleToggleScreenEvent(e);
  });
  m_events->addHandler(EventTypes::ServerKeyboardBroadcast, m_inputFilter, [this](const auto &e) {
    handleKeyboardBroadcastEvent(e);
  });
  m_events->addHandler(EventTypes::ServerLockCursorToScreen, m_inputFilter, [this](const auto &e) {
    handleLockCursorToScreenEvent(e);
  });
  m_events->addHandler(EventTypes::PrimaryScreenFakeInputBegin, m_inputFilter, [this](const auto &) {
    m_primaryClient->fakeInputBegin();
  });
  m_events->addHandler(EventTypes::PrimaryScreenFakeInputEnd, m_inputFilter, [this](const auto &) {
    m_primaryClient->fakeInputEnd();
  });

  // add connection
  addClient(m_primaryClient);

  // set initial configuration
  setConfig(config);

  // enable primary client
  m_primaryClient->enable();
  m_inputFilter->setPrimaryClient(m_primaryClient);

  // Determine if scroll lock is already set. If so, lock the cursor to the
  // primary screen (unless the user has disabled lock to screen in config)
  if (!m_disableLockToScreen && (m_primaryClient->getToggleMask() & KeyModifierScrollLock)) {
    LOG_INFO("scroll lock is on, locking cursor to screen");
    m_lockedToScreen = true;
  } else if (m_defaultLockToScreenState) {
    LOG_INFO("default screen lock is on, locking cursor to screen");
    m_lockedToScreen = true;
  }
}

Server::~Server()
{
  // remove event handlers and timers
  using enum EventTypes;
  m_events->removeHandler(KeyStateKeyDown, m_inputFilter);
  m_events->removeHandler(KeyStateKeyUp, m_inputFilter);
  m_events->removeHandler(KeyStateKeyRepeat, m_inputFilter);
  m_events->removeHandler(PrimaryScreenButtonDown, m_inputFilter);
  m_events->removeHandler(PrimaryScreenButtonUp, m_inputFilter);
  m_events->removeHandler(PrimaryScreenMotionOnPrimary, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryScreenMotionOnSecondary, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryScreenWheel, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryScreenSaverActivated, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryScreenSaverDeactivated, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryScreenFakeInputBegin, m_inputFilter);
  m_events->removeHandler(PrimaryScreenFakeInputEnd, m_inputFilter);
  m_events->removeHandler(Timer, this);
  stopSwitch();

  try {
    // force immediate disconnection of secondary clients
    disconnect();
  } catch (std::exception &e) { // NOSONAR
    LOG_ERR("failed to disconnect: %s", e.what());
  }

  for (auto index = m_oldClients.begin(); index != m_oldClients.end(); ++index) {
    BaseClientProxy *client = index->first;
    m_events->deleteTimer(index->second);
    m_events->removeHandler(Timer, client);
    m_events->removeHandler(ClientProxyDisconnected, client);
    delete client;
  }

  // remove input filter
  m_inputFilter->setPrimaryClient(nullptr);

  // disable and disconnect primary client
  m_primaryClient->disable();
  removeClient(m_primaryClient);
}

size_t Server::getMaximumClipboardSizeBytes() const
{
  return m_maximumClipboardSize * 1024;
}

bool Server::setConfig(const ServerConfig &config)
{
  // refuse configuration if it doesn't include the primary screen
  if (!config.isScreen(m_primaryClient->getName())) {
    return false;
  }

  // close clients that are connected but being dropped from the
  // configuration.
  closeClients(config);

  // cut over
  processOptions();

  // add ScrollLock as a hotkey to lock to the screen.  this was a
  // built-in feature in earlier releases and is now supported via
  // the user configurable hotkey mechanism.  if the user has already
  // registered ScrollLock for something else then that will win but
  // we will unfortunately generate a warning.  if the user has
  // configured a LockCursorToScreenAction then we don't add
  // ScrollLock as a hotkey.
  if (!m_disableLockToScreen && !m_config->hasLockToScreenAction()) {
    IPlatformScreen::KeyInfo *key = IPlatformScreen::KeyInfo::alloc(kKeyScrollLock, 0, 0, 0);
    InputFilter::Rule rule(new InputFilter::KeystrokeCondition(m_events, key));
    rule.adoptAction(new InputFilter::LockCursorToScreenAction(m_events), true);
    m_inputFilter->addFilterRule(rule);
  }

  // tell primary screen about reconfiguration
  m_primaryClient->reconfigure(getActivePrimarySides());

  // tell all (connected) clients about current options
  for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
    BaseClientProxy *client = index->second;
    sendOptions(client);
  }

  return true;
}

void Server::adoptClient(BaseClientProxy *client)
{
  assert(client != nullptr);

  // watch for client disconnection
  m_events->addHandler(EventTypes::ClientProxyDisconnected, client, [this, client](const auto &) {
    handleClientDisconnected(client);
  });

  // name must be in our configuration
  if (!m_config->isScreen(client->getName())) {
    LOG_WARN("unrecognised client name \"%s\", check server config", client->getName().c_str());
    ipcSendToClient("unrecognisedClient", QString::fromStdString(client->getName()));
    closeClient(client, kMsgEUnknown);
    return;
  }

  // add client to client list
  if (!addClient(client)) {
    // can only have one screen with a given name at any given time
    LOG_WARN("a client with name \"%s\" is already connected", getName(client).c_str());
    closeClient(client, kMsgEBusy);
    return;
  }
  LOG_DEBUG("client \"%s\" has connected", getName(client).c_str());
  ipcSendConnectionState(synergy::core::ConnectionState::Connected);
  sendConnectedClientsIpc();

  // send configuration options to client
  sendOptions(client);

  // activate screen saver on new client if active on the primary screen
  if (m_activeSaver != nullptr) {
    client->screensaver(true);
  }

  // send notification
  auto *info = new Server::ScreenConnectedInfo(getName(client));
  m_events->addEvent(Event(EventTypes::ServerConnected, m_primaryClient->getEventTarget(), info));
}

void Server::disconnect()
{
  // close all secondary clients
  if (m_clients.size() > 1 || !m_oldClients.empty()) {
    Config emptyConfig(m_events);
    closeClients(emptyConfig);
  } else {
    m_events->addEvent(Event(EventTypes::ServerDisconnected, this));
  }
}

std::string Server::protocolString() const
{
  if (m_protocol == NetworkProtocol::Unknown)
    throw InvalidProtocolException();
  return networkProtocolToName(m_protocol).toStdString();
}

uint32_t Server::getNumClients() const
{
  return (int32_t)m_clients.size();
}

void Server::getClients(std::vector<std::string> &list) const
{
  list.clear();
  for (auto index = m_clients.begin(); index != m_clients.end(); ++index) {
    list.push_back(index->first);
  }
}

void Server::sendConnectedClientsIpc() const
{
  const auto primaryName = getName(m_primaryClient);
  QStringList clientList;
  for (const auto &[name, _] : m_clients) {
    if (name != primaryName) {
      clientList.append(QString::fromStdString(name));
    }
  }
  ipcSendToClient("connectedClients", clientList.join(","));
}

std::string Server::getName(const BaseClientProxy *client) const
{
  std::string name = m_config->getCanonicalName(client->getName());
  if (name.empty()) {
    name = client->getName();
  }
  return name;
}

uint32_t Server::getActivePrimarySides() const
{
  using enum DirectionMask;
  using enum Direction;
  uint32_t sides = 0;
  if (!isLockedToScreenServer()) {
    if (hasAnyNeighbor(m_primaryClient, Left)) {
      sides |= static_cast<int>(LeftMask);
    }
    if (hasAnyNeighbor(m_primaryClient, Right)) {
      sides |= static_cast<int>(RightMask);
    }
    if (hasAnyNeighbor(m_primaryClient, Top)) {
      sides |= static_cast<int>(TopMask);
    }
    if (hasAnyNeighbor(m_primaryClient, Bottom)) {
      sides |= static_cast<int>(BottomMask);
    }
  }
  return sides;
}

void Server::stopRelativeMoves()
{
  if (m_relativeMoves && m_active != m_primaryClient) {
    // warp to the center of the active client so we know where we are
    int32_t ax;
    int32_t ay;
    int32_t aw;
    int32_t ah;
    m_active->getShape(ax, ay, aw, ah);
    m_x = ax + (aw >> 1);
    m_y = ay + (ah >> 1);
    m_xDelta = 0;
    m_yDelta = 0;
    m_xDelta2 = 0;
    m_yDelta2 = 0;
    LOG_VERBOSE("synchronize move on %s by %d,%d", getName(m_active).c_str(), m_x, m_y);
    m_active->mouseMove(m_x, m_y);
  }
}

void Server::sendOptions(BaseClientProxy *client) const
{
  OptionsList optionsList;

  // look up options for client
  const Config::ScreenOptions *options = m_config->getOptions(getName(client));
  if (options != nullptr) {
    // convert options to a more convenient form for sending
    optionsList.reserve(2 * options->size());
    for (auto [optionId, optionValue] : *options) {
      optionsList.push_back(optionId);
      optionsList.push_back(static_cast<uint32_t>(optionValue));
    }
  }

  // look up global options
  options = m_config->getOptions("");
  if (options != nullptr) {
    // convert options to a more convenient form for sending
    optionsList.reserve(optionsList.size() + 2 * options->size());
    for (auto [optionId, optionValue] : *options) {
      optionsList.push_back(optionId);
      optionsList.push_back(static_cast<uint32_t>(optionValue));
    }
  }

  // send the options
  client->resetOptions();
  client->setOptions(optionsList);
}

void Server::processOptions()
{
  const Config::ScreenOptions *options = m_config->getOptions("");
  if (options == nullptr) {
    return;
  }

  m_switchNeedsShift = false;   // it seems if i don't add these
  m_switchNeedsControl = false; // lines, the 'reload config' option
  m_switchNeedsAlt = false;     // doesnt' work correct.

  bool newRelativeMoves = m_relativeMoves;
  for (auto [optionId, optionValue] : *options) {
    const OptionID id = optionId;
    const OptionValue value = optionValue;
    if (id == kOptionScreenSwitchDelay) {
      m_switchWaitDelay = 1.0e-3 * static_cast<double>(value);
      if (m_switchWaitDelay < 0.0) {
        m_switchWaitDelay = 0.0;
      }
      stopSwitchWait();
    } else if (id == kOptionScreenSwitchTwoTap) {
      m_switchTwoTapDelay = 1.0e-3 * static_cast<double>(value);
      if (m_switchTwoTapDelay < 0.0) {
        m_switchTwoTapDelay = 0.0;
      }
      stopSwitchTwoTap();
    } else if (id == kOptionScreenSwitchNeedsControl) {
      m_switchNeedsControl = (value != 0);
    } else if (id == kOptionScreenSwitchNeedsShift) {
      m_switchNeedsShift = (value != 0);
    } else if (id == kOptionScreenSwitchNeedsAlt) {
      m_switchNeedsAlt = (value != 0);
    } else if (id == kOptionRelativeMouseMoves) {
      newRelativeMoves = (value != 0);
    } else if (id == kOptionDefaultLockToScreenState) {
      m_defaultLockToScreenState = (value != 0);
    } else if (id == kOptionDisableLockToScreen) {
      m_disableLockToScreen = (value != 0);
    } else if (id == kOptionClipboardSharing) {
      m_enableClipboard = value;
      if (!m_enableClipboard) {
        LOG_INFO("clipboard sharing is disabled");
      }
    } else if (id == kOptionClipboardSharingSize) {
      if (value <= 0) {
        m_maximumClipboardSize = 0;
        LOG_INFO(
            "clipboard sharing is disabled because the "
            "maximum shared clipboard size is set to 0"
        );
      } else {
        m_maximumClipboardSize = static_cast<size_t>(value);
      }
    }
  }
  if (m_relativeMoves && !newRelativeMoves) {
    stopRelativeMoves();
  }
  m_relativeMoves = newRelativeMoves;
}

void Server::handleShapeChanged(BaseClientProxy *client)
{
  if (!m_clientSet.contains(client)) {
    return;
  }

  LOG_DEBUG("screen \"%s\" shape changed", getName(client).c_str());

  // update jump coordinate
  int32_t x;
  int32_t y;
  client->getCursorPos(x, y);
  client->setJumpCursorPos(x, y);

  // update the mouse coordinates
  if (client == m_active) {
    m_x = x;
    m_y = y;
  }

  // handle resolution change to primary screen
  if (client == m_primaryClient) {
    if (client == m_active) {
      onMouseMovePrimary(m_x, m_y);
    } else {
      onMouseMoveSecondary(0, 0);
    }
  }
}

void Server::handleSwitchWaitTimeout()
{
  // the target client may have disconnected, or the switch may have been
  // stopped, while this one-shot timer was in flight
  if (m_switchScreen == nullptr) {
    stopSwitchWait();
    return;
  }

  // ignore if mouse is locked to screen
  if (isLockedToScreen()) {
    LOG_VERBOSE("locked to screen");
    stopSwitch();
    return;
  }

  // switch screen
  switchScreen(m_switchScreen, m_switchWaitX, m_switchWaitY, false);
}

void Server::handleClientDisconnected(BaseClientProxy *client)
{
  // client has disconnected.  it might be an old client or an
  // active client.  we don't care so just handle it both ways.
  removeActiveClient(client);
  removeOldClient(client);

  // m_clients always contains the primary (server) screen, so 1 means no remote clients.
  using enum synergy::core::ConnectionState;
  ipcSendConnectionState(m_clients.size() <= 1 ? Listening : Connected);
  sendConnectedClientsIpc();

  delete client;
}

void Server::handleClientCloseTimeout(BaseClientProxy *client)
{
  // client took too long to disconnect.  just dump it.
  LOG_INFO("forced disconnection of client \"%s\"", getName(client).c_str());
  removeOldClient(client);

  delete client;
}

bool Server::addClient(BaseClientProxy *client)
{
  std::string name = getName(client);
  if (m_clients.contains(name)) {
    return false;
  }

  // add event handlers
  m_events->addHandler(EventTypes::ScreenShapeChanged, client->getEventTarget(), [this, client](const auto &) {
    handleShapeChanged(client);
  });
  m_events->addHandler(EventTypes::ClipboardGrabbed, client->getEventTarget(), [this, client](const auto &e) {
    handleClipboardGrabbed(e, client);
  });
  m_events->addHandler(EventTypes::ClipboardChanged, client->getEventTarget(), [this, client](const auto &e) {
    handleClipboardChanged(e, client);
  });

  // add to list
  m_clientSet.insert(client);
  m_clients.insert(std::make_pair(name, client));

  // initialize client data
  int32_t x;
  int32_t y;
  client->getCursorPos(x, y);
  client->setJumpCursorPos(x, y);

  // tell primary client about the active sides
  m_primaryClient->reconfigure(getActivePrimarySides());

  return true;
}

bool Server::removeClient(BaseClientProxy *client)
{
  using enum EventTypes;
  // return false if not in list
  ClientSet::iterator i = m_clientSet.find(client);
  if (i == m_clientSet.end()) {
    return false;
  }

  // remove event handlers
  m_events->removeHandler(ScreenShapeChanged, client->getEventTarget());
  m_events->removeHandler(ClipboardGrabbed, client->getEventTarget());
  m_events->removeHandler(ClipboardChanged, client->getEventTarget());

  // remove from list
  const std::string name = getName(client);
  m_clients.erase(name);
  m_clientSet.erase(i);

  // reset clipboard ownership so a late update from the removed screen
  // cannot reference a stale owner entry
  const std::string primaryName = getName(m_primaryClient);
  for (auto &clipboard : m_clipboards) {
    if (clipboard.m_clipboardOwner == name) {
      clipboard.m_clipboardOwner = primaryName;
    }
  }

  // if we were waiting to switch to this client, cancel the pending switch
  // before the client object is destroyed
  if (client == m_switchScreen) {
    stopSwitch();
  }

  return true;
}

void Server::closeClient(BaseClientProxy *client, const char *msg)
{
  assert(client != m_primaryClient);
  assert(msg != nullptr);

  // send message to client.  this message should cause the client
  // to disconnect.  we add this client to the closed client list
  // and install a timer to remove the client if it doesn't respond
  // quickly enough.  we also remove the client from the active
  // client list since we're not going to listen to it anymore.
  // note that this method also works on clients that are not in
  // the m_clients list.  adoptClient() may call us with such a
  // client.
  LOG_INFO("disconnecting client \"%s\"", getName(client).c_str());

  // send message
  // FIXME -- avoid type cast (kinda hard, though)
  auto clientProxy = static_cast<ClientProxy *>(client);
  clientProxy->close(msg);

  // install timer.  wait timeout seconds for client to close.
  double timeout = 5.0;
  EventQueueTimer *timer = m_events->newOneShotTimer(timeout, nullptr);
  m_events->addHandler(EventTypes::Timer, timer, [this, client](const auto &) { handleClientCloseTimeout(client); });

  // move client to closing list
  removeClient(client);

  m_oldClients.insert(std::make_pair(client, timer));

  // if this client is the active screen then we have to
  // jump off of it
  forceLeaveClient(client);
}

void Server::closeClients(const ServerConfig &config)
{
  // collect the clients that are connected but are being dropped
  // from the configuration (or who's canonical name is changing).
  using RemovedClients = std::set<BaseClientProxy *>;
  RemovedClients removed;
  for (auto index = m_clients.begin(); index != m_clients.end(); ++index) {
    if (!config.isCanonicalName(index->first)) {
      removed.insert(index->second);
    }
  }

  // don't close the primary client
  removed.erase(m_primaryClient);

  // now close them.  we collect the list then close in two steps
  // because closeClient() modifies the collection we iterate over.
  for (auto &client : removed) {
    closeClient(client, kMsgCClose);
  }
}

void Server::removeActiveClient(BaseClientProxy *client)
{
  if (removeClient(client)) {
    forceLeaveClient(client);
    m_events->removeHandler(EventTypes::ClientProxyDisconnected, client);
    if (m_clients.size() == 1 && m_oldClients.empty()) {
      m_events->addEvent(Event(EventTypes::ServerDisconnected, this));
    }
  }
}

void Server::removeOldClient(BaseClientProxy *client)
{
  using enum EventTypes;
  OldClients::iterator i = m_oldClients.find(client);
  if (i != m_oldClients.end()) {
    m_events->removeHandler(ClientProxyDisconnected, client);
    m_events->removeHandler(Timer, i->second);
    m_events->deleteTimer(i->second);
    m_oldClients.erase(i);
    if (m_clients.size() == 1 && m_oldClients.empty()) {
      m_events->addEvent(Event(ServerDisconnected, this));
    }
  }
}

void Server::forceLeaveClient(const BaseClientProxy *client)
{
  if (const auto *active = (m_activeSaver != nullptr) ? m_activeSaver : m_active; active == client) {
    // record new position (center of primary screen)
    m_primaryClient->getCursorCenter(m_x, m_y);

    // stop waiting to switch to this client
    if (active == m_switchScreen) {
      stopSwitch();
    }

    // don't notify active screen since it has probably already
    // disconnected.
    LOG(
        (CLOG_INFO "jump from \"%s\" to \"%s\" at %d,%d", getName(active).c_str(), getName(m_primaryClient).c_str(),
         m_x, m_y)
    );

    // cut over
    m_active = m_primaryClient;

    // enter new screen (unless we already have because of the
    // screen saver)
    if (m_activeSaver == nullptr) {
      m_primaryClient->enter(m_x, m_y, m_seqNum, m_primaryClient->getToggleMask(), false);
    }
  }

  // if this screen had the cursor when the screen saver activated
  // then we can't switch back to it when the screen saver
  // deactivates.
  if (m_activeSaver == client) {
    m_activeSaver = nullptr;
  }

  // tell primary client about the active sides
  m_primaryClient->reconfigure(getActivePrimarySides());
}
