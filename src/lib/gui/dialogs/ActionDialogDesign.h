/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * 动作配置对话框界面由代码构建，替代旧的 .ui 文件。
 * 业务代码只依赖下列控件指针，不关心构建方式。
 */
#pragma once

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>

#include "widgets/KeySequenceWidget.h"

namespace Ui {

class ActionDialog
{
public:
  void setupUi(QDialog *dialog);
  void retranslateUi(QDialog *dialog);

  QScrollArea *scrollArea = nullptr;
  QWidget *scrollAreaWidgetContents = nullptr;
  QGroupBox *actionCard = nullptr;
  QLabel *labelTrigger = nullptr;
  QLabel *labelActionType = nullptr;
  QLabel *labelKeys = nullptr;
  QLabel *labelTarget = nullptr;
  QLabel *labelDirection = nullptr;
  QLabel *labelCursor = nullptr;
  QLabel *labelRecipients = nullptr;
  QComboBox *comboRecipients = nullptr;
  QComboBox *comboTriggerOn = nullptr;
  QComboBox *comboActionType = nullptr;
  QComboBox *comboSwitchToScreen = nullptr;
  QComboBox *comboSwitchInDirection = nullptr;
  QComboBox *comboLockCursorToScreen = nullptr;
  KeySequenceWidget *keySequenceWidget = nullptr;
  QWidget *keySequenceField = nullptr;
  QGroupBox *groupScreens = nullptr;
  QListWidget *listScreens = nullptr;
  QDialogButtonBox *buttonBox = nullptr;
};

} // namespace Ui
