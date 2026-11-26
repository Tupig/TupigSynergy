/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2024 - 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreProcess.h"

#include "common/ExitCodes.h"
#include "common/IpcToken.h"
#include "common/RelativePath.h"
#include "gui/ipc/CoreIpcClient.h"
#include "gui/ipc/DaemonIpcClient.h"

#if defined(Q_OS_MACOS)
#include "OSXHelpers.h"
#endif

#ifdef Q_OS_LINUX
#include <signal.h>
#include <sys/prctl.h>
#endif

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaEnum>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace synergy::gui {

const int kRetryDelay = 1000;

//! Upper bound for the blocking waitForStarted() in startForegroundProcess().
//! The GUI mutex is held for this wait, so it must stay short: 5s is ample for
//! a local exec, while the default 30s freezes the GUI on slow spawns.
constexpr int kStartTimeoutMs = 5000;

// The watchdog may spend up to ten seconds closing the previous process.
constexpr int kServiceStartTimeoutMs = 20000;
constexpr int kServiceIpcRetryMs = 500;
constexpr int kServiceCancelRetryMs = 2000;

const auto kLineSplitRegex = QRegularExpression("\r|\n|\r\n");

QString CoreProcess::processModeToString(const Settings::ProcessMode mode)
{
  return QVariant::fromValue(mode).toString().toLower();
}

QString CoreProcess::processStateToString(const CoreProcess::ProcessState state)
{
  return QVariant::fromValue(state).toString().toLower();
}

/**
 * @brief Wraps options that contain spaces in quotes
 *
 * Useful to handle things like paths with spaces (e.g. "C:\Program Files").
 *
 * Can also be used to create a representation of a command that can be pasted
 * into a terminal.
 */
QString CoreProcess::makeQuotedArgs(const QString &app, const QStringList &args)
{
  QStringList command = {app};
  command.append(args);

  static const auto quote = QStringLiteral("\"");
  static const auto space = QStringLiteral(" ");
  QStringList quoted;
  for (const auto &item : std::as_const(command)) {
    auto temp = item.simplified();
    if (const auto wrapped = (temp.startsWith(quote) && temp.endsWith(quote)); temp.contains(space) && !wrapped) {
      temp = QStringLiteral("%1%2%1").arg(quote, temp);
    }
    quoted.append(temp);
  }

  return quoted.join(space);
}

/**
 * @brief If IPv6, ensures the IP is surround in square brackets.
 */
QString CoreProcess::wrapIpv6(const QString &address)
{
  static const auto colon = QStringLiteral(":");
  static const auto openBracket = QStringLiteral("[");
  static const auto closeBracket = QStringLiteral("]");

  if (!address.contains(colon) || address.isEmpty()) {
    return address;
  }

  QString wrapped = address;

  if (!address.startsWith(openBracket)) {
    wrapped.prepend(openBracket);
  }

  if (!address.endsWith(closeBracket)) {
    wrapped.append(closeBracket);
  }

  return wrapped;
}

//
// CoreProcess
//

CoreProcess::CoreProcess(const synergy::gui::ServerConfig &serverConfig)
    : m_serverConfig(serverConfig),
      m_daemonIpcClient{new ipc::DaemonIpcClient(this)}
{
  m_appPath = QStringLiteral("%1/%2").arg(QCoreApplication::applicationDirPath(), kCoreBinName);
  if (!QFile::exists(m_appPath)) {
    qCritical().noquote() << tr("core server binary does not exist");
  }

  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::connected, this, &CoreProcess::daemonIpcClientConnected);
  const auto daemonUnavailable = [this] {
    if (m_pendingServiceStop.isEmpty())
      Q_EMIT daemonIpcClientConnectionFailed();
  };
  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::connectionFailed, this, daemonUnavailable);
  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::versionMismatch, this, [this, daemonUnavailable] {
    m_daemonIpcClient->disconnectFromServer();
    daemonUnavailable();
  });
  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::logPathReceived, this, &CoreProcess::setupDaemonLogTail);
  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::stopAccepted, this, [this](const QString &launchId) {
    if (launchId.isEmpty() || launchId != m_pendingServiceStop)
      return;
    m_pendingServiceStop.clear();
    m_serviceCancelTimer.stop();
    if (m_processState == ProcessState::Stopping)
      setProcessState(ProcessState::Stopped);
    sendPendingServiceCommands();
  });

  m_coreIpcTimer.setSingleShot(true);
  connect(&m_coreIpcTimer, &QTimer::timeout, this, &CoreProcess::connectCoreIpc);
  m_serviceStartTimer.setSingleShot(true);
  connect(&m_serviceStartTimer, &QTimer::timeout, this, [this] {
    qWarning().noquote() << tr("Failed to establish the core IPC connection");
    failServiceStart();
  });
  m_serviceCancelTimer.setInterval(kServiceCancelRetryMs);
  connect(&m_serviceCancelTimer, &QTimer::timeout, this, &CoreProcess::retryServiceCancellation);

  connect(&m_retryTimer, &QTimer::timeout, this, [this] {
    if (m_processState == ProcessState::RetryPending) {
      start();
    } else {
      qDebug("retry cancelled, process state is not retry pending");
    }
  });
}

