/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/Constants.h"
#include "common/ExitCodes.h"
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
#include <memory>

#ifdef Q_OS_MACOS
#include "CoreArgParser.h"
#include "common/RuntimeContext.h"
#include "platform/macos/InstanceGuard.h"
#include <QLocalSocket>
#include <QLockFile>
#include <QScopeGuard>
#include <cerrno>
#include <cstdio>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
class MacStartupScenario
{
public:
  bool initialize(bool longSocketPath = false)
  {
    if (!m_scratch.isValid())
      return false;
    m_sandbox = QFileInfo(m_scratch.path()).canonicalFilePath();
    if (m_sandbox.isEmpty())
      return false;
    if (longSocketPath) {
      const QString longName(110, QLatin1Char('s'));
      if (!QDir(m_sandbox).mkdir(longName))
        return false;
      m_sandbox += QLatin1Char('/') + longName;
    }
    m_root = m_sandbox + "/rt";
    return QDir(m_sandbox).mkdir("rt");
  }

  QString socketPath() const
  {
    return m_sandbox + QLatin1Char('/') + m_key;
  }

  QString guiLockPath() const
  {
    return m_root + QStringLiteral("/gui.lock");
  }

  QJsonObject receipt(const QString &role) const
  {
    QFile file(m_root + QLatin1Char('/') + role + QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly))
      return {};
    return QJsonDocument::fromJson(file.readAll()).object();
  }

  bool launch(QProcess &child, const QString &role) const
  {
    auto childEnvironment = QProcessEnvironment::systemEnvironment();
    childEnvironment.insert("TMPDIR", m_sandbox);
    childEnvironment.insert("SYNERGY_R3_PROBE_ROOT", m_root);
    childEnvironment.insert("SYNERGY_R3_PROBE_FIXTURE", "current");
    childEnvironment.insert("SYNERGY_R3_PROBE_KEY", m_key);
    childEnvironment.insert("SYNERGY_R3_PROBE_AUTOMATED", "1");
    childEnvironment.insert("SYNERGY_R3_PROBE_INSTANCE_ROLE", role);
    child.setProcessEnvironment(childEnvironment);
    child.start(QStringLiteral(R3_STARTUP_PROBE_PATH), {});
    return child.waitForStarted(3000);
  }

  bool writeControl(const QString &name) const
  {
    QFile marker(m_root + QLatin1Char('/') + name);
    return marker.open(QIODevice::WriteOnly | QIODevice::NewOnly);
  }

  void retainOwnedEvidence()
  {
    m_scratch.setAutoRemove(false);
    qWarning().noquote() << "Startup fixture retained its private evidence:" << m_scratch.path();
  }

private:
  QTemporaryDir m_scratch{QDir::temp().filePath("ts-XXXXXX")};
  QString m_sandbox;
  QString m_root;
  const QString m_key = "ts-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(10);
};

bool hasPhase(const QJsonObject &receipt, const QString &phase)
{
  return receipt.value("phases").toArray().contains(phase);
}

int coreGuardChild(char *argv[])
{
  using namespace synergy::platform::macos;
  const auto root = QString::fromLocal8Bit(argv[2]);
  const auto key = QString::fromLatin1(argv[3]);
  const auto command = QByteArray(argv[4]);
  if (QCoreApplication::instance() || (command != "hold" && command != "once" && command != "--new-instance") ||
      !synergy::runtime::configureIsolatedRuntime({root, key}))
    return s_exitFailed;

  QStringList arguments{QString::fromUtf8(kCoreBinName), "server"};
  if (command == "--new-instance")
    arguments.append("--new-instance");
  const CoreArgParser parser(arguments);
  if (!parser.errorText().isEmpty())
    return s_exitArgs;
  const auto guard = acquireCoreInstance(parser.singleInstanceOnly());
  const auto path = QDir(root).filePath("core.lock");
  if (guard.status == InstanceStatus::Duplicate)
    return s_exitDuplicate;
  if (guard.status == InstanceStatus::Bypassed)
    return command == "--new-instance" && !guard.lock ? s_exitSuccess : s_exitFailed;
  if (guard.status != InstanceStatus::Primary || command == "--new-instance" || !guard.lock ||
      !QFileInfo(path).isFile())
    return s_exitFailed;
  if (command == "once")
    return s_exitSuccess;

  std::puts("core-lock-ready-before-application");
  std::fflush(stdout);
  pollfd input{STDIN_FILENO, POLLIN, 0};
  char control = '\0';
  return ::poll(&input, 1, 8000) == 1 && ::read(STDIN_FILENO, &control, 1) == 1 && control == 'q' ? s_exitSuccess
                                                                                                  : s_exitFailed;
}
} // namespace
#endif

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

