/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/NetworkProtocol.h"
#include "common/Settings.h"
#include "gui/config/Screen.h"
#include "gui/config/ScreenList.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/widgets/LogWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QtTest>

class EditorTransactionTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void clientCancelDoesNotWrite();
  void clientSaveWrites();
  void screenAliasesReplaceRatherThanAppend();
  void logsFilterCountAndBoundHistory();
  void serverCancelRestoresProtocol();
  void protocolValuesSupportStoredAndDefaultTypes();
};

void EditorTransactionTests::clientCancelDoesNotWrite()
{
  Settings::setValue(Settings::Client::InvertXScroll, false);
  ClientConfigDialog editor(nullptr);
  editor.findChild<QCheckBox *>(QStringLiteral("cbXScrollInvert"))->setChecked(true);
  editor.reject();
  QVERIFY(!Settings::value(Settings::Client::InvertXScroll).toBool());
}

void EditorTransactionTests::clientSaveWrites()
{
  Settings::setValue(Settings::Client::InvertXScroll, false);
  ClientConfigDialog editor(nullptr);
  editor.findChild<QCheckBox *>(QStringLiteral("cbXScrollInvert"))->setChecked(true);
  auto *buttons = editor.findChild<QDialogButtonBox *>();
  QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
  buttons->button(QDialogButtonBox::Save)->click();
  QVERIFY(Settings::value(Settings::Client::InvertXScroll).toBool());
  QCOMPARE(editor.result(), static_cast<int>(QDialog::Accepted));
}

void EditorTransactionTests::screenAliasesReplaceRatherThanAppend()
{
  synergy::gui::Screen screen(QStringLiteral("test-screen"));
  ScreenSettingsDialog initial(nullptr, &screen, nullptr);
  initial.findChild<QListWidget *>(QStringLiteral("listAliases"))->addItem(QStringLiteral("old-alias"));
  initial.accept();
  synergy::gui::ScreenList screens;
  screens.append(screen);
  ScreenSettingsDialog editor(nullptr, &screen, &screens);
  auto *aliases = editor.findChild<QListWidget *>(QStringLiteral("listAliases"));
  aliases->clear();
  aliases->addItem(QStringLiteral("new-alias"));
  editor.accept();
  QCOMPARE(editor.result(), static_cast<int>(QDialog::Accepted));
  QCOMPARE(std::as_const(screen).aliases(), QStringList{QStringLiteral("new-alias")});
}

void EditorTransactionTests::protocolValuesSupportStoredAndDefaultTypes()
{
  QCOMPARE(networkProtocolFromValue(QVariant::fromValue(NetworkProtocol::Barrier)), NetworkProtocol::Barrier);
  QCOMPARE(networkProtocolFromValue(QStringLiteral("barrier")), NetworkProtocol::Barrier);
  QCOMPARE(networkProtocolFromValue(QStringLiteral("synergy")), NetworkProtocol::Synergy);
}

void EditorTransactionTests::serverCancelRestoresProtocol()
{
  Settings::setValue(Settings::Server::Protocol, networkProtocolToOption(NetworkProtocol::Synergy));
  synergy::gui::ServerConfig config;
  ServerConfigDialog editor(nullptr, config);
  editor.findChild<QRadioButton *>(QStringLiteral("rbProtocolBarrier"))->setChecked(true);
  QCOMPARE(networkProtocolFromValue(Settings::value(Settings::Server::Protocol)), NetworkProtocol::Barrier);
  editor.reject();
  QCOMPARE(networkProtocolFromValue(Settings::value(Settings::Server::Protocol)), NetworkProtocol::Synergy);
}

void EditorTransactionTests::logsFilterCountAndBoundHistory()
{
  LogWidget log;
  log.appendLine(QStringLiteral("[time] INFO: first"));
  log.appendLine(QStringLiteral("[time] WARNING: second"));
  log.appendLine(QStringLiteral("[time] ERROR: third"));
  const std::array<int, 3> counts{1, 1, 1};
  QCOMPARE(log.levelCounts(), counts);
  log.setLevelFilter(QStringLiteral("warn"));
  QVERIFY(log.allText().contains(QStringLiteral("second")));
  QVERIFY(!log.allText().contains(QStringLiteral("first")));
  log.setLevelFilter(QString());
  log.clear();
  for (int i = 0; i < 10003; ++i)
    log.appendLine(QStringLiteral("[time] INFO: record-%1").arg(i));
  QCOMPARE(log.levelCounts()[0], 10000);
  QVERIFY(!log.allText().contains(QStringLiteral("record-0\n")));
  QVERIFY(log.allText().contains(QStringLiteral("record-10002")));
  log.clear();
  QCOMPARE(log.levelCounts()[0], 0);
  QVERIFY(log.allText().isEmpty());
  log.appendLine(QStringLiteral("[time] ERROR: ") + QString(20000, QLatin1Char('x')));
  QVERIFY(log.maximumHeight() <= 236);
}

int main(int argc, char **argv)
{
  // 在 Settings 首次初始化前隔离设置和状态，不触碰宿主日常配置。
  QTemporaryDir directory;
  if (!directory.isValid())
    return 1;
  qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
  qputenv("XDG_STATE_HOME", directory.path().toUtf8());
  QApplication app(argc, argv);
  Settings::setSettingsFile(directory.filePath(QStringLiteral("test.conf")));
  Settings::setStateFile(directory.filePath(QStringLiteral("test.state")));
  EditorTransactionTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "EditorTransactionTests.moc"
