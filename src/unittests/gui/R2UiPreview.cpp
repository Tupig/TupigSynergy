/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/Constants.h"
#include "common/I18N.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"
#include "gui/Action.h"
#include "gui/AppStyle.h"
#include "gui/KeySequence.h"
#include "gui/MainWindow.h"
#include "gui/TlsUtility.h"
#include "gui/config/ServerConfig.h"
#include "synergy/gui/TestSettings.h"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QUuid>

// Test-only link seam approved for this preview: actual MainWindow invokes this
// callback after creating its real controllers. Production migration uses daily
// overlay paths, so do not link/run that object in this isolated observer.
namespace synergy::gui::migration {
void showNoticeIfPending(QWidget *)
{
}
} // namespace synergy::gui::migration

int main(int argc, char **argv)
{
  QTemporaryDir isolated(QDir::tempPath() + QStringLiteral("/tupig-r2-preview-XXXXXX"));
  if (!isolated.isValid())
    return 1;
  // COMPLETE-R1 harness lineage: bind all Settings paths before QApplication.
  const auto root = QFileInfo(isolated.path()).canonicalFilePath();
  const auto nonce = QUuid::createUuid().toString(QUuid::Id128).left(16);
  const auto instanceSocketName = QStringLiteral("tsr2-ui-") + nonce;
  QString isolationError;
  if (!synergy::runtime::configureIsolatedRuntime({root, instanceSocketName}, &isolationError))
    return 10;
  const auto configRoot = isolated.filePath(QStringLiteral("config"));
  const auto stateRoot = isolated.filePath(QStringLiteral("state"));
  const auto configDirectory = root + QStringLiteral("/user");
  if (!QDir().mkpath(configDirectory) || !QDir().mkpath(stateRoot))
    return 1;
  qputenv("XDG_CONFIG_HOME", configRoot.toUtf8());
  qputenv("XDG_STATE_HOME", stateRoot.toUtf8());
  const auto configFile = Settings::userSettingsFile();
  QSettings test(synergy::runtime::runtimeTestSettingsFile(), QSettings::IniFormat);
  test.setValue("test/enabled", false);
  test.sync();
  QSettings fixture(configFile, QSettings::IniFormat);
  fixture.setValue(Settings::Core::ComputerName, QStringLiteral("Test-Server"));
  fixture.setValue(Settings::Core::CoreMode, int(Settings::Server));
  fixture.setValue(Settings::Core::ProcessMode, int(Settings::Desktop));
  fixture.setValue(Settings::Core::Language, QStringLiteral("en"));
  fixture.setValue(Settings::Core::Interface, QStringLiteral("192.0.2.10"));
  fixture.setValue(Settings::Core::PreventSleep, false);
  fixture.setValue(Settings::Core::EnableEnterCommand, false);
  fixture.setValue(Settings::Core::EnableExitCommand, false);
  fixture.setValue(Settings::Gui::AutoStartCore, false);
  fixture.setValue(Settings::Gui::CheckForUpdates, false);
  fixture.setValue(Settings::Gui::Autohide, false);
  fixture.setValue(Settings::Gui::CloseToTray, false);
  fixture.setValue(Settings::Gui::LogExpanded, true);
  fixture.setValue(Settings::Client::RemoteHost, QStringLiteral("192.0.2.20"));
  fixture.setValue(Settings::Log::ToFile, false);
  fixture.setValue(Settings::Security::TlsEnabled, false);
  fixture.setValue(Settings::Security::Certificate, root + "/user/fixture.pem");
  fixture.setValue(Settings::Log::File, root + "/logs/fixture.log");
  fixture.sync();
  QCoreApplication::setApplicationName(kAppName);
  QCoreApplication::setOrganizationName(kAppName);
  QCoreApplication::setApplicationVersion(kVersion);
  QApplication app(argc, argv);
  Settings::setSettingsFile(configFile);
  Settings::setStateFile(isolated.filePath(QStringLiteral("preview.state")));
  // COMPLETE-R1 native M04 fixture: real generated certificate, TLS remains off.
  if (!synergy::gui::TlsUtility::generateCertificateAt(root + "/user/fixture.pem", 2048))
    return 14;
  auto font = app.font();
  font.setPixelSize(13);
  app.setFont(font);
  app.setStyleSheet(synergy::gui::appStyleSheet());
  app.setWindowIcon(QIcon(QStringLiteral(":/image/app-icon.png")));
  synergy::gui::installButtonFeedback();
  I18N::setLanguage(QStringLiteral("en"));

  // Populate supported serialization, never product defaults or host settings.
  auto &settings = Settings::proxy().get();
  settings.beginGroup(QStringLiteral("internalConfig"));
  settings.beginWriteArray(QStringLiteral("hotkeys"), 1);
  settings.setArrayIndex(0);
  KeySequence trigger;
  trigger.appendKey(Qt::Key_K, Qt::ControlModifier);
  trigger.saveSettings(settings);
  settings.beginWriteArray(QStringLiteral("actions"), 1);
  settings.setArrayIndex(0);
  settings.setValue(SettingsKeys::ActionType, int(Action::Type::switchInDirection));
  settings.setValue(SettingsKeys::SwitchDirection, int(Action::SwitchDirection::right));
  settings.setValue(SettingsKeys::RestartServer, false);
  settings.endArray();
  settings.endArray();
  settings.endGroup();
  settings.sync();
  synergy::gui::ServerConfig config;
  config.addClient(QStringLiteral("Studio"));
  config.addClient(QStringLiteral("Workstation"));
  config.commit();

  // Actual isolation check: only unique fixture IPC endpoints are ever touched.
  const auto witnessSocketName = QStringLiteral("tsr2-witness-") + nonce;
  QLocalServer witness;
  if (!witness.listen(witnessSocketName))
    return 2;
  MainWindow window(instanceSocketName);
  // COMPLETE-R1 observer safety: no real core/update command is reachable from
  // the preview's menu, buttons, shortcuts, hostname Return or automatic flags.
  // The production action graph remains covered by the unchanged routing tests.
  int blockedActions = 0;
  for (auto *action : window.findChildren<QAction *>()) {
    const auto key = action->shortcut();
    const bool clearSettings = action->text() == QStringLiteral("Clear settings");
    if (!clearSettings && key != QKeySequence(Qt::CTRL | Qt::Key_S) && key != QKeySequence(Qt::CTRL | Qt::Key_R) &&
        key != QKeySequence(Qt::CTRL | Qt::Key_U))
      continue;
    ++blockedActions;
    QObject::disconnect(action, &QAction::triggered, &window, nullptr);
    const auto keepDisabled = [action] {
      if (action->isEnabled())
        action->setEnabled(false);
    };
    QObject::connect(action, &QAction::changed, action, keepDisabled);
    keepDisabled();
  }
  if (blockedActions != 4)
    return 13;
  for (const char *name : {"btnToggleCore", "btnRestartCore", "btnUpdatesShortcut"}) {
    auto *button = window.findChild<QPushButton *>(QString::fromLatin1(name));
    if (!button)
      return 11;
    QObject::disconnect(button, &QPushButton::clicked, nullptr, nullptr);
    button->setEnabled(false);
  }
  auto *hostname = window.findChild<QLineEdit *>("lineHostname");
  if (!hostname)
    return 12;
  QObject::disconnect(hostname, &QLineEdit::returnPressed, nullptr, nullptr);
  QObject::connect(Settings::instance(), &Settings::settingsChanged, &window, [](const QString &) {
    for (const auto &key : {Settings::Gui::AutoStartCore, Settings::Gui::CheckForUpdates})
      if (Settings::value(key).toBool())
        Settings::setValue(key, false);
  });
  // No core can become Started; accepted child drafts cannot take conditional
  // restart branches, and no incoming-client callback can arrive without it.
  // Incoming-client signals require a running core; all paths that can start
  // it have been blocked above, including direct button/Return connections.

  auto *previewServer = window.findChild<QLocalServer *>();
  if (!previewServer || !previewServer->isListening() || previewServer->serverName() != instanceSocketName)
    return 3;
  QLocalSocket previewProbe;
  previewProbe.connectToServer(instanceSocketName);
  if (!previewProbe.waitForConnected(1000))
    return 4;
  previewProbe.disconnectFromServer();
  QLocalSocket witnessProbe;
  witnessProbe.connectToServer(witnessSocketName);
  if (!witness.isListening() || !witnessProbe.waitForConnected(1000))
    return 5;
  witnessProbe.disconnectFromServer();

  // Observer-only shortcut: present the actual tray menu object, never a copy.
  // This does not prove that CUA can activate the physical NSStatusItem entry.
  auto *trayObserver = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_9), &window);
  QObject::connect(trayObserver, &QShortcut::activated, &window, [&window] {
    auto *tray = window.findChild<QSystemTrayIcon *>();
    if (tray && tray->contextMenu())
      tray->contextMenu()->popup(window.mapToGlobal(QPoint(20, 20)));
  });

  // Preserve only generated fixture paths for independent observation evidence.
  QFile marker(QDir::temp().filePath(QStringLiteral("tupig-r2-preview-latest.json")));
  if (marker.open(QIODevice::WriteOnly)) {
    marker.write(QJsonDocument(
                     QJsonObject{
                         {QStringLiteral("fixture_root"), isolated.path()},
                         {QStringLiteral("settings_file"), configFile},
                         {QStringLiteral("entry_difference"),
                          QStringLiteral("No onPreInit/onAppStart; no-op migration notice only")},
                         {QStringLiteral("auto_start_core"), false},
                         {QStringLiteral("check_updates"), false},
                         {QStringLiteral("unique_socket_verified"), true},
                         {QStringLiteral("witness_socket_preserved"), true},
                         {QStringLiteral("instance_socket_name"), instanceSocketName},
                         {QStringLiteral("runtime_root"), root},
                         {QStringLiteral("system_settings"), Settings::systemSettingsFile()},
                         {QStringLiteral("state_file"), isolated.filePath(QStringLiteral("preview.state"))},
                         {QStringLiteral("overlay_file"), Settings::userDir() + "/" + kAppName + ".extra.conf"},
                         {QStringLiteral("legacy_user"), root + "/legacy/user.ini"},
                         {QStringLiteral("legacy_system"), root + "/legacy/system.ini"},
                         {QStringLiteral("test_settings"), synergy::runtime::runtimeTestSettingsFile()},
                         {QStringLiteral("core_commands_blocked"), true},
                         {QStringLiteral("blocked_actions"), blockedActions},
                         {QStringLiteral("test_settings_enabled"), synergy::gui::TestSettings::instance().isEnabled()}
                     }
    ).toJson());
    marker.close();
  }

  // Do not call production entry hooks: they load/migrate daily overlay stores.
  window.open();
  return app.exec();
}