void CoreProcess::onProcessReadyReadStandardOutput()
{
  if (m_process) {
    handleLogLines(m_process->readAllStandardOutput());
  }
}

void CoreProcess::onProcessReadyReadStandardError()
{
  if (m_process) {
    handleLogLines(m_process->readAllStandardError());
  }
}

void CoreProcess::daemonIpcClientConnected()
{
  applyLogLevel();
  m_daemonIpcClient->requestLogPath();
  sendPendingServiceCommands();
}

void CoreProcess::checkExistingProcess()
{
  qInfo().noquote() << tr("Checking the existing core");

  auto *client = new ipc::CoreIpcClient(this);
  connect(client, &ipc::CoreIpcClient::connected, this, [client] {
    qInfo().noquote() << tr("The existing core version matches; leaving it running");
    client->deleteLater();
  });
  connect(client, &ipc::CoreIpcClient::versionMismatch, this, [client] {
    qInfo().noquote() << tr("The existing core version does not match; requesting it to stop");
    client->sendStop();
  });
  connect(client, &ipc::CoreIpcClient::serverShutdown, this, [this, client] {
    qInfo().noquote() << tr("The existing core stopped successfully");
    client->deleteLater();
    setProcessState(ProcessState::RetryPending);
    m_retryTimer.setSingleShot(true);
    m_retryTimer.start(kRetryDelay);
  });
  connect(client, &ipc::CoreIpcClient::connectionFailed, this, [client] {
    qCritical().noquote() << tr("could not contact existing core");
    client->deleteLater();
  });
  client->connectToServer();
}

void CoreProcess::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
  using enum ProcessState;
  closeCoreIpc();
  clearConnectionState();

  if (m_retryTimer.isActive()) {
    m_retryTimer.stop();
  }

  // QProcess::close() 会终止进程；用户主动停止时不能把该退出误报为崩溃。
  if (m_processState == Stopping) {
    setProcessState(Stopped);
    qInfo().noquote() << tr("The core desktop process stopped");
    return;
  }

  if (exitStatus == QProcess::CrashExit) {
    setProcessState(Stopped);
    qWarning().noquote() << tr("The desktop process crashed");
    return;
  }

  if (exitCode != s_exitSuccess) {
    setProcessState(Stopped);
    if (exitCode == s_exitDuplicate) {
      checkExistingProcess();
      return;
    }
    qWarning().noquote() << tr("The desktop process exited with code: %1").arg(exitCode);
    return;
  }

  qDebug("desktop process exited normally");

  if (const auto wasStarted = m_processState == Started; wasStarted) {
    // a process that ran for a while counts as a healthy start; a quick exit
    // keeps accumulating the retry delay
    constexpr auto kStableUptime = std::chrono::seconds(10);
    if (std::chrono::steady_clock::now() - m_startedAt >= kStableUptime) {
      m_retryDelayMs = kRetryDelayMs;
    }
    qDebug("desktop process was running, retrying in %d ms", m_retryDelayMs);
    setProcessState(RetryPending);
    m_retryTimer.setSingleShot(true);
    m_retryTimer.start(m_retryDelayMs);
    m_retryDelayMs = std::min(m_retryDelayMs * 2, kMaxRetryDelayMs);
  } else {
    setProcessState(Stopped);
  }
}

