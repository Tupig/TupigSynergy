/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2026 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "App.h"

#include "DisplayInvalidException.h"
#include "SynergyException.h"
#include "arch/Arch.h"
#include "base/Log.h"
#include "base/LogOutputters.h"
#include "common/ExitCodes.h"
#include "common/RuntimeLogPaths.h"
#include "common/Settings.h"
#include "mt/ThreadException.h"

#if defined(Q_OS_WIN)
#include "base/IEventQueue.h"
#endif

#include <QDir>
#include <QFileInfo>
#include <stdexcept>

#if defined(Q_OS_MAC)
#include "platform/macos/OSXSDKHeaders.h"
#endif

#if defined(WINAPI_XWINDOWS) or defined(WINAPI_LIBEI)
#include "XDGPortalRegistry.h"
#endif

using namespace synergy;

App *App::s_instance = nullptr;

//
// App
//

App::App(IEventQueue *events, const QString &processName)
    : m_bye(&exit),
      m_events(events),
      m_appUtil(events),
      m_pname(processName)
{
  assert(s_instance == nullptr);
  s_instance = this;
#if defined(WINAPI_XWINDOWS) or defined(WINAPI_LIBEI)
  synergy::platform::setAppId();
#endif
}

App::~App()
{
  s_instance = nullptr;
}

void App::run(QThread &coreThread)
{
  LOG_INFO(QT_TRANSLATE_NOOP("EngineLogMessages", "starting core"));

  // Important: Move the daemon app to the daemon thread before creating any more Qt objects
  // owned by the daemon app, as they will be created on the daemon thread.
  moveToThread(&coreThread);

  connect(&coreThread, &QThread::started, this, [this, &coreThread]() {
    LOG_DEBUG("core thread started");

    // install application in to arch
    appUtil().adoptApp(this);

    // HACK: fail by default (saves us setting result in each catch)
    int result = s_exitFailed;

    try {
      result = appUtil().run();
    } catch (ExitAppException &e) {
      // instead of showing a nasty error, just exit with the error code.
      // not sure if i like this behaviour, but it's probably better than
      // using the exit(int) function!
      result = e.getCode();
    } catch (DisplayInvalidException &die) {
      LOG_CRIT("a display invalid exception error occurred: %s\n", die.what());
      // display invalid exceptions can occur when going to sleep. When this
      // process exits, the UI will restart us instantly. We don't really want
      // that behevior, so we quies for a bit
      Arch::sleep(10);
    } catch (std::runtime_error &re) {
      LOG_CRIT("a runtime error occurred: %s\n", re.what());
    } catch (std::exception &e) {
      LOG_CRIT("an error occurred: %s\n", e.what());
    } catch (...) {
      LOG_CRIT(QT_TRANSLATE_NOOP("EngineLogMessages", "an unknown error occurred") "\n");
    }

    if (result == s_exitSuccess) {
      LOG_INFO(QT_TRANSLATE_NOOP("EngineLogMessages", "core stopped successfully"));
    } else {
      updateExitCode(result);
      LOG_ERR("core stopped with error code: %d", result);
    }

    coreThread.quit();
    LOG_DEBUG("core thread finished");
  });

  LOG_DEBUG("starting core thread");
  coreThread.start();
}

void App::setupFileLogging()
{
  if (m_fileLog || m_serviceLaunch) {
    return;
  }

  const bool managed = synergy::usesManagedRuntimeLogs();
  if (!managed && !Settings::value(Settings::Log::ToFile).toBool()) {
    return;
  }

  const auto file = managed ? synergy::managedRuntimeLogDirectory(synergy::RuntimeLogRole::Core)
                            : Settings::value(Settings::Log::File).toString();
  if (file.isEmpty() || (managed && !QDir().mkpath(file))) {
    LOG_WARN(QT_TRANSLATE_NOOP("EngineLogMessages", "could not prepare the core log directory"));
    return;
  }

  m_fileLog = new FileLogOutputter(
      file, managed ? FileLogOutputter::Mode::SegmentedDirectory : FileLogOutputter::Mode::SingleFile
  ); // NOSONAR - Adopted by `Log`
  // A file error must not interrupt console messages or the GUI's log stream.
  CLOG->insert(m_fileLog, true);
  LOG_DEBUG(QT_TRANSLATE_NOOP("EngineLogFormats", "core file log ready: %s"), qUtf8Printable(file));
}

void App::initApp()
{
  parseArgs();

  // set log filter
  if (const auto logLevel = Settings::logLevelText(); !CLOG->setFilter(logLevel)) {
    LOG_CRIT(
        "%s: unrecognized log level `%s'" BYE, qPrintable(processName()), qPrintable(logLevel),
        qPrintable(processName())
    );
    m_bye(s_exitArgs);
  }
  // Managed logs can be shared for diagnosis. Verbose traces can contain raw
  // protocol fields and key IDs. Keep the complete DEBUG level for these logs.
  if (synergy::usesManagedRuntimeLogs() && CLOG->getFilter() > LogLevel::Level::Debug) {
    CLOG->setFilter(QStringLiteral("DEBUG"));
    LOG_INFO(QT_TRANSLATE_NOOP("EngineLogMessages", "managed core logging is limited to DEBUG"));
  }

  // setup file logging after parsing args
  setupFileLogging();

  // load configuration
  loadConfig();
}

void App::handleScreenError() const
{
  LOG_CRIT(QT_TRANSLATE_NOOP("EngineLogMessages", "error on screen"));
  getEvents()->addEvent(Event(EventTypes::Quit));
}

void App::quit() const
{
  LOG_INFO(QT_TRANSLATE_NOOP("EngineLogMessages", "quitting"));
  getEvents()->addEvent(Event(EventTypes::Quit));
}

void App::runEventsLoop(const void *)
{
  int exitCode = m_events->loop();
  if (exitCode != s_exitSuccess) {
    throw ThreadExitException(new LoopErrorCode(exitCode));
  }
}
