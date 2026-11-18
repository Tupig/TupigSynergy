/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * 计算机设置对话框界面由代码按设计稿构建，替代旧的 .ui 文件。
 * 业务代码只依赖下列控件指针，不关心构建方式。
 */
#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace Ui {

class ScreenSettingsDialog
{
public:
  void setupUi(QDialog *dialog);
  void retranslateUi(QDialog *dialog);

  QTabWidget *tabWidget = nullptr;
  QVBoxLayout *_2 = nullptr;
  QScrollArea *scrollArea = nullptr;
  QWidget *scrollAreaWidgetContents = nullptr;
  QHBoxLayout *layoutOptions = nullptr;

  QVBoxLayout *layoutLeftSide = nullptr;

  QGroupBox *groupComputerName = nullptr;
  QVBoxLayout *_3 = nullptr;
  QFormLayout *_4 = nullptr;
  QLabel *label_7 = nullptr;
  QLineEdit *lineNameEdit = nullptr;
  QLabel *lblNameError = nullptr;

  QGroupBox *groupModifers = nullptr;
  QFormLayout *formLayout_2 = nullptr;
  QLabel *label_2 = nullptr;
  QComboBox *comboShift = nullptr;
  QLabel *label_3 = nullptr;
  QComboBox *comboCtrl = nullptr;
  QLabel *label_4 = nullptr;
  QComboBox *comboAlt = nullptr;
  QLabel *label_5 = nullptr;
  QComboBox *comboMeta = nullptr;
  QLabel *label_6 = nullptr;
  QComboBox *comboSuper = nullptr;
  QLabel *labelAltGr = nullptr;
  QComboBox *comboAltGr = nullptr;

  QVBoxLayout *layoutRightSide = nullptr;

  QGroupBox *groupDeadCorners = nullptr;
  QVBoxLayout *verticalLayout_2 = nullptr;
  QHBoxLayout *layoutDeadCornersTop = nullptr;
  QCheckBox *chkDeadTopLeft = nullptr;
  QCheckBox *chkDeadTopRight = nullptr;
  QFormLayout *layoutCornerSize = nullptr;
  QLabel *label = nullptr;
  QSpinBox *sbSwitchCornerSize = nullptr;
  QHBoxLayout *layoutDeadCornersBottom = nullptr;
  QCheckBox *chkDeadBottomLeft = nullptr;
  QCheckBox *chkDeadBottomRight = nullptr;

  QGroupBox *groupFixes = nullptr;
  QVBoxLayout *verticalLayout_3 = nullptr;
  QCheckBox *chkFixScrollLock = nullptr;
  QCheckBox *chkFixCapsLock = nullptr;
  QCheckBox *chkFixNumLock = nullptr;
  QCheckBox *chkFixXTest = nullptr;

  QGroupBox *groupAliases = nullptr;
  QVBoxLayout *verticalLayout = nullptr;
  QHBoxLayout *horizontalLayout = nullptr;
  QLineEdit *lineAddAlias = nullptr;
  QPushButton *btnAddAlias = nullptr;
  QLabel *lblAliasError = nullptr;
  QListWidget *listAliases = nullptr;
  QPushButton *btnRemoveAlias = nullptr;

  QDialogButtonBox *buttonBox = nullptr;
};

} // namespace Ui
