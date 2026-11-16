/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "HotkeyDialogDesign.h"

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
  auto *hotkeyCardLayout = new QVBoxLayout(hotkeyCard);
  hotkeyCardLayout->setSpacing(8);
  hotkeyCardLayout->setObjectName(QStringLiteral("hotkeyCardLayout"));
  hotkeyCardLayout->setContentsMargins(0, 0, 0, 0);

  label = new QLabel(hotkeyCard);
  label->setObjectName(QStringLiteral("label"));
  label->setWordWrap(false);
  hotkeyCardLayout->addWidget(label);

  m_pKeySequenceWidgetHotkey = new KeySequenceWidget(hotkeyCard);
  m_pKeySequenceWidgetHotkey->setObjectName(QStringLiteral("m_pKeySequenceWidgetHotkey"));
  m_pKeySequenceWidgetHotkey->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
  m_pKeySequenceWidgetHotkey->setMaximumSize(QSize(260, 16777215));
  hotkeyCardLayout->addWidget(m_pKeySequenceWidgetHotkey);

  pageLayout->addWidget(hotkeyCard);

  scrollArea->setWidget(scrollAreaWidgetContents);
  vboxLayout->addWidget(scrollArea);

  buttonBox = new QDialogButtonBox(dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  vboxLayout->addWidget(buttonBox);

  retranslateUi(dialog);

  QObject::connect(buttonBox, &QDialogButtonBox::accepted, dialog, qOverload<>(&QDialog::accept));
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, dialog, qOverload<>(&QDialog::reject));
}

void Ui::HotkeyDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(QCoreApplication::translate("HotkeyDialog", "Hotkey"));
  hotkeyCard->setTitle(QString());
  label->setText(QCoreApplication::translate("HotkeyDialog", "Enter the specification for the hotkey:"));
}
