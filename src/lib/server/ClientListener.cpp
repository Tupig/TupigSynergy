/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientListener.h"
#include "server/Server.h"

#include "PacketStreamFilter.h"
#include "arch/Arch.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "net/IDataSocket.h"
#include "net/IListenSocket.h"
#include "net/ISocketFactory.h"
#include "net/SocketException.h"
#include "server/ClientProxy.h"
#include "server/ClientProxyUnknown.h"

//
// ClientListener
//

namespace {

//! Upper bound on concurrent connections (active and still handshaking).
//! Refusing the excess keeps a flood of unauthenticated connections from
//! exhausting descriptors and memory.
constexpr size_t kMaxConnections = 32;

//! First-packet (hello) timeout for a newly accepted connection.  A hello is
//! tiny, so a short deadline limits how long silent connections hold resources.
constexpr double kHandshakeTimeoutSeconds = 10.0;

class ListenerPacketStream final : public PacketStreamFilter
{
public:
  ListenerPacketStream(
      IEventQueue *events, std::unique_ptr<IDataSocket> socket, std::shared_ptr<std::size_t> activeConnectionCount
  )
      : PacketStreamFilter(events, socket.get(), false),
        m_activeConnectionCount(std::move(activeConnectionCount))
  {
    adoptStream();
    socket.release();
    ++*m_activeConnectionCount;
  }

  ~ListenerPacketStream() override
  {
    --*m_activeConnectionCount;
  }

private:
  std::shared_ptr<std::size_t> m_activeConnectionCount;
};

} // namespace

ClientListener::ClientListener(
    const NetworkAddress &address, std::unique_ptr<ISocketFactory> socketFactory, IEventQueue *events,
    SecurityLevel securityLevel
)
    : m_socketFactory{std::move(socketFactory)},
      m_events(events),
      m_securityLevel(securityLevel),
      m_address(address)
{
  if (m_socketFactory == nullptr) {
    throw SocketException(QStringLiteral("socket factory is null"));
  }

  try {
    start();
  } catch (SocketAddressInUseException &) {
    cleanupListenSocket();
    m_socketFactory.reset();
    throw;
  } catch (BaseException &) {
    cleanupListenSocket();
    m_socketFactory.reset();
    throw;
  }
  LOG_VERBOSE("listening for clients");
}

ClientListener::~ClientListener()
{
  stop();
  m_socketFactory.reset();
}

void ClientListener::setServer(Server *server)
{
  if (server == nullptr) {
    throw SocketException(QStringLiteral("server is null"));
  }
  m_server = server;
}

ClientProxy *ClientListener::getNextClient()
{
  ClientProxy *client = nullptr;
  if (!m_waitingClients.empty()) {
    client = m_waitingClients.front();
    m_waitingClients.pop_front();
    m_events->removeHandler(EventTypes::ClientProxyDisconnected, client);
  }
  return client;
}

void ClientListener::start()
{
  m_listen = m_socketFactory->createListen(ARCH->getAddrFamily(m_address.getAddress()), m_securityLevel);

  // setup event handler
  m_events->addHandler(EventTypes::ListenSocketConnecting, m_listen.get(), [this](const auto &) {
    handleClientConnecting();
  });

  // bind listen address
  LOG_VERBOSE("binding listen socket");
  m_listen->bind(m_address);
}

void ClientListener::stop()
{
  using enum EventTypes;
  LOG_VERBOSE("stop listening for clients");

  // discard already connected clients
  for (auto index = m_newClients.begin(); index != m_newClients.end(); ++index) {
    ClientProxyUnknown *client = *index;
    m_events->removeHandler(ClientProxyUnknownSuccess, client);
    m_events->removeHandler(ClientProxyUnknownFailure, client);
    m_events->removeHandler(ClientProxyDisconnected, client);
    delete client;
  }

  // discard waiting clients
  ClientProxy *client = getNextClient();
  while (client != nullptr) {
    delete client;
    client = getNextClient();
  }

  m_events->removeHandler(ListenSocketConnecting, m_listen.get());
  cleanupListenSocket();
  cleanupClientSockets();
}

void ClientListener::removeUnknownClient(ClientProxyUnknown *unknownClient)
{
  if (unknownClient) {
    m_events->removeHandler(EventTypes::ClientProxyUnknownSuccess, unknownClient);
    m_events->removeHandler(EventTypes::ClientProxyUnknownFailure, unknownClient);
    m_newClients.erase(unknownClient);
    delete unknownClient;
  }
}

