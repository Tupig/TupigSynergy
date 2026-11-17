/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

class QAction;
class QMenu;
class QMenuBar;

namespace synergy::gui {

enum class MenuPlatform
{
  MacOS,
  WindowsLinux
};

// The caller owns and connects each action. Menus only present these shared actions.
struct AppMenuActions
{
  QAction *open = nullptr;
  QAction *start = nullptr;
  QAction *restart = nullptr;
  QAction *stop = nullptr;
  QAction *quit = nullptr;
  QAction *settings = nullptr;
  QAction *clearSettings = nullptr;
  QAction *updates = nullptr;
  QAction *about = nullptr;
  QAction *log = nullptr;
  QAction *help = nullptr;
};

void populateMainMenus(QMenuBar *bar, const AppMenuActions &actions, MenuPlatform platform);
void populateTrayMenu(QMenu *menu, const AppMenuActions &actions);

} // namespace synergy::gui
