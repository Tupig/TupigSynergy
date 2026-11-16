/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ActionDialogDesign.h"

#include <QAbstractScrollArea>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

void Ui::ActionDialog::setupUi(QDialog *dialog)
{
  if (dialog->objectName().isEmpty())
    dialog->setObjectName(QStringLiteral("ActionDialog"));
  dialog->resize(600, 460);

  auto *mainLayout = new QVBoxLayout(dialog);
  mainLayout->setSpacing(8);
  mainLayout->setObjectName(QStringLiteral("_2"));
  mainLayout->setContentsMargins(10, 10, 10, 10);

  scrollArea = new QScrollArea(dialog);
  scrollArea->setObjectName(QStringLiteral("scrollArea"));
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  scrollAreaWidgetContents = new QWidget();
  scrollAreaWidgetContents->setObjectName(QStringLiteral("scrollAreaWidgetContents"));
  auto *pageLayout = new QVBoxLayout(scrollAreaWidgetContents);
  pageLayout->setSpacing(8);
  pageLayout->setObjectName(QStringLiteral("pageLayout"));
  pageLayout->setContentsMargins(0, 0, 0, 0);

  actionCard = new QGroupBox(scrollAreaWidgetContents);
  actionCard->setObjectName(QStringLiteral("actionCard"));
  auto *actionCardLayout = new QVBoxLayout(actionCard);
  actionCardLayout->setSpacing(8);
  actionCardLayout->setObjectName(QStringLiteral("actionCardLayout"));
  actionCardLayout->setContentsMargins(0, 0, 0, 0);

  auto *combosLayout = new QHBoxLayout();
  combosLayout->setSpacing(8);
  combosLayout->setObjectName(QStringLiteral("horizontalLayout_4"));
  combosLayout->setContentsMargins(0, 0, 0, 0);

  comboTriggerOn = new QComboBox(actionCard);
  comboTriggerOn->addItem(QString());
  comboTriggerOn->addItem(QString());
  comboTriggerOn->setObjectName(QStringLiteral("comboTriggerOn"));
  comboTriggerOn->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed));
  comboTriggerOn->setMaximumSize(QSize(240, 16777215));
  comboTriggerOn->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  combosLayout->addWidget(comboTriggerOn, 0, Qt::AlignLeft);

  comboActionType = new QComboBox(actionCard);
  comboActionType->addItem(QString());
  comboActionType->addItem(QString());
  comboActionType->addItem(QString());
  comboActionType->addItem(QString());
  comboActionType->addItem(QString());
  comboActionType->addItem(QString());
  comboActionType->addItem(QString());
  comboActionType->addItem(QString());
  comboActionType->setObjectName(QStringLiteral("comboActionType"));
  comboActionType->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed));
  comboActionType->setMaximumSize(QSize(240, 16777215));
  comboActionType->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  combosLayout->addWidget(comboActionType, 0, Qt::AlignLeft);

  actionCardLayout->addLayout(combosLayout);

  auto *optionsLayout = new QGridLayout();
  optionsLayout->setObjectName(QStringLiteral("horizontalLayout_2"));
  optionsLayout->setHorizontalSpacing(8);
  optionsLayout->setVerticalSpacing(8);
  optionsLayout->setContentsMargins(0, 0, 0, 0);

  comboSwitchToScreen = new QComboBox(actionCard);
  comboSwitchToScreen->setObjectName(QStringLiteral("comboSwitchToScreen"));
  comboSwitchToScreen->setEnabled(true);
  comboSwitchToScreen->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed));
  comboSwitchToScreen->setMaximumSize(QSize(240, 16777215));
  comboSwitchToScreen->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  optionsLayout->addWidget(comboSwitchToScreen, 0, 0, 1, 1, Qt::AlignLeft);

  comboSwitchInDirection = new QComboBox(actionCard);
  comboSwitchInDirection->addItem(QString());
  comboSwitchInDirection->addItem(QString());
  comboSwitchInDirection->addItem(QString());
  comboSwitchInDirection->addItem(QString());
  comboSwitchInDirection->setObjectName(QStringLiteral("comboSwitchInDirection"));
  comboSwitchInDirection->setEnabled(true);
  comboSwitchInDirection->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed));
  comboSwitchInDirection->setMaximumSize(QSize(240, 16777215));
  comboSwitchInDirection->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  optionsLayout->addWidget(comboSwitchInDirection, 0, 1, 1, 1, Qt::AlignLeft);

  comboLockCursorToScreen = new QComboBox(actionCard);
  comboLockCursorToScreen->addItem(QString());
  comboLockCursorToScreen->addItem(QString());
  comboLockCursorToScreen->addItem(QString());
  comboLockCursorToScreen->setObjectName(QStringLiteral("comboLockCursorToScreen"));
  comboLockCursorToScreen->setEnabled(true);
  comboLockCursorToScreen->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed));
  comboLockCursorToScreen->setMaximumSize(QSize(240, 16777215));
  comboLockCursorToScreen->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  optionsLayout->addWidget(comboLockCursorToScreen, 1, 0, 1, 1, Qt::AlignLeft);

  keySequenceWidget = new KeySequenceWidget(actionCard);
  keySequenceWidget->setObjectName(QStringLiteral("keySequenceWidget"));
  keySequenceWidget->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed));
  keySequenceWidget->setMaximumSize(QSize(240, 16777215));
  optionsLayout->addWidget(keySequenceWidget, 1, 1, 1, 1, Qt::AlignLeft);

  actionCardLayout->addLayout(optionsLayout);

  pageLayout->addWidget(actionCard);

  groupScreens = new QGroupBox(scrollAreaWidgetContents);
  groupScreens->setObjectName(QStringLiteral("groupScreens"));
  groupScreens->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
  auto *screensLayout = new QVBoxLayout(groupScreens);
  screensLayout->setSpacing(8);
  screensLayout->setObjectName(QStringLiteral("_3"));
  screensLayout->setContentsMargins(0, 0, 0, 0);

  listScreens = new QListWidget(groupScreens);
  listScreens->setObjectName(QStringLiteral("listScreens"));
  listScreens->setMinimumSize(QSize(0, 120));
  listScreens->setMaximumSize(QSize(16777215, 180));
  listScreens->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
  screensLayout->addWidget(listScreens);

  pageLayout->addWidget(groupScreens);

  scrollArea->setWidget(scrollAreaWidgetContents);
  mainLayout->addWidget(scrollArea);

  buttonBox = new QDialogButtonBox(dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  mainLayout->addWidget(buttonBox);

  retranslateUi(dialog);
}

