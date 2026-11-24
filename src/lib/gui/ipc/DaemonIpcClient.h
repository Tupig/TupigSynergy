/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcClient.h"

#include <QObject>

namespace synergy::gui::ipc {

class DaemonIpcClient : public IpcClient
{
  Q_OBJECT

public:
  explicit DaemonIpcClient(QObject *parent = nullptr);
  void sendLogLevel(const QString &logLevel);
  void sendStartProcess(const QString &launchId, const QString &configFile);
  void sendStopProcess(const QString &launchId);
  void sendClearSettings();
  void requestLogPath();

Q_SIGNALS:
  void logPathReceived(const QString &logPath);
  void stopAccepted(const QString &launchId);

protected:
  void processCommand(const QString &command, const QStringList &parts) override;
};

} // namespace synergy::gui::ipc
