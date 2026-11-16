/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialogDesign.h"

#include <QCoreApplication>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace {

QLabel *createLabel(QWidget *parent, const QString &name, int minimumWidth = 0)
{
  auto *result = new QLabel(parent);
  result->setObjectName(name);
  if (minimumWidth > 0)
    result->setMinimumWidth(minimumWidth);
  result->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  result->setWordWrap(false);
  result->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  return result;
}

QCheckBox *createCheckBox(QWidget *parent, const QString &name)
{
  auto *result = new QCheckBox(parent);
  result->setObjectName(name);
  return result;
}

QRadioButton *createRadioButton(QWidget *parent, const QString &name)
{
  auto *result = new QRadioButton(parent);
  result->setObjectName(name);
  return result;
}

QSpinBox *createSpinBox(QWidget *parent, const QString &name)
{
  auto *result = new QSpinBox(parent);
  result->setObjectName(name);
  result->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  return result;
}

QPushButton *createPushButton(QWidget *parent, const QString &name, const QString &icon)
{
  auto *button = new QPushButton(parent);
  button->setObjectName(name);
  button->setIcon(QIcon::fromTheme(icon));
  return button;
}

QSpacerItem *createSpacer()
{
  return new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
}

QGroupBox *createGroupBox(QWidget *parent, const QString &name)
{
  auto *result = new QGroupBox(parent);
  result->setObjectName(name);
  result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  return result;
}

QGridLayout *createGrid(const QString &name, int column1Stretch, int column2Stretch, int column3Stretch)
{
  auto *result = new QGridLayout;
  result->setObjectName(name);
  result->setContentsMargins(0, 0, 0, 0);
  result->setSpacing(8);
  result->setColumnStretch(0, column1Stretch);
  result->setColumnStretch(1, column2Stretch);
  result->setColumnStretch(2, column3Stretch);
  return result;
}

} // namespace

