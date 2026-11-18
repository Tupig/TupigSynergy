/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/Action.h"
#include "gui/Hotkey.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/CoreProcess.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <deque>

namespace {
struct ExitMessage
{
  QtMsgType type;
  QString text;
};
thread_local QList<ExitMessage> *exitMessages = nullptr;
void captureExitMessage(QtMsgType type, const QMessageLogContext &, const QString &text)
{
  if (exitMessages)
    exitMessages->append({type, text});
}

enum class ExitClassification
{
  Crash,
  IntentionalStop,
  Normal,
  Failure
};

void seedConfig(int type, int direction, int lock, int modifier)
{
  auto &settings = Settings::proxy().get();
  settings.remove("internalConfig");
  settings.beginGroup("internalConfig");
  settings.beginWriteArray("hotkeys", 1);
  settings.setArrayIndex(0);
  KeySequence trigger;
  trigger.appendKey(Qt::Key_Control, Qt::ControlModifier);
  trigger.appendKey(Qt::Key_K, Qt::ControlModifier);
  trigger.saveSettings(settings);
  settings.beginWriteArray("actions", 1);
  settings.setArrayIndex(0);
  KeySequence keys;
  keys.appendKey(Qt::Key_Alt, Qt::AltModifier);
  keys.appendKey(Qt::Key_J, Qt::AltModifier);
  keys.saveSettings(settings);
  settings.setValue(SettingsKeys::ActionType, type);
  settings.setValue(SettingsKeys::SwitchDirection, direction);
  settings.setValue(SettingsKeys::LockToScreen, lock);
  settings.setValue(SettingsKeys::SwitchToScreen, "Studio");
  settings.setValue(SettingsKeys::RestartServer, type == 7);
  settings.endArray();
  settings.endArray();
  settings.beginWriteArray("screens", 1);
  settings.setArrayIndex(0);
  settings.setValue("name", "Test-Server");
  settings.beginWriteArray("modifierArray", 7);
  for (int i = 0; i < 7; ++i) {
    settings.setArrayIndex(i);
    settings.setValue("modifier", i == 0 ? modifier : i);
  }
  settings.endArray();
  settings.endArray();
  settings.endGroup();
  settings.sync();
}
QByteArray readFile(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
} // namespace
class R3SerializationTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void invalidConfigCannotTruncateOrPartiallyWrite_data();
  void invalidConfigCannotTruncateOrPartiallyWrite();
  void validCanonicalEncodings_data();
  void validCanonicalEncodings();
  void ioRefusalPreservesExistingPayload();
  void actualCorePersistenceDoesNotLaunchOrTruncate();
  void actualCoreExitClassification_data();
  void actualCoreExitClassification();
};
void R3SerializationTests::invalidConfigCannotTruncateOrPartiallyWrite_data()
{
  QTest::addColumn<int>("type");
  QTest::addColumn<int>("direction");
  QTest::addColumn<int>("lock");
  QTest::addColumn<int>("modifier");
  // QTest::newRow 只保存传入的 char 指针，不复制内容；若直接传 qPrintable(临时)，
  // 全表达式结束后指针悬垂。deque 在 push_back 时不移动既有元素，数据行名始终有效。
  std::deque<QByteArray> rowNames;
  const auto rowName = [&rowNames](const char *kind, int value) -> const char * {
    rowNames.push_back(QByteArray(kind) + '-' + QByteArray::number(value));
    return rowNames.back().constData();
  };
  for (int invalid : {-1, 11, 2000000})
    QTest::newRow(rowName("type", invalid)) << invalid << 0 << 0 << 0;
  for (int invalid : {-1, 4, 2000000})
    QTest::newRow(rowName("direction", invalid)) << 4 << invalid << 0 << 0;
  for (int invalid : {-1, 3, 2000000})
    QTest::newRow(rowName("lock", invalid)) << 6 << 0 << invalid << 0;
  for (int invalid : {-2, 7, 2000000})
    QTest::newRow(rowName("modifier", invalid)) << 3 << 0 << 0 << invalid;
}
void R3SerializationTests::invalidConfigCannotTruncateOrPartiallyWrite()
{
  QFETCH(int, type);
  QFETCH(int, direction);
  QFETCH(int, lock);
  QFETCH(int, modifier);
  seedConfig(type, direction, lock, modifier);
  synergy::gui::ServerConfig config;
  QTemporaryDir dir;
  const auto path = dir.filePath("existing.conf");
  const QByteArray sentinel = QByteArray("existing-config\nkeep-exact-bytes") + '\0' + "tail";
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(sentinel), qint64(sentinel.size()));
  file.close();
  QVERIFY(!config.save(path));
  QCOMPARE(readFile(path), sentinel);
  const auto missing = dir.filePath("new.conf");
  QVERIFY(!config.save(missing));
  QVERIFY(!QFile::exists(missing));
  QString output("prefix-must-survive");
  QTextStream stream(&output, QIODevice::Append);
  stream << config;
  QCOMPARE(output, QString("prefix-must-survive"));
  QVERIFY(stream.status() != QTextStream::Ok);
  // Safe summaries must never index out of range for an unknown action.
  const auto &action = std::as_const(config).hotkeys().first().actions().first();
  if (type < 0 || type > 10)
    QVERIFY(action.text().isEmpty());
}
void R3SerializationTests::validCanonicalEncodings_data()
{
  QTest::addColumn<int>("type");
  QTest::addColumn<int>("direction");
  QTest::addColumn<int>("lock");
  QTest::addColumn<QString>("expected");
  const char *commands[]{
      "keyDown(Alt+j,*)",
      "keyUp(Alt+j,*)",
      "keystroke(Alt+j,*)",
      "switchToScreen(Studio)",
      "switchInDirection(left)",
      "switchToNextScreen",
      "lockCursorToScreen(toggle)",
      "restartServer",
      "mouseDown",
      "mouseUp",
      "mousebutton"
  };
  for (int type = 0; type <= 10; ++type)
    QTest::newRow(qPrintable(QStringLiteral("legacy-valid-%1").arg(type)))
        << type << 0 << 0 << QString::fromLatin1(commands[type]);
  const char *directions[]{"left", "right", "up", "down"};
  for (int direction = 0; direction < 4; ++direction)
    QTest::newRow(
        directions[direction]
    ) << 4
      << direction << 0 << QStringLiteral("switchInDirection(%1)").arg(QString::fromLatin1(directions[direction]));
  const char *locks[]{"toggle", "on", "off"};
  for (int lock = 0; lock < 3; ++lock)
    QTest::newRow(locks[lock]) << 6 << 0 << lock
                               << QStringLiteral("lockCursorToScreen(%1)").arg(QString::fromLatin1(locks[lock]));
  QTest::newRow("irrelevant-invalid-fields-preserved") << 3 << -70 << 99 << QString("switchToScreen(Studio)");
}
void R3SerializationTests::validCanonicalEncodings()
{
  QFETCH(int, type);
  QFETCH(int, direction);
  QFETCH(int, lock);
  QFETCH(QString, expected);
  seedConfig(type, direction, lock, -1);
  synergy::gui::ServerConfig config;
  QCOMPARE(std::as_const(config).hotkeys().size(), 1);
  QCOMPARE(std::as_const(config).hotkeys().first().actions().size(), 1);
  const auto &action = std::as_const(config).hotkeys().first().actions().first();
  QCOMPARE(action.text(), expected);
  QString actionBytes;
  QTextStream actionStream(&actionBytes);
  actionStream << action;
  QCOMPARE(actionBytes, expected);
  QTemporaryDir dir;
  const auto path = dir.filePath("valid.conf");
  QVERIFY(config.save(path));
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
  const auto bytes = file.readAll();
  QVERIFY(bytes.startsWith("section: screens\n"));
  QVERIFY(bytes.contains(expected.toUtf8()));
  QVERIFY(bytes.endsWith("end\n\n"));
}
void R3SerializationTests::ioRefusalPreservesExistingPayload()
{
  seedConfig(3, 0, 0, 0);
  synergy::gui::ServerConfig config;
  QTemporaryDir dir;
  const auto targetDirectory = dir.filePath("occupied.conf");
  QVERIFY(QDir().mkpath(targetDirectory));
  const auto sentinelPath = targetDirectory + "/existing.conf";
  QFile sentinel(sentinelPath);
  QVERIFY(sentinel.open(QIODevice::WriteOnly));
  const QByteArray bytes("retain-on-io-refusal\n");
  QCOMPARE(sentinel.write(bytes), qint64(bytes.size()));
  sentinel.close();
  // Deterministic open/commit refusal, not a short-write fault injection.
  QVERIFY(!config.save(targetDirectory));
  QCOMPARE(readFile(sentinelPath), bytes);
  QVERIFY(QFileInfo(targetDirectory).isDir());
  const auto missingParent = dir.filePath("absent-parent/output.conf");
  QVERIFY(!config.save(missingParent));
  QVERIFY(!QFile::exists(missingParent));
  QCOMPARE(readFile(sentinelPath), bytes);
}
void R3SerializationTests::actualCorePersistenceDoesNotLaunchOrTruncate()
{
  seedConfig(-1, 0, 0, 0);
  synergy::gui::ServerConfig config;
  Settings::setValue(Settings::Server::ExternalConfig, false);
  Settings::setValue(Settings::Core::ProcessMode, QVariant::fromValue(Settings::ProcessMode::Desktop));
  const auto path = Settings::defaultValue(Settings::Server::ExternalConfigFile).toString();
  QVERIFY(path.startsWith(QDir::tempPath()) || path.startsWith(QFileInfo(QDir::tempPath()).canonicalFilePath()));
  QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  const QByteArray sentinel("existing-core-config\n");
  QCOMPARE(file.write(sentinel), qint64(sentinel.size()));
  file.close();
  synergy::gui::CoreProcess core(config);
  const auto initial = core.processState();
  QSignalSpy changed(&core, &synergy::gui::CoreProcess::processStateChanged);
  // Narrow friend access invokes the exact production persistence helper only.
  const auto result = core.persistServerConfig();
  QVERIFY(!result.first);
  QCOMPARE(result.second, path);
  QCOMPARE(readFile(path), sentinel);
  QCOMPARE(core.processState(), initial);
  QCOMPARE(changed.count(), 0);
  QVERIFY(core.findChildren<QProcess *>().isEmpty());
}

