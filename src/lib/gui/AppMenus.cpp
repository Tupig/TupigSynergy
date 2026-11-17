/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "AppMenus.h"

#include "common/Constants.h"
#include <QAction>
#include <QCoreApplication>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>

namespace synergy::gui {
namespace {
QString text(const char *source)
{
  return QCoreApplication::translate("MainWindow", source);
}

QMenu *addMenu(QMenuBar *bar, const QString &title, const char *name)
{
  auto *menu = new QMenu(title, bar);
  menu->setObjectName(QString::fromLatin1(name));
  bar->addMenu(menu);
  return menu;
}

void shortcut(QAction *action, Qt::Key key)
{
  // Qt's logical Ctrl modifier maps to Command on macOS.
  action->setShortcut(QKeySequence(Qt::CTRL | key));
}
} // namespace

void populateMainMenus(QMenuBar *bar, const AppMenuActions &a, MenuPlatform platform)
{
  const auto previous = bar->actions();
  bar->clear();
  for (auto *action : previous) {
    if (auto *menu = action->menu(); menu && menu->parent() == bar)
      delete menu;
  }

  const bool mac = platform == MenuPlatform::MacOS;
  a.about->setMenuRole(QAction::AboutRole);
  a.updates->setMenuRole(mac ? QAction::ApplicationSpecificRole : QAction::NoRole);
  a.settings->setMenuRole(QAction::PreferencesRole);
  a.clearSettings->setMenuRole(mac ? QAction::ApplicationSpecificRole : QAction::NoRole);
  a.quit->setMenuRole(QAction::QuitRole);
  for (auto *action : {a.open, a.start, a.restart, a.stop, a.log, a.help})
    action->setMenuRole(QAction::NoRole);

  shortcut(a.start, Qt::Key_S);
  shortcut(a.restart, Qt::Key_R);
  shortcut(a.stop, Qt::Key_T);
  shortcut(a.updates, Qt::Key_U);
  shortcut(a.settings, Qt::Key_Comma);
  shortcut(a.log, Qt::Key_J);
  shortcut(a.quit, Qt::Key_Q);

  if (mac) {
    // Cocoa merges these role-bearing actions into the native application menu.
    auto *application = addMenu(bar, QString::fromUtf8(kAppName), "menuApplication");
    application->addAction(a.about);
    application->addAction(a.updates);
    application->addSeparator();
    application->addAction(a.settings);
    application->addAction(a.clearSettings);
    application->addSeparator();
    application->addAction(a.quit);
  }

  auto *file = addMenu(bar, text("&File"), "menuFile");
  file->addActions({a.start, a.restart, a.stop});
  if (!mac) {
    file->addSeparator();
    file->addAction(a.quit);
    auto *edit = addMenu(bar, text("&Edit"), "menuEdit");
    edit->addAction(a.settings);
  }
  auto *view = addMenu(bar, text("&View"), "menuView");
  view->addAction(a.log);
  auto *help = addMenu(bar, text(mac ? "&Support" : "&Help"), "menuHelp");
  if (mac) {
    help->addAction(a.help);
  } else {
    help->addAction(a.updates);
    help->addAction(a.help);
    help->addSeparator();
    help->addAction(a.clearSettings);
    help->addSeparator();
    help->addAction(a.about);
  }
}

void populateTrayMenu(QMenu *menu, const AppMenuActions &a)
{
  menu->clear();
  menu->addAction(a.open);
  menu->addSeparator();
  menu->addActions({a.start, a.restart, a.stop});
  menu->addSeparator();
  // A QuitRole item is merged into Cocoa's application menu and omitted from
  // its original menu. Give the tray its own presentation without a shortcut;
  // the canonical application action remains the sole owner of quit behavior.
  auto *quit = new QAction(menu);
  quit->setObjectName(QStringLiteral("trayQuitAction"));
  quit->setMenuRole(QAction::NoRole);
  const auto synchronize = [quit, source = a.quit] {
    quit->setText(source->text());
    quit->setIcon(source->icon());
    quit->setEnabled(source->isEnabled());
    quit->setVisible(source->isVisible());
    quit->setStatusTip(source->statusTip());
    quit->setToolTip(source->toolTip());
  };
  synchronize();
  QObject::connect(a.quit, &QAction::changed, quit, synchronize);
  QObject::connect(a.quit, &QObject::destroyed, quit, &QObject::deleteLater);
  QObject::connect(quit, &QAction::triggered, a.quit, &QAction::trigger);
  menu->addAction(quit);
}
} // namespace synergy::gui
