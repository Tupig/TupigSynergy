/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Constants.h"
#include "common/ExitCodes.h"
#include "common/I18N.h"
#include "common/PlatformInfo.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "gui/Diagnostic.h"
#include "gui/MainWindow.h"
#include "gui/Messages.h"
#include "gui/StyleUtils.h"

#ifdef SYNERGY_EXTRA_HEADER
#include "synergy/hooks/gui_hook.h"
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPushButton>
#include <QSharedMemory>
#include <QTextStream>
#include <QThread>
#include <QUrl>

#if defined(Q_OS_MACOS)
#include "platform/macos/OSXSDKHeaders.h"
#include <cstdlib>
#endif

#if defined(Q_OS_UNIX) && defined(QT_DEBUG)
#include <QLoggingCategory>
#endif

#if !defined(Q_OS_MAC) && !defined(Q_OS_WIN)
#include "XDGPortalRegistry.h"
#endif

using namespace synergy::gui;

#if defined(Q_OS_MACOS)
bool checkMacAssistiveDevices();
#endif

const static auto kHeader = QStringLiteral("%1: %2\n").arg(kAppName, kDisplayVersion);

int main(int argc, char *argv[])
{
#if defined(Q_OS_UNIX) && defined(QT_DEBUG)
  // Fixes Fedora bug where qDebug() messages aren't printed.
  QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true\nqt.*=false"));
#endif

#if !defined(Q_OS_MAC) && !defined(Q_OS_WIN)
  synergy::platform::setAppId();
#endif

  QCoreApplication::setApplicationName(kAppName);
  QCoreApplication::setOrganizationName(kAppName);
  QCoreApplication::setApplicationVersion(kVersion);
  QCoreApplication::setOrganizationDomain(kOrgDomain); // used in prefix, can't be a url
  QGuiApplication::setDesktopFileName(kRevFqdnName);

  QApplication app(argc, argv);

#ifdef SYNERGY_EXTRA_HEADER
  synergy::hooks::onPreInit();
#endif

  // Ensure the I18N object is made before strings
  QTextStream(stdout) << "initial language: " << I18N::currentLanguage() << '\n';

  // Add Command Line Options
  auto helpOption = QCommandLineOption({"h", "help"}, "Display Help on the command line");
  auto versionOption = QCommandLineOption({"v", "version"}, "Display version information");
  auto resetOption = QCommandLineOption("reset", "Reset all settings");

  QCommandLineParser parser;
  parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
  parser.addOption(helpOption);
  parser.addOption(versionOption);
  parser.addOption(resetOption);
  parser.parse(QCoreApplication::arguments());

  if (!parser.errorText().isEmpty()) {
    qCritical().noquote() << parser.errorText() << "\nUse --help for more information.";
    return s_exitArgs;
  }

  if (parser.isSet(helpOption)) {
    QTextStream(stdout) << kHeader << QStringLiteral("  %1\n\n").arg(kAppDescription)
                        << parser.helpText().replace(QApplication::applicationFilePath(), kAppId);
    return s_exitSuccess;
  }

  if (parser.isSet(versionOption)) {
    QTextStream(stdout) << kHeader << kCopyright << '\n';
    return s_exitSuccess;
  }

  const auto shmId = QStringLiteral("%1-gui").arg(kAppId);
  // Create a shared memory segment with a unique key
  // This is to prevent a new instance from running if one is already running
  QSharedMemory sharedMemory(shmId);

  // Attempt to attach first and detach in order to clean up stale shm chunks
  // This can happen if the previous instance was killed or crashed
  if (sharedMemory.attach())
    sharedMemory.detach();

  // If we can create 1 byte of SHM we are the only instance
  if (!sharedMemory.create(1)) {
    // Ping the running instance to have it show itself
    QLocalSocket socket;
    socket.connectToServer(shmId, QLocalSocket::ReadOnly);
    // A local-socket connect completes immediately or the peer is gone;
    // do not stall startup on the multi-second default timeout.
    if (!socket.waitForConnected(1000)) {
      // If we can't connect to the other instance tell the user its running.
      // This should never happen but just incase we should show something
      QMessageBox::information(nullptr, kAppName, QObject::tr("%1 is already running").arg(kAppName));
    }
    socket.disconnectFromServer();
    return s_exitDuplicate;
  }

  if (!synergy::platform::isMac() && qEnvironmentVariable("XDG_CURRENT_DESKTOP") != QLatin1String("KDE")) {
    QApplication::setStyle("fusion");
  }

  // Sets the fallback icon path and fallback theme
  updateIconTheme();

  qInstallMessageHandler(synergy::gui::messages::messageHandler);
  qInfo("%s v%s", kAppName, kDisplayVersion);

#if defined(Q_OS_MACOS)

  if (app.applicationDirPath().startsWith("/Volumes/")) {
    QString msgBody = QStringLiteral(
        "Please drag %1 to the Applications folder, "
        "and open it from there."
    );
    QMessageBox::information(nullptr, kAppName, msgBody.arg(kAppName));
    return 1;
  }

  if (!checkMacAssistiveDevices()) {
    return 1;
  }
#endif

  // --no-reset
  if (parser.isSet(resetOption)) {
    diagnostic::clearSettings(false);
  }

  MainWindow mainWindow;
  mainWindow.open();

#ifdef SYNERGY_EXTRA_HEADER
  if (!synergy::hooks::onAppStart()) {
    return s_exitFailed;
  }
#endif

  return QApplication::exec();
}

