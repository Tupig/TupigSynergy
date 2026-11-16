/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * 客户端配置对话框界面由代码按设计稿构建，替代旧的 .ui 文件。
 * 业务代码只依赖下列控件指针，不关心构建方式。
 */
#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace Ui {

class ClientConfigDialog
{
public:
  void setupUi(QDialog *dialog);
  void retranslateUi(QDialog *dialog);

  QVBoxLayout *verticalLayout = nullptr;
  QScrollArea *scrollArea = nullptr;
  QWidget *scrollAreaWidgetContents = nullptr;
  QVBoxLayout *pageLayout = nullptr;

  QGroupBox *connectionBehaviorGroup = nullptr;
  QVBoxLayout *connectionBehaviorLayout = nullptr;
  QCheckBox *cbDynamicConnectTime = nullptr;
  QCheckBox *cbLanguageSync = nullptr;

  QGroupBox *groupBox = nullptr;
  QFormLayout *gridLayout = nullptr;

  QLabel *lblHScroll = nullptr;
  QWidget *scrollHorizontalField = nullptr;
  QHBoxLayout *horizontalLayout_3 = nullptr;
  QCheckBox *cbXScrollInvert = nullptr;
  QLabel *lblXScrollSpeed = nullptr;
  QDoubleSpinBox *sbXScrollScale = nullptr;

  QLabel *lblVScroll = nullptr;
  QWidget *scrollVerticalField = nullptr;
  QHBoxLayout *horizontalLayout_2 = nullptr;
  QCheckBox *cbYScrollInvert = nullptr;
  QLabel *lblScrollSpeed = nullptr;
  QDoubleSpinBox *sbYScrollScale = nullptr;

  QDialogButtonBox *buttonBox = nullptr;
};

} // namespace Ui
