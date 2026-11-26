/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsDialogDesign.h"
#include "CompactLayout.h"
#include "gui/StyleUtils.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QListView>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

QLabel *createLabel(QWidget *parent, const QString &name, int minimumWidth = 0)
{
  auto *result = new QLabel(parent);
  result->setObjectName(name);
  if (minimumWidth > 0)
    result->setFixedWidth(minimumWidth);
  result->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  result->setWordWrap(true);
  result->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  return result;
}

QCheckBox *createCheckBox(QWidget *parent, const QString &name)
{
  auto *result = new QCheckBox(parent);
  result->setObjectName(name);
  result->setAttribute(Qt::WA_LayoutUsesWidgetRect);
  result->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  return result;
}

QRadioButton *createRadioButton(QWidget *parent, const QString &name)
{
  auto *result = new QRadioButton(parent);
  result->setObjectName(name);
  return result;
}

QComboBox *createComboBox(QWidget *parent, const QString &name, int itemCount = 0)
{
  auto *result = new QComboBox(parent);
  result->setView(new QListView(result));
  result->setObjectName(name);
  result->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  result->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  result->setFixedWidth(180);
  for (int i = 0; i < itemCount; ++i)
    result->addItem(QString());
  return result;
}

QSpinBox *createSpinBox(QWidget *parent, const QString &name)
{
  auto *result = new QSpinBox(parent);
  result->setObjectName(name);
  result->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  result->setFixedWidth(180);
  return result;
}

QLineEdit *createLineEdit(QWidget *parent, const QString &name)
{
  auto *result = new QLineEdit(parent);
  result->setObjectName(name);
  result->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  return result;
}

QPushButton *createBrowseButton(QWidget *parent, const QString &name)
{
  auto *button = new QPushButton(parent);
  button->setObjectName(name);
  button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  button->setCursor(Qt::PointingHandCursor);
  button->setIcon(synergy::gui::appIcon(QStringLiteral("document-open")));
  button->setFlat(true);
  button->setProperty("iconButton32", true);
  return button;
}

QGroupBox *createCard(QWidget *parent, const QString &name)
{
  auto *result = new QGroupBox(parent);
  result->setObjectName(name);
  result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  return result;
}

QGridLayout *createForm(const QString &name)
{
  auto *result = new QGridLayout;
  result->setObjectName(name);
  result->setContentsMargins(0, 0, 0, 0);
  result->setHorizontalSpacing(10);
  result->setVerticalSpacing(8);
  result->setColumnMinimumWidth(0, 120);
  result->setColumnMinimumWidth(1, 180);
  result->setColumnStretch(0, 0);
  result->setColumnStretch(1, 1);
  return result;
}

} // namespace

