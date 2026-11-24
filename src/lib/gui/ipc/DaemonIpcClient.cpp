/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DaemonIpcClient.h"

#include "common/Constants.h"

#include <QDebug>

namespace synergy::gui::ipc {
namespace {

QString daemonExecutable()
{
#ifdef Q_OS_WIN
  return QString::fromLatin1(kDaemonBinName) + QStringLiteral(".exe");
#else
  return QString::fromLatin1(kDaemonBinName);
#endif
}

} // namespace

DaemonIpcClient::DaemonIpcClient(QObject *parent)
    : IpcClient(parent, kDaemonIpcName, QStringLiteral("daemon"), daemonExecutable())
{
}

void DaemonIpcClient::sendLogLevel(const QString &logLevel)
{
  sendMessage(QStringLiteral("logLevel=%1").arg(logLevel));
}

void DaemonIpcClient::sendStartProcess(const QString &launchId, const QString &configFile)
{
  sendMessage(QStringLiteral("start=%1 %2").arg(launchId, configFile));
}

void DaemonIpcClient::sendStopProcess(const QString &launchId)
{
  sendMessage(QStringLiteral("stop=%1").arg(launchId));
}

void DaemonIpcClient::sendClearSettings()
{
  sendMessage(QStringLiteral("clearSettings"));
}

void DaemonIpcClient::requestLogPath()
{
  sendMessage(QStringLiteral("logPath"));
}

void DaemonIpcClient::processCommand(const QString &command, const QStringList &parts)
{
  if (parts.size() != 2) {
    return;
  }
  if (command == QStringLiteral("logPath")) {
    Q_EMIT logPathReceived(parts.at(1));
  } else if (command == QStringLiteral("stopAccepted")) {
    Q_EMIT stopAccepted(parts.at(1));
  }
}

} // namespace synergy::gui::ipc
