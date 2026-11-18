/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/AppMenus.h"
#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSignalSpy>
#include <QtTest>

class R3TrayQuitTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void visibleQuitCannotBeMergedOutAndRoutesOnce()
  {
    using namespace synergy::gui;
    QObject owner;
    AppMenuActions actions;
    const auto action = [&owner](const char *name) {
      auto *result = new QAction(QString::fromLatin1(name), &owner);
      result->setObjectName(QString::fromLatin1(name));
      return result;
    };
    actions.open = action("Open");
    actions.start = action("Start");
    actions.restart = action("Restart");
    actions.stop = action("Stop");
    actions.quit = action("Quit TuPig Synergy");
    actions.settings = action("Settings");
    actions.clearSettings = action("Clear settings");
    actions.updates = action("Updates");
    actions.about = action("About");
    actions.log = action("Log");
    actions.help = action("Help");
    QMenuBar menuBar;
    QMenu tray;
    populateMainMenus(&menuBar, actions, MenuPlatform::MacOS);
    QSignalSpy quitRouted(actions.quit, &QAction::triggered);
    for (int generation = 0; generation < 3; ++generation) {
      populateTrayMenu(&tray, actions);
      QList<QAction *> quitCandidates;
      for (auto *item : tray.actions())
        if (!item->isSeparator() && item->text() == actions.quit->text())
          quitCandidates << item;
      QCOMPARE(quitCandidates.size(), 1);
      auto *trayQuit = quitCandidates.first();
      QVERIFY(trayQuit->isVisible());
      QVERIFY(trayQuit->isEnabled());
      QCOMPARE(trayQuit->objectName(), QString("trayQuitAction"));
      QCOMPARE(trayQuit->menuRole(), QAction::NoRole);
      QVERIFY(trayQuit->shortcut().isEmpty());
      QCOMPARE(actions.quit->menuRole(), QAction::QuitRole);
      QCOMPARE(actions.quit->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Q));
      const int before = quitRouted.count();
      trayQuit->trigger();
      QCOMPARE(quitRouted.count(), before + 1);
      actions.quit->setEnabled(false);
      QVERIFY(!trayQuit->isEnabled());
      actions.quit->setEnabled(true);
      QVERIFY(trayQuit->isEnabled());
      const auto translatedLabel = QStringLiteral("Quit translated generation %1").arg(generation);
      actions.quit->setText(translatedLabel);
      QCOMPARE(trayQuit->text(), translatedLabel);
      actions.quit->setVisible(false);
      QVERIFY(!trayQuit->isVisible());
      actions.quit->setVisible(true);
      QVERIFY(trayQuit->isVisible());
    }
    // Keep the application menu's real QuitRole action despite the NoRole tray proxy.
    auto *application = menuBar.findChild<QMenu *>("menuApplication");
    QVERIFY(application);
    QVERIFY(application->actions().contains(actions.quit));
    QCOMPARE(quitRouted.count(), 3);
  }
};
QTEST_MAIN(R3TrayQuitTests)
#include "R3TrayQuitTests.moc"
