/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * 主窗口界面由代码按设计稿构建，替代旧的 .ui 文件。
 * 业务代码只依赖下列控件指针，不关心构建方式。
 */
#pragma once

#include <QMainWindow>

#include "widgets/LogPanel.h"
#include "widgets/ModeCard.h"
#include "widgets/PageHost.h"
#include "widgets/StatusBar.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>

namespace Ui {

class MainWindow
{
public:
  // 设计稿尺寸（CSS px）
  static constexpr int kWindowWidth = 500;
  static constexpr int kWindowHeight = 390;
  static constexpr int kPageBarHeight = 36;
  static constexpr int kStatusBarHeight = 32;
  static constexpr int kWorkspacePadding = 10;
  static constexpr int kGap = 8;

  void setupUi(QMainWindow *window);
  void retranslateUi(QMainWindow *window);
  void updateShortcutPresentation(int width);
  void setAvailableUpdateVersion(const QString &version);

  QWidget *topLevelWidget = nullptr;
  QWidget *pageBar = nullptr;
  QLabel *pageTitle = nullptr;
  QPushButton *btnBack = nullptr;
  QPushButton *btnSettingsShortcut = nullptr;
  QPushButton *btnUpdatesShortcut = nullptr;
  QPushButton *btnAboutShortcut = nullptr;
  PageHost *pageHost = nullptr;
  QStackedWidget *pageStack = nullptr;
  QScrollArea *workspace = nullptr;

  QLabel *lblComputerName = nullptr;
  QLineEdit *lineEditName = nullptr;
  QPushButton *btnEditName = nullptr;
  QPushButton *btnSaveName = nullptr;
  QPushButton *btnCancelName = nullptr;
  QLabel *lblIpAddresses = nullptr;

  QWidget *groupBox = nullptr;
  ModeCard *rbModeServer = nullptr;
  ModeCard *rbModeClient = nullptr;
  QWidget *clientOptions = nullptr;
  QLabel *m_pLabelServerName = nullptr;
  QLineEdit *lineHostname = nullptr;
  QSpinBox *spinPort = nullptr;
  QLabel *lblNoMode = nullptr;
  QWidget *serverOptions = nullptr;
  QPushButton *btnConfigureServer = nullptr;
  QPushButton *btnSaveServerConfig = nullptr;
  QPushButton *btnConfigureClient = nullptr;
  QPushButton *btnToggleCore = nullptr;
  QPushButton *btnRestartCore = nullptr;

  LogPanel *logPanel = nullptr;
  StatusBar *statusBar = nullptr;

private:
  QWidget *m_updateBadge = nullptr;
  QString m_availableUpdateVersion;
};

} // namespace Ui