void CoreProcess::applyLogLevel()
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  if (processMode == ProcessMode::Service) {
    qDebug() << "setting daemon log level:" << Settings::logLevelText();
    m_daemonIpcClient->sendLogLevel(Settings::logLevelText());
  }
}

QStringList CoreProcess::desktopProcessArguments() const
{
  // CLI tokens are not display text. Pin the core to the GUI's exact selected
  // settings file; the service path forwards its configuration separately.
  return {
      m_mode == Settings::CoreMode::Server ? QStringLiteral("server") : QStringLiteral("client"),
      QStringLiteral("--settings"), Settings::settingsFile()
  };
}

void CoreProcess::startForegroundProcess(const QStringList &args)
{
  using enum ProcessState;

  if (m_processState != Starting) {
    qCritical().noquote() << tr("core process must be in starting state");
    return;
  }

  // only make quoted args for printing the command for convenience; so that the
  // core command can be easily copy/pasted to the terminal for testing.
  const auto quoted = makeQuotedArgs(m_appPath, args);
  qInfo().noquote() << tr("Running command: %1").arg(quoted);

#ifdef Q_OS_LINUX
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  m_process->setChildProcessModifier([] {
    // the core process becomes orphaned when the gui process exits abruptly (e.g. with kill -9),
    // so ensure the os also kills the core when that happens to the gui.
    prctl(PR_SET_PDEATHSIG, SIGTERM);
  });
#endif
#endif

  m_process->start(m_appPath, args);

  if (m_process->waitForStarted(kStartTimeoutMs)) {
    m_startedAt = std::chrono::steady_clock::now();
    setProcessState(Started);
  } else {
    clearConnectionState();
    setProcessState(Stopped);
    Q_EMIT error(Error::StartFailed);
  }
}

void CoreProcess::startProcessFromDaemon()
{
  if (m_processState != ProcessState::Starting) {
    qCritical().noquote() << tr("core process must be in starting state");
    return;
  }

  Settings::save(false);
  const auto settingsDir = Settings::settingsPath();
  auto configFile = synergy::relativeToDirectory(settingsDir, Settings::settingsFile());
  if (configFile.isEmpty()) {
    const auto source = Settings::settingsFile();
    const auto fileName = QFileInfo(source).fileName();
    const auto dest = synergy::resolveRelativePath(settingsDir, fileName);
    if (dest.isEmpty() || !QFileInfo::exists(source) || !QDir().mkpath(settingsDir)) {
      qCritical().noquote() << tr("cannot copy settings into %1").arg(settingsDir);
      failServiceStart();
      return;
    }
    // harden the machine-wide directory before copying so that other local
    // users cannot tamper with the daemon's config; best effort, the copied
    // file is explicitly restricted below
    if (!synergy::restrictSystemDirectoryPermissions(settingsDir)) {
      qWarning().noquote() << tr("Failed to restrict the settings directory: %1").arg(settingsDir);
    }
    if ((QFileInfo::exists(dest) && !QFile::remove(dest)) || !QFile::copy(source, dest)) {
      qCritical().noquote() << tr("cannot copy settings into %1").arg(settingsDir);
      failServiceStart();
      return;
    }
    // the config that the SYSTEM service executes must not be modifiable by
    // other local users (it can carry screen enter/exit commands)
    if (!synergy::restrictSecretFilePermissions(dest)) {
      qCritical().noquote() << tr("cannot restrict the copied settings file: %1").arg(dest);
      failServiceStart();
      return;
    }
    configFile = fileName;
  }
  qInfo().noquote() << tr("Sending start request to the service (configuration: %1)").arg(configFile);

  m_serviceLaunchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  m_pendingServiceConfig = configFile;
  m_serviceStartTimer.start(kServiceStartTimeoutMs);
  if (m_daemonIpcClient->isConnected()) {
    sendPendingServiceCommands();
  } else {
    m_daemonIpcClient->connectToServer();
  }
}

