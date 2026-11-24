/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcServer.h"

#include "base/Log.h"
#include "common/Constants.h"

#include <QLocalSocket>

namespace synergy::core::ipc {

static CoreIpcServer *s_instance = nullptr;

static QString serverBindAddressMessage(const QString &address)
{
  return address.isEmpty() ? QStringLiteral("serverBindAddress") : QStringLiteral("serverBindAddress=%1").arg(address);
}

CoreIpcServer::CoreIpcServer(QObject *parent, const QString &launchId)
    : IpcServer(parent, kCoreIpcName, QStringLiteral("core"), launchId)
{
  assert(s_instance == nullptr);
  s_instance = this;
}

CoreIpcServer &CoreIpcServer::instance()
{
  assert(s_instance != nullptr);
  return *s_instance;
}

void CoreIpcServer::setServerBindAddress(const QString &address)
{
  if (address == m_serverBindAddress)
    return;
  m_serverBindAddress = address;
  broadcastToAuthenticatedClients(serverBindAddressMessage(address));
}

void CoreIpcServer::clientAuthenticated(QLocalSocket *clientSocket)
{
  if (!m_serverBindAddress.isEmpty())
    writeToClientSocket(clientSocket, serverBindAddressMessage(m_serverBindAddress));
}

void CoreIpcServer::processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts)
{
  Q_UNUSED(parts)
  if (command == QStringLiteral("stop")) {
    LOG_DEBUG("core ipc server got stop message");
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    broadcastCommand(QStringLiteral("bye"));
    Q_EMIT stopProcessRequested();
    return;
  }
  LOG_WARN("core ipc server got unknown command: %s", command.toUtf8().constData());
}

} // namespace synergy::core::ipc
