/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * 服务端配置对话框界面由代码构建，替代旧的 .ui 文件。
 * 业务代码只依赖下列控件指针，不关心构建方式。
 */
#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>

#include "widgets/NewScreenWidget.h"
#include "widgets/ScreenSetupView.h"
#include "widgets/TrashScreenWidget.h"

namespace Ui {

class ServerConfigDialog
{
public:
  void setupUi(QDialog *dialog);
  void retranslateUi(QDialog *dialog);

  QTabWidget *tabWidget = nullptr;
  QWidget *tabComputers = nullptr;
  QScrollArea *computersScrollArea = nullptr;
  QWidget *computersScrollContents = nullptr;
  QGroupBox *groupComputerLayout = nullptr;
  QLabel *label_2 = nullptr;
  ScreenSetupView *screenSetupView = nullptr;
  TrashScreenWidget *lblRemoveScreen = nullptr;
  NewScreenWidget *lblNewScreen = nullptr;
  QWidget *tabHotkeys = nullptr;
  QScrollArea *hotkeysScrollArea = nullptr;
  QWidget *hotkeysScrollContents = nullptr;
  QGroupBox *groupHotkeys = nullptr;
  QListWidget *listHotkeys = nullptr;
  QPushButton *btnNewHotkey = nullptr;
  QPushButton *btnEditHotkey = nullptr;
  QPushButton *btnRemoveHotkey = nullptr;
  QGroupBox *groupActions = nullptr;
  QListWidget *listActions = nullptr;
  QPushButton *btnNewAction = nullptr;
  QPushButton *btnEditAction = nullptr;
  QPushButton *btnRemoveAction = nullptr;
  QWidget *tabAdvanced = nullptr;
  QScrollArea *sharingScrollArea = nullptr;
  QWidget *sharingScrollContents = nullptr;
  QGroupBox *groupMisc = nullptr;
  QCheckBox *cbRelativeMouseMoves = nullptr;
  QCheckBox *cbDefaultLockToScreenState = nullptr;
  QCheckBox *cbDisableLockToScreen = nullptr;
  QCheckBox *cbWin32KeepForeground = nullptr;
  QCheckBox *cbEnableClipboard = nullptr;
  QLabel *label_7 = nullptr;
  QSpinBox *sbClipboardSizeLimit = nullptr;
  QGroupBox *groupCorners = nullptr;
  QCheckBox *cbCornerTopLeft = nullptr;
  QCheckBox *cbCornerTopRight = nullptr;
  QCheckBox *cbCornerBottomLeft = nullptr;
  QCheckBox *cbCornerBottomRight = nullptr;
  QLabel *label = nullptr;
  QSpinBox *sbSwitchCornerSize = nullptr;
  QGroupBox *groupNetwork = nullptr;
  QCheckBox *cbHeartbeat = nullptr;
  QSpinBox *sbHeartbeat = nullptr;
  QLabel *labelProtocol = nullptr;
  QRadioButton *rbProtocolSynergy = nullptr;
  QRadioButton *rbProtocolBarrier = nullptr;
  QGroupBox *groupSwitch = nullptr;
  QCheckBox *cbSwitchDelay = nullptr;
  QSpinBox *sbSwitchDelay = nullptr;
  QCheckBox *cbSwitchDoubleTap = nullptr;
  QSpinBox *sbSwitchDoubleTap = nullptr;
  QWidget *sharingBottomStretch = nullptr;
  QWidget *tabConfigFile = nullptr;
  QScrollArea *configScrollArea = nullptr;
  QWidget *configScrollContents = nullptr;
  QLabel *label_5 = nullptr;
  QLabel *label_6 = nullptr;
  QGroupBox *groupExternalConfig = nullptr;
  QWidget *widgetExternalConfigControls = nullptr;
  QLabel *labelConfigFile = nullptr;
  QLineEdit *lineConfigFile = nullptr;
  QPushButton *btnBrowseConfigFile = nullptr;
  QWidget *configBottomStretch = nullptr;
  QDialogButtonBox *buttonBox = nullptr;
};

} // namespace Ui
