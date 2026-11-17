/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/Constants.h"
#include "synergy/gui/SettingsMigration.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class R3StartupContractTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void actualMainAndHooks_data()
  {
    QTest::addColumn<QString>("fixture");
    for (const char *name : {"fresh", "current", "legacy"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }
  void actualMainAndHooks()
  {
    QFETCH(QString, fixture);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto root = QFileInfo(temp.path()).canonicalFilePath();
    const auto key = "tsr3-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer unrelatedOwnedWitness;
    QVERIFY(unrelatedOwnedWitness.listen(key + "-witness"));
    QSignalSpy contacted(&unrelatedOwnedWitness, &QLocalServer::newConnection);
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("SYNERGY_R3_PROBE_ROOT", root);
    environment.insert("SYNERGY_R3_PROBE_FIXTURE", fixture);
    environment.insert("SYNERGY_R3_PROBE_KEY", key);
    environment.insert("SYNERGY_R3_PROBE_AUTOMATED", "1");
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral(R3_STARTUP_PROBE_PATH), {});
    QVERIFY2(process.waitForStarted(10000), qPrintable(process.errorString()));
    if (!process.waitForFinished(20000)) {
      process.kill();
      process.waitForFinished(5000);
      // COMPLETE-R1 mechanical diagnostic supplement: preserve the failed
      // disposable fixture receipt before QTemporaryDir cleanup; assertions stay.
      const auto timeoutStderr = process.readAllStandardError();
      const auto timeoutStdout = process.readAllStandardOutput();
      const auto evidence = qEnvironmentVariable("SYNERGY_R3_EVIDENCE_DIR");
      if (!evidence.isEmpty() && QDir().mkpath(evidence)) {
        QFile receipt(root + "/receipt.json");
        QFile output(QDir(evidence).filePath(fixture + "-timeout-receipt.json"));
        if (receipt.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly))
          output.write(receipt.readAll());
        QFile stderrOutput(QDir(evidence).filePath(fixture + "-timeout-stderr.log"));
        if (stderrOutput.open(QIODevice::WriteOnly))
          stderrOutput.write(timeoutStderr);
        QFile stdoutOutput(QDir(evidence).filePath(fixture + "-timeout-stdout.log"));
        if (stdoutOutput.open(QIODevice::WriteOnly))
          stdoutOutput.write(timeoutStdout);
      }

      QFAIL(qPrintable(
          "Real-main probe timed out; unexpected dialogs are never approved. stderr=" + QString::fromUtf8(timeoutStderr)
      ));
    }
    const auto stderrBytes = process.readAllStandardError();
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(process.exitCode() == 0, stderrBytes.constData());
    QFile receiptFile(root + "/receipt.json");
    QVERIFY(receiptFile.open(QIODevice::ReadOnly));
    const auto receiptBytes = receiptFile.readAll();
    const auto receipt = QJsonDocument::fromJson(receiptBytes).object();
    QVERIFY(!receipt.isEmpty());
    QVERIFY(!receipt.contains("unexpectedDialog"));
    QCOMPARE(receipt.value("fixture").toString(), fixture);
    QCOMPARE(receipt.value("instanceKey").toString(), key);
    QCOMPARE(receipt.value("byteIdenticalDeliveredExecutable").toBool(), false);
    const QStringList expectedPhases{"context-ready",    "application-ready", "style-ready",      "preinit-ready",
                                     "i18n-ready",       "instance-ready",    "mainwindow-ready", "appstart-ready",
                                     "event-loop-ready", "clean-exit"};
    QStringList actualPhases;
    for (const auto phase : receipt.value("phases").toArray())
      actualPhases << phase.toString();
    QCOMPARE(actualPhases, expectedPhases);
    for (const char *name :
         {"userSettings", "systemSettings", "state", "overlay", "legacyUser", "legacySystem", "testSettings"})
      QVERIFY2(receipt.value(name).toString().startsWith(root + "/"), name);
    QCOMPARE(receipt.value("schemaAfterMigration").toInt(), synergy::gui::migration::kCurrentSchemaVersion);
    QCOMPARE(receipt.value("notifiedAfterMainWindow").toInt(), synergy::gui::migration::kCurrentSchemaVersion);
    QVERIFY(receipt.value("mainWindowVisible").toBool());
    QVERIFY(receipt.value("hasRealPageHost").toBool());
    QCOMPARE(receipt.value("coreChildProcessCount").toInt(), 0);
    QVERIFY(receipt.value("autoCoreDisabled").toBool());
    QVERIFY(receipt.value("updatesDisabled").toBool());
    QVERIFY(!receipt.value("testSettingsEnabled").toBool());
    QCOMPARE(receipt.value("testSettingsResolved").toString(), root + "/test-settings.ini");
    QCOMPARE(receipt.value("exitCode").toInt(-1), 0);
    QCOMPARE(contacted.count(), 0);
    if (fixture == "current")
      QCOMPARE(receipt.value("userPortAfterMigration").toInt(), 25252);
    if (fixture == "legacy") {
      QCOMPARE(receipt.value("userPortAfterMigration").toInt(), 25444);
      QCOMPARE(receipt.value("systemPortAfterMigration").toInt(), 25555);
      QVERIFY(receipt.value("userAutoCoreBeforeSafetyOverride").toBool());
      QVERIFY(receipt.value("systemAutoCoreBeforeSafetyOverride").toBool());
      QVERIFY(receipt.value("migrationNoticeObserved").toBool());
      for (const char *scope : {"user", "system"}) {
        const auto source = root + "/legacy/" + scope + ".ini";
        QSettings cleared(source, QSettings::IniFormat);
        QVERIFY(cleared.allKeys().isEmpty());
        const auto backupPath = root + "/" + scope + "/" + kAppName + ".conf.legacy.bak";
        QSettings backup(backupPath, QSettings::IniFormat);
        QCOMPARE(backup.value("obsoleteFixture").toString(), QString("backup-only-marker"));
        QCOMPARE(backup.value("port").toInt(), QByteArray(scope) == "user" ? 25444 : 25555);
      }
    }
    // Persist raw owned evidence when requested; this is not a product output path.
    const auto evidence = qEnvironmentVariable("SYNERGY_R3_EVIDENCE_DIR");
    if (!evidence.isEmpty()) {
      QVERIFY(QDir().mkpath(evidence));
      QFile output(QDir(evidence).filePath(fixture + "-startup.json"));
      QVERIFY(output.open(QIODevice::WriteOnly));
      output.write(receiptBytes);
      QFile stderrFile(QDir(evidence).filePath(fixture + "-stderr.log"));
      QVERIFY(stderrFile.open(QIODevice::WriteOnly));
      stderrFile.write(stderrBytes);
    }
  }
};
int main(int argc, char **argv)
{
  QCoreApplication application(argc, argv);
  R3StartupContractTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "R3StartupContractTests.moc"