void CoreProcess::sendPendingServiceCommands()
{
  if (!m_pendingServiceStop.isEmpty()) {
    m_daemonIpcClient->sendStopProcess(m_pendingServiceStop);
    return;
  }
  if (m_processState == ProcessState::Starting && !m_pendingServiceConfig.isEmpty()) {
    m_daemonIpcClient->sendStartProcess(m_serviceLaunchId, std::exchange(m_pendingServiceConfig, {}));
    m_coreIpcTimer.start(kServiceIpcRetryMs);
  }
}

void CoreProcess::retryServiceCancellation()
{
  if (m_daemonIpcClient->isConnected()) {
    sendPendingServiceCommands();
  } else {
    m_daemonIpcClient->connectToServer();
  }
}

void CoreProcess::failServiceStart()
{
  stopProcessFromDaemon();
  Q_EMIT error(Error::StartFailed);
}

void CoreProcess::stopForegroundProcess()
{
  if (m_processState != ProcessState::Stopping) {
    qCritical().noquote() << tr("core process must be in stopping state");
    return;
  }

  if (!m_process || m_process->state() == QProcess::NotRunning) {
    qDebug("process is not running, skipping terminate");
    setProcessState(ProcessState::Stopped);
    return;
  }

  qDebug("process is running or starting, closing");
  m_process->close();
  if (m_process->state() == QProcess::NotRunning) {
    setProcessState(ProcessState::Stopped);
  }
}

void CoreProcess::stopProcessFromDaemon()
{
  m_serviceStartTimer.stop();
  const auto launchId = std::exchange(m_serviceLaunchId, {});
  const bool dispatched = !launchId.isEmpty() && m_pendingServiceConfig.isEmpty();
  m_pendingServiceConfig.clear();
  closeCoreIpc();
  clearConnectionState();
  if (dispatched) {
    // Only a dispatched request needs cancellation in the daemon.
    m_pendingServiceStop = launchId;
    m_serviceCancelTimer.start();
  }
  if (!m_pendingServiceStop.isEmpty()) {
    setProcessState(ProcessState::Stopping);
    retryServiceCancellation();
  } else {
    setProcessState(ProcessState::Stopped);
  }
}

void CoreProcess::connectCoreIpc()
{
  auto *client = new ipc::CoreIpcClient(this, m_serviceLaunchId);
  m_coreIpcClient = client;
  connect(client, &ipc::CoreIpcClient::commandReceived, this, &CoreProcess::onCoreIpcMessageReceived);
  connect(client, &ipc::CoreIpcClient::connected, this, [this] {
    qDebug("connected to core ipc server");
    if (m_serviceStartTimer.isActive()) {
      m_serviceStartTimer.stop();
      setProcessState(ProcessState::Started);
    }
  });
  const auto disconnected = [this] {
    closeCoreIpc();
    if (!m_serviceLaunchId.isEmpty()) {
      if (m_processState == ProcessState::Started) {
        clearConnectionState();
      }
      m_coreIpcTimer.start(kServiceIpcRetryMs);
    } else {
      qWarning().noquote() << tr("Failed to establish the core IPC connection");
    }
  };
  connect(client, &ipc::CoreIpcClient::connectionFailed, this, disconnected);
  connect(client, &ipc::CoreIpcClient::versionMismatch, this, disconnected);
  connect(client, &ipc::CoreIpcClient::serverShutdown, this, [this, disconnected] {
    qDebug("core ipc server shut down cleanly");
    if (!m_serviceLaunchId.isEmpty()) {
      disconnected();
    }
  });
  client->connectToServer();
}

void CoreProcess::closeCoreIpc()
{
  m_coreIpcTimer.stop();
  if (auto *client = std::exchange(m_coreIpcClient, nullptr)) {
    client->disconnect(this);
    client->disconnectFromServer();
    client->deleteLater();
  }
}

void CoreProcess::clearConnectionState()
{
  setConnectionState(ConnectionState::Disconnected);
  if (!m_secureSocketVersion.isEmpty()) {
    m_secureSocketVersion.clear();
    Q_EMIT securityLevelChanged({});
  }
  Q_EMIT connectedClientsChanged({});
  Q_EMIT serverBindAddressChanged({});
}