void Ui::ActionDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(QCoreApplication::translate("ActionDialog", "Configure Action"));
  actionCard->setTitle(QString());

  comboTriggerOn->setItemText(0, QCoreApplication::translate("ActionDialog", "When the hotkey is pressed"));
  comboTriggerOn->setItemText(1, QCoreApplication::translate("ActionDialog", "When the hotkey is released"));

  comboActionType->setItemText(0, QCoreApplication::translate("ActionDialog", "Press key(s)"));
  comboActionType->setItemText(1, QCoreApplication::translate("ActionDialog", "Release key(s)"));
  comboActionType->setItemText(2, QCoreApplication::translate("ActionDialog", "Press and release key(s)"));
  comboActionType->setItemText(3, QCoreApplication::translate("ActionDialog", "Switch to a computer"));
  comboActionType->setItemText(4, QCoreApplication::translate("ActionDialog", "Switch in a direction"));
  comboActionType->setItemText(5, QCoreApplication::translate("ActionDialog", "Switch to next computer"));
  comboActionType->setItemText(6, QCoreApplication::translate("ActionDialog", "Modify the cursor lock"));
  comboActionType->setItemText(7, QCoreApplication::translate("ActionDialog", "Restart the server"));

  comboSwitchInDirection->setItemText(
      0, QCoreApplication::translate("ActionDialog", "Switch to the computer on the left")
  );
  comboSwitchInDirection->setItemText(
      1, QCoreApplication::translate("ActionDialog", "Switch to the computer on the right")
  );
  comboSwitchInDirection->setItemText(2, QCoreApplication::translate("ActionDialog", "Switch to the computer above"));
  comboSwitchInDirection->setItemText(3, QCoreApplication::translate("ActionDialog", "Switch to the computer below"));

  comboLockCursorToScreen->setItemText(0, QCoreApplication::translate("ActionDialog", "Toggle the cursor lock"));
  comboLockCursorToScreen->setItemText(1, QCoreApplication::translate("ActionDialog", "Enable the cursor lock"));
  comboLockCursorToScreen->setItemText(2, QCoreApplication::translate("ActionDialog", "Disable the cursor lock"));

  groupScreens->setTitle(QCoreApplication::translate("ActionDialog", "Computers to receive this event"));
}