void Ui::ServerConfigDialog::setupUi(QDialog *dialog)
{
  dialog->setObjectName(QStringLiteral("ServerConfigDialog"));
  dialog->resize(600, 600);
  dialog->setMinimumSize(0, 0);

  auto *dialogLayout = new QVBoxLayout(dialog);
  dialogLayout->setObjectName(QStringLiteral("vboxLayout"));
  dialogLayout->setSpacing(8);
  dialogLayout->setContentsMargins(12, 12, 12, 12);

  tabWidget = new QTabWidget(dialog);
  tabWidget->setObjectName(QStringLiteral("tabWidget"));

  // Computer layout
  tabComputers = new QWidget;
  tabComputers->setObjectName(QStringLiteral("tabComputers"));
  auto *computerPageLayout = new QVBoxLayout(tabComputers);
  computerPageLayout->setObjectName(QStringLiteral("computerPageLayout"));
  computerPageLayout->setSpacing(8);
  computerPageLayout->setContentsMargins(0, 0, 0, 0);

  computersScrollArea = new QScrollArea(tabComputers);
  computersScrollArea->setObjectName(QStringLiteral("computersScrollArea"));
  computersScrollArea->setWidgetResizable(true);
  computersScrollArea->setFrameShape(QFrame::NoFrame);
  computersScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  computersScrollContents = new QWidget;
  computersScrollContents->setObjectName(QStringLiteral("computersScrollContents"));
  auto *computersScrollLayout = new QVBoxLayout(computersScrollContents);
  computersScrollLayout->setObjectName(QStringLiteral("computersScrollLayout"));
  computersScrollLayout->setSpacing(8);
  computersScrollLayout->setContentsMargins(0, 0, 0, 0);

  groupComputerLayout = new QGroupBox(computersScrollContents);
  groupComputerLayout->setObjectName(QStringLiteral("groupComputerLayout"));
  groupComputerLayout->setTitle(QString());
  auto *computerCardLayout = new QVBoxLayout(groupComputerLayout);
  computerCardLayout->setObjectName(QStringLiteral("computerCardLayout"));
  computerCardLayout->setSpacing(8);
  computerCardLayout->setContentsMargins(12, 12, 12, 12);
  computerCardLayout->setStretch(0, 0);
  computerCardLayout->setStretch(1, 1);
  computerCardLayout->setStretch(2, 0);

  label_2 = new QLabel(groupComputerLayout);
  label_2->setObjectName(QStringLiteral("label_2"));
  auto label2Policy = QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  label2Policy.setHorizontalStretch(1);
  label_2->setSizePolicy(label2Policy);
  label_2->setWordWrap(true);
  label_2->setIndent(-1);
  label_2->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  computerCardLayout->addWidget(label_2);

  screenSetupView = new ScreenSetupView(groupComputerLayout);
  screenSetupView->setObjectName(QStringLiteral("screenSetupView"));
  screenSetupView->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
  screenSetupView->setAcceptDrops(true);
  screenSetupView->setAutoFillBackground(false);
  screenSetupView->setFrameShape(QFrame::NoFrame);
  computerCardLayout->addWidget(screenSetupView);

  auto *layoutComputers = new QHBoxLayout;
  layoutComputers->setObjectName(QStringLiteral("layoutComputers"));
  layoutComputers->setSpacing(8);
  layoutComputers->setContentsMargins(0, 0, 0, 0);
  layoutComputers->setStretch(0, 0);
  layoutComputers->setStretch(1, 1);
  layoutComputers->setStretch(2, 0);

  lblRemoveScreen = new TrashScreenWidget(groupComputerLayout);
  lblRemoveScreen->setObjectName(QStringLiteral("lblRemoveScreen"));
  lblRemoveScreen->setAcceptDrops(true);
  lblRemoveScreen->setFrameShape(QFrame::NoFrame);
  lblRemoveScreen->setFrameShadow(QFrame::Plain);
  layoutComputers->addWidget(lblRemoveScreen);
  layoutComputers->addItem(createSpacer());

  lblNewScreen = new NewScreenWidget(groupComputerLayout);
  lblNewScreen->setObjectName(QStringLiteral("lblNewScreen"));
  lblNewScreen->setFrameShape(QFrame::NoFrame);
  lblNewScreen->setFrameShadow(QFrame::Plain);
  layoutComputers->addWidget(lblNewScreen);
  computerCardLayout->addLayout(layoutComputers);

  computersScrollLayout->addWidget(groupComputerLayout);
  computersScrollArea->setWidget(computersScrollContents);
  computerPageLayout->addWidget(computersScrollArea);
  tabWidget->addTab(tabComputers, QString());

  // Hotkeys
  tabHotkeys = new QWidget;
  tabHotkeys->setObjectName(QStringLiteral("tabHotkeys"));
  auto *tabHotkeysLayout = new QVBoxLayout(tabHotkeys);
  tabHotkeysLayout->setObjectName(QStringLiteral("_2"));
  tabHotkeysLayout->setSpacing(8);
  tabHotkeysLayout->setContentsMargins(0, 0, 0, 0);

  hotkeysScrollArea = new QScrollArea(tabHotkeys);
  hotkeysScrollArea->setObjectName(QStringLiteral("hotkeysScrollArea"));
  hotkeysScrollArea->setWidgetResizable(true);
  hotkeysScrollArea->setFrameShape(QFrame::NoFrame);
  hotkeysScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  hotkeysScrollContents = new QWidget;
  hotkeysScrollContents->setObjectName(QStringLiteral("hotkeysScrollContents"));
  auto *hotkeysScrollLayout = new QVBoxLayout(hotkeysScrollContents);
  hotkeysScrollLayout->setObjectName(QStringLiteral("hotkeysScrollLayout"));
  hotkeysScrollLayout->setSpacing(8);
  hotkeysScrollLayout->setContentsMargins(0, 0, 0, 0);
  hotkeysScrollLayout->setStretch(0, 1);
  hotkeysScrollLayout->setStretch(1, 1);

  groupHotkeys = new QGroupBox(hotkeysScrollContents);
  groupHotkeys->setObjectName(QStringLiteral("groupHotkeys"));
  groupHotkeys->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  auto *hotkeysLayout = new QVBoxLayout(groupHotkeys);
  hotkeysLayout->setObjectName(QStringLiteral("_3"));
  hotkeysLayout->setSpacing(8);
  hotkeysLayout->setContentsMargins(12, 12, 12, 12);
  hotkeysLayout->setStretch(0, 1);
  hotkeysLayout->setStretch(1, 0);

  listHotkeys = new QListWidget(groupHotkeys);
  listHotkeys->setObjectName(QStringLiteral("listHotkeys"));
  listHotkeys->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  hotkeysLayout->addWidget(listHotkeys);

  auto *hotkeyButtonsLayout = new QHBoxLayout;
  hotkeyButtonsLayout->setObjectName(QStringLiteral("hotkeyButtonsLayout"));
  hotkeyButtonsLayout->setSpacing(6);
  hotkeyButtonsLayout->setContentsMargins(0, 0, 0, 0);
  btnNewHotkey = createPushButton(groupHotkeys, QStringLiteral("btnNewHotkey"), QStringLiteral("list-add"));
  btnNewHotkey->setEnabled(true);
  hotkeyButtonsLayout->addWidget(btnNewHotkey);
  btnEditHotkey = createPushButton(groupHotkeys, QStringLiteral("btnEditHotkey"), QStringLiteral("document-edit"));
  btnEditHotkey->setEnabled(false);
  hotkeyButtonsLayout->addWidget(btnEditHotkey);
  btnRemoveHotkey = createPushButton(groupHotkeys, QStringLiteral("btnRemoveHotkey"), QStringLiteral("list-remove"));
  btnRemoveHotkey->setEnabled(false);
  hotkeyButtonsLayout->addWidget(btnRemoveHotkey);
  hotkeyButtonsLayout->addItem(createSpacer());
  hotkeysLayout->addLayout(hotkeyButtonsLayout);
  hotkeysScrollLayout->addWidget(groupHotkeys);

  groupActions = new QGroupBox(hotkeysScrollContents);
  groupActions->setObjectName(QStringLiteral("groupActions"));
  QFont actionsFont;
  actionsFont.setKerning(true);
  groupActions->setFont(actionsFont);
  groupActions->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  auto *actionsCardLayout = new QVBoxLayout(groupActions);
  actionsCardLayout->setObjectName(QStringLiteral("actionsCardLayout"));
  actionsCardLayout->setSpacing(8);
  actionsCardLayout->setContentsMargins(12, 12, 12, 12);
  actionsCardLayout->setStretch(0, 1);
  actionsCardLayout->setStretch(1, 0);

  listActions = new QListWidget(groupActions);
  listActions->setObjectName(QStringLiteral("listActions"));
  listActions->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  actionsCardLayout->addWidget(listActions);

  auto *actionButtonsLayout = new QHBoxLayout;
  actionButtonsLayout->setObjectName(QStringLiteral("actionButtonsLayout"));
  actionButtonsLayout->setSpacing(6);
  actionButtonsLayout->setContentsMargins(0, 0, 0, 0);
  btnNewAction = createPushButton(groupActions, QStringLiteral("btnNewAction"), QStringLiteral("list-add"));
  btnNewAction->setEnabled(false);
  actionButtonsLayout->addWidget(btnNewAction);
  btnEditAction = createPushButton(groupActions, QStringLiteral("btnEditAction"), QStringLiteral("document-edit"));
  btnEditAction->setEnabled(false);
  actionButtonsLayout->addWidget(btnEditAction);
  btnRemoveAction = createPushButton(groupActions, QStringLiteral("btnRemoveAction"), QStringLiteral("list-remove"));
  btnRemoveAction->setEnabled(false);
  actionButtonsLayout->addWidget(btnRemoveAction);
  actionButtonsLayout->addItem(createSpacer());
  actionsCardLayout->addLayout(actionButtonsLayout);
  hotkeysScrollLayout->addWidget(groupActions);

  hotkeysScrollArea->setWidget(hotkeysScrollContents);
  tabHotkeysLayout->addWidget(hotkeysScrollArea);
  tabWidget->addTab(tabHotkeys, QString());

  // Sharing & switching
  tabAdvanced = new QWidget;
  tabAdvanced->setObjectName(QStringLiteral("tabAdvanced"));
  auto *advancedPageLayout = new QVBoxLayout(tabAdvanced);
  advancedPageLayout->setObjectName(QStringLiteral("advancedPageLayout"));
  advancedPageLayout->setSpacing(8);
  advancedPageLayout->setContentsMargins(0, 0, 0, 0);

  sharingScrollArea = new QScrollArea(tabAdvanced);
  sharingScrollArea->setObjectName(QStringLiteral("sharingScrollArea"));
  sharingScrollArea->setWidgetResizable(true);
  sharingScrollArea->setFrameShape(QFrame::NoFrame);
  sharingScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  sharingScrollContents = new QWidget;
  sharingScrollContents->setObjectName(QStringLiteral("sharingScrollContents"));
  auto *m_pGridCorners = new QGridLayout(sharingScrollContents);
  m_pGridCorners->setObjectName(QStringLiteral("m_pGridCorners"));
  m_pGridCorners->setSpacing(8);
  m_pGridCorners->setContentsMargins(0, 0, 0, 0);
  m_pGridCorners->setColumnStretch(0, 1);
  m_pGridCorners->setColumnStretch(1, 1);
  m_pGridCorners->setRowStretch(0, 0);
  m_pGridCorners->setRowStretch(1, 0);
  m_pGridCorners->setRowStretch(2, 0);
  m_pGridCorners->setRowStretch(3, 1);

  groupMisc = createGroupBox(sharingScrollContents, QStringLiteral("groupMisc"));
  QFont miscFont;
  miscFont.setKerning(false);
  groupMisc->setFont(miscFont);
  auto *verticalLayout_4 = new QVBoxLayout(groupMisc);
  verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
  verticalLayout_4->setSpacing(8);
  verticalLayout_4->setContentsMargins(12, 12, 12, 12);

  cbRelativeMouseMoves = createCheckBox(groupMisc, QStringLiteral("cbRelativeMouseMoves"));
  cbRelativeMouseMoves->setEnabled(true);
  verticalLayout_4->addWidget(cbRelativeMouseMoves);
  cbDefaultLockToScreenState = createCheckBox(groupMisc, QStringLiteral("cbDefaultLockToScreenState"));
  verticalLayout_4->addWidget(cbDefaultLockToScreenState);
  cbDisableLockToScreen = createCheckBox(groupMisc, QStringLiteral("cbDisableLockToScreen"));
  verticalLayout_4->addWidget(cbDisableLockToScreen);
  cbWin32KeepForeground = createCheckBox(groupMisc, QStringLiteral("cbWin32KeepForeground"));
  cbWin32KeepForeground->setEnabled(true);
  verticalLayout_4->addWidget(cbWin32KeepForeground);

  cbEnableClipboard = createCheckBox(groupMisc, QStringLiteral("cbEnableClipboard"));
  cbEnableClipboard->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  cbEnableClipboard->setChecked(true);
  verticalLayout_4->addWidget(cbEnableClipboard);

  auto *layoutClipboard = new QHBoxLayout;
  layoutClipboard->setObjectName(QStringLiteral("layoutClipboard"));
  layoutClipboard->setSpacing(8);
  layoutClipboard->setContentsMargins(0, 0, 0, 0);
  label_7 = createLabel(groupMisc, QStringLiteral("label_7"), 120);
  layoutClipboard->addWidget(label_7);
  sbClipboardSizeLimit = createSpinBox(groupMisc, QStringLiteral("sbClipboardSizeLimit"));
  sbClipboardSizeLimit->setEnabled(false);
  sbClipboardSizeLimit->setMaximum(10000);
  sbClipboardSizeLimit->setValue(3);
  layoutClipboard->addWidget(sbClipboardSizeLimit);
  layoutClipboard->addItem(createSpacer());
  verticalLayout_4->addLayout(layoutClipboard);
  m_pGridCorners->addWidget(groupMisc, 0, 0, 1, 2);

  groupCorners = new QGroupBox(sharingScrollContents);
  groupCorners->setObjectName(QStringLiteral("groupCorners"));
  QFont cornersFont;
  cornersFont.setKerning(true);
  groupCorners->setFont(cornersFont);
  groupCorners->setChecked(false);
  groupCorners->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *verticalLayout_6 = new QVBoxLayout(groupCorners);
  verticalLayout_6->setObjectName(QStringLiteral("verticalLayout_6"));
  verticalLayout_6->setSpacing(8);
  verticalLayout_6->setContentsMargins(12, 12, 12, 12);

  auto *m_pLayoutCorners = new QGridLayout;
  m_pLayoutCorners->setObjectName(QStringLiteral("m_pLayoutCorners"));
  m_pLayoutCorners->setSpacing(8);
  m_pLayoutCorners->setContentsMargins(0, 0, 0, 0);
  m_pLayoutCorners->setColumnStretch(0, 1);
  m_pLayoutCorners->setColumnStretch(1, 1);
  cbCornerTopLeft = createCheckBox(groupCorners, QStringLiteral("cbCornerTopLeft"));
  m_pLayoutCorners->addWidget(cbCornerTopLeft, 0, 0);
  cbCornerTopRight = createCheckBox(groupCorners, QStringLiteral("cbCornerTopRight"));
  m_pLayoutCorners->addWidget(cbCornerTopRight, 0, 1);
  cbCornerBottomLeft = createCheckBox(groupCorners, QStringLiteral("cbCornerBottomLeft"));
  m_pLayoutCorners->addWidget(cbCornerBottomLeft, 1, 0);
  cbCornerBottomRight = createCheckBox(groupCorners, QStringLiteral("cbCornerBottomRight"));
  m_pLayoutCorners->addWidget(cbCornerBottomRight, 1, 1);
  verticalLayout_6->addLayout(m_pLayoutCorners);

  auto *m_pLayoutCornerSize = new QHBoxLayout;
  m_pLayoutCornerSize->setObjectName(QStringLiteral("m_pLayoutCornerSize"));
  m_pLayoutCornerSize->setSpacing(8);
  m_pLayoutCornerSize->setContentsMargins(0, 0, 0, 0);
  label = createLabel(groupCorners, QStringLiteral("label"), 120);
  m_pLayoutCornerSize->addWidget(label);
  sbSwitchCornerSize = createSpinBox(groupCorners, QStringLiteral("sbSwitchCornerSize"));
  sbSwitchCornerSize->setMaximum(30000);
  m_pLayoutCornerSize->addWidget(sbSwitchCornerSize);
  label->setBuddy(sbSwitchCornerSize);
  m_pLayoutCornerSize->addItem(createSpacer());
  verticalLayout_6->addLayout(m_pLayoutCornerSize);
  m_pGridCorners->addWidget(groupCorners, 1, 0, Qt::AlignTop);

  groupNetwork = createGroupBox(sharingScrollContents, QStringLiteral("groupNetwork"));
  auto *verticalLayout_5 = createGrid(QStringLiteral("verticalLayout_5"), 0, 0, 1);
  verticalLayout_5->setContentsMargins(12, 12, 12, 12);
  cbHeartbeat = createCheckBox(groupNetwork, QStringLiteral("cbHeartbeat"));
  cbHeartbeat->setEnabled(true);
  verticalLayout_5->addWidget(cbHeartbeat, 0, 0, 1, 2);
  sbHeartbeat = createSpinBox(groupNetwork, QStringLiteral("sbHeartbeat"));
  sbHeartbeat->setEnabled(false);
  sbHeartbeat->setMinimum(1000);
  sbHeartbeat->setMaximum(30000);
  sbHeartbeat->setSingleStep(1000);
  sbHeartbeat->setValue(5000);
  verticalLayout_5->addWidget(sbHeartbeat, 1, 1);
  labelProtocol = createLabel(groupNetwork, QStringLiteral("labelProtocol"), 120);
  verticalLayout_5->addWidget(labelProtocol, 2, 0, 1, 2);
  rbProtocolSynergy = createRadioButton(groupNetwork, QStringLiteral("rbProtocolSynergy"));
  rbProtocolSynergy->setText(QStringLiteral("Synergy"));
  rbProtocolSynergy->setChecked(true);
  verticalLayout_5->addWidget(rbProtocolSynergy, 3, 0);
  rbProtocolBarrier = createRadioButton(groupNetwork, QStringLiteral("rbProtocolBarrier"));
  rbProtocolBarrier->setText(QStringLiteral("Barrier"));
  verticalLayout_5->addWidget(rbProtocolBarrier, 3, 1);
  verticalLayout_5->addItem(createSpacer(), 0, 2, 4, 1);
  groupNetwork->setLayout(verticalLayout_5);
  m_pGridCorners->addWidget(groupNetwork, 1, 1, Qt::AlignTop);

  groupSwitch = new QGroupBox(sharingScrollContents);
  groupSwitch->setObjectName(QStringLiteral("groupSwitch"));
  QFont switchFont;
  switchFont.setKerning(true);
  groupSwitch->setFont(switchFont);
  groupSwitch->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *switchCardLayout = createGrid(QStringLiteral("switchCardLayout"), 0, 0, 1);
  switchCardLayout->setContentsMargins(12, 12, 12, 12);
  cbSwitchDelay = createCheckBox(groupSwitch, QStringLiteral("cbSwitchDelay"));
  cbSwitchDelay->setEnabled(true);
  cbSwitchDelay->setMinimumWidth(120);
  switchCardLayout->addWidget(cbSwitchDelay, 0, 0);
  sbSwitchDelay = createSpinBox(groupSwitch, QStringLiteral("sbSwitchDelay"));
  sbSwitchDelay->setEnabled(false);
  sbSwitchDelay->setMinimum(10);
  sbSwitchDelay->setMaximum(10000);
  sbSwitchDelay->setSingleStep(10);
  sbSwitchDelay->setValue(250);
  switchCardLayout->addWidget(sbSwitchDelay, 0, 1);
  cbSwitchDoubleTap = createCheckBox(groupSwitch, QStringLiteral("cbSwitchDoubleTap"));
  cbSwitchDoubleTap->setEnabled(true);
  cbSwitchDoubleTap->setMinimumWidth(120);
  switchCardLayout->addWidget(cbSwitchDoubleTap, 1, 0);
  sbSwitchDoubleTap = createSpinBox(groupSwitch, QStringLiteral("sbSwitchDoubleTap"));
  sbSwitchDoubleTap->setEnabled(false);
  sbSwitchDoubleTap->setMinimum(10);
  sbSwitchDoubleTap->setMaximum(10000);
  sbSwitchDoubleTap->setSingleStep(10);
  sbSwitchDoubleTap->setValue(250);
  switchCardLayout->addWidget(sbSwitchDoubleTap, 1, 1);
  switchCardLayout->addItem(createSpacer(), 0, 2, 2, 1);
  groupSwitch->setLayout(switchCardLayout);
  m_pGridCorners->addWidget(groupSwitch, 2, 0, 1, 2);

  sharingBottomStretch = new QWidget(sharingScrollContents);
  sharingBottomStretch->setObjectName(QStringLiteral("sharingBottomStretch"));
  m_pGridCorners->addWidget(sharingBottomStretch, 3, 0, 1, 2);

  sharingScrollArea->setWidget(sharingScrollContents);
  advancedPageLayout->addWidget(sharingScrollArea);
  tabWidget->addTab(tabAdvanced, QString());

  // Config file
  tabConfigFile = new QWidget;
  tabConfigFile->setObjectName(QStringLiteral("tabConfigFile"));
  tabConfigFile->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  auto *verticalLayout = new QVBoxLayout(tabConfigFile);
  verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
  verticalLayout->setSpacing(8);
  verticalLayout->setContentsMargins(0, 0, 0, 0);

  configScrollArea = new QScrollArea(tabConfigFile);
  configScrollArea->setObjectName(QStringLiteral("configScrollArea"));
  configScrollArea->setWidgetResizable(true);
  configScrollArea->setFrameShape(QFrame::NoFrame);
  configScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  configScrollContents = new QWidget;
  configScrollContents->setObjectName(QStringLiteral("configScrollContents"));
  auto *configScrollLayout = new QVBoxLayout(configScrollContents);
  configScrollLayout->setObjectName(QStringLiteral("configScrollLayout"));
  configScrollLayout->setSpacing(8);
  configScrollLayout->setContentsMargins(0, 0, 0, 0);
  configScrollLayout->setStretch(0, 0);
  configScrollLayout->setStretch(1, 0);
  configScrollLayout->setStretch(2, 0);
  configScrollLayout->setStretch(3, 1);

  label_5 = new QLabel(configScrollContents);
  label_5->setObjectName(QStringLiteral("label_5"));
  QFont titleFont;
  titleFont.setPointSize(13);
  titleFont.setBold(true);
  label_5->setFont(titleFont);
  label_5->setTextFormat(Qt::PlainText);
  label_5->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  configScrollLayout->addWidget(label_5);

  label_6 = new QLabel(configScrollContents);
  label_6->setObjectName(QStringLiteral("label_6"));
  label_6->setTextFormat(Qt::MarkdownText);
  label_6->setWordWrap(true);
  label_6->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  configScrollLayout->addWidget(label_6);

  groupExternalConfig = createGroupBox(configScrollContents, QStringLiteral("groupExternalConfig"));
  groupExternalConfig->setCheckable(true);
  groupExternalConfig->setChecked(false);
  auto *horizontalLayout = new QVBoxLayout(groupExternalConfig);
  horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
  horizontalLayout->setSpacing(8);
  horizontalLayout->setContentsMargins(12, 12, 12, 12);

  widgetExternalConfigControls = new QWidget(groupExternalConfig);
  widgetExternalConfigControls->setObjectName(QStringLiteral("widgetExternalConfigControls"));
  widgetExternalConfigControls->setEnabled(false);
  auto *horizontalLayout_2 = new QHBoxLayout(widgetExternalConfigControls);
  horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
  horizontalLayout_2->setSpacing(8);
  horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
  horizontalLayout_2->setStretch(0, 0);
  horizontalLayout_2->setStretch(1, 1);
  horizontalLayout_2->setStretch(2, 0);
  labelConfigFile = createLabel(widgetExternalConfigControls, QStringLiteral("labelConfigFile"), 120);
  horizontalLayout_2->addWidget(labelConfigFile);
  lineConfigFile = new QLineEdit(widgetExternalConfigControls);
  lineConfigFile->setObjectName(QStringLiteral("lineConfigFile"));
  lineConfigFile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  horizontalLayout_2->addWidget(lineConfigFile);
  btnBrowseConfigFile = new QPushButton(widgetExternalConfigControls);
  btnBrowseConfigFile->setObjectName(QStringLiteral("btnBrowseConfigFile"));
  btnBrowseConfigFile->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  btnBrowseConfigFile->setMinimumSize(0, 0);
  btnBrowseConfigFile->setCursor(Qt::PointingHandCursor);
  btnBrowseConfigFile->setText(QString());
  btnBrowseConfigFile->setFlat(true);
  horizontalLayout_2->addWidget(btnBrowseConfigFile);
  horizontalLayout->addWidget(widgetExternalConfigControls);
  configScrollLayout->addWidget(groupExternalConfig);

  configBottomStretch = new QWidget(configScrollContents);
  configBottomStretch->setObjectName(QStringLiteral("configBottomStretch"));
  configScrollLayout->addWidget(configBottomStretch);

  configScrollArea->setWidget(configScrollContents);
  verticalLayout->addWidget(configScrollArea);
  tabWidget->addTab(tabConfigFile, QString());

  dialogLayout->addWidget(tabWidget);

  buttonBox = new QDialogButtonBox(dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  dialogLayout->addWidget(buttonBox);

  retranslateUi(dialog);
  tabWidget->setCurrentIndex(0);

  QWidget::setTabOrder(screenSetupView, listHotkeys);
  QWidget::setTabOrder(listHotkeys, btnNewHotkey);
  QWidget::setTabOrder(btnNewHotkey, btnEditHotkey);
  QWidget::setTabOrder(btnEditHotkey, btnRemoveHotkey);
  QWidget::setTabOrder(btnRemoveHotkey, listActions);
  QWidget::setTabOrder(listActions, btnNewAction);
  QWidget::setTabOrder(btnNewAction, btnEditAction);
  QWidget::setTabOrder(btnEditAction, btnRemoveAction);
  QWidget::setTabOrder(btnRemoveAction, cbSwitchDelay);
  QWidget::setTabOrder(cbSwitchDelay, sbSwitchDelay);
  QWidget::setTabOrder(sbSwitchDelay, cbSwitchDoubleTap);
  QWidget::setTabOrder(cbSwitchDoubleTap, sbSwitchDoubleTap);
  QWidget::setTabOrder(sbSwitchDoubleTap, cbEnableClipboard);
  QWidget::setTabOrder(cbEnableClipboard, sbClipboardSizeLimit);
  QWidget::setTabOrder(sbClipboardSizeLimit, cbHeartbeat);
  QWidget::setTabOrder(cbHeartbeat, sbHeartbeat);
  QWidget::setTabOrder(sbHeartbeat, cbRelativeMouseMoves);
  QWidget::setTabOrder(cbRelativeMouseMoves, sbSwitchCornerSize);
  QWidget::setTabOrder(sbSwitchCornerSize, tabWidget);
}

void Ui::ServerConfigDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(QCoreApplication::translate("ServerConfigDialog", "Server Configuration", nullptr));

  tabWidget->setTabText(
      tabWidget->indexOf(tabComputers), QCoreApplication::translate("ServerConfigDialog", "Computer Layout", nullptr)
  );
  tabWidget->setTabText(
      tabWidget->indexOf(tabHotkeys), QCoreApplication::translate("ServerConfigDialog", "Hotkeys", nullptr)
  );
  tabWidget->setTabText(
      tabWidget->indexOf(tabAdvanced), QCoreApplication::translate("ServerConfigDialog", "Sharing & Switching", nullptr)
  );
  tabWidget->setTabText(
      tabWidget->indexOf(tabConfigFile), QCoreApplication::translate("ServerConfigDialog", "Config file", nullptr)
  );

  groupComputerLayout->setTitle(QString());
  label_2->setText(
      QCoreApplication::translate(
          "ServerConfigDialog", "Configure the layout of your computer displays by dragging to where you want.", nullptr
      )
  );
  lblRemoveScreen->setToolTip(
      QCoreApplication::translate(
          "ServerConfigDialog", "Drag a computer from the grid to the trashcan to remove it.", nullptr
      )
  );
  lblNewScreen->setToolTip(
      QCoreApplication::translate("ServerConfigDialog", "Drag to the grid to add a new computer.", nullptr)
  );

  groupHotkeys->setTitle(QCoreApplication::translate("ServerConfigDialog", "&Hotkeys", nullptr));
  btnNewHotkey->setText(QCoreApplication::translate("ServerConfigDialog", "&New", nullptr));
  btnEditHotkey->setText(QCoreApplication::translate("ServerConfigDialog", "&Edit", nullptr));
  btnRemoveHotkey->setText(QCoreApplication::translate("ServerConfigDialog", "&Remove", nullptr));
  groupActions->setTitle(QCoreApplication::translate("ServerConfigDialog", "A&ctions", nullptr));
  btnNewAction->setText(QCoreApplication::translate("ServerConfigDialog", "Ne&w", nullptr));
  btnEditAction->setText(QCoreApplication::translate("ServerConfigDialog", "E&dit", nullptr));
  btnRemoveAction->setText(QCoreApplication::translate("ServerConfigDialog", "Re&move", nullptr));

  groupMisc->setTitle(QCoreApplication::translate("ServerConfigDialog", "&Misc", nullptr));
  cbRelativeMouseMoves->setText(
      QCoreApplication::translate("ServerConfigDialog", "Use &relative mouse movements", nullptr)
  );
  cbDefaultLockToScreenState->setText(
      QCoreApplication::translate("ServerConfigDialog", "Enable lock to computer at startup", nullptr)
  );
  cbDisableLockToScreen->setText(
      QCoreApplication::translate("ServerConfigDialog", "Disable lock to computer (scroll lock key)", nullptr)
  );
  cbWin32KeepForeground->setText(
      QCoreApplication::translate("ServerConfigDialog", "Don't take &foreground window (Windows only)", nullptr)
  );
  cbEnableClipboard->setText(QCoreApplication::translate("ServerConfigDialog", "Enable clipboard sharing", nullptr));
  label_7->setText(QCoreApplication::translate("ServerConfigDialog", "Limit to:", nullptr));
  sbClipboardSizeLimit->setSuffix(QCoreApplication::translate("ServerConfigDialog", "MB", nullptr));

  groupCorners->setTitle(
      QCoreApplication::translate("ServerConfigDialog", "&Dead corners (for this computer)", nullptr)
  );
  cbCornerTopLeft->setText(QCoreApplication::translate("ServerConfigDialog", "To&p-left", nullptr));
  cbCornerTopRight->setText(QCoreApplication::translate("ServerConfigDialog", "Top-rig&ht", nullptr));
  cbCornerBottomLeft->setText(QCoreApplication::translate("ServerConfigDialog", "&Bottom-left", nullptr));
  cbCornerBottomRight->setText(QCoreApplication::translate("ServerConfigDialog", "Bottom-ri&ght", nullptr));
  label->setText(QCoreApplication::translate("ServerConfigDialog", "Cor&ner size", nullptr));
  sbSwitchCornerSize->setSuffix(QCoreApplication::translate("ServerConfigDialog", "px", nullptr));

  groupNetwork->setTitle(QCoreApplication::translate("ServerConfigDialog", "&Network", nullptr));
  cbHeartbeat->setText(QCoreApplication::translate("ServerConfigDialog", "&Check clients every", nullptr));
  cbHeartbeat->setWhatsThis(
      QCoreApplication::translate(
          "ServerConfigDialog",
          "<html><head/><body><p>Enables the network heartbeat to ping clients every <span style=\" "
          "font-style:italic;\">n</span> seconds. This may help to diagnose network problems by retrying the "
          "connection "
          "if the client becomes unresponsive.</p></body></html>",
          nullptr
      )
  );
  sbHeartbeat->setSuffix(QCoreApplication::translate("ServerConfigDialog", "ms", nullptr));
  labelProtocol->setText(QCoreApplication::translate("ServerConfigDialog", "Network protocol", nullptr));
  labelProtocol->setWhatsThis(
      QCoreApplication::translate(
          "ServerConfigDialog",
          "<html><head/><body><p>Enables compatibility with programs that use either the Synergy or Barrier "
          "protocols:</p><ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; "
          "-qt-list-indent: 1;\"><li style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; margin-right:0px; "
          "-qt-block-indent:0; text-indent:0px;\">Synergy 3 uses the Synergy protocol.</li><li style=\" "
          "margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; "
          "text-indent:0px;\">Barrier, Input-Leap and Synergy 1 uses the Barrier protocol.</li></ul><p>A %1 client "
          "will automatically use either the Synergy or Barrier protocol depending on the server "
          "protocol.</p></body></html>",
          nullptr
      )
  );

  groupSwitch->setTitle(QCoreApplication::translate("ServerConfigDialog", "&Switching", nullptr));
  cbSwitchDelay->setText(QCoreApplication::translate("ServerConfigDialog", "Switch &after waiting", nullptr));
  sbSwitchDelay->setSuffix(QCoreApplication::translate("ServerConfigDialog", "ms", nullptr));
  cbSwitchDoubleTap->setText(
      QCoreApplication::translate("ServerConfigDialog", "Switch on double &tap within", nullptr)
  );
  sbSwitchDoubleTap->setSuffix(QCoreApplication::translate("ServerConfigDialog", "ms", nullptr));

  label_5->setText(QCoreApplication::translate("ServerConfigDialog", "Core server config file", nullptr));
  label_6->setText(
      QCoreApplication::translate(
          "ServerConfigDialog",
          "Use a server config file to create complex computer layouts that are not possible with the simple "
          "grid-based computer layout editor.\n\nEnabling this setting will disable the server config GUI.",
          nullptr
      )
  );
  groupExternalConfig->setTitle(QCoreApplication::translate("ServerConfigDialog", "Use a server config file", nullptr));
  labelConfigFile->setText(QCoreApplication::translate("ServerConfigDialog", "Config file path", nullptr));
  btnBrowseConfigFile->setText(QString());
}
