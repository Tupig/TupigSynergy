/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * 热键对话框界面由代码构建，替代旧的 .ui 文件。
 * 业务代码只依赖下列控件指针，不关心构建方式。
 */
#pragma once

#include <QDialog>

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>

#include "widgets/KeySequenceWidget.h"

namespace Ui {

class HotkeyDialog
{
public:
  void setupUi(QDialog *dialog);
  void retranslateUi(QDialog *dialog);

  QScrollArea *scrollArea = nullptr;
  QWidget *scrollAreaWidgetContents = nullptr;
  QGroupBox *hotkeyCard = nullptr;
  QLabel *label = nullptr;
  KeySequenceWidget *m_pKeySequenceWidgetHotkey = nullptr;
  QDialogButtonBox *buttonBox = nullptr;
  QGroupBox *actionsCard = nullptr;
  QListWidget *listAssociatedActions = nullptr;
  QPushButton *btnAddAction = nullptr;
  QPushButton *btnConfigureAction = nullptr;
  QPushButton *btnRemoveAction = nullptr;
};

} // namespace Ui