void ClientListener::handleClientConnecting()
{
  // accept client connection
  auto socket = m_listen->accept();

  if (!socket)
    return;

  const auto connections = connectionCount();
  if (connections >= kMaxConnections) {
    LOG_WARN("refusing client connection: %zu connections already active or pending", connections);
    return;
  }

  auto *rawSocketPointer = socket.get();
  m_clientSockets.emplace(rawSocketPointer, std::move(socket));

  m_events->addHandler(
      EventTypes::ClientListenerAccepted, rawSocketPointer->getEventTarget(),
      [this, rawSocketPointer](const auto &) { handleClientAccepted(rawSocketPointer); }
  );

  m_events->addHandler(
      EventTypes::ClientListenerDisconnectedOnAccept, rawSocketPointer->getEventTarget(),
      [this, rawSocketPointer](const auto &) {
        LOG_DEBUG("disconnected client before accept");
        removeClientSocket(rawSocketPointer);
      }
  );

  // When using non SSL, server accepts clients immediately, while SSL
  // has to call secure accept which may require retry
  if (m_securityLevel == SecurityLevel::PlainText) {
    m_events->addEvent(Event(EventTypes::ClientListenerAccepted, rawSocketPointer->getEventTarget()));
  }
}

void ClientListener::handleClientAccepted(IDataSocket *socket)
{
  if (m_server == nullptr) {
    throw SocketException(QStringLiteral("client listener has no server"));
  }

  // Transfer the socket and its capacity slot to the protocol stream.
  auto pending = m_clientSockets.extract(socket);
  if (pending.empty()) {
    return;
  }
  LOG_INFO(QT_TRANSLATE_NOOP("EngineLogMessages", "accepted client connection"));
  auto stream = std::make_unique<ListenerPacketStream>(m_events, std::move(pending.mapped()), m_activeConnectionCount);

  // create proxy for unknown client
  auto *client = new ClientProxyUnknown(stream.get(), kHandshakeTimeoutSeconds, m_server, m_events);
  stream.release();

  m_newClients.insert(client);

  // watch for events from unknown client
  m_events->addHandler(EventTypes::ClientProxyUnknownSuccess, client, [this, client](const auto &) {
    handleUnknownClient(client);
  });
  m_events->addHandler(EventTypes::ClientProxyUnknownFailure, client, [this, client](const auto &) {
    removeUnknownClient(client);
  });
}

void ClientListener::handleUnknownClient(ClientProxyUnknown *unknownClient)
{
  // we should have the client in our new client list
  assert(m_newClients.count(unknownClient) == 1);

  // get the real client proxy and install it
  if (auto client = unknownClient->orphanClientProxy(); client) {
    // handshake was successful
    m_waitingClients.push_back(client);
    m_events->addEvent(Event(EventTypes::ClientListenerAccepted, this));

    // watch for client to disconnect while it's in our queue
    m_events->addHandler(EventTypes::ClientProxyDisconnected, client, [this, client](const auto &) {
      handleClientDisconnected(client);
    });
  } else {
    auto *stream = unknownClient->getStream();
    if (stream) {
      stream->close();
    }
  }

  // now finished with unknown client
  removeUnknownClient(unknownClient);
}

void ClientListener::handleClientDisconnected(ClientProxy *client)
{
  // find client in waiting clients queue
  for (WaitingClients::iterator i = m_waitingClients.begin(), n = m_waitingClients.end(); i != n; ++i) {
    if (*i == client) {
      m_waitingClients.erase(i);
      m_events->removeHandler(EventTypes::ClientProxyDisconnected, client);

      delete client;

      break;
    }
  }
}

void ClientListener::removeClientSocket(IDataSocket *socket)
{
  auto pending = m_clientSockets.find(socket);
  if (pending == m_clientSockets.end()) {
    return;
  }
  m_events->removeHandlers(socket->getEventTarget());
  m_clientSockets.erase(pending);
}

void ClientListener::cleanupListenSocket()
{
  m_listen.reset();
}

void ClientListener::cleanupClientSockets()
{
  for (const auto &client : m_clientSockets) {
    m_events->removeHandlers(client.first->getEventTarget());
  }
  m_clientSockets.clear();
}
