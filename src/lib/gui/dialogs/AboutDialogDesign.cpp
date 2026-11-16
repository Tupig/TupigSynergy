/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AboutDialogDesign.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QWidget>

void Ui::AboutDialog::setupUi(QDialog *dialog)
{
  if (dialog->objectName().isEmpty())
    dialog->setObjectName(QStringLiteral("AboutDialog"));
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->resize(560, 620);
  dialog->setWindowIcon(QIcon::fromTheme(QStringLiteral("help-about")));
  dialog->setModal(true);

  auto *verticalLayout = new QVBoxLayout(dialog);
  verticalLayout->setSpacing(8);
  verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
  verticalLayout->setContentsMargins(10, 10, 10, 10);

  scrollArea = new QScrollArea(dialog);
  scrollArea->setObjectName(QStringLiteral("scrollArea"));
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  scrollAreaWidgetContents = new QWidget();
  scrollAreaWidgetContents->setObjectName(QStringLiteral("scrollAreaWidgetContents"));
  auto *verticalLayout_4 = new QVBoxLayout(scrollAreaWidgetContents);
  verticalLayout_4->setSpacing(8);
  verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
  verticalLayout_4->setContentsMargins(0, 0, 0, 0);

  frameLogo = new QGroupBox(scrollAreaWidgetContents);
  frameLogo->setObjectName(QStringLiteral("frameLogo"));
  auto *gridLayout = new QVBoxLayout(frameLogo);
  gridLayout->setSpacing(8);
  gridLayout->setObjectName(QStringLiteral("gridLayout"));
  gridLayout->setContentsMargins(12, 12, 12, 12);

  lblIcon = new QLabel(frameLogo);
  lblIcon->setObjectName(QStringLiteral("lblIcon"));
  lblIcon->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
  lblIcon->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  gridLayout->addWidget(lblIcon);

  lblName = new QLabel(frameLogo);
  lblName->setObjectName(QStringLiteral("lblName"));
  lblName->setMinimumSize(QSize(0, 74));
  lblName->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  gridLayout->addWidget(lblName);

  lblDescription = new QLabel(frameLogo);
  lblDescription->setObjectName(QStringLiteral("lblDescription"));
  lblDescription->setText(QStringLiteral("lblDescription"));
  lblDescription->setWordWrap(true);
  gridLayout->addWidget(lblDescription);

  lblCopyright = new QLabel(frameLogo);
  lblCopyright->setObjectName(QStringLiteral("lblCopyright"));
  lblCopyright->setText(QStringLiteral("lblCopyright"));
  lblCopyright->setWordWrap(true);
  gridLayout->addWidget(lblCopyright);

  program_name = new QLabel(frameLogo);
  program_name->setObjectName(QStringLiteral("program_name"));
  program_name->setWordWrap(true);
  gridLayout->addWidget(program_name);

  openExternalLinks = new QLabel(frameLogo);
  openExternalLinks->setObjectName(QStringLiteral("openExternalLinks"));
  openExternalLinks->setOpenExternalLinks(true);
  openExternalLinks->setTextFormat(Qt::RichText);
  openExternalLinks->setWordWrap(true);
  gridLayout->addWidget(openExternalLinks);

  widget_2 = new QWidget(frameLogo);
  widget_2->setObjectName(QStringLiteral("widget_2"));
  auto *horizontalLayout = new QHBoxLayout(widget_2);
  horizontalLayout->setSpacing(8);
  horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
  horizontalLayout->setContentsMargins(0, 0, 0, 0);

  label = new QLabel(widget_2);
  label->setObjectName(QStringLiteral("label"));
  horizontalLayout->addWidget(label);

  lblVersion = new QLabel(widget_2);
  lblVersion->setObjectName(QStringLiteral("lblVersion"));
  lblVersion->setText(QStringLiteral("lblVersion"));
  horizontalLayout->addWidget(lblVersion);

  btnCopyVersion = new QPushButton(widget_2);
  btnCopyVersion->setObjectName(QStringLiteral("btnCopyVersion"));
  btnCopyVersion->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
  btnCopyVersion->setFlat(true);
  horizontalLayout->addWidget(btnCopyVersion);

  auto *horizontalSpacer = new QSpacerItem(8, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
  horizontalLayout->addItem(horizontalSpacer);

  gridLayout->addWidget(widget_2);

  verticalLayout_4->addWidget(frameLogo);

  groupBox = new QGroupBox(scrollAreaWidgetContents);
  groupBox->setObjectName(QStringLiteral("groupBox"));
  auto *verticalLayout_2 = new QVBoxLayout(groupBox);
  verticalLayout_2->setSpacing(8);
  verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
  verticalLayout_2->setContentsMargins(12, 12, 12, 12);

  lblImportantDevs = new QLabel(groupBox);
  lblImportantDevs->setObjectName(QStringLiteral("lblImportantDevs"));
  verticalLayout_2->addWidget(lblImportantDevs);

  linkContributors = new QLabel(groupBox);
  linkContributors->setObjectName(QStringLiteral("linkContributors"));
  linkContributors->setTextFormat(Qt::RichText);
  linkContributors->setWordWrap(true);
  verticalLayout_2->addWidget(linkContributors);

  verticalLayout_4->addWidget(groupBox);

  groupLicenses = new QGroupBox(scrollAreaWidgetContents);
  groupLicenses->setObjectName(QStringLiteral("groupLicenses"));
  auto *licensesLayout = new QVBoxLayout(groupLicenses);
  licensesLayout->setSpacing(8);
  licensesLayout->setObjectName(QStringLiteral("licensesLayout"));
  licensesLayout->setContentsMargins(12, 12, 12, 12);

  linkGpl = new QLabel(groupLicenses);
  linkGpl->setObjectName(QStringLiteral("linkGpl"));
  linkGpl->setTextFormat(Qt::RichText);
  linkGpl->setOpenExternalLinks(true);
  linkGpl->setWordWrap(true);
  licensesLayout->addWidget(linkGpl);

  verticalLayout_4->addWidget(groupLicenses);

  auto *verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
  verticalLayout_4->addItem(verticalSpacer);

  scrollArea->setWidget(scrollAreaWidgetContents);
  verticalLayout->addWidget(scrollArea);

  widget = new QWidget(dialog);
  widget->setObjectName(QStringLiteral("widget"));
  auto *verticalSpacerLayout = new QHBoxLayout(widget);
  verticalSpacerLayout->setObjectName(QStringLiteral("verticalSpacer_2"));
  verticalSpacerLayout->setContentsMargins(0, 0, 0, 0);

  auto *verticalSpacer_3 = new QSpacerItem(8, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
  verticalSpacerLayout->addItem(verticalSpacer_3);

  btnOk = new QPushButton(widget);
  btnOk->setObjectName(QStringLiteral("btnOk"));
  btnOk->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
  verticalSpacerLayout->addWidget(btnOk);

  verticalLayout->addWidget(widget);

  retranslateUi(dialog);
}

void Ui::AboutDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(QCoreApplication::translate("AboutDialog", "About %1"));
  frameLogo->setTitle(QString());
  label->setText(QCoreApplication::translate("AboutDialog", "Version:"));
  btnCopyVersion->setToolTip(QCoreApplication::translate("AboutDialog", "Copy version information"));
  groupBox->setTitle(QString());
  lblImportantDevs->setText(QCoreApplication::translate("AboutDialog", "Important Developers"));
  groupLicenses->setTitle(QString());
  btnOk->setText(QCoreApplication::translate("AboutDialog", "&Close"));
}