void CoreProcess::handleLogLines(const QString &text)
{
  const auto lines = text.split(kLineSplitRegex);
  for (const auto &line : lines) {
    if (line.isEmpty()) {
      continue;
    }

#if defined(Q_OS_MACOS)
    // HACK: macOS 10.13.4+ spamming error lines in logs making them
    // impossible to read and debug; giving users a red herring.
    if (line.contains("calling TIS/TSM in non-main thread environment")) {
      continue;
    }

    // the core process is not allowed to show the permission prompt
    // (called "notification permission") and the notification log line is emitted from
    // deep inside cocoa code in the core binary to stdout, so it can't be sent over
    // ipc from the core to the gui and instead the gui has to parse the core output.
    static const QString needle = "OSX Notification: ";
    if (line.contains(needle) && line.contains('|')) {
      const int delimiterPosition = line.indexOf('|');
      const int start = line.indexOf(needle);
      const QString title = line.mid(start + needle.length(), delimiterPosition - start - needle.length());
      const QString body = line.mid(delimiterPosition + 1, line.length() - delimiterPosition);
      if (!showOSXNotification(title, body)) {
        qDebug("osx notification was not shown");
      }
    }
#endif

    Q_EMIT logLine(line);
  }
}

void CoreProcess::start(std::optional<ProcessMode> processModeOption)
{
  if (m_processState == ProcessState::Started || m_processState == ProcessState::Starting) {
    qCritical().noquote() << tr("core process already started");
    return;
  }

  if (m_mode == Settings::CoreMode::None) {
    qCritical().noquote() << tr("core mode is not set, skipping core start");
    return;
  }

  QMutexLocker locker(&m_processMutex);

  const auto currentMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  const auto processMode = processModeOption.value_or(currentMode);
  const auto coreMode = m_mode == Settings::CoreMode::Server ? tr("server") : tr("client");
  const auto modeLabel = processMode == ProcessMode::Service ? tr("service mode") : tr("desktop mode");
  qInfo().noquote() << tr("Starting %1 process (%2)").arg(coreMode, modeLabel);

  m_lastProcessMode = processMode;
  m_retryTimer.stop();
  setProcessState(ProcessState::Starting);

#ifdef Q_OS_MACOS
  requestOSXNotificationPermission();
#endif

  setConnectionState(ConnectionState::Connecting);

  if (processMode == ProcessMode::Desktop) {
    // a previous run may have left a finished QProcess behind; drop it so
    // retry cycles do not accumulate process objects and signal connections
    if (m_process != nullptr) {
      m_process->disconnect(this);
      m_process->deleteLater();
      m_process = nullptr;
    }
    m_process = new QProcess(this);
    connect(
        m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &CoreProcess::onProcessFinished,
        Qt::UniqueConnection
    );
    connect(
        m_process, &QProcess::readyReadStandardOutput, this, &CoreProcess::onProcessReadyReadStandardOutput,
        Qt::UniqueConnection
    );
    connect(
        m_process, &QProcess::readyReadStandardError, this, &CoreProcess::onProcessReadyReadStandardError,
        Qt::UniqueConnection
    );
  }

  if (m_mode == Settings::CoreMode::Server) {
    const auto [hasNeededPermissions, configFilename] = persistServerConfig();
    if (configFilename.isEmpty()) {
      qCritical().noquote() << tr("config file name empty for server args");
      setProcessState(ProcessState::Stopped);
      setConnectionState(ConnectionState::Disconnected);
      Q_EMIT error(Error::StartFailed);
      return;
    }
    if (!hasNeededPermissions) {
      setProcessState(ProcessState::Stopped);
      setConnectionState(ConnectionState::Disconnected);
      Q_EMIT error(Error::StartFailed);
      return;
    }
    qInfo().noquote() << tr("Core configuration file: %1").arg(configFilename);
  }

  qDebug().noquote() << "log level:" << Settings::logLevelText();

  if (Settings::value(Settings::Log::ToFile).toBool()) {
    const auto logFile = Settings::value(Settings::Log::File).toString();
    QDir(QFileInfo(logFile).absolutePath()).mkpath(".");
    qInfo().noquote() << tr("Log file: %1").arg(logFile);
  }

  if (processMode == ProcessMode::Desktop) {
    startForegroundProcess(desktopProcessArguments());
    if (m_processState == ProcessState::Started) {
      m_coreIpcTimer.start(kRetryDelay);
    }
  } else if (processMode == ProcessMode::Service) {
    startProcessFromDaemon();
  }
}

