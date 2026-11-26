/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpc.h"

#include "CoreIpcServer.h"

void ipcSendToClient(const QString &command, const QString &args)
{
  // Queued because callers may not be on the main thread,
  // and QLocalSocket can only be written to from its owning thread.
  auto &server = synergy::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [command, args] { synergy::core::ipc::CoreIpcServer::instance().broadcastCommand(command, args); },
      Qt::QueuedConnection
  );
}

void ipcSendConnectionState(synergy::core::ConnectionState state)
{
  auto &server = synergy::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(&server, [&server, state] { server.setConnectionState(state); }, Qt::QueuedConnection);
}

void ipcSendServerBindAddress(const QString &address)
{
  auto &server = synergy::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [&server, address] { server.setServerBindAddress(address); }, Qt::QueuedConnection
  );
}