void R3SerializationTests::actualCoreExitClassification_data()
{
  using State = synergy::core::ProcessState;
  using Classification = ExitClassification;
  QTest::addColumn<int>("initialState");
  QTest::addColumn<int>("rawExitCode");
  QTest::addColumn<int>("exitStatus");
  QTest::addColumn<bool>("stableUptime");
  QTest::addColumn<int>("expectedState");
  QTest::addColumn<bool>("expectedRetry");
  QTest::addColumn<int>("expectedInterval");
  QTest::addColumn<int>("expectedNextDelay");
  QTest::addColumn<int>("expectedClassification");
  struct Row
  {
    const char *name;
    State initial;
    int code;
    QProcess::ExitStatus status;
    bool stable;
    State expected;
    bool retry;
    int interval;
    int nextDelay;
    Classification classification;
  };
  // No raw 5 row: the unfixed handler could contact the fixed live core IPC.
  const Row rows[]{
      {"crash-stopped-0", State::Stopped, 0, QProcess::CrashExit, false, State::Stopped, false, 0, 2000,
       Classification::Crash},
      {"crash-stopped-9", State::Stopped, 9, QProcess::CrashExit, false, State::Stopped, false, 0, 2000,
       Classification::Crash},
      {"crash-started-0", State::Started, 0, QProcess::CrashExit, false, State::Stopped, false, 0, 2000,
       Classification::Crash},
      {"crash-started-9", State::Started, 9, QProcess::CrashExit, false, State::Stopped, false, 0, 2000,
       Classification::Crash},
      {"crash-stopping-0", State::Stopping, 0, QProcess::CrashExit, false, State::Stopped, false, 0, 2000,
       Classification::IntentionalStop},
      {"crash-stopping-9", State::Stopping, 9, QProcess::CrashExit, false, State::Stopped, false, 0, 2000,
       Classification::IntentionalStop},
      {"normal-stopped-0", State::Stopped, 0, QProcess::NormalExit, false, State::Stopped, false, 0, 2000,
       Classification::Normal},
      {"normal-started-recent-0", State::Started, 0, QProcess::NormalExit, false, State::RetryPending, true, 2000, 4000,
       Classification::Normal},
      {"normal-started-stable-0", State::Started, 0, QProcess::NormalExit, true, State::RetryPending, true, 1000, 2000,
       Classification::Normal},
      {"normal-stopping-0", State::Stopping, 0, QProcess::NormalExit, false, State::Stopped, false, 0, 2000,
       Classification::IntentionalStop},
      {"normal-stopped-9", State::Stopped, 9, QProcess::NormalExit, false, State::Stopped, false, 0, 2000,
       Classification::Failure},
      {"normal-started-9", State::Started, 9, QProcess::NormalExit, false, State::Stopped, false, 0, 2000,
       Classification::Failure},
      {"normal-stopping-9", State::Stopping, 9, QProcess::NormalExit, false, State::Stopped, false, 0, 2000,
       Classification::IntentionalStop},
  };
  for (const auto &row : rows)
    QTest::newRow(row.name) << int(row.initial) << row.code << int(row.status) << row.stable << int(row.expected)
                            << row.retry << row.interval << row.nextDelay << int(row.classification);
}