void CoreProcess::stop(std::optional<ProcessMode> processModeOption)
{
  QMutexLocker locker(&m_processMutex);
  m_retryTimer.stop();

  const auto currentMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  const auto processMode = processModeOption.value_or(m_lastProcessMode.value_or(currentMode));

  const auto modeLabel = processMode == ProcessMode::Service ? tr("service mode") : tr("desktop mode");
  qInfo().noquote() << tr("Stopping core process (%1)").arg(modeLabel);

  if (m_processState != ProcessState::Stopped) {
    setProcessState(ProcessState::Stopping);

    if (processMode == ProcessMode::Service) {
      stopProcessFromDaemon();
    } else if (processMode == ProcessMode::Desktop) {
      closeCoreIpc();
      clearConnectionState();
      stopForegroundProcess();
    }

  } else {
    qWarning().noquote() << tr("The core process is already stopped");
  }
}

void CoreProcess::restart()
{
  qDebug("restarting core process");

  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();

  if (m_lastProcessMode != std::nullopt && m_lastProcessMode != processMode) {
    const auto debugMessage =
        QStringLiteral("process mode changed to %1, stopping %2 process")
            .arg(processModeToString(processMode), processModeToString(m_lastProcessMode.value()));
    qDebug().noquote() << debugMessage;
    stop(m_lastProcessMode);
  } else {
    // in service mode: though there is technically no need to stop the service
    // before restarting it, it does make for cleaner process state tracking,
    // especially if something goes wrong with starting the service.
    stop();
  }

  start();
}

void CoreProcess::cleanup()
{
  qInfo().noquote() << tr("Cleaning up the core process");

  const auto isDesktop = m_lastProcessMode == ProcessMode::Desktop;
  if (m_processState == ProcessState::Starting || (isDesktop && m_processState == ProcessState::Started)) {
    stop();
  }
  if (!m_pendingServiceStop.isEmpty() && m_daemonIpcClient->isConnected()) {
    m_daemonIpcClient->sendStopProcess(m_pendingServiceStop);
    m_daemonIpcClient->flushPendingWrites();
  }
}

#ifdef Q_OS_MACOS
bool CoreProcess::stopDesktopForUpdate()
{
  QMutexLocker locker(&m_processMutex);
  m_retryTimer.stop();
  if (m_lastProcessMode != ProcessMode::Desktop)
    return true;

  // The only process we may terminate is the child this GUI started.
  if (m_process && m_process->state() != QProcess::NotRunning) {
    setProcessState(ProcessState::Stopping);
    closeCoreIpc();
    clearConnectionState();
    m_process->terminate();
    if (m_process->state() != QProcess::NotRunning && !m_process->waitForFinished(2000)) {
      m_process->kill();
      if (m_process->state() != QProcess::NotRunning && !m_process->waitForFinished(1000))
        return false;
    }
  }
  closeCoreIpc();
  clearConnectionState();
  setProcessState(ProcessState::Stopped);
  return true;
}
#endif

QPair<bool, QString> CoreProcess::persistServerConfig() const
{
  if (Settings::value(Settings::Server::ExternalConfig).toBool()) {
    return {Settings::isServerConfigFileReadable(), Settings::value(Settings::Server::ExternalConfigFile).toString()};
  }

  const auto configFilePath = Settings::defaultValue(Settings::Server::ExternalConfigFile).toString();
  if (!m_serverConfig.save(configFilePath)) {
    qWarning().noquote() << tr("Failed to persist the core configuration: %1").arg(configFilePath);
    return {false, configFilePath};
  }
  return {Settings::isServerConfigFileReadable(), configFilePath};
}

void CoreProcess::setConnectionState(ConnectionState state)
{
  if (m_connectionState == state) {
    return;
  }

  m_connectionState = state;
  Q_EMIT connectionStateChanged(state);
}

