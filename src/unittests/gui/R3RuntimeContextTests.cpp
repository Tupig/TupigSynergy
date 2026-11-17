/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/Constants.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>
#include <QtTest>

namespace {
int contextChild(const QString &mode)
{
  using namespace synergy::runtime;
  QTemporaryDir temp;
  QTemporaryDir outside;
  if (!temp.isValid() || !outside.isValid())
    return 90;
  const QString root = QFileInfo(temp.path()).canonicalFilePath();
  IsolatedRuntimeContext context{root, "tsr3-" + QUuid::createUuid().toString(QUuid::WithoutBraces)};
  if (mode == "empty")
    context.root.clear();
  if (mode == "relative")
    context.root = "relative-fixture";
  if (mode == "missing")
    context.root += "/does-not-exist";
  if (mode == "temp-root")
    context.root = QFileInfo(QDir::tempPath()).canonicalFilePath();
  if (mode == "key-slash")
    context.instanceKey = "invalid/key";
  if (mode == "key-control")
    context.instanceKey = "invalid\nkey";
  if (mode == "key-empty")
    context.instanceKey.clear();
  if (mode == "key-production")
    context.instanceKey = QStringLiteral("%1-gui").arg(kAppId);
  if (mode == "key-long")
    context.instanceKey = QString(100, QLatin1Char('a'));
  if (mode == "root-symlink") {
    const auto link = outside.filePath("root-link");
    if (!QFile::link(root, link))
      return 91;
    context.root = link;
  }
  if (mode == "descendant-symlink") {
    if (!QFile::link(outside.path(), temp.filePath("user")))
      return 91;
  }
  if (mode == "late")
    (void)Settings::userDir();
  const auto before = QDir(root).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
  QString error;
  const bool configured = configureIsolatedRuntime(context, &error);
  const bool expected = mode == "valid" || mode == "repeat";
  if (configured != expected)
    return 10;
  if (QDir(root).entryList(QDir::AllEntries | QDir::NoDotAndDotDot) != before)
    return 11;
  if (!expected)
    return !error.isEmpty() && !isIsolatedRuntime() ? 0 : 12;
  if (mode == "repeat") {
    error.clear();
    return !configureIsolatedRuntime(context, &error) && !error.isEmpty() ? 0 : 13;
  }
  const QString user = root + "/user";
  const QString system = root + "/system";
  if (Settings::userDir() != user || Settings::systemDir() != system)
    return 20;
  if (Settings::userSettingsFile() != user + "/" + kAppName + ".conf")
    return 21;
  if (Settings::systemSettingsFile() != system + "/" + kAppName + ".conf")
    return 22;
  if (runtimeStateFile() != root + "/state/app.state")
    return 23;
  if (runtimeTestSettingsFile() != root + "/test-settings.ini")
    return 24;
  if (runtimeInstanceKey("production-fallback") != context.instanceKey)
    return 25;
  auto legacyUser = legacySettings(QSettings::UserScope);
  auto legacySystem = legacySettings(QSettings::SystemScope);
  if (!legacyUser || legacyUser->format() != QSettings::IniFormat ||
      legacyUser->fileName() != root + "/legacy/user.ini")
    return 26;
  if (!legacySystem || legacySystem->format() != QSettings::IniFormat ||
      legacySystem->fileName() != root + "/legacy/system.ini")
    return 27;
  // Exact empty fixture stores may not inherit production values through QSettings fallbacks.
  if (!legacyUser->allKeys().isEmpty() || !legacySystem->allKeys().isEmpty())
    return 28;
  const QString log = Settings::defaultValue(Settings::Log::File).toString();
  if (log != root + "/logs/app.log" || Settings::value(Settings::Log::ToFile).toBool())
    return 29;
  error.clear();
  if (configureIsolatedRuntime(context, &error) || error.isEmpty())
    return 30;
  QJsonObject receipt{
      {"user", user},
      {"system", system},
      {"state", runtimeStateFile()},
      {"legacyUser", legacyUser->fileName()},
      {"legacySystem", legacySystem->fileName()},
      {"testSettings", runtimeTestSettingsFile()},
      {"instanceKey", context.instanceKey},
      {"log", log}
  };
  QTextStream(stdout) << QJsonDocument(receipt).toJson(QJsonDocument::Compact) << '\n';
  return 0;
}
} // namespace
class R3RuntimeContextTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void contextIsFailClosed_data()
  {
    QTest::addColumn<QString>("mode");
    for (const char *mode :
         {"valid", "repeat", "late", "empty", "relative", "missing", "temp-root", "root-symlink", "descendant-symlink",
          "key-empty", "key-slash", "key-control", "key-production", "key-long"})
      QTest::newRow(mode) << QString::fromLatin1(mode);
  }
  void contextIsFailClosed()
  {
    QFETCH(QString, mode);
    QProcess process;
    process.start(QCoreApplication::applicationFilePath(), {"--context-child", mode});
    QVERIFY(process.waitForStarted(5000));
    QVERIFY(process.waitForFinished(15000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        process.exitCode() == 0, qPrintable(QStringLiteral("mode=%1 code=%2 stderr=%3")
                                                .arg(mode)
                                                .arg(process.exitCode())
                                                .arg(QString::fromUtf8(process.readAllStandardError())))
    );
    if (mode == "valid") {
      const auto object = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
      QVERIFY(object.value("instanceKey").toString().startsWith("tsr3-"));
      QCOMPARE(object.size(), 8);
    }
  }
};
int main(int argc, char **argv)
{
  if (argc == 3 && QByteArray(argv[1]) == "--context-child")
    return contextChild(QString::fromLocal8Bit(argv[2]));
  QCoreApplication app(argc, argv);
  R3RuntimeContextTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "R3RuntimeContextTests.moc"
