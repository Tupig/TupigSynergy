/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * 首选项对话框界面由代码构建，替代旧的 .ui 文件。
 * 业务代码只依赖下列控件指针，不关心构建方式。
 */
#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>

namespace Ui {

class SettingsDialog
{
public:
  void setupUi(QDialog *dialog);
  void retranslateUi(QDialog *dialog);

  QTabWidget *tabWidget = nullptr;
  QWidget *tabGeneral = nullptr;
  QScrollArea *generalScroll = nullptr;
  QWidget *generalScrollContents = nullptr;
  QGroupBox *generalCard = nullptr;
  QCheckBox *cbPreventSleep = nullptr;
  QCheckBox *checkForUpdates = nullptr;
  QLabel *lblLanguage = nullptr;
  QComboBox *comboLanguage = nullptr;
  QLabel *lblTrayIconStyle = nullptr;
  QWidget *trayIconOptions = nullptr;
  QRadioButton *rbIconMono = nullptr;
  QRadioButton *rbIconColorful = nullptr;
  QWidget *tabWindow = nullptr;
  QScrollArea *windowScroll = nullptr;
  QWidget *windowScrollContents = nullptr;
  QGroupBox *windowCard = nullptr;
  QLabel *label_3 = nullptr;
  QWidget *widget = nullptr;
  QRadioButton *rbShowOnStart = nullptr;
  QRadioButton *rbAutoHide = nullptr;
  QLabel *label = nullptr;
  QWidget *widget_2 = nullptr;
  QRadioButton *rbExitOnClose = nullptr;
  QRadioButton *rbCloseToTray = nullptr;
  QCheckBox *cbShowVersion = nullptr;
  QWidget *tabLog = nullptr;
  QScrollArea *logScroll = nullptr;
  QWidget *logScrollContents = nullptr;
  QGroupBox *logCard = nullptr;
  QLabel *lblLogLevel = nullptr;
  QComboBox *comboLogLevel = nullptr;
  QLabel *lblDebugWarning = nullptr;
  QGroupBox *groupLogToFile = nullptr;
  QWidget *widgetLogFilename = nullptr;
  QLabel *labelLogPath = nullptr;
  QWidget *logPathControls = nullptr;
  QLineEdit *lineLogFilename = nullptr;
  QPushButton *btnBrowseLog = nullptr;
  QCheckBox *cbGuiDebug = nullptr;
  QWidget *tabNetwork = nullptr;
  QScrollArea *networkScroll = nullptr;
  QWidget *networkScrollContents = nullptr;
  QGroupBox *networkCard = nullptr;
  QLabel *lblNetworkIp = nullptr;
  QComboBox *comboInterface = nullptr;
  QLabel *lblPort = nullptr;
  QSpinBox *sbPort = nullptr;
  QGroupBox *groupSecurity = nullptr;
  QWidget *widgetTlsCert = nullptr;
  QLabel *lblTlsCert = nullptr;
  QWidget *certificateControls = nullptr;
  QLineEdit *lineTlsCertPath = nullptr;
  QPushButton *btnTlsCertPath = nullptr;
  QLabel *lblTlsCertInfo = nullptr;
  QLabel *lblTlsKeyLength = nullptr;
  QWidget *keyLengthControls = nullptr;
  QComboBox *comboTlsKeyLength = nullptr;
  QPushButton *btnTlsRegenCert = nullptr;
  QCheckBox *cbRequireClientCert = nullptr;
  QWidget *tabAdvanced = nullptr;
  QScrollArea *advancedScroll = nullptr;
  QWidget *advancedScrollContents = nullptr;
  QGroupBox *groupService = nullptr;
  QCheckBox *cbElevateDaemon = nullptr;
  QGroupBox *commandsCard = nullptr;
  QCheckBox *cbRunEnterCommand = nullptr;
  QLineEdit *lineCommandEnter = nullptr;
  QCheckBox *cbRunExitCommand = nullptr;
  QLineEdit *lineCommandExit = nullptr;
  QDialogButtonBox *buttonBox = nullptr;
};

} // namespace Ui
