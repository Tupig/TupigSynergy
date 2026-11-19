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
#include <QSet>
#include <QSignalSpy>
#include <QtTest>

#include <array>
#include <memory>

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

  void mainAndTrayMenusDispatchSharedActions_data()
  {
    QTest::addColumn<bool>("mac");
    QTest::newRow("macOS") << true;
    QTest::newRow("Windows-Linux") << false;
  }

  void mainAndTrayMenusDispatchSharedActions()
  {
    using namespace synergy::gui;
    QFETCH(bool, mac);
    QObject owner;
    const auto action = [&owner](const char *name) {
      auto *result = new QAction(QString::fromLatin1(name), &owner);
      result->setObjectName(QString::fromLatin1(name));
      return result;
    };
    const AppMenuActions actions{action("Open"),  action("Start"),       action("Restart"), action("Stop"),
                                 action("Quit"),  action("Preferences"), action("Reset"),   action("Updates"),
                                 action("About"), action("Log"),         action("Help")};
    const std::array<QAction *, 11> shared{actions.open,  actions.start,    actions.restart,       actions.stop,
                                           actions.quit,  actions.settings, actions.clearSettings, actions.updates,
                                           actions.about, actions.log,      actions.help};
    std::array<std::unique_ptr<QSignalSpy>, 11> routed;
    for (std::size_t index = 0; index < shared.size(); ++index) {
      routed.at(index) = std::make_unique<QSignalSpy>(shared.at(index), &QAction::triggered);
      QVERIFY(routed.at(index)->isValid());
    }
    actions.log->setCheckable(true);
    QMenuBar menuBar;
    QMenu tray;
    const auto visibleActions = [](QMenu *menu) {
      QList<QAction *> result;
      for (auto *item : menu->actions())
        if (!item->isSeparator())
          result.append(item);
      return result;
    };
    for (int generation = 0; generation < 3; ++generation) {
      populateMainMenus(&menuBar, actions, mac ? MenuPlatform::MacOS : MenuPlatform::WindowsLinux);
      populateTrayMenu(&tray, actions);
      auto *file = menuBar.findChild<QMenu *>("menuFile");
      auto *view = menuBar.findChild<QMenu *>("menuView");
      auto *help = menuBar.findChild<QMenu *>("menuHelp");
      QVERIFY(file);
      QVERIFY(view);
      QVERIFY(help);
      QCOMPARE(menuBar.actions().size(), 4);
      auto expectedFile = QList<QAction *>{actions.start, actions.restart, actions.stop};
      if (!mac)
        expectedFile.append(actions.quit);
      QCOMPARE(visibleActions(file), expectedFile);
      QCOMPARE(visibleActions(view), QList<QAction *>{actions.log});
      if (mac) {
        auto *application = menuBar.findChild<QMenu *>("menuApplication");
        QVERIFY(application);
        const QList<QAction *> expectedApplication{
            actions.about, actions.updates, actions.settings, actions.clearSettings, actions.quit
        };
        QCOMPARE(visibleActions(application), expectedApplication);
        QCOMPARE(visibleActions(help), QList<QAction *>{actions.help});
        QVERIFY(!menuBar.findChild<QMenu *>("menuEdit"));
      } else {
        auto *edit = menuBar.findChild<QMenu *>("menuEdit");
        QVERIFY(edit);
        QCOMPARE(visibleActions(edit), QList<QAction *>{actions.settings});
        const QList<QAction *> expectedHelp{actions.updates, actions.help, actions.clearSettings, actions.about};
        QCOMPARE(visibleActions(help), expectedHelp);
        QVERIFY(!menuBar.findChild<QMenu *>("menuApplication"));
      }

      QSet<QString> shortcuts;
      for (auto *item : shared) {
        if (item->shortcut().isEmpty())
          continue;
        const auto key = item->shortcut().toString(QKeySequence::PortableText);
        QVERIFY2(!shortcuts.contains(key), qPrintable(key));
        shortcuts.insert(key);
      }
      // Trigger the action obtained from each actual menu, rather than the
      // input bundle, and require exactly one route with no sibling dispatch.
      for (auto *top : menuBar.actions()) {
        auto *menu = top->menu();
        QVERIFY(menu);
        for (auto *item : visibleActions(menu)) {
          std::array<int, 11> before;
          for (std::size_t index = 0; index < shared.size(); ++index)
            before.at(index) = routed.at(index)->count();
          const bool checked = item->isChecked();
          item->trigger();
          for (std::size_t index = 0; index < shared.size(); ++index)
            QCOMPARE(routed.at(index)->count(), before.at(index) + (shared.at(index) == item ? 1 : 0));
          if (item == actions.log)
            QCOMPARE(item->isChecked(), !checked);
        }
      }

      const auto trayActions = visibleActions(&tray);
      QCOMPARE(trayActions.size(), 5);
      QCOMPARE(trayActions.at(0), actions.open);
      QCOMPARE(trayActions.at(1), actions.start);
      QCOMPARE(trayActions.at(2), actions.restart);
      QCOMPARE(trayActions.at(3), actions.stop);
      QCOMPARE(trayActions.at(4)->objectName(), QStringLiteral("trayQuitAction"));
      for (auto *item : trayActions) {
        auto *source = item == trayActions.last() ? actions.quit : item;
        std::array<int, 11> before;
        for (std::size_t index = 0; index < shared.size(); ++index)
          before.at(index) = routed.at(index)->count();
        item->trigger();
        for (std::size_t index = 0; index < shared.size(); ++index)
          QCOMPARE(routed.at(index)->count(), before.at(index) + (shared.at(index) == source ? 1 : 0));
      }

      // Main and tray contain the same lifecycle actions, so live enabled
      // state must be shared; the menu must not dispatch a disabled command.
      for (auto *item : {actions.start, actions.restart, actions.stop}) {
        QVERIFY(file->actions().contains(item));
        QVERIFY(tray.actions().contains(item));
        QSignalSpy triggered(item, &QAction::triggered);
        item->setEnabled(false);
        QVERIFY(!file->actions().at(file->actions().indexOf(item))->isEnabled());
        QVERIFY(!tray.actions().at(tray.actions().indexOf(item))->isEnabled());
        item->trigger();
        QCOMPARE(triggered.count(), 0);
        item->setEnabled(true);
        item->trigger();
        QCOMPARE(triggered.count(), 1);
      }
    }
  }
};
QTEST_MAIN(R3TrayQuitTests)
#include "R3TrayQuitTests.moc"