void CoreProcess::setProcessState(ProcessState state)
{
  if (m_processState == state) {
    return;
  }

  qDebug(
      "core process state changed: %s -> %s", //
      qPrintable(processStateToString(m_processState)), qPrintable(processStateToString(state))
  );
  m_processState = state;
  Q_EMIT processStateChanged(state);
}

void CoreProcess::onCoreIpcMessageReceived(const QString &command, const QString &args)
{
  if (command == "connectionState") {
    const auto metaEnum = QMetaEnum::fromType<ConnectionState>();
    bool ok = false;
    const auto state = static_cast<ConnectionState>(metaEnum.keyToValue(args.toUtf8().constData(), &ok));
    if (!ok) {
      qWarning().noquote() << tr("Core IPC received an unknown connection state: %1").arg(args);
      return;
    }
    setConnectionState(state);
  } else if (command == "connectedClients") {
    const auto clients = args.isEmpty() ? QStringList() : args.split(",");
    Q_EMIT connectedClientsChanged(clients);
  } else if (command == "serverBindAddress") {
    Q_EMIT serverBindAddressChanged(args);
  } else if (command == "secureSocket") {
    Q_EMIT secureSocket(true);
    if (args != m_secureSocketVersion) {
      m_secureSocketVersion = args;
      Q_EMIT securityLevelChanged(args);
    }
  } else if (command == "unrecognisedClient") {
    Q_EMIT unrecognisedClient(args);
  } else if (command == "connectionRefused") {
    const auto metaEnum = QMetaEnum::fromType<synergy::core::ConnectionRefusal>();
    bool ok = false;
    const auto reason =
        static_cast<synergy::core::ConnectionRefusal>(metaEnum.keyToValue(args.toUtf8().constData(), &ok));
    if (ok) {
      Q_EMIT connectionRefused(reason);
    } else {
      qWarning().noquote() << tr("Core IPC received an unknown connection refusal: %1").arg(args);
    }
  } else if (command == "retryIn") {
    Q_EMIT retryIn(args.toInt());
  } else if (command == "peerFingerprint") {
    Q_EMIT peerFingerprint(args);
  } else if (command == "missingKeyboardLayouts") {
    Q_EMIT missingKeyboardLayouts(args);
  }
}

bool CoreProcess::checkSecureSocket(const QString &line)
{
  static const QString tlsCheckString = "network encryption protocol: ";
  const auto index = line.indexOf(tlsCheckString, 0, Qt::CaseInsensitive);
  if (index == -1) {
    return false;
  }

  Q_EMIT secureSocket(true);
  if (const auto ssv = line.mid(index + tlsCheckString.size()); ssv != m_secureSocketVersion) {
    m_secureSocketVersion = ssv;
    Q_EMIT securityLevelChanged(ssv);
  }

  return true;
}

QString CoreProcess::correctedAddress(const QString &address) const
{
  return wrapIpv6(address.simplified());
}

void CoreProcess::setupDaemonLogTail(const QString &logPath)
{
  qDebug() << "daemon log path:" << logPath;

  if (QFileInfo logFile(logPath); !logFile.isFile()) {
    auto file = QFile(logPath);
    if (!file.open(QFile::ReadWrite)) {
      qCritical().noquote() << tr("Cannot write to the service log file: %1").arg(logPath);
      return;
    }
    file.write(""); // Create an empty file
  }

  if (m_daemonFileTail) {
    m_daemonFileTail->setWatchedFile(logPath);
  } else {
    m_daemonFileTail = new FileTail(logPath, this);
    connect(m_daemonFileTail, &FileTail::newLine, this, &CoreProcess::handleLogLines);
  }
}

void CoreProcess::clearSettings()
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  if (processMode == ProcessMode::Desktop) {
    qDebug("no core settings to clear in desktop mode");
    return;
  }

  if (processMode != ProcessMode::Service) {
    qCritical().noquote() << tr("invalid process mode");
    return;
  }

  qInfo().noquote() << tr("Clearing core settings through the daemon");
  m_daemonIpcClient->sendClearSettings();
}

void CoreProcess::retryDaemon()
{
  m_daemonIpcClient->connectToServer();
}

} // namespace synergy::gui