void R3SerializationTests::actualCoreExitClassification()
{
  QFETCH(int, initialState);
  QFETCH(int, rawExitCode);
  QFETCH(int, exitStatus);
  QFETCH(bool, stableUptime);
  QFETCH(int, expectedState);
  QFETCH(bool, expectedRetry);
  QFETCH(int, expectedInterval);
  QFETCH(int, expectedNextDelay);
  QFETCH(int, expectedClassification);
  using State = synergy::core::ProcessState;
  synergy::gui::ServerConfig config;
  synergy::gui::CoreProcess core(config);
  const auto stopRetryOnExit = qScopeGuard([&] { core.m_retryTimer.stop(); });
  core.m_processState = State(initialState);
  core.m_connectionState = synergy::core::ConnectionState::Connected;
  core.m_retryDelayMs = 2000;
  core.m_startedAt = std::chrono::steady_clock::now() - std::chrono::seconds(stableUptime ? 11 : 0);
  core.m_retryTimer.setSingleShot(true);
  core.m_retryTimer.start(60000);
  const bool seededTimerActive = core.m_retryTimer.isActive();
  QList<ExitMessage> messages;
  State observedState = State::Stopped;
  auto observedConnection = synergy::core::ConnectionState::Disconnected;
  int observedNextDelay = 0;
  bool retryActive = false;
  int retryInterval = 0;
  {
    // Capture only the handler, not constructor diagnostics about an absent
    // core binary next to this unit executable. No process or IPC is started.
    auto *previousMessages = exitMessages;
    exitMessages = &messages;
    const auto previousHandler = qInstallMessageHandler(captureExitMessage);
    const auto restoreMessages = qScopeGuard([&] {
      qInstallMessageHandler(previousHandler);
      exitMessages = previousMessages;
    });
    core.onProcessFinished(rawExitCode, QProcess::ExitStatus(exitStatus));
    observedState = core.processState();
    observedConnection = core.connectionState();
    observedNextDelay = core.m_retryDelayMs;
    retryInterval = core.m_retryTimer.interval();
    retryActive = core.m_retryTimer.isActive();
    core.m_retryTimer.stop();
  }
  // Every timer is stopped and the message handler restored before assertions.
  const auto hasMessage = [&](QtMsgType type, const QString &text) {
    return std::any_of(messages.cbegin(), messages.cend(), [&](const auto &message) {
      return message.type == type && message.text == text;
    });
  };
  const auto hasText = [&](const QString &text) {
    return std::any_of(messages.cbegin(), messages.cend(), [&](const auto &message) {
      return message.text.contains(text);
    });
  };
  QStringList observed;
  for (const auto &message : messages)
    observed.append(QStringLiteral("%1:%2").arg(int(message.type)).arg(message.text));
  qInfo().noquote() << "CE1 handler observation" << QTest::currentDataTag() << "state" << int(observedState)
                    << "retry-active-before-stop" << retryActive << "retry-interval" << retryInterval << "next-delay"
                    << observedNextDelay << observed;
  QVERIFY(seededTimerActive);
  QVERIFY(!core.m_retryTimer.isActive());
  QCOMPARE(observedState, State(expectedState));
  QCOMPARE(observedConnection, synergy::core::ConnectionState::Disconnected);
  QCOMPARE(retryActive, expectedRetry);
  if (expectedRetry)
    QCOMPARE(retryInterval, expectedInterval);
  QCOMPARE(observedNextDelay, expectedNextDelay);
  const auto classification = ExitClassification(expectedClassification);
  QCOMPARE(
      hasMessage(QtWarningMsg, QStringLiteral("The desktop process crashed")),
      classification == ExitClassification::Crash
  );
  QCOMPARE(
      hasMessage(QtDebugMsg, QStringLiteral("desktop process exited normally")),
      classification == ExitClassification::Normal
  );
  QCOMPARE(
      hasMessage(QtWarningMsg, QStringLiteral("The desktop process exited with code: 9")),
      classification == ExitClassification::Failure
  );
  QCOMPARE(
      hasMessage(QtInfoMsg, QStringLiteral("The core desktop process stopped")),
      classification == ExitClassification::IntentionalStop
  );
  QCOMPARE(hasText(QStringLiteral("retrying")), expectedRetry);
  QVERIFY(!hasText(QStringLiteral("Checking the existing core")));
  QVERIFY(core.m_process == nullptr);
  QVERIFY(core.findChildren<QProcess *>().isEmpty());
}

int main(int argc, char **argv)
{
  QTemporaryDir dir;
  if (!dir.isValid())
    return 1;
  const auto root = QFileInfo(dir.path()).canonicalFilePath();
  QString contextError;
  if (!synergy::runtime::configureIsolatedRuntime(
          {root, "tsr3-" + QUuid::createUuid().toString(QUuid::WithoutBraces)}, &contextError
      ))
    return 2;
  qputenv("XDG_CONFIG_HOME", root.toUtf8());
  qputenv("XDG_STATE_HOME", root.toUtf8());
  QApplication app(argc, argv);
  Settings::setSettingsFile(root + "/user/test.conf");
  Settings::setStateFile(root + "/state/app.state");
  Settings::setValue(Settings::Core::ComputerName, "Test-Server");
  R3SerializationTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "R3SerializationTests.moc"
