/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "R3StartupProbeBootstrap.h"
#ifdef SYNERGY_R4_VISUAL_PROBE
#include "R4VisualContractTests.h"
#endif
#include "common/Constants.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "common/UrlConstants.h"
#include "gui/MainWindow.h"
#include "gui/widgets/PageHost.h"
#include "synergy/gui/SettingsMigration.h"
#include "synergy/gui/TestSettings.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <memory>

namespace synergy::testing {
namespace {
QString root;
QString fixture;
QJsonObject receipt;
QJsonArray phases;
std::unique_ptr<QTemporaryDir> nativeDirectory;
QString extraFile()
{
  return Settings::userDir() + "/" + kAppName + ".extra.conf";
}
void writeReceipt()
{
  receipt["phases"] = phases;
  QFile file(root + "/receipt.json");
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    file.write(QJsonDocument(receipt).toJson(QJsonDocument::Indented));
}
class NoticeObserver : public QObject
{
protected:
  bool eventFilter(QObject *object, QEvent *event) override
  {
    if (event->type() != QEvent::Show)
      return false;
    auto *message = qobject_cast<QMessageBox *>(object);
    if (!message)
      return false;
    // C-UI-R3-STARTUP-NOTICE-R2: macOS may omit QMessageBox titles. Match the
    // complete owned migration notice, its expected icon and sole Ok action;
    // never approve any permission/trust/other message with a broad substring.
    const auto expectedBody =
        QObject::tr(
            "<p>We've migrated your settings to a new format used by this version of Synergy.</p>"
            "<p>Your previous settings have been backed up to:</p>"
            "<p><code>%1</code></p>"
            "<p>If anything looks different, report it at <a href=\"%2\">%2</a>.</p>"
        )
            .arg(root + "/system/" + kAppName + ".conf.legacy.bak", QString::fromUtf8(kProjectContact));
    bool expectedTitle = message->windowTitle() == QObject::tr("Settings updated");
#ifdef Q_OS_MACOS
    expectedTitle = expectedTitle || message->windowTitle().isEmpty();
#endif
    if (fixture == "legacy" && expectedTitle && message->text() == expectedBody &&
        message->icon() == QMessageBox::Information && message->standardButtons() == QMessageBox::Ok) {
      receipt["migrationNoticeObserved"] = true;
      receipt["migrationNoticeTitle"] = message->windowTitle();
      receipt["migrationNoticeBodySha256"] =
          QString::fromLatin1(QCryptographicHash::hash(expectedBody.toUtf8(), QCryptographicHash::Sha256).toHex());
      receipt["migrationNoticeMatcher"] = "complete owned body; Information; Ok-only; expected or macOS-empty title";
      writeReceipt();
      QPointer<QMessageBox> guarded(message);
      QTimer::singleShot(0, message, [guarded] {
        if (guarded)
          guarded->accept();
      });
    } else {
      // Never approve an unexpected permission, trust, license or data-loss prompt.
      receipt["unexpectedDialog"] = message->windowTitle();
      writeReceipt();
    }
    return false;
  }
};
} // namespace
bool configureStartupProbe(const QStringList &arguments)
{
  Q_UNUSED(arguments);
  // Test-target-only bootstrap. These names are never read by the default product target.
  root = qEnvironmentVariable("SYNERGY_R3_PROBE_ROOT");
  fixture = qEnvironmentVariable("SYNERGY_R3_PROBE_FIXTURE", "current");
  if (root.isEmpty()) {
    nativeDirectory = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/tupig-r3-startup-XXXXXX");
    if (!nativeDirectory->isValid())
      return false;
    nativeDirectory->setAutoRemove(false);
    root = nativeDirectory->path();
  }
  root = QFileInfo(root).canonicalFilePath();
  if (fixture != "fresh" && fixture != "current" && fixture != "legacy")
    return false;
  QString error;
  const auto identity =
      qEnvironmentVariable("SYNERGY_R3_PROBE_KEY", "tsr3-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
  if (!runtime::configureIsolatedRuntime({root, identity}, &error))
    return false;
  for (const auto &path : {"user", "system", "state", "legacy", "logs"})
    if (!QDir().mkpath(root + "/" + path))
      return false;
  QSettings test(runtime::runtimeTestSettingsFile(), QSettings::IniFormat);
  test.setValue("test/enabled", false);
  test.sync();
  if (fixture == "current") {
    QSettings extra(extraFile(), QSettings::IniFormat);
    extra.setValue("migration/schemaVersion", gui::migration::kCurrentSchemaVersion);
    extra.setValue("migration/notifiedFor", gui::migration::kCurrentSchemaVersion);
    extra.setValue("scope/preferSystem", false);
    extra.sync();
    QSettings user(Settings::userSettingsFile(), QSettings::IniFormat);
    user.setValue(Settings::Core::ComputerName, "Current-Fixture");
    user.setValue(Settings::Core::Port, 25252);
    user.sync();
  } else if (fixture == "legacy") {
    for (const auto scope : {QSettings::UserScope, QSettings::SystemScope}) {
      auto legacy = runtime::legacySettings(scope);
      legacy->setValue("startedBefore", true);
      legacy->setValue("screenName", scope == QSettings::UserScope ? "Legacy-User" : "Legacy-System");
      legacy->setValue("port", scope == QSettings::UserScope ? 25444 : 25555);
      legacy->setValue("autoHide", false);
      legacy->setValue("obsoleteFixture", "backup-only-marker");
      if (scope == QSettings::UserScope)
        legacy->setValue("systemScope", false);
      legacy->sync();
    }
  }
  receipt = QJsonObject{
      {"root", root},
      {"fixture", fixture},
      {"instanceKey", identity},
      {"userSettings", Settings::userSettingsFile()},
      {"systemSettings", Settings::systemSettingsFile()},
      {"state", runtime::runtimeStateFile()},
      {"overlay", extraFile()},
      {"legacyUser", root + "/legacy/user.ini"},
      {"legacySystem", root + "/legacy/system.ini"},
      {"testSettings", runtime::runtimeTestSettingsFile()},
      {"backend", "temporary INI; actual migration algorithm"},
      {"byteIdenticalDeliveredExecutable", false}
  };
  startupProbePhase("context-ready");
  return true;
}
void startupProbePhase(const char *phase)
{
  phases.append(QString::fromLatin1(phase));
  writeReceipt();
}
void startupProbeApplicationReady(QApplication *application)
{
  auto *observer = new NoticeObserver;
  observer->setParent(application);
  application->installEventFilter(observer);
}
bool afterStartupProbePreInit()
{
  QSettings user(Settings::userSettingsFile(), QSettings::IniFormat);
  QSettings system(Settings::systemSettingsFile(), QSettings::IniFormat);
  QSettings extra(extraFile(), QSettings::IniFormat);
  receipt["schemaAfterMigration"] = extra.value("migration/schemaVersion").toInt();
  receipt["userPortAfterMigration"] = user.value(Settings::Core::Port).toInt();
  receipt["systemPortAfterMigration"] = system.value(Settings::Core::Port).toInt();
  receipt["userAutoCoreBeforeSafetyOverride"] = user.value(Settings::Gui::AutoStartCore, false).toBool();
  receipt["systemAutoCoreBeforeSafetyOverride"] = system.value(Settings::Gui::AutoStartCore, false).toBool();
  receipt["safetyOverrideAfterMigration"] = true;
  writeReceipt();
  // Preserve actual migration receipts above; only then prevent runtime operations.
  Settings::setValue(Settings::Gui::AutoStartCore, false);
  Settings::setValue(Settings::Gui::CheckForUpdates, false);
  Settings::setValue(Settings::Gui::Autohide, false);
  Settings::setValue(Settings::Gui::CloseToTray, false);
  Settings::setValue(Settings::Gui::CloseReminder, false);
  Settings::setValue(Settings::Core::Language, "en");
  Settings::setValue(Settings::Core::ProcessMode, QVariant::fromValue(Settings::ProcessMode::Desktop));
  Settings::setValue(Settings::Core::EnableEnterCommand, false);
  Settings::setValue(Settings::Core::EnableExitCommand, false);
  Settings::setValue(Settings::Core::ScreenEnterCommand, QString());
  Settings::setValue(Settings::Core::ScreenExitCommand, QString());
  Settings::setValue(Settings::Log::ToFile, false);
  Settings::setValue(Settings::Log::File, root + "/logs/app.log");
  Settings::setValue(Settings::Security::Certificate, root + "/user/fixture.pem");
  Settings::setValue(Settings::Security::TlsEnabled, false);
  if (fixture == "fresh")
    Settings::setValue(Settings::Core::ComputerName, "Fresh-Fixture");
  Settings::save();
#ifdef SYNERGY_R4_VISUAL_PROBE
  return prepareR4VisualFixture(root);
#endif
  return true;
}
void startupProbeWindowReady(MainWindow *window)
{
  QPointer<MainWindow> guarded(window);
  QTimer::singleShot(0, window, [guarded] {
    if (!guarded)
      return;
    receipt["mainWindowVisible"] = guarded->isVisible();
    receipt["hasRealPageHost"] = guarded->findChild<PageHost *>() != nullptr;
    receipt["coreChildProcessCount"] = guarded->findChildren<QProcess *>().size();
    receipt["testSettingsEnabled"] = gui::TestSettings::instance().isEnabled();
    receipt["testSettingsResolved"] = gui::TestSettings::instance().fileName();
    receipt["autoCoreDisabled"] = !Settings::value(Settings::Gui::AutoStartCore).toBool();
    receipt["updatesDisabled"] = !Settings::value(Settings::Gui::CheckForUpdates).toBool();
    QSettings extra(extraFile(), QSettings::IniFormat);
    receipt["notifiedAfterMainWindow"] = extra.value("migration/notifiedFor").toInt();
    startupProbePhase("event-loop-ready");
#ifdef SYNERGY_R4_VISUAL_PROBE
    observeR4VisualMain(guarded, root);
    return;
#endif
    const bool automated = qEnvironmentVariableIsSet("SYNERGY_R3_PROBE_AUTOMATED");
    QTimer::singleShot(automated ? 10 : 120000, guarded, [guarded] {
      if (guarded)
        guarded->close();
      QApplication::quit();
    });
  });
}
void startupProbeFinished(int exitCode)
{
  receipt["exitCode"] = exitCode;
  startupProbePhase("clean-exit");
}
} // namespace synergy::testing