void Ui::SettingsDialog::setupUi(QDialog *dialog)
{
  dialog->setObjectName(QStringLiteral("SettingsDialog"));
  dialog->resize(560, 420);
  dialog->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  dialog->setModal(true);

  auto *verticalLayout_3 = new QVBoxLayout(dialog);
  verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
  verticalLayout_3->setSpacing(8);
  verticalLayout_3->setContentsMargins(12, 12, 12, 12);

  tabWidget = new QTabWidget(dialog);
  tabWidget->setObjectName(QStringLiteral("tabWidget"));

  // General
  tabGeneral = new QWidget;
  tabGeneral->setObjectName(QStringLiteral("tabGeneral"));
  auto *tabGeneralLayout = new QVBoxLayout(tabGeneral);
  tabGeneralLayout->setObjectName(QStringLiteral("tabGeneralLayout"));
  tabGeneralLayout->setSpacing(8);
  tabGeneralLayout->setContentsMargins(0, 0, 0, 0);

  generalScroll = new QScrollArea(tabGeneral);
  generalScroll->setObjectName(QStringLiteral("generalScroll"));
  generalScroll->setWidgetResizable(true);
  generalScroll->setFrameShape(QFrame::NoFrame);
  generalScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  generalScrollContents = new QWidget;
  generalScrollContents->setObjectName(QStringLiteral("generalScrollContents"));
  auto *generalScrollLayout = new QVBoxLayout(generalScrollContents);
  generalScrollLayout->setObjectName(QStringLiteral("generalScrollLayout"));
  generalScrollLayout->setSpacing(8);
  generalScrollLayout->setContentsMargins(0, 0, 0, 0);

  generalCard = createCard(generalScrollContents, QStringLiteral("generalCard"));
  generalCard->setTitle(QString());
  auto *generalCardLayout = new QVBoxLayout(generalCard);
  generalCardLayout->setObjectName(QStringLiteral("generalCardLayout"));
  generalCardLayout->setSpacing(8);
  generalCardLayout->setContentsMargins(12, 10, 12, 12);

  cbPreventSleep = createCheckBox(generalCard, QStringLiteral("cbPreventSleep"));
  generalCardLayout->addWidget(cbPreventSleep);
  checkForUpdates = createCheckBox(generalCard, QStringLiteral("checkForUpdates"));
  generalCardLayout->addWidget(checkForUpdates);

  auto *generalForm = createForm(QStringLiteral("generalForm"));
  lblLanguage = createLabel(generalCard, QStringLiteral("lblLanguage"), 120);
  generalForm->addWidget(lblLanguage, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  comboLanguage = createComboBox(generalCard, QStringLiteral("comboLanguage"));
  generalForm->addWidget(comboLanguage, 0, 1);

  lblTrayIconStyle = createLabel(generalCard, QStringLiteral("lblTrayIconStyle"), 120);
  generalForm->addWidget(lblTrayIconStyle, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
  trayIconOptions = new QWidget(generalCard);
  trayIconOptions->setObjectName(QStringLiteral("trayIconOptions"));
  auto *trayIconOptionsLayout = new QHBoxLayout(trayIconOptions);
  trayIconOptionsLayout->setObjectName(QStringLiteral("trayIconOptionsLayout"));
  trayIconOptionsLayout->setSpacing(8);
  trayIconOptionsLayout->setContentsMargins(0, 0, 0, 0);
  rbIconMono = createRadioButton(trayIconOptions, QStringLiteral("rbIconMono"));
  trayIconOptionsLayout->addWidget(rbIconMono);
  rbIconColorful = createRadioButton(trayIconOptions, QStringLiteral("rbIconColorful"));
  trayIconOptionsLayout->addWidget(rbIconColorful);
  // 保留旧单选按钮作为设置绑定，不参与可见布局；呈现使用设计稿下拉框。
  trayIconOptions->hide();
  comboTrayIcon = createComboBox(generalCard, QStringLiteral("comboTrayIcon"), 2);
  comboTrayIcon->setItemData(0, false);
  comboTrayIcon->setItemData(1, true);
  generalForm->addWidget(comboTrayIcon, 1, 1);
  generalCardLayout->addLayout(generalForm);

  generalScrollLayout->addWidget(generalCard, 0, Qt::AlignTop);
  generalScroll->setWidget(generalScrollContents);
  tabGeneralLayout->addWidget(generalScroll);
  tabWidget->addTab(tabGeneral, QString());

  // Window
  tabWindow = new QWidget;
  tabWindow->setObjectName(QStringLiteral("tabWindow"));
  auto *tabWindowLayout = new QVBoxLayout(tabWindow);
  tabWindowLayout->setObjectName(QStringLiteral("tabWindowLayout"));
  tabWindowLayout->setSpacing(8);
  tabWindowLayout->setContentsMargins(0, 0, 0, 0);

  windowScroll = new QScrollArea(tabWindow);
  windowScroll->setObjectName(QStringLiteral("windowScroll"));
  windowScroll->setWidgetResizable(true);
  windowScroll->setFrameShape(QFrame::NoFrame);
  windowScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  windowScrollContents = new QWidget;
  windowScrollContents->setObjectName(QStringLiteral("windowScrollContents"));
  auto *windowScrollLayout = new QVBoxLayout(windowScrollContents);
  windowScrollLayout->setObjectName(QStringLiteral("windowScrollLayout"));
  windowScrollLayout->setSpacing(8);
  windowScrollLayout->setContentsMargins(0, 0, 0, 0);

  windowCard = createCard(windowScrollContents, QStringLiteral("windowCard"));
  windowCard->setTitle(QString());
  auto *windowCardLayout = new QVBoxLayout(windowCard);
  windowCardLayout->setObjectName(QStringLiteral("windowCardLayout"));
  windowCardLayout->setSpacing(8);
  windowCardLayout->setContentsMargins(12, 10, 12, 12);

  auto *windowForm = createForm(QStringLiteral("windowForm"));
  label_3 = createLabel(windowCard, QStringLiteral("label_3"), 120);
  windowForm->addWidget(label_3, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  widget = new QWidget(windowCard);
  widget->setObjectName(QStringLiteral("widget"));
  auto *verticalLayout_2 = new QVBoxLayout(widget);
  verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
  verticalLayout_2->setSpacing(6);
  verticalLayout_2->setContentsMargins(0, 0, 0, 0);
  rbShowOnStart = createRadioButton(widget, QStringLiteral("rbShowOnStart"));
  verticalLayout_2->addWidget(rbShowOnStart);
  rbAutoHide = createRadioButton(widget, QStringLiteral("rbAutoHide"));
  verticalLayout_2->addWidget(rbAutoHide);
  widget->hide();
  comboStartBehavior = createComboBox(windowCard, QStringLiteral("comboStartBehavior"), 2);
  comboStartBehavior->setItemData(0, false);
  comboStartBehavior->setItemData(1, true);
  windowForm->addWidget(comboStartBehavior, 0, 1);

  label = createLabel(windowCard, QStringLiteral("label"), 120);
  windowForm->addWidget(label, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
  widget_2 = new QWidget(windowCard);
  widget_2->setObjectName(QStringLiteral("widget_2"));
  auto *verticalLayout_13 = new QVBoxLayout(widget_2);
  verticalLayout_13->setObjectName(QStringLiteral("verticalLayout_13"));
  verticalLayout_13->setSpacing(6);
  verticalLayout_13->setContentsMargins(0, 0, 0, 0);
  rbExitOnClose = createRadioButton(widget_2, QStringLiteral("rbExitOnClose"));
  verticalLayout_13->addWidget(rbExitOnClose);
  rbCloseToTray = createRadioButton(widget_2, QStringLiteral("rbCloseToTray"));
  verticalLayout_13->addWidget(rbCloseToTray);
  widget_2->hide();
  comboCloseBehavior = createComboBox(windowCard, QStringLiteral("comboCloseBehavior"), 2);
  comboCloseBehavior->setItemData(0, true);
  comboCloseBehavior->setItemData(1, false);
  windowForm->addWidget(comboCloseBehavior, 1, 1);
  windowCardLayout->addLayout(windowForm);

  cbShowVersion = createCheckBox(windowCard, QStringLiteral("cbShowVersion"));
  windowCardLayout->addWidget(cbShowVersion);
  windowScrollLayout->addWidget(windowCard, 0, Qt::AlignTop);
  windowScroll->setWidget(windowScrollContents);
  tabWindowLayout->addWidget(windowScroll);
  tabWidget->addTab(tabWindow, QString());

  // Logs
  tabLog = new QWidget;
  tabLog->setObjectName(QStringLiteral("tabLog"));
  auto *tabLogLayout = new QVBoxLayout(tabLog);
  tabLogLayout->setObjectName(QStringLiteral("tabLogLayout"));
  tabLogLayout->setSpacing(8);
  tabLogLayout->setContentsMargins(0, 0, 0, 0);

  logScroll = new QScrollArea(tabLog);
  logScroll->setObjectName(QStringLiteral("logScroll"));
  logScroll->setWidgetResizable(true);
  logScroll->setFrameShape(QFrame::NoFrame);
  logScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  logScrollContents = new QWidget;
  logScrollContents->setObjectName(QStringLiteral("logScrollContents"));
  auto *logScrollLayout = new QVBoxLayout(logScrollContents);
  logScrollLayout->setObjectName(QStringLiteral("logScrollLayout"));
  logScrollLayout->setSpacing(8);
  logScrollLayout->setContentsMargins(0, 0, 0, 0);

  logCard = createCard(logScrollContents, QStringLiteral("logCard"));
  logCard->setTitle(QString());
  auto *logCardLayout = new QVBoxLayout(logCard);
  logCardLayout->setObjectName(QStringLiteral("logCardLayout"));
  logCardLayout->setSpacing(8);
  logCardLayout->setContentsMargins(12, 10, 12, 12);

  auto *logLevelForm = createForm(QStringLiteral("logLevelForm"));
  lblLogLevel = createLabel(logCard, QStringLiteral("lblLogLevel"), 120);
  logLevelForm->addWidget(lblLogLevel, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  comboLogLevel = createComboBox(logCard, QStringLiteral("comboLogLevel"));
  logLevelForm->addWidget(comboLogLevel, 0, 1);
  logCardLayout->addLayout(logLevelForm);

  lblDebugWarning = new QLabel(logCard);
  lblDebugWarning->setObjectName(QStringLiteral("lblDebugWarning"));
  lblDebugWarning->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  lblDebugWarning->setWordWrap(true);
  lblDebugWarning->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  groupLogToFile = createCard(logScrollContents, QStringLiteral("groupLogToFile"));
  groupLogToFile->setTitle(QString());
  groupLogToFile->setCheckable(true);
  groupLogToFile->setChecked(false);
  groupLogToFile->hide();
  auto *logFileLayout = new QVBoxLayout(groupLogToFile);
  logFileLayout->setObjectName(QStringLiteral("logFileLayout"));
  logFileLayout->setSpacing(8);
  logFileLayout->setContentsMargins(12, 26, 12, 12);

  widgetLogFilename = new QWidget(logCard);
  widgetLogFilename->setObjectName(QStringLiteral("widgetLogFilename"));
  widgetLogFilename->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *logPathForm = createForm(QStringLiteral("logPathForm"));
  labelLogPath = createLabel(widgetLogFilename, QStringLiteral("labelLogPath"), 120);
  logPathForm->addWidget(labelLogPath, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  logPathControls = new QWidget(widgetLogFilename);
  logPathControls->setObjectName(QStringLiteral("logPathControls"));
  logPathControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *logPathControlsLayout = new QHBoxLayout(logPathControls);
  logPathControlsLayout->setObjectName(QStringLiteral("logPathControlsLayout"));
  logPathControlsLayout->setSpacing(8);
  logPathControlsLayout->setContentsMargins(0, 0, 0, 0);
  lineLogFilename = createLineEdit(logPathControls, QStringLiteral("lineLogFilename"));
  lineLogFilename->setMinimumWidth(180);
  logPathControlsLayout->addWidget(lineLogFilename);
  btnBrowseLog = createBrowseButton(logPathControls, QStringLiteral("btnBrowseLog"));
  logPathControlsLayout->addWidget(btnBrowseLog);
  logPathForm->addWidget(logPathControls, 0, 1);
  widgetLogFilename->setLayout(logPathForm);
  logCardLayout->addWidget(widgetLogFilename);
  cbLogToFile = createCheckBox(logCard, QStringLiteral("cbLogToFile"));
  logCardLayout->addWidget(cbLogToFile);

  cbGuiDebug = createCheckBox(logCard, QStringLiteral("cbGuiDebug"));
  logCardLayout->addWidget(cbGuiDebug);
  logCardLayout->addWidget(lblDebugWarning);
  logScrollLayout->addWidget(logCard);
  logScrollLayout->setAlignment(Qt::AlignTop);
  logScroll->setWidget(logScrollContents);
  tabLogLayout->addWidget(logScroll);
  tabWidget->addTab(tabLog, QString());

  // Network
  tabNetwork = new QWidget;
  tabNetwork->setObjectName(QStringLiteral("tabNetwork"));
  auto *tabNetworkLayout = new QVBoxLayout(tabNetwork);
  tabNetworkLayout->setObjectName(QStringLiteral("tabNetworkLayout"));
  tabNetworkLayout->setSpacing(8);
  tabNetworkLayout->setContentsMargins(0, 0, 0, 0);

  networkScroll = new QScrollArea(tabNetwork);
  networkScroll->setObjectName(QStringLiteral("networkScroll"));
  networkScroll->setWidgetResizable(true);
  networkScroll->setFrameShape(QFrame::NoFrame);
  networkScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  networkScrollContents = new QWidget;
  networkScrollContents->setObjectName(QStringLiteral("networkScrollContents"));
  auto *networkScrollLayout = new QVBoxLayout(networkScrollContents);
  networkScrollLayout->setObjectName(QStringLiteral("networkScrollLayout"));
  networkScrollLayout->setSpacing(8);
  networkScrollLayout->setContentsMargins(0, 0, 0, 0);

  networkCard = createCard(networkScrollContents, QStringLiteral("networkCard"));
  networkCard->setTitle(QString());
  auto *networkCardLayout = new QVBoxLayout(networkCard);
  networkCardLayout->setObjectName(QStringLiteral("networkCardLayout"));
  networkCardLayout->setSpacing(8);
  networkCardLayout->setContentsMargins(12, 10, 12, 12);

  auto *networkForm = createForm(QStringLiteral("networkForm"));
  lblNetworkIp = createLabel(networkCard, QStringLiteral("lblNetworkIp"), 120);
  networkForm->addWidget(lblNetworkIp, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  comboInterface = createComboBox(networkCard, QStringLiteral("comboInterface"), 1);
  networkForm->addWidget(comboInterface, 0, 1);

  lblPort = createLabel(networkCard, QStringLiteral("lblPort"), 120);
  networkForm->addWidget(lblPort, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
  sbPort = createSpinBox(networkCard, QStringLiteral("sbPort"));
  sbPort->setMaximum(65535);
  sbPort->setValue(24800);
  networkForm->addWidget(sbPort, 1, 1);
  networkCardLayout->addLayout(networkForm);

  groupSecurity = createCard(networkScrollContents, QStringLiteral("groupSecurity"));
  groupSecurity->setTitle(QString());
  groupSecurity->setCheckable(true);
  groupSecurity->setChecked(false);
  groupSecurity->hide();
  auto *securityLayout = new QVBoxLayout(groupSecurity);
  securityLayout->setObjectName(QStringLiteral("securityLayout"));
  securityLayout->setSpacing(8);
  securityLayout->setContentsMargins(12, 26, 12, 12);

  widgetTlsCert = new QWidget(networkCard);
  widgetTlsCert->setObjectName(QStringLiteral("widgetTlsCert"));
  auto *certificateForm = createForm(QStringLiteral("certificateForm"));
  lblTlsCert = createLabel(widgetTlsCert, QStringLiteral("lblTlsCert"), 120);
  certificateForm->addWidget(lblTlsCert, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  certificateControls = new QWidget(widgetTlsCert);
  certificateControls->setObjectName(QStringLiteral("certificateControls"));
  certificateControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *certificateControlsLayout = new QHBoxLayout(certificateControls);
  certificateControlsLayout->setObjectName(QStringLiteral("certificateControlsLayout"));
  certificateControlsLayout->setSpacing(8);
  certificateControlsLayout->setContentsMargins(0, 0, 0, 0);
  lineTlsCertPath = createLineEdit(certificateControls, QStringLiteral("lineTlsCertPath"));
  certificateControlsLayout->addWidget(lineTlsCertPath);
  btnTlsCertPath = new QPushButton(certificateControls);
  btnTlsCertPath->setObjectName(QStringLiteral("btnTlsCertPath"));
  btnTlsCertPath->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  btnTlsCertPath->setCursor(Qt::PointingHandCursor);
  certificateControlsLayout->addWidget(btnTlsCertPath);

  certificateForm->addWidget(certificateControls, 0, 1);
  widgetTlsCert->setLayout(certificateForm);
  networkCardLayout->addWidget(widgetTlsCert);

  auto *keyLengthForm = createForm(QStringLiteral("keyLengthForm"));
  lblTlsKeyLength = createLabel(groupSecurity, QStringLiteral("lblTlsKeyLength"), 120);
  keyLengthForm->addWidget(lblTlsKeyLength, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  keyLengthControls = new QWidget(networkCard);
  keyLengthControls->setObjectName(QStringLiteral("keyLengthControls"));
  keyLengthControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *keyLengthControlsLayout = new QHBoxLayout(keyLengthControls);
  keyLengthControlsLayout->setObjectName(QStringLiteral("keyLengthControlsLayout"));
  keyLengthControlsLayout->setSpacing(8);
  keyLengthControlsLayout->setContentsMargins(0, 0, 0, 0);
  comboTlsKeyLength = createComboBox(keyLengthControls, QStringLiteral("comboTlsKeyLength"), 2);
  comboTlsKeyLength->setItemData(0, 2048);
  comboTlsKeyLength->setItemData(1, 4096);
  keyLengthControlsLayout->addWidget(comboTlsKeyLength);
  btnTlsRegenCert = new QPushButton(keyLengthControls);
  btnTlsRegenCert->setObjectName(QStringLiteral("btnTlsRegenCert"));
  btnTlsRegenCert->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  btnTlsRegenCert->setIcon(synergy::gui::appIcon(QStringLiteral("view-refresh")));

  keyLengthForm->addWidget(keyLengthControls, 0, 1);
  networkCardLayout->addLayout(keyLengthForm);
  cbTlsEnabled = createCheckBox(networkCard, QStringLiteral("cbTlsEnabled"));
  networkCardLayout->addWidget(cbTlsEnabled);

  cbRequireClientCert = createCheckBox(networkCard, QStringLiteral("cbRequireClientCert"));
  networkCardLayout->addWidget(cbRequireClientCert);
  auto *certificateActions = new QHBoxLayout;
  certificateActions->setSpacing(8);
  certificateActions->addWidget(btnTlsRegenCert);
  btnViewFingerprint = new QPushButton(networkCard);
  btnViewFingerprint->setObjectName(QStringLiteral("btnViewFingerprint"));
  btnViewFingerprint->setIcon(synergy::gui::appIcon(QStringLiteral("fingerprint")));
  certificateActions->addWidget(btnViewFingerprint);
  certificateActions->addStretch(1);
  networkCardLayout->addLayout(certificateActions);

  tlsRegenerationNotice = new QFrame(networkCard);
  tlsRegenerationNotice->setObjectName(QStringLiteral("tlsRegenerationNotice"));
  tlsRegenerationNotice->hide();
  auto *noticeLayout = new QHBoxLayout(tlsRegenerationNotice);
  noticeLayout->setContentsMargins(10, 6, 10, 6);
  noticeLayout->setSpacing(8);
  lblTlsRegenerationResult = createLabel(tlsRegenerationNotice, QStringLiteral("lblTlsRegenerationResult"));
  lblTlsRegenerationResult->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  noticeLayout->addWidget(lblTlsRegenerationResult, 1);
  btnDismissTlsRegeneration = new QPushButton(tlsRegenerationNotice);
  btnDismissTlsRegeneration->setObjectName(QStringLiteral("btnDismissTlsRegeneration"));
  btnDismissTlsRegeneration->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  btnDismissTlsRegeneration->setProperty("primary", true);
  noticeLayout->addWidget(btnDismissTlsRegeneration, 0, Qt::AlignVCenter);
  networkCardLayout->addWidget(tlsRegenerationNotice);

  networkScrollLayout->addWidget(networkCard);
  networkScrollLayout->setAlignment(Qt::AlignTop);
  networkScroll->setWidget(networkScrollContents);
  tabNetworkLayout->addWidget(networkScroll);
  tabWidget->addTab(tabNetwork, QString());

  // Advanced
  tabAdvanced = new QWidget;
  tabAdvanced->setObjectName(QStringLiteral("tabAdvanced"));
  auto *tabAdvancedLayout = new QVBoxLayout(tabAdvanced);
  tabAdvancedLayout->setObjectName(QStringLiteral("tabAdvancedLayout"));
  tabAdvancedLayout->setSpacing(8);
  tabAdvancedLayout->setContentsMargins(0, 0, 0, 0);

  advancedScroll = new QScrollArea(tabAdvanced);
  advancedScroll->setObjectName(QStringLiteral("advancedScroll"));
  advancedScroll->setWidgetResizable(true);
  advancedScroll->setFrameShape(QFrame::NoFrame);
  advancedScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  advancedScrollContents = new QWidget;
  advancedScrollContents->setObjectName(QStringLiteral("advancedScrollContents"));
  auto *advancedScrollLayout = new QVBoxLayout(advancedScrollContents);
  advancedScrollLayout->setObjectName(QStringLiteral("advancedScrollLayout"));
  advancedScrollLayout->setSpacing(8);
  advancedScrollLayout->setContentsMargins(0, 0, 0, 0);

  groupService = createCard(advancedScrollContents, QStringLiteral("groupService"));
  groupService->setTitle(QString());
  groupService->setCheckable(true);
  groupService->setChecked(false);
  groupService->hide();
  auto *serviceLayout = new QVBoxLayout(groupService);
  serviceLayout->setObjectName(QStringLiteral("_6"));
  serviceLayout->setSpacing(8);
  serviceLayout->setContentsMargins(12, 26, 12, 12);
  cbElevateDaemon = createCheckBox(groupService, QStringLiteral("cbElevateDaemon"));
  cbElevateDaemon->setAutoExclusive(false);
  serviceLayout->addWidget(cbElevateDaemon);

  commandsCard = createCard(advancedScrollContents, QStringLiteral("commandsCard"));
  commandsCard->setTitle(QString());
  auto *commandsCardLayout = new QVBoxLayout(commandsCard);
  commandsCardLayout->setObjectName(QStringLiteral("commandsCardLayout"));
  commandsCardLayout->setSpacing(8);
  commandsCardLayout->setContentsMargins(12, 10, 12, 12);
  cbServiceEnabled = createCheckBox(commandsCard, QStringLiteral("cbServiceEnabled"));
  commandsCardLayout->addWidget(cbServiceEnabled);
  commandsCardLayout->addWidget(cbElevateDaemon);

#ifndef Q_OS_WIN
  auto *serviceHint = new QLabel(commandsCard);
  serviceHint->setObjectName(QStringLiteral("serviceUnavailableHint"));
  serviceHint->setProperty("muted", true);
  serviceHint->setWordWrap(true);
  commandsCardLayout->addWidget(serviceHint);
#endif
  const auto commandOption = [commandsCardLayout](QCheckBox *enabled, QLineEdit *input) {
    auto *option = new QVBoxLayout;
    option->setSpacing(4);
    option->addWidget(enabled);
    auto *field = new QHBoxLayout;
    field->setContentsMargins(24, 0, 0, 0);
    field->addWidget(input);
    option->addLayout(field);
    commandsCardLayout->addLayout(option);
  };
  cbRunEnterCommand = createCheckBox(commandsCard, QStringLiteral("cbRunEnterCommand"));
  cbRunEnterCommand->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  lineCommandEnter = createLineEdit(commandsCard, QStringLiteral("lineCommandEnter"));
  commandOption(cbRunEnterCommand, lineCommandEnter);

  cbRunExitCommand = createCheckBox(commandsCard, QStringLiteral("cbRunExitCommand"));
  cbRunExitCommand->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  lineCommandExit = createLineEdit(commandsCard, QStringLiteral("lineCommandExit"));
  commandOption(cbRunExitCommand, lineCommandExit);
  advancedScrollLayout->addWidget(commandsCard, 0, Qt::AlignTop);

  advancedScroll->setWidget(advancedScrollContents);
  tabAdvancedLayout->addWidget(advancedScroll);
  tabWidget->addTab(tabAdvanced, QString());

  verticalLayout_3->addWidget(tabWidget);

  buttonBox = new QDialogButtonBox(dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(
      QDialogButtonBox::Cancel | QDialogButtonBox::Reset | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Save
  );
  verticalLayout_3->addWidget(buttonBox);

  retranslateUi(dialog);
  tabWidget->setCurrentIndex(0);
}

void Ui::SettingsDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(QCoreApplication::translate("SettingsDialog", "Preferences", nullptr));

  tabWidget->setTabText(
      tabWidget->indexOf(tabGeneral), QCoreApplication::translate("SettingsDialog", "&General", nullptr)
  );
  tabWidget->setTabText(
      tabWidget->indexOf(tabWindow), QCoreApplication::translate("SettingsDialog", "&Window", nullptr)
  );
  tabWidget->setTabText(tabWidget->indexOf(tabLog), QCoreApplication::translate("SettingsDialog", "&Logs", nullptr));
  tabWidget->setTabText(
      tabWidget->indexOf(tabNetwork), QCoreApplication::translate("SettingsDialog", "&Network", nullptr)
  );
  tabWidget->setTabText(
      tabWidget->indexOf(tabAdvanced), QCoreApplication::translate("SettingsDialog", "&Advanced", nullptr)
  );

  generalCard->setTitle(QString());
  cbPreventSleep->setText(QCoreApplication::translate("SettingsDialog", "Prevent sleep", nullptr));
  checkForUpdates->setText(QCoreApplication::translate("SettingsDialog", "Check for updates on startup", nullptr));
  lblLanguage->setText(QCoreApplication::translate("SettingsDialog", "Language", nullptr));
  lblTrayIconStyle->setText(QCoreApplication::translate("SettingsDialog", "Tray icon", nullptr));
  rbIconMono->setText(QCoreApplication::translate("SettingsDialog", "Monocolor", nullptr));
  rbIconColorful->setText(QCoreApplication::translate("SettingsDialog", "Colorful", nullptr));
  comboTrayIcon->setItemText(0, QCoreApplication::translate("SettingsDialog", "Colorful", nullptr));
  comboTrayIcon->setItemText(1, QCoreApplication::translate("SettingsDialog", "Monocolor", nullptr));

  windowCard->setTitle(QString());
  label_3->setText(QCoreApplication::translate("SettingsDialog", "On launch", nullptr));
  rbShowOnStart->setText(QCoreApplication::translate("SettingsDialog", "Show the main window", nullptr));
  rbAutoHide->setText(QCoreApplication::translate("SettingsDialog", "Send to background", nullptr));
  label->setText(QCoreApplication::translate("SettingsDialog", "On close", nullptr));
  rbExitOnClose->setText(QCoreApplication::translate("SettingsDialog", "Exit", nullptr));
  rbCloseToTray->setText(QCoreApplication::translate("SettingsDialog", "Send to background", nullptr));
  comboStartBehavior->setItemText(0, rbShowOnStart->text());
  comboStartBehavior->setItemText(1, rbAutoHide->text());
  comboCloseBehavior->setItemText(0, rbCloseToTray->text());
  comboCloseBehavior->setItemText(1, rbExitOnClose->text());

  cbShowVersion->setText(QCoreApplication::translate("SettingsDialog", "Include version in the window title", nullptr));

  logCard->setTitle(QString());
  lblLogLevel->setText(QCoreApplication::translate("SettingsDialog", "Level", nullptr));
  synergy::gui::compact::setSubtleText(
      lblDebugWarning, QCoreApplication::translate(
                           "SettingsDialog", "Using a log level higher than Info may affect performance.", nullptr
                       )
  );
  groupLogToFile->setTitle(QCoreApplication::translate("SettingsDialog", "Log to file", nullptr));
  cbLogToFile->setText(groupLogToFile->title());
  labelLogPath->setText(QCoreApplication::translate("SettingsDialog", "Log path", nullptr));
  btnBrowseLog->setText(QString());
  cbGuiDebug->setText(QCoreApplication::translate("SettingsDialog", "Enable GUI debug messages", nullptr));

  networkCard->setTitle(QString());
  lblNetworkIp->setText(QCoreApplication::translate("SettingsDialog", "Network IP", nullptr));
  comboInterface->setItemText(0, QCoreApplication::translate("SettingsDialog", "Automatic", nullptr));
  lblPort->setText(QCoreApplication::translate("SettingsDialog", "Port", nullptr));
  groupSecurity->setTitle(QCoreApplication::translate("SettingsDialog", "Enable TLS Encryption", nullptr));
  cbTlsEnabled->setText(groupSecurity->title());
  lblTlsCert->setText(QCoreApplication::translate("SettingsDialog", "Certificate", nullptr));
  btnTlsCertPath->setText(QCoreApplication::translate("SettingsDialog", "Browse…"));
  const auto browseCertificate = QCoreApplication::translate("SettingsDialog", "Choose TLS certificate file");
  btnTlsCertPath->setAccessibleName(browseCertificate);
  btnTlsCertPath->setToolTip(browseCertificate);
  lblTlsKeyLength->setText(QCoreApplication::translate("SettingsDialog", "Key length", nullptr));
  comboTlsKeyLength->setItemText(0, QStringLiteral("2048"));
  comboTlsKeyLength->setItemText(1, QStringLiteral("4096"));
  btnTlsRegenCert->setText(QCoreApplication::translate("SettingsDialog", "Regenerate certificate", nullptr));
  btnViewFingerprint->setText(QCoreApplication::translate("SettingsDialog", "View saved certificate fingerprint"));
  lblTlsRegenerationResult->setText(
      QCoreApplication::translate("SettingsDialog", "TLS certificate regenerated successfully.", nullptr)
  );
  btnDismissTlsRegeneration->setText(QObject::tr("OK"));
  btnTlsRegenCert->setToolTip(
      QCoreApplication::translate(
          "SettingsDialog",
          "Immediately regenerate the saved certificate with this key length. Other changes remain unsaved."
      )
  );
  cbRequireClientCert->setText(QCoreApplication::translate("SettingsDialog", "Require client certificates", nullptr));

  groupService->setTitle(QCoreApplication::translate("SettingsDialog", "Use background service (daemon)", nullptr));
  cbServiceEnabled->setText(groupService->title());
  cbElevateDaemon->setText(
      QCoreApplication::translate("SettingsDialog", "Always run as system (work at login screen and UAC)", nullptr)
  );
  commandsCard->setTitle(QString());
  cbRunEnterCommand->setText(QCoreApplication::translate("SettingsDialog", "Run command on enter", nullptr));
  cbRunExitCommand->setText(QCoreApplication::translate("SettingsDialog", "Run command on exit", nullptr));
  if (auto *hint = dialog->findChild<QLabel *>(QStringLiteral("serviceUnavailableHint")))
    hint->setText(
        QCoreApplication::translate("SettingsDialog", "The background service is available on Windows only.")
    );
  for (auto *field : {lblLanguage, lblTrayIconStyle, label_3, label}) {
    field->setAccessibleName(field->text());
    field->setToolTip(field->text());
  }
  cbPreventSleep->setAccessibleName(cbPreventSleep->text());
  cbPreventSleep->setToolTip(cbPreventSleep->text());
  synergy::gui::compact::normalizeCards(dialog);
  synergy::gui::compact::styleButtons(buttonBox);
}
