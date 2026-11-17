/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialogDesign.h"
#include "CompactLayout.h"
#include "common/NetworkProtocol.h"
#include "gui/StyleUtils.h"

#include <QCoreApplication>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QListView>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QTextLayout>
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
  result->setAttribute(Qt::WA_LayoutUsesWidgetRect);
  return result;
}

// Preserve the complete translated enable caption while keeping the reference
// label column fixed. Native checkbox text painting supports explicit lines.
void fitEnableCaption(QCheckBox *checkBox)
{
  const QString original = checkBox->text();
  checkBox->setAttribute(Qt::WA_LayoutUsesWidgetRect);
  checkBox->setFixedWidth(120);
  checkBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  checkBox->ensurePolished();
  QTextLayout text(original, checkBox->font());
  QTextOption option;
  option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  text.setTextOption(option);
  QStringList lines;
  text.beginLayout();
  while (true) {
    auto line = text.createLine();
    if (!line.isValid())
      break;
    line.setLineWidth(88);
    lines.append(original.mid(line.textStart(), line.textLength()).trimmed());
  }
  text.endLayout();
  checkBox->setText(lines.join(QLatin1Char('\n')));
  checkBox->setMinimumHeight(qMax(22, static_cast<int>(lines.size()) * checkBox->fontMetrics().lineSpacing() + 4));
  checkBox->setToolTip(original);
  checkBox->setAccessibleName(QString(original).remove(QLatin1Char('&')));
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
  result->setFixedWidth(180);
  return result;
}

QPushButton *createPushButton(QWidget *parent, const QString &name, const QString &icon)
{
  auto *button = new QPushButton(parent);
  button->setObjectName(name);
  button->setIcon(synergy::gui::appIcon(icon));
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
  result->setHorizontalSpacing(10);
  result->setVerticalSpacing(8);
  result->setColumnMinimumWidth(0, 120);
  result->setColumnMinimumWidth(1, 180);
  result->setColumnStretch(0, column1Stretch);
  result->setColumnStretch(1, column2Stretch);
  result->setColumnStretch(2, column3Stretch);
  return result;
}

} // namespace

