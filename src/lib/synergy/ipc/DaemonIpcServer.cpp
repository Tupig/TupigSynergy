/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DaemonIpcServer.h"

#include "base/Log.h"
#include "common/Constants.h"

#include <QLocalSocket>
#include <QUuid>

namespace synergy::core::ipc {

const auto kAckMessage = "ok";
const auto kErrorMessage = "error";

DaemonIpcServer::DaemonIpcServer(QObject *parent, const QString &logFilename)
    : IpcServer(parent, kDaemonIpcName, QStringLiteral("daemon")),
      m_logFilename(logFilename)
{
  // do nothing
}

void DaemonIpcServer::processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts)
{
  if (command == QStringLiteral("logLevel")) {
    processLogLevel(clientSocket, parts);
  } else if (command == QStringLiteral("configFile")) {
    processConfigFile(clientSocket, parts);
  } else if (command == QStringLiteral("start") || command == QStringLiteral("stop")) {
    const bool starting = command == QStringLiteral("start");
    QString launchId;
    QString configFile;
    if (parts.size() == 2) {
      const auto args = parts.at(1);
      const auto separator = args.indexOf(QLatin1Char(' '));
      if (starting && (separator < 0 || separator + 1 == args.size())) {
        writeToClientSocket(clientSocket, kErrorMessage);
        return;
      }
      launchId = starting ? args.left(separator) : args;
      if (starting)
        configFile = args.mid(separator + 1);
      const QUuid id(launchId);
      if (id.isNull() || id.toString(QUuid::WithoutBraces) != launchId) {
        writeToClientSocket(clientSocket, kErrorMessage);
        return;
      }
    }
    if (starting) {
      if (!configFile.isEmpty())
        Q_EMIT configFileChanged(configFile);
      Q_EMIT startRequested(launchId);
    } else {
      Q_EMIT stopRequested(launchId);
      if (!launchId.isEmpty()) {
        // Receipt means the daemon applied the stop request or had already
        // replaced it. The watchdog still owns the actual process shutdown.
        writeToClientSocket(clientSocket, QStringLiteral("stopAccepted=%1").arg(launchId));
        return;
      }
    }
    writeToClientSocket(clientSocket, kAckMessage);
  } else if (command == QStringLiteral("logPath")) {
    LOG_DEBUG("daemon ipc server got log path request");
    writeToClientSocket(clientSocket, QStringLiteral("logPath=%1").arg(m_logFilename));
  } else if (command == QStringLiteral("clearSettings")) {
    LOG_DEBUG("daemon ipc server got clear settings message");
    Q_EMIT clearSettingsRequested();
    writeToClientSocket(clientSocket, kAckMessage);
  } else {
    LOG_WARN("daemon ipc server got unknown command: %s", command.toUtf8().constData());
  }
}

void DaemonIpcServer::processLogLevel(QLocalSocket *&clientSocket, const QStringList &messageParts)
{
  if (messageParts.size() < 2) {
    LOG_ERR(QT_TRANSLATE_NOOP("EngineLogMessages", "daemon ipc server got invalid log level message"));
    writeToClientSocket(clientSocket, kErrorMessage);
    return;
  }

  const auto &logLevel = messageParts.at(1);
  if (logLevel.isEmpty()) {
    LOG_ERR(QT_TRANSLATE_NOOP("EngineLogMessages", "daemon ipc server got empty log level"));
    writeToClientSocket(clientSocket, kErrorMessage);
    return;
  }

  LOG_DEBUG("daemon ipc server got new log level: %s", logLevel.toUtf8().constData());
  Q_EMIT logLevelChanged(logLevel);
  writeToClientSocket(clientSocket, kAckMessage);
}

void DaemonIpcServer::processConfigFile(QLocalSocket *&clientSocket, const QStringList &messageParts)
{
  if (messageParts.size() < 2) {
    LOG_ERR(QT_TRANSLATE_NOOP("EngineLogMessages", "daemon ipc server got invalid config file message"));
    writeToClientSocket(clientSocket, kErrorMessage);
    return;
  }

  const auto &configFile = messageParts.at(1);
  if (configFile.isEmpty()) {
    LOG_ERR(QT_TRANSLATE_NOOP("EngineLogMessages", "daemon ipc server got empty config file path"));
    writeToClientSocket(clientSocket, kErrorMessage);
    return;
  }

  LOG_DEBUG("daemon ipc server got config file: %s", configFile.toUtf8().constData());
  Q_EMIT configFileChanged(configFile);
  writeToClientSocket(clientSocket, kAckMessage);
}

} // namespace synergy::core::ipc