#if defined(Q_OS_MACOS)
bool checkMacAssistiveDevices()
{
  // Applications are trusted individually for the accessibility API. The
  // system prompt only appears once; when the user has already granted the
  // permission the check below succeeds. When it does not (for example an
  // ad-hoc signed build whose code identity changed after an update), explain
  // the exact setting and let the user grant it without losing the window.
  if (AXIsProcessTrusted()) {
    return true;
  }

  const auto appName = QString::fromUtf8(kAppName);

  // Register the app in the Accessibility list and let the system show its own
  // prompt first; the dialog below then points at the exact pane.
  const void *keys[] = {kAXTrustedCheckOptionPrompt};
  const void *trueValue[] = {kCFBooleanTrue};
  CFDictionaryRef options = CFDictionaryCreate(nullptr, keys, trueValue, 1, nullptr, nullptr);
  AXIsProcessTrustedWithOptions(options);
  CFRelease(options);

  QMessageBox box(
      QMessageBox::Warning, appName,
      QObject::tr("macOS requires Accessibility permission for %1 to control the keyboard and mouse.").arg(appName),
      QMessageBox::NoButton
  );
  box.setInformativeText(
      QObject::tr(
          "Open System Settings → Privacy & Security → Accessibility, enable %1, then choose Continue. "
          "If it stays disabled, quit and relaunch the app."
      )
          .arg(appName)
  );
  auto *openSettings = box.addButton(QObject::tr("Open System Settings"), QMessageBox::ActionRole);
  auto *retry = box.addButton(QObject::tr("Continue"), QMessageBox::AcceptRole);
  box.addButton(QObject::tr("Quit"), QMessageBox::RejectRole);
  QObject::connect(openSettings, &QPushButton::clicked, []() {
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"))
    );
  });

  // TCC publishes a grant to the running process after the user toggles it;
  // poll instead of forcing a restart. The loop only ends when the app is
  // trusted or the user quits.
  while (true) {
    box.exec();
    if (box.clickedButton() != retry) {
      return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
      if (AXIsProcessTrusted()) {
        return true;
      }
      QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
      QThread::msleep(100);
    }

    QMessageBox::warning(
        nullptr, appName,
        QObject::tr(
            "Accessibility permission is still not granted. Enable %1 in System Settings, then quit and relaunch it."
        )
            .arg(appName)
    );
  }
}
#endif