void Ui::ServerConfigDialog::setupUi(QDialog *dialog)
{
  dialog->setObjectName(QStringLiteral("ServerConfigDialog"));
  dialog->resize(560, 420);
  dialog->setMinimumSize(0, 0);

  auto *dialogLayout = new QVBoxLayout(dialog);
  dialogLayout->setObjectName(QStringLiteral("vboxLayout"));
  dialogLayout->setSpacing(8);
  dialogLayout->setContentsMargins(10, 10, 10, 10);

  tabWidget = new QTabWidget(dialog);
  tabWidget->setObjectName(QStringLiteral("tabWidget"));

  // Computer layout
  tabComputers = new QWidget;
  tabComputers->setObjectName(QStringLiteral("tabComputers"));
  auto *computerPageLayout = new QVBoxLayout(tabComputers);
  computerPageLayout->setObjectName(QStringLiteral("computerPageLayout"));
  computerPageLayout->setSpacing(8);
  computerPageLayout->setContentsMargins(0, 0, 0, 0);

  // 计算机布局直接放入页签，不再套第二层滚动区。
  // 页面宿主只在窗口小到无法容纳操作栏时提供整体滚动兜底。
  groupComputerLayout = new QGroupBox(tabComputers);
  groupComputerLayout->setObjectName(QStringLiteral("groupComputerLayout"));
  groupComputerLayout->setTitle(
      QCoreApplication::translate("ServerConfigDialog", "Arrange computer displays by dragging")
  );
  auto *computerCardLayout = new QVBoxLayout(groupComputerLayout);
  computerCardLayout->setObjectName(QStringLiteral("computerCardLayout"));
  computerCardLayout->setSpacing(8);
  computerCardLayout->setContentsMargins(12, 26, 12, 12);
  groupComputerLayout->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

  label_2 = new QLabel(groupComputerLayout);
  label_2->setObjectName(QStringLiteral("label_2"));
  auto label2Policy = QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  label2Policy.setHorizontalStretch(1);
  label_2->setSizePolicy(label2Policy);
  label_2->setWordWrap(true);
  label_2->setIndent(-1);
  label_2->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label_2->setProperty("muted", true);
  computerCardLayout->addWidget(label_2);

  screenSetupView = new ScreenSetupView(groupComputerLayout);
  screenSetupView->setObjectName(QStringLiteral("screenSetupView"));
  screenSetupView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  screenSetupView->setFixedHeight(180);
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

  computerPageLayout->addWidget(groupComputerLayout);
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
  hotkeysScrollLayout->setAlignment(Qt::AlignTop);

  groupHotkeys = new QGroupBox(hotkeysScrollContents);
  groupHotkeys->setObjectName(QStringLiteral("groupHotkeys"));
  groupHotkeys->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *hotkeysLayout = new QVBoxLayout(groupHotkeys);
  hotkeysLayout->setObjectName(QStringLiteral("_3"));
  hotkeysLayout->setSpacing(8);
  hotkeysLayout->setContentsMargins(12, 26, 12, 12);

  listHotkeys = new QListWidget(groupHotkeys);
  listHotkeys->setObjectName(QStringLiteral("listHotkeys"));
  listHotkeys->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  hotkeysLayout->addWidget(listHotkeys);
  synergy::gui::compact::fitRows(
      listHotkeys, QCoreApplication::translate("ServerConfigDialog", "No hotkeys configured")
  );

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
  groupActions->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *actionsCardLayout = new QVBoxLayout(groupActions);
  actionsCardLayout->setObjectName(QStringLiteral("actionsCardLayout"));
  actionsCardLayout->setSpacing(8);
  actionsCardLayout->setContentsMargins(12, 26, 12, 12);

  listActions = new QListWidget(groupActions);
  listActions->setObjectName(QStringLiteral("listActions"));
  listActions->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  actionsCardLayout->addWidget(listActions);
  synergy::gui::compact::fitRows(
      listActions, QCoreApplication::translate("ServerConfigDialog", "Select a hotkey to view its actions")
  );

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
  m_pGridCorners->setColumnStretch(1, 0);
  m_pGridCorners->setRowStretch(0, 0);
  m_pGridCorners->setRowStretch(1, 0);
  m_pGridCorners->setRowStretch(2, 0);
  m_pGridCorners->setAlignment(Qt::AlignTop);

  groupMisc = createGroupBox(sharingScrollContents, QStringLiteral("groupMisc"));
  QFont miscFont;
  miscFont.setKerning(false);
  groupMisc->setFont(miscFont);
  auto *verticalLayout_4 = new QVBoxLayout(groupMisc);
  verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
  verticalLayout_4->setSpacing(8);
  verticalLayout_4->setContentsMargins(12, 26, 12, 12);

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
  verticalLayout_4->insertWidget(0, cbEnableClipboard);

  auto *layoutClipboard = new QHBoxLayout;
  layoutClipboard->setObjectName(QStringLiteral("layoutClipboard"));
  layoutClipboard->setSpacing(10);
  layoutClipboard->setContentsMargins(0, 0, 0, 0);
  label_7 = createLabel(groupMisc, QStringLiteral("label_7"), 120);
  label_7->setFixedWidth(120);
  label_7->setWordWrap(true);
  layoutClipboard->addWidget(label_7);
  sbClipboardSizeLimit = createSpinBox(groupMisc, QStringLiteral("sbClipboardSizeLimit"));
  sbClipboardSizeLimit->setEnabled(false);
  sbClipboardSizeLimit->setMaximum(10000);
  sbClipboardSizeLimit->setValue(3);
  layoutClipboard->addWidget(sbClipboardSizeLimit);
  layoutClipboard->addItem(createSpacer());
  verticalLayout_4->addLayout(layoutClipboard);
  m_pGridCorners->addWidget(groupMisc, 0, 0);

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
  verticalLayout_6->setContentsMargins(12, 26, 12, 12);

  auto *m_pLayoutCorners = new QGridLayout;
  m_pLayoutCorners->setObjectName(QStringLiteral("m_pLayoutCorners"));
  m_pLayoutCorners->setSpacing(8);
  m_pLayoutCorners->setContentsMargins(0, 0, 0, 0);
  m_pLayoutCorners->setColumnStretch(0, 1);
  m_pLayoutCorners->setColumnStretch(1, 1);
  cbCornerTopLeft = createCheckBox(groupCorners, QStringLiteral("cbCornerTopLeft"));
  m_pLayoutCorners->addWidget(cbCornerTopLeft, 0, 0);
  cbCornerTopRight = createCheckBox(groupCorners, QStringLiteral("cbCornerTopRight"));
  m_pLayoutCorners->addWidget(cbCornerTopRight, 1, 0);
  cbCornerBottomLeft = createCheckBox(groupCorners, QStringLiteral("cbCornerBottomLeft"));
  m_pLayoutCorners->addWidget(cbCornerBottomLeft, 2, 0);
  cbCornerBottomRight = createCheckBox(groupCorners, QStringLiteral("cbCornerBottomRight"));
  m_pLayoutCorners->addWidget(cbCornerBottomRight, 3, 0);
  verticalLayout_6->addLayout(m_pLayoutCorners);

  auto *m_pLayoutCornerSize = new QHBoxLayout;
  m_pLayoutCornerSize->setObjectName(QStringLiteral("m_pLayoutCornerSize"));
  m_pLayoutCornerSize->setSpacing(10);
  m_pLayoutCornerSize->setContentsMargins(0, 0, 0, 0);
  label = createLabel(groupCorners, QStringLiteral("label"), 120);
  label->setFixedWidth(120);
  label->setWordWrap(true);
  m_pLayoutCornerSize->addWidget(label);
  sbSwitchCornerSize = createSpinBox(groupCorners, QStringLiteral("sbSwitchCornerSize"));
  sbSwitchCornerSize->setMaximum(30000);
  m_pLayoutCornerSize->addWidget(sbSwitchCornerSize);
  label->setBuddy(sbSwitchCornerSize);
  m_pLayoutCornerSize->addItem(createSpacer());
  verticalLayout_6->addLayout(m_pLayoutCornerSize);
  m_pGridCorners->addWidget(groupCorners, 1, 0);

  groupNetwork = createGroupBox(sharingScrollContents, QStringLiteral("groupNetwork"));
  auto *verticalLayout_5 = createGrid(QStringLiteral("verticalLayout_5"), 0, 0, 1);
  verticalLayout_5->setContentsMargins(12, 26, 12, 12);
  cbHeartbeat = createCheckBox(groupNetwork, QStringLiteral("cbHeartbeat"));
  cbHeartbeat->setEnabled(true);
  verticalLayout_5->addWidget(cbHeartbeat, 0, 0);
  sbHeartbeat = createSpinBox(groupNetwork, QStringLiteral("sbHeartbeat"));
  sbHeartbeat->setEnabled(false);
  sbHeartbeat->setMinimum(1000);
  sbHeartbeat->setMaximum(30000);
  sbHeartbeat->setSingleStep(1000);
  sbHeartbeat->setValue(5000);
  verticalLayout_5->addWidget(sbHeartbeat, 0, 1);
  labelProtocol = createLabel(groupNetwork, QStringLiteral("labelProtocol"), 120);
  labelProtocol->setFixedWidth(120);
  labelProtocol->setWordWrap(true);
  verticalLayout_5->addWidget(labelProtocol, 1, 0);
  rbProtocolSynergy = createRadioButton(groupNetwork, QStringLiteral("rbProtocolSynergy"));
  rbProtocolSynergy->setText(QStringLiteral("Synergy"));
  rbProtocolSynergy->setChecked(true);
  rbProtocolSynergy->hide();
  rbProtocolBarrier = createRadioButton(groupNetwork, QStringLiteral("rbProtocolBarrier"));
  rbProtocolBarrier->setText(QStringLiteral("Barrier"));
  rbProtocolBarrier->hide();
  comboProtocol = new QComboBox(groupNetwork);
  comboProtocol->setObjectName(QStringLiteral("comboProtocol"));
  comboProtocol->setView(new QListView(comboProtocol));
  comboProtocol->setFixedWidth(180);
  comboProtocol->addItem(QStringLiteral("Synergy"), QVariant::fromValue(NetworkProtocol::Synergy));
  comboProtocol->addItem(QStringLiteral("Barrier"), QVariant::fromValue(NetworkProtocol::Barrier));
  labelProtocol->setBuddy(comboProtocol);
  verticalLayout_5->addWidget(comboProtocol, 1, 1);
  verticalLayout_5->addItem(createSpacer(), 0, 2, 2, 1);
  groupNetwork->setLayout(verticalLayout_5);
  m_pGridCorners->addWidget(groupNetwork, 2, 0);

  groupSwitch = new QGroupBox(sharingScrollContents);
  groupSwitch->setObjectName(QStringLiteral("groupSwitch"));
  QFont switchFont;
  switchFont.setKerning(true);
  groupSwitch->setFont(switchFont);
  groupSwitch->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *switchCardLayout = createGrid(QStringLiteral("switchCardLayout"), 0, 0, 1);
  switchCardLayout->setContentsMargins(12, 26, 12, 12);
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
  m_pGridCorners->addWidget(groupSwitch, 3, 0);

  synergy::gui::compact::combineCards(
      sharingScrollContents, {groupMisc, groupCorners, groupNetwork, groupSwitch}, QStringLiteral("sharingCard")
  );
  for (auto *section : {groupMisc, groupCorners, groupNetwork, groupSwitch})
    section->layout()->setContentsMargins(0, 0, 0, 0);
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
  configScrollLayout->setAlignment(Qt::AlignTop);

  label_5 = new QLabel(configScrollContents);
  label_5->setObjectName(QStringLiteral("label_5"));
  QFont titleFont;
  titleFont.setPointSize(13);
  titleFont.setBold(true);
  label_5->setFont(titleFont);
  label_5->setTextFormat(Qt::PlainText);
  label_5->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label_5->hide();

  label_6 = new QLabel(configScrollContents);
  label_6->setObjectName(QStringLiteral("label_6"));
  label_6->setTextFormat(Qt::MarkdownText);
  label_6->setWordWrap(true);
  label_6->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  groupExternalConfig = createGroupBox(configScrollContents, QStringLiteral("groupExternalConfig"));
  groupExternalConfig->setCheckable(true);
  groupExternalConfig->setChecked(false);
  groupExternalConfig->hide();
  auto *externalCard = createGroupBox(configScrollContents, QStringLiteral("externalConfigCard"));
  auto *externalLayout = new QVBoxLayout(externalCard);
  externalLayout->setContentsMargins(12, 10, 12, 12);
  externalLayout->setSpacing(8);
  cbExternalConfig = createCheckBox(externalCard, QStringLiteral("cbExternalConfig"));
  externalLayout->addWidget(cbExternalConfig);
  auto *horizontalLayout = new QVBoxLayout(groupExternalConfig);
  horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
  horizontalLayout->setSpacing(8);
  horizontalLayout->setContentsMargins(12, 26, 12, 12);

  widgetExternalConfigControls = new QWidget(externalCard);
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
  btnBrowseConfigFile->setProperty("iconButton32", true);
  horizontalLayout_2->addWidget(btnBrowseConfigFile);
  externalLayout->addWidget(widgetExternalConfigControls);
  externalLayout->addWidget(label_6);
  configScrollLayout->addWidget(externalCard);

  configScrollArea->setWidget(configScrollContents);
  verticalLayout->addWidget(configScrollArea);
  tabWidget->addTab(tabConfigFile, QString());

  dialogLayout->addWidget(tabWidget);

  buttonBox = new QDialogButtonBox(dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  dialogLayout->addWidget(buttonBox);
  const auto placeActions = [tabs = tabWidget, box = buttonBox, dialogLayout] {
    const auto name = tabs->currentIndex() == 2 ? QStringLiteral("sharingCard") : QStringLiteral("externalConfigCard");
    if (tabs->currentIndex() == 2 || tabs->currentIndex() == 3) {
      synergy::gui::compact::placeActions(tabs->currentWidget()->findChild<QGroupBox *>(name), box);
    } else {
      if (auto *parent = box->parentWidget(); parent && parent->layout())
        parent->layout()->removeWidget(box);
      dialogLayout->addWidget(box);
      box->show();
    }
  };
  QObject::connect(tabWidget, &QTabWidget::currentChanged, dialog, placeActions);
  placeActions();

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

  groupComputerLayout->setTitle(
      QCoreApplication::translate("ServerConfigDialog", "Arrange computer displays by dragging")
  );
  synergy::gui::compact::setSubtleText(
      label_2, QCoreApplication::translate(
                   "ServerConfigDialog",
                   "Double-click an existing computer to edit its settings. Add computer provides an alternative to "
                   "dragging, including keyboard and screen-reader access.",
                   nullptr
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

  groupMisc->setTitle(QString());
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

  groupCorners->setTitle(QString());
  cbCornerTopLeft->setText(QCoreApplication::translate("ServerConfigDialog", "To&p-left", nullptr));
  cbCornerTopRight->setText(QCoreApplication::translate("ServerConfigDialog", "Top-rig&ht", nullptr));
  cbCornerBottomLeft->setText(QCoreApplication::translate("ServerConfigDialog", "&Bottom-left", nullptr));
  cbCornerBottomRight->setText(QCoreApplication::translate("ServerConfigDialog", "Bottom-ri&ght", nullptr));
  label->setText(QCoreApplication::translate("ServerConfigDialog", "Cor&ner size", nullptr));
  sbSwitchCornerSize->setSuffix(QCoreApplication::translate("ServerConfigDialog", "px", nullptr));

  groupNetwork->setTitle(QString());
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
  comboProtocol->setAccessibleName(labelProtocol->text());
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

  groupSwitch->setTitle(QString());
  cbSwitchDelay->setText(QCoreApplication::translate("ServerConfigDialog", "Switch &after waiting", nullptr));
  sbSwitchDelay->setSuffix(QCoreApplication::translate("ServerConfigDialog", "ms", nullptr));
  cbSwitchDoubleTap->setText(
      QCoreApplication::translate("ServerConfigDialog", "Switch on double &tap within", nullptr)
  );
  sbSwitchDoubleTap->setSuffix(QCoreApplication::translate("ServerConfigDialog", "ms", nullptr));
  for (auto *checkBox : {cbHeartbeat, cbSwitchDelay, cbSwitchDoubleTap})
    fitEnableCaption(checkBox);

  label_5->setText(QCoreApplication::translate("ServerConfigDialog", "Core server config file", nullptr));
  synergy::gui::compact::setSubtleText(
      label_6, QCoreApplication::translate(
                   "ServerConfigDialog",
                   "Use a server config file to create complex computer layouts that are not possible with the simple "
                   "grid-based computer layout editor.\n\nEnabling this setting will disable the server config GUI.",
                   nullptr
               )
  );
  groupExternalConfig->setTitle(QCoreApplication::translate("ServerConfigDialog", "Use a server config file", nullptr));
  cbExternalConfig->setText(groupExternalConfig->title());
  labelConfigFile->setText(QCoreApplication::translate("ServerConfigDialog", "Config file path", nullptr));
  btnBrowseConfigFile->setText(QString());
  synergy::gui::compact::normalizeCards(dialog);
  synergy::gui::compact::styleButtons(buttonBox);
}
