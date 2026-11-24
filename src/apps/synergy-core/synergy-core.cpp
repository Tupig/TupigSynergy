/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 - 2016, 2025 - 2026 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreArgParser.h"

#include "ClientApp.h"
#include "ServerApp.h"
#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "common/Constants.h"
#include "common/ExitCodes.h"
#include "common/IpcToken.h"
#include "ipc/CoreIpcServer.h"

#if defined(Q_OS_WIN)
#include "arch/win32/ArchMiscWindows.h"
#include <wtsapi32.h>
#endif

#include <QApplication>
#include <QFileInfo>
#if !defined(Q_OS_MACOS)
#include <QSharedMemory>
#endif
#include <QTextStream>
#include <QThread>
#include <QTimer>

#if defined(Q_OS_MACOS)
#include "platform/macos/InstanceGuard.h"
#include <ApplicationServices/ApplicationServices.h>
#endif

#include <memory>

void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
  const auto utf8 = message.toUtf8();
  switch (type) {
  case QtDebugMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_DEBUG "%s", utf8.constData());
    break;
  case QtInfoMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_INFO "%s", utf8.constData());
    break;
  case QtWarningMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_WARN "%s", utf8.constData());
    break;
  case QtCriticalMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_ERR "%s", utf8.constData());
    break;
  case QtFatalMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_CRIT "%s", utf8.constData());
    break;
  }
  if (type == QtFatalMsg) {
    abort();
  }
}

void showHelp(const CoreArgParser &parser)
{
  QTextStream(stdout) << parser.helpText();
}

App *createApp(const CoreArgParser &parser, EventQueue &events, const QString &processName)
{
  if (parser.serverMode()) {
    return new ServerApp(&events, processName);
  } else if (parser.clientMode()) {
    return new ClientApp(&events, processName);
  }
  return nullptr;
}

int main(int argc, char **argv)
{
  QStringList args;
#if defined(Q_OS_WIN)
  {
    // HACK to make sure settings gets the correct qApp path
    QCoreApplication m(argc, argv);
    // Qt reads the native Unicode command line on Windows.
    args = QCoreApplication::arguments();
  }

  ArchMiscWindows::setInstanceWin32(GetModuleHandle(nullptr));
#else
  for (int i = 0; i < argc; i++)
    args.append(argv[i]);
#endif

  Arch arch;
  arch.init();

  Log log;
  qInstallMessageHandler(qtMessageHandler);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  // Qt5's QCommandLineParser::helpText() dereferences QCoreApplication, but the real
  // QApplication is built further down (it needs a display); use a display-less app for
  // the help/version fast paths so they don't crash headless.
  auto earlyApp = std::make_unique<QCoreApplication>(argc, argv);
#endif

  CoreArgParser parser(args);

  // Print any parser errors
  if (!parser.errorText().isEmpty()) {
    QTextStream(stdout) << parser.errorText() << "\n";
  }

  if (parser.help()) {
    showHelp(parser);
    return s_exitSuccess;
  }

  if (parser.version()) {
    QTextStream(stdout) << parser.versionText();
    return s_exitSuccess;
  }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  earlyApp.reset();
#endif

#if defined(Q_OS_MACOS)
  using synergy::platform::macos::InstanceStatus;
  const auto instance = synergy::platform::macos::acquireCoreInstance(parser.singleInstanceOnly());
  if (instance.status == InstanceStatus::Duplicate) {
    LOG_WARN("an instance of %s is already running", kCoreBinName);
    return s_exitDuplicate;
  }
  if (instance.status == InstanceStatus::Failure) {
    LOG_ERR("%s", instance.detail);
    return s_exitFailed;
  }
#else
  // Before we check any more args we need to check for a duplicate process.
  // Create a shared memory segment with a unique key
  // This is to prevent a new instance from running if one is already running
  std::unique_ptr<QSharedMemory> sharedMemory;
  if (parser.singleInstanceOnly()) {
    sharedMemory = std::make_unique<QSharedMemory>(kCoreBinName);

    // Attempt to attach first and detach in order to clean up stale shm chunks
    // This can happen if the previous instance was killed or crashed
    if (sharedMemory->attach())
      sharedMemory->detach();

    if (!sharedMemory->create(1)) {
      LOG_WARN("an instance of %s is already running", kCoreBinName);
      return s_exitDuplicate;
    }
  }
#endif

  if (!parser.parse()) {
    showHelp(parser);
    return s_exitSuccess;
  }

  EventQueue events;
  const auto processName = QFileInfo(argv[0]).fileName();

  App *coreApp = createApp(parser, events, processName);

#if defined(Q_OS_MACOS)
  // Demote the bundled helper before Cocoa initializes. Qt otherwise promotes
  // it during QApplication construction, even if it was already backgrounded.
  {
    ProcessSerialNumber psn = {0, kCurrentProcess};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GetCurrentProcess(&psn);
#pragma GCC diagnostic pop
    TransformProcessType(&psn, kProcessTransformToBackgroundApplication);
  }
  constexpr auto foregroundTransform = "QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM";
  const auto initialForegroundTransform = qgetenv(foregroundTransform);
  qputenv(foregroundTransform, "1");
#endif

  QApplication app(argc, argv);

#if defined(Q_OS_MACOS)
  // Do not change the environment inherited by commands launched later.
  if (initialForegroundTransform.isNull()) {
    qunsetenv(foregroundTransform);
  } else {
    qputenv(foregroundTransform, initialForegroundTransform);
  }
#endif
  QApplication::setApplicationName(QStringLiteral("%1 Core").arg(kAppName));

  const auto ipcServer = new synergy::core::ipc::CoreIpcServer(&app, parser.serviceLaunchId()); // NOSONAR - Qt managed
  QObject::connect(
      ipcServer, &synergy::core::ipc::IpcServer::stopProcessRequested, coreApp, &App::quit, Qt::DirectConnection
  );
  if (!ipcServer->listen()) {
    LOG_ERR(QT_TRANSLATE_NOOP("EngineLogMessages", "ipc server failed to listen"));
    return s_exitFailed;
  }

#if defined(Q_OS_WIN)
  // Core can start on the secure desktop before a console user exists. Widen
  // the token ACL when the active console session changes (logon, logout,
  // switch user) instead of rewriting the DACL every second. On other
  // platforms the token file is created owner-only and needs no refresh.
  QTimer tokenAccessTimer;
  QObject::connect(&tokenAccessTimer, &QTimer::timeout, &app, [] {
    static constexpr DWORD kNoConsoleSession = 0xFFFFFFFF;
    static DWORD lastSessionId = kNoConsoleSession;
    const DWORD sessionId = WTSGetActiveConsoleSessionId();
    if (sessionId == lastSessionId) {
      return;
    }
    lastSessionId = sessionId;
    if (sessionId != kNoConsoleSession) {
      synergy::refreshIpcTokenAccess(QString::fromLatin1(kCoreIpcName));
    }
  });
  tokenAccessTimer.start(1000);
#endif

  QThread coreThread;
  QObject::connect(&coreThread, &QThread::finished, &app, &QApplication::quit);
  coreApp->run(coreThread);

  int exitCode = QApplication::exec();
  coreThread.wait();

  if (exitCode == s_exitSuccess) {
    exitCode = coreApp->getExitCode();
  }

  LOG_DEBUG("core exited, code: %d", exitCode);
  return exitCode;
}
