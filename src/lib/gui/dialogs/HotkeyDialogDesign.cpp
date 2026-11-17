/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "HotkeyDialogDesign.h"
#include "CompactLayout.h"

#include "gui/StyleUtils.h"
#include <QCoreApplication>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

void Ui::HotkeyDialog::setupUi(QDialog *dialog)
{
  if (dialog->objectName().isEmpty())
    dialog->setObjectName(QStringLiteral("HotkeyDialog"));
  dialog->resize(420, 180);

  auto *vboxLayout = new QVBoxLayout(dialog);
  vboxLayout->setSpacing(8);
  vboxLayout->setObjectName(QStringLiteral("vboxLayout"));
  vboxLayout->setContentsMargins(10, 10, 10, 10);

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

  hotkeyCard = new QGroupBox(scrollAreaWidgetContents);
  hotkeyCard->setObjectName(QStringLiteral("hotkeyCard"));
  hotkeyCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *hotkeyCardLayout = new QVBoxLayout(hotkeyCard);
  hotkeyCardLayout->setSpacing(8);
  hotkeyCardLayout->setObjectName(QStringLiteral("hotkeyCardLayout"));
  hotkeyCardLayout->setContentsMargins(12, 26, 12, 12);

  label = new QLabel(hotkeyCard);
  label->setObjectName(QStringLiteral("label"));
  label->setWordWrap(true);
  label->setProperty("muted", true);

  m_pKeySequenceWidgetHotkey = new KeySequenceWidget(hotkeyCard);
  m_pKeySequenceWidgetHotkey->setObjectName(QStringLiteral("m_pKeySequenceWidgetHotkey"));
  m_pKeySequenceWidgetHotkey->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
  auto *keysRow = new QHBoxLayout;
  keysRow->setSpacing(10);
  auto *keyLabel = new QLabel(hotkeyCard);
  keyLabel->setObjectName(QStringLiteral("hotkeyFieldLabel"));
  keyLabel->setFixedWidth(108);
  keyLabel->setBuddy(m_pKeySequenceWidgetHotkey);
  keysRow->addWidget(keyLabel);
  keysRow->addWidget(m_pKeySequenceWidgetHotkey, 1);
  hotkeyCardLayout->addLayout(keysRow);
  auto *recordRow = new QHBoxLayout;
  recordRow->setSpacing(8);
  recordRow->addWidget(m_pKeySequenceWidgetHotkey->createRecordButton(hotkeyCard));
  recordRow->addWidget(label, 1);
  hotkeyCardLayout->addLayout(recordRow);

  pageLayout->addWidget(hotkeyCard);

  actionsCard = new QGroupBox(scrollAreaWidgetContents);
  actionsCard->setObjectName(QStringLiteral("associatedActionsCard"));
  actionsCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *actionsLayout = new QVBoxLayout(actionsCard);
  actionsLayout->setContentsMargins(12, 10, 12, 12);
  actionsLayout->setSpacing(8);
  listAssociatedActions = new QListWidget(actionsCard);
  listAssociatedActions->setObjectName(QStringLiteral("listAssociatedActions"));
  actionsLayout->addWidget(listAssociatedActions);
  synergy::gui::compact::fitRows(
      listAssociatedActions, QCoreApplication::translate("HotkeyDialog", "No actions configured")
  );
  auto *actionButtons = new QHBoxLayout;
  actionButtons->setSpacing(8);
  btnAddAction = new QPushButton(actionsCard);
  btnAddAction->setObjectName(QStringLiteral("btnAddAction"));
  btnAddAction->setIcon(synergy::gui::appIcon(QStringLiteral("list-add")));
  btnConfigureAction = new QPushButton(actionsCard);
  btnConfigureAction->setObjectName(QStringLiteral("btnConfigureAction"));
  btnConfigureAction->setIcon(synergy::gui::appIcon(QStringLiteral("configure")));
  btnRemoveAction = new QPushButton(actionsCard);
  btnRemoveAction->setObjectName(QStringLiteral("btnRemoveAction"));
  btnRemoveAction->setIcon(synergy::gui::appIcon(QStringLiteral("user-trash")));
  btnRemoveAction->setProperty("danger", true);
  actionButtons->addWidget(btnAddAction);
  actionButtons->addWidget(btnConfigureAction);
  actionButtons->addWidget(btnRemoveAction);
  actionButtons->addStretch();
  actionsLayout->addLayout(actionButtons);
  pageLayout->addWidget(actionsCard);
  synergy::gui::compact::combineCards(
      scrollAreaWidgetContents, {hotkeyCard, actionsCard}, QStringLiteral("hotkeyEditorCard")
  );

  scrollArea->setWidget(scrollAreaWidgetContents);
  vboxLayout->addWidget(scrollArea);

  buttonBox = new QDialogButtonBox(dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  synergy::gui::compact::placeActions(
      scrollAreaWidgetContents->findChild<QGroupBox *>(QStringLiteral("hotkeyEditorCard")), buttonBox
  );

  retranslateUi(dialog);

  QObject::connect(buttonBox, &QDialogButtonBox::accepted, dialog, qOverload<>(&QDialog::accept));
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, dialog, qOverload<>(&QDialog::reject));
}

void Ui::HotkeyDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(QCoreApplication::translate("HotkeyDialog", "Hotkey"));
  hotkeyCard->setTitle(QCoreApplication::translate("HotkeyDialog", "Key combination"));
  dialog->findChild<QLabel *>(QStringLiteral("hotkeyFieldLabel"))
      ->setText(QCoreApplication::translate("HotkeyDialog", "Key combination"));
  label->setText(QCoreApplication::translate("HotkeyDialog", "Enter the specification for the hotkey:"));
  actionsCard->setTitle(QCoreApplication::translate("HotkeyDialog", "Associated actions"));
  btnAddAction->setText(QCoreApplication::translate("HotkeyDialog", "Add action"));
  btnConfigureAction->setText(QCoreApplication::translate("HotkeyDialog", "Configure action"));
  btnRemoveAction->setText(QCoreApplication::translate("HotkeyDialog", "Remove action"));
  synergy::gui::compact::normalizeCards(dialog);
  synergy::gui::compact::styleButtons(buttonBox);
}
