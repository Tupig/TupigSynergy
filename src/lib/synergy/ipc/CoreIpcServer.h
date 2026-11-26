/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcServer.h"
#include "common/Enums.h"

#include <QObject>
#include <QSet>

class QLocalSocket;

namespace synergy::core::ipc {

class CoreIpcServer : public IpcServer
{
  Q_OBJECT

public:
  explicit CoreIpcServer(QObject *parent, const QString &launchId = {});

  static CoreIpcServer &instance();
  void setConnectionState(ConnectionState state);
  void setServerBindAddress(const QString &address);

private:
  void clientAuthenticated(QLocalSocket *clientSocket) override;
  void processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts) override;
  ConnectionState m_connectionState = ConnectionState::Disconnected;
  QString m_serverBindAddress;
};

} // namespace synergy::core::ipc
