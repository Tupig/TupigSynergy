/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * 关于对话框界面由代码构建，替代旧的 .ui 文件。
 * 业务代码只依赖下列控件指针，不关心构建方式。
 */
#pragma once

#include <QDialog>

#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>

namespace Ui {

class AboutDialog
{
public:
  void setupUi(QDialog *dialog);
  void retranslateUi(QDialog *dialog);

  QScrollArea *scrollArea = nullptr;
  QWidget *scrollAreaWidgetContents = nullptr;
  QGroupBox *frameLogo = nullptr;
  QLabel *lblIcon = nullptr;
  QLabel *lblName = nullptr;
  QLabel *lblDescription = nullptr;
  QLabel *lblCopyright = nullptr;
  QLabel *program_name = nullptr;
  QLabel *openExternalLinks = nullptr;
  QWidget *widget_2 = nullptr;
  QLabel *label = nullptr;
  QLabel *lblVersion = nullptr;
  QPushButton *btnCopyVersion = nullptr;
  QGroupBox *groupBox = nullptr;
  QLabel *lblImportantDevs = nullptr;
  QLabel *linkContributors = nullptr;
  QGroupBox *groupLicenses = nullptr;
  QLabel *linkGpl = nullptr;
  QWidget *widget = nullptr;
  QPushButton *btnOk = nullptr;
};

} // namespace Ui