#ifdef Q_OS_MACOS
  void macGuiCleanQuitAndRelaunch()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    QProcess primary;
    QVERIFY2(scenario.launch(primary, "primary"), qPrintable(primary.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(hasPhase(scenario.receipt("primary"), "event-loop-ready"), 5000);
    QVERIFY(scenario.receipt("primary").value("mainWindowVisible").toBool());
    QVERIFY(QFileInfo::exists(scenario.guiLockPath()));
    QVERIFY(QFileInfo::exists(scenario.socketPath()));
    QVERIFY(scenario.writeControl("quit-primary"));
    QVERIFY(primary.waitForFinished(4000));
    QCOMPARE(primary.exitStatus(), QProcess::NormalExit);
    QCOMPARE(primary.exitCode(), s_exitSuccess);
    QVERIFY(!QFileInfo::exists(scenario.guiLockPath()));
    QVERIFY(!QFileInfo::exists(scenario.socketPath()));

    QProcess successor;
    QVERIFY2(scenario.launch(successor, "verify"), qPrintable(successor.errorString()));
    QVERIFY(successor.waitForFinished(5000));
    QCOMPARE(successor.exitStatus(), QProcess::NormalExit);
    QCOMPARE(successor.exitCode(), s_exitSuccess);
    QVERIFY(hasPhase(scenario.receipt("verify"), "clean-exit"));
    QVERIFY(scenario.receipt("verify").value("mainWindowVisible").toBool());
  }

  void macGuiExistingLegacyReceiver()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    QLocalServer legacy;
    QVERIFY2(legacy.listen(scenario.socketPath()), qPrintable(legacy.errorString()));
    QSignalSpy activations(&legacy, &QLocalServer::newConnection);
    struct stat before{}, after{};
    const auto endpoint = QFile::encodeName(scenario.socketPath());
    QCOMPARE(::lstat(endpoint.constData(), &before), 0);
    QVERIFY(S_ISSOCK(before.st_mode));

    QProcess contender;
    QVERIFY2(scenario.launch(contender, "contender"), qPrintable(contender.errorString()));
    QTRY_COMPARE_WITH_TIMEOUT(activations.count(), 1, 3000);
    std::unique_ptr<QLocalSocket> accepted(legacy.nextPendingConnection());
    QVERIFY(accepted);
    QVERIFY(contender.waitForFinished(4000) || contender.state() == QProcess::NotRunning);
    QCOMPARE(contender.exitStatus(), QProcess::NormalExit);
    QCOMPARE(contender.exitCode(), s_exitDuplicate);
    QVERIFY(!hasPhase(scenario.receipt("contender"), "mainwindow-ready"));
    QVERIFY(!QFileInfo::exists(scenario.guiLockPath()));
    QCOMPARE(::lstat(endpoint.constData(), &after), 0);
    QCOMPARE(after.st_dev, before.st_dev);
    QCOMPARE(after.st_ino, before.st_ino);
  }

  void macGuiContenderActivatesPrimary()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    QLocalServer witness;
    QVERIFY(witness.listen(scenario.socketPath() + "-w"));
    QSignalSpy contacted(&witness, &QLocalServer::newConnection);
    QProcess primary;
    QVERIFY2(scenario.launch(primary, "primary"), qPrintable(primary.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(hasPhase(scenario.receipt("primary"), "event-loop-ready"), 5000);
    QVERIFY(scenario.receipt("primary").value("mainWindowVisible").toBool());

    QProcess contender;
    QVERIFY2(scenario.launch(contender, "contender"), qPrintable(contender.errorString()));
    QVERIFY(contender.waitForFinished(4000) || contender.state() == QProcess::NotRunning);
    QCOMPARE(contender.exitStatus(), QProcess::NormalExit);
    QCOMPARE(contender.exitCode(), s_exitDuplicate);
    QVERIFY(!hasPhase(scenario.receipt("contender"), "mainwindow-ready"));
    QTRY_COMPARE_WITH_TIMEOUT(scenario.receipt("primary").value("activationDeliveries").toInt(), 1, 1500);
    QCOMPARE(primary.state(), QProcess::Running);
    QVERIFY(QFileInfo::exists(scenario.guiLockPath()));
    QVERIFY(QFileInfo::exists(scenario.socketPath()));
    QCOMPARE(contacted.count(), 0);

    QVERIFY(scenario.writeControl("quit-primary"));
    QVERIFY(primary.waitForFinished(4000));
    QCOMPARE(primary.exitStatus(), QProcess::NormalExit);
    QCOMPARE(primary.exitCode(), s_exitSuccess);
  }

  void macGuiRepeatedActivationRestoresExistingWindow()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    QVERIFY(scenario.writeControl("hide-primary"));
    QLocalServer witness;
    QVERIFY(witness.listen(scenario.socketPath() + "-w"));
    QSignalSpy unrelatedActivations(&witness, &QLocalServer::newConnection);

    QProcess primary;
    QVERIFY2(scenario.launch(primary, "primary"), qPrintable(primary.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(scenario.receipt("primary").value("hiddenBeforeActivation").toBool(), 5000);
    QCOMPARE(scenario.receipt("primary").value("activationSocket").toString(), scenario.socketPath());
    for (int delivery = 1; delivery <= 40; ++delivery) {
      QLocalSocket socket;
      socket.connectToServer(scenario.socketPath(), QIODevice::ReadOnly);
      QVERIFY2(
          socket.state() == QLocalSocket::ConnectedState || socket.waitForConnected(600),
          qPrintable(socket.errorString())
      );
      socket.disconnectFromServer();
      QTRY_COMPARE_WITH_TIMEOUT(scenario.receipt("primary").value("activationDeliveries").toInt(), delivery, 600);
    }
    const auto observation = scenario.receipt("primary");
    QVERIFY(observation.value("activationWindowVisible").toBool());
    QVERIFY(observation.value("activationQueueDrained").toBool());
    QCOMPARE(unrelatedActivations.count(), 0);
    QVERIFY(scenario.writeControl("quit-primary"));
    QVERIFY(primary.waitForFinished(4000));
    QCOMPARE(primary.exitStatus(), QProcess::NormalExit);
    QCOMPARE(primary.exitCode(), s_exitSuccess);
  }

  void macGuiConcurrentStartsHaveOnePrimary()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    QProcess first, second;
    QVERIFY2(scenario.launch(first, "primary"), qPrintable(first.errorString()));
    QVERIFY2(scenario.launch(second, "contender"), qPrintable(second.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(first.state() == QProcess::NotRunning || second.state() == QProcess::NotRunning, 5000);

    const bool firstWon = first.state() != QProcess::NotRunning;
    auto &owner = firstWon ? first : second;
    auto &other = firstWon ? second : first;
    const auto ownerRole = firstWon ? "primary" : "contender";
    const auto otherRole = firstWon ? "contender" : "primary";
    QTRY_VERIFY_WITH_TIMEOUT(hasPhase(scenario.receipt(ownerRole), "event-loop-ready"), 3000);
    QVERIFY(!hasPhase(scenario.receipt(otherRole), "mainwindow-ready"));
    QCOMPARE(other.exitStatus(), QProcess::NormalExit);
    const auto result = scenario.receipt(otherRole);
    QVERIFY(
        other.exitCode() == s_exitDuplicate ||
        (other.exitCode() == s_exitFailed && result.value("ownedStartupDiagnostic").toString() == "busy")
    );
    QVERIFY(scenario.writeControl("quit-primary"));
    QVERIFY(owner.waitForFinished(4000));
    QCOMPARE(owner.exitStatus(), QProcess::NormalExit);
    QCOMPARE(owner.exitCode(), s_exitSuccess);
  }

  void macGuiRecoversAfterOwnerCrash()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    QProcess primary;
    QVERIFY2(scenario.launch(primary, "primary"), qPrintable(primary.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(hasPhase(scenario.receipt("primary"), "event-loop-ready"), 5000);
    const auto pid = primary.processId();
    QVERIFY(pid > 0);
    primary.kill();
    QVERIFY(primary.waitForFinished(4000));
    QCOMPARE(primary.exitStatus(), QProcess::CrashExit);
    QVERIFY(QFileInfo::exists(scenario.guiLockPath()));
    QVERIFY(QFileInfo::exists(scenario.socketPath()));

    QProcess successor;
    QVERIFY2(scenario.launch(successor, "verify"), qPrintable(successor.errorString()));
    QVERIFY(successor.waitForFinished(5000) || successor.state() == QProcess::NotRunning);
    QCOMPARE(successor.exitStatus(), QProcess::NormalExit);
    QCOMPARE(successor.exitCode(), s_exitSuccess);
    QVERIFY(hasPhase(scenario.receipt("verify"), "clean-exit"));
    QVERIFY(!QFileInfo::exists(scenario.guiLockPath()));
    QVERIFY(!QFileInfo::exists(scenario.socketPath()));
  }

  void macGuiHeldLockIsBusy()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    QLockFile owner(scenario.guiLockPath());
    owner.setStaleLockTime(0);
    QVERIFY(owner.tryLock(0));
    struct stat before{}, after{};
    const auto lock = QFile::encodeName(scenario.guiLockPath());
    QCOMPARE(::lstat(lock.constData(), &before), 0);

    QProcess contender;
    QVERIFY2(scenario.launch(contender, "contender"), qPrintable(contender.errorString()));
    QVERIFY(contender.waitForFinished(4000) || contender.state() == QProcess::NotRunning);
    QCOMPARE(contender.exitStatus(), QProcess::NormalExit);
    const auto result = scenario.receipt("contender");
    QVERIFY(!hasPhase(result, "mainwindow-ready"));
    QCOMPARE(::lstat(lock.constData(), &after), 0);
    QCOMPARE(after.st_dev, before.st_dev);
    QCOMPARE(after.st_ino, before.st_ino);
    QVERIFY(!QFileInfo::exists(scenario.socketPath()));
    QCOMPARE(contender.exitCode(), s_exitFailed);
    QCOMPARE(result.value("ownedStartupDiagnostic").toString(), QStringLiteral("busy"));
  }

  void macGuiInvalidEndpointIsPreserved_data()
  {
    QTest::addColumn<bool>("isSymlink");
    QTest::newRow("ordinary file") << false;
    QTest::newRow("symlink") << true;
  }

  void macGuiInvalidEndpointIsPreserved()
  {
    QFETCH(bool, isSymlink);
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    QLocalServer witness;
    QVERIFY(witness.listen(scenario.socketPath() + "-w"));
    QSignalSpy contacted(&witness, &QLocalServer::newConnection);
    const auto path = scenario.socketPath();
    QFile sentinel(isSymlink ? path + "-target" : path);
    const QByteArray bytes("owned test sentinel\n");
    QVERIFY(sentinel.open(QIODevice::WriteOnly | QIODevice::NewOnly));
    QCOMPARE(sentinel.write(bytes), bytes.size());
    sentinel.close();
    if (isSymlink)
      QVERIFY(sentinel.link(path));
    struct stat before{}, after{};
    const auto encodedPath = QFile::encodeName(path);
    QCOMPARE(::lstat(encodedPath.constData(), &before), 0);
    QVERIFY(isSymlink ? S_ISLNK(before.st_mode) : S_ISREG(before.st_mode));

    QProcess process;
    QVERIFY2(scenario.launch(process, "verify"), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished(4000) || process.state() == QProcess::NotRunning);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), s_exitFailed);
    const auto result = scenario.receipt("verify");
    QCOMPARE(result.value("ownedStartupDiagnostic").toString(), QStringLiteral("startup-failure"));
    QVERIFY(!hasPhase(result, "mainwindow-ready"));
    QCOMPARE(::lstat(encodedPath.constData(), &after), 0);
    QCOMPARE(after.st_mode, before.st_mode);
    QCOMPARE(after.st_dev, before.st_dev);
    QCOMPARE(after.st_ino, before.st_ino);
    QVERIFY(sentinel.open(QIODevice::ReadOnly));
    QCOMPARE(sentinel.readAll(), bytes);
    QCOMPARE(contacted.count(), 0);
    QVERIFY(!QFileInfo::exists(scenario.guiLockPath()));
  }

  void macGuiListenFailureReleasesLock()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize(true));
    struct sockaddr_un capacity{};
    QVERIFY(QFile::encodeName(scenario.socketPath()).size() >= qsizetype(sizeof(capacity.sun_path)));

    QProcess process;
    QVERIFY2(scenario.launch(process, "verify"), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished(4000) || process.state() == QProcess::NotRunning);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), s_exitFailed);
    const auto result = scenario.receipt("verify");
    QCOMPARE(result.value("ownedStartupDiagnostic").toString(), QStringLiteral("startup-failure"));
    QVERIFY(!hasPhase(result, "mainwindow-ready"));
    QVERIFY(!QFileInfo::exists(scenario.guiLockPath()));
    QVERIFY(!QFileInfo::exists(scenario.socketPath()));
  }

  void macGuiUnusableLockPathIsFailure()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    const auto path = scenario.guiLockPath();
    QVERIFY(QDir().mkdir(path));
    QVERIFY(QFileInfo(path).isDir());

    QProcess process;
    QVERIFY2(scenario.launch(process, "verify"), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished(4000) || process.state() == QProcess::NotRunning);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), s_exitFailed);
    const auto result = scenario.receipt("verify");
    QCOMPARE(result.value("ownedStartupDiagnostic").toString(), QStringLiteral("startup-failure"));
    QVERIFY(!hasPhase(result, "mainwindow-ready"));
    QVERIFY(QFileInfo(path).isDir());
    QVERIFY(!QFileInfo::exists(scenario.socketPath()));
  }

  void macGuiRestartHandsOffAfterTeardown()
  {
    MacStartupScenario scenario;
    QVERIFY(scenario.initialize());
    // Restart is the one route that creates a detached successor. If its
    // exact child receipt or exit cannot be confirmed, preserve its fixture.
    bool successorExited = false;
    const auto preserve = qScopeGuard([&] {
      if (!successorExited)
        scenario.retainOwnedEvidence();
    });
    QProcess outgoing;
    QVERIFY2(scenario.launch(outgoing, "restart"), qPrintable(outgoing.errorString()));
    const auto parentPid = outgoing.processId();
    QVERIFY(parentPid > 0);
    QVERIFY(outgoing.waitForFinished(6000) || outgoing.state() == QProcess::NotRunning);
    QTRY_VERIFY_WITH_TIMEOUT(scenario.receipt("restart-successor").value("successorPid").toInteger() > 0, 5000);
    const auto successorPidValue = scenario.receipt("restart-successor").value("successorPid").toInteger();
    QCOMPARE(scenario.receipt("restart-successor").value("parentPid").toInteger(), parentPid);
    const auto successorPid = static_cast<pid_t>(successorPidValue);
    QVERIFY(successorPid > 0 && successorPid != parentPid && successorPid == successorPidValue);
    const auto exited = [successorPid] { return ::kill(successorPid, 0) == -1 && errno == ESRCH; };
    QTRY_VERIFY_WITH_TIMEOUT(exited(), 5000);
    successorExited = true;

    QCOMPARE(outgoing.exitStatus(), QProcess::NormalExit);
    QCOMPARE(outgoing.exitCode(), s_exitSuccess);
    const auto outgoingReceipt = scenario.receipt("restart");
    const auto successor = scenario.receipt("restart-successor");
    QVERIFY(!outgoingReceipt.value("successorSeenBeforeTeardown").toBool(true));
    QVERIFY(!successor.value("restartSawOldGuard").toBool(true));
    const QJsonObject observation{
        {"successorSeenBeforeTeardown", outgoingReceipt.value("successorSeenBeforeTeardown")},
        {"phases", successor.value("phases")},
        {"exitCode", successor.value("exitCode")},
        {"mainWindowVisible", successor.value("mainWindowVisible")},
        {"restartSawOldGuard", successor.value("restartSawOldGuard")}
    };
    const auto diagnostic = QJsonDocument(observation).toJson(QJsonDocument::Compact);
    QVERIFY2(successor.value("mainWindowVisible").toBool(), diagnostic.constData());
    QVERIFY(hasPhase(successor, "clean-exit"));
    QVERIFY(!QFileInfo::exists(scenario.guiLockPath()));
    QVERIFY(!QFileInfo::exists(scenario.socketPath()));
  }

  void macCoreArgumentPolicy_data()
  {
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<bool>("ownLock");
    const auto executable = QString::fromUtf8(kCoreBinName);
    QTest::newRow("server") << QStringList{executable, "server", "--settings", "unused-fixture.ini"} << true;
    QTest::newRow("client") << QStringList{executable, "client", "--settings", "unused-fixture.ini"} << true;
    QTest::newRow("new-server") << QStringList{executable, "server", "--new-instance"} << false;
    QTest::newRow("new-client") << QStringList{executable, "client", "--new-instance"} << false;
  }

  void macCoreArgumentPolicy()
  {
    QFETCH(QStringList, arguments);
    QFETCH(bool, ownLock);
    const CoreArgParser parser(arguments);
    QVERIFY2(parser.errorText().isEmpty(), qPrintable(parser.errorText()));
    QCOMPARE(parser.singleInstanceOnly(), ownLock);
  }

  void macCoreGuardBeforeApplication()
  {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto root = QFileInfo(temp.path()).canonicalFilePath();
    const auto key = "ts-core-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto executable = QCoreApplication::applicationFilePath();
    const auto childArgs = [&](const QString &command) {
      return QStringList{"--mac-core-guard-child", root, key, command};
    };

    QProcess owner;
    owner.start(executable, childArgs("hold"));
    QVERIFY2(owner.waitForStarted(3000), qPrintable(owner.errorString()));
    QVERIFY(owner.bytesAvailable() > 0 || owner.waitForReadyRead(4000));
    QCOMPARE(owner.readLine().trimmed(), QByteArray("core-lock-ready-before-application"));
    const auto lockPath = QDir(root).filePath("core.lock");
    const auto lock = QFile::encodeName(lockPath);
    struct stat before{}, after{};
    QCOMPARE(::lstat(lock.constData(), &before), 0);
    QVERIFY(S_ISREG(before.st_mode));

    QProcess contender;
    contender.start(executable, childArgs("once"));
    QVERIFY(contender.waitForFinished(4000));
    QCOMPARE(contender.exitStatus(), QProcess::NormalExit);
    QCOMPARE(contender.exitCode(), s_exitDuplicate);

    QProcess multi;
    multi.start(executable, childArgs("--new-instance"));
    QVERIFY(multi.waitForFinished(4000));
    QCOMPARE(multi.exitStatus(), QProcess::NormalExit);
    QCOMPARE(multi.exitCode(), s_exitSuccess);
    QCOMPARE(::lstat(lock.constData(), &after), 0);
    QCOMPARE(after.st_dev, before.st_dev);
    QCOMPARE(after.st_ino, before.st_ino);

    QCOMPARE(owner.write("q"), qint64(1));
    QVERIFY(owner.waitForFinished(4000));
    QCOMPARE(owner.exitStatus(), QProcess::NormalExit);
    QCOMPARE(owner.exitCode(), s_exitSuccess);
    QVERIFY(!QFileInfo::exists(lockPath));
    QProcess successor;
    successor.start(executable, childArgs("once"));
    QVERIFY(successor.waitForFinished(4000));
    QCOMPARE(successor.exitStatus(), QProcess::NormalExit);
    QCOMPARE(successor.exitCode(), s_exitSuccess);
    QVERIFY(!QFileInfo::exists(lockPath));

    QVERIFY(QDir().mkdir(lockPath));
    multi.start(executable, childArgs("--new-instance"));
    QVERIFY(multi.waitForFinished(4000));
    QCOMPARE(multi.exitStatus(), QProcess::NormalExit);
    QCOMPARE(multi.exitCode(), s_exitSuccess);
    QVERIFY(QFileInfo(lockPath).isDir());

    QVERIFY(QDir().rmdir(lockPath));
    QFile malformed(lockPath);
    const QByteArray unreadableOwner("invalid lock fixture\n");
    QVERIFY(malformed.open(QIODevice::WriteOnly | QIODevice::NewOnly));
    QCOMPARE(malformed.write(unreadableOwner), unreadableOwner.size());
    malformed.close();
    contender.start(executable, childArgs("once"));
    QVERIFY(contender.waitForFinished(4000));
    QCOMPARE(contender.exitStatus(), QProcess::NormalExit);
    QCOMPARE(contender.exitCode(), s_exitFailed);
    QVERIFY(malformed.open(QIODevice::ReadOnly));
    QCOMPARE(malformed.readAll(), unreadableOwner);
  }

  void macGuiPendingConnectCompletes()
  {
    int pair[2]{};
    QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM, 0, pair), 0);
    const auto cleanup = qScopeGuard([&] {
      ::close(pair[0]);
      ::close(pair[1]);
    });
    // Force the pending-stage input and use a real local socket that has
    // completed, so poll and SO_ERROR must jointly establish its success.
    using synergy::platform::macos::detail::activationConnectError;
    QCOMPARE(activationConnectError(pair[0], EINPROGRESS), 0);
    QCOMPARE(activationConnectError(pair[0], ECONNREFUSED), ECONNREFUSED);
    QCOMPARE(activationConnectError(pair[0], EINVAL), EINVAL);
  }
#endif
};
int main(int argc, char **argv)
{
#ifdef Q_OS_MACOS
  if (argc == 5 && QByteArray(argv[1]) == "--mac-core-guard-child")
    return coreGuardChild(argv);
#endif
  QCoreApplication application(argc, argv);
  R3StartupContractTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "R3StartupContractTests.moc"
