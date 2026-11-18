/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientConfigDialogDesign.h"
#include "CompactLayout.h"

#include <QAbstractSpinBox>
#include <QCoreApplication>
#include <QFrame>

namespace {

} // namespace

void Ui::ClientConfigDialog::setupUi(QDialog *dialog)
{
  dialog->setObjectName(QStringLiteral("ClientConfigDialog"));
  dialog->resize(560, 360);

  verticalLayout = new QVBoxLayout(dialog);
  verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
  verticalLayout->setSpacing(8);
  verticalLayout->setContentsMargins(10, 10, 10, 10);

  scrollArea = new QScrollArea(dialog);
  scrollArea->setObjectName(QStringLiteral("scrollArea"));
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  scrollAreaWidgetContents = new QWidget();
  scrollAreaWidgetContents->setObjectName(QStringLiteral("scrollAreaWidgetContents"));

  pageLayout = new QVBoxLayout(scrollAreaWidgetContents);
  pageLayout->setObjectName(QStringLiteral("pageLayout"));
  pageLayout->setSpacing(8);
  pageLayout->setContentsMargins(0, 0, 0, 0);

  // 连接行为
  connectionBehaviorGroup = new QGroupBox(scrollAreaWidgetContents);
  connectionBehaviorGroup->setObjectName(QStringLiteral("connectionBehaviorGroup"));
  connectionBehaviorGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  connectionBehaviorLayout = new QVBoxLayout(connectionBehaviorGroup);
  connectionBehaviorLayout->setObjectName(QStringLiteral("connectionBehaviorLayout"));
  connectionBehaviorLayout->setSpacing(8);
  connectionBehaviorLayout->setContentsMargins(12, 26, 12, 12);

  cbDynamicConnectTime = new QCheckBox(connectionBehaviorGroup);
  cbDynamicConnectTime->setObjectName(QStringLiteral("cbDynamicConnectTime"));
  connectionBehaviorLayout->addWidget(cbDynamicConnectTime);

  cbLanguageSync = new QCheckBox(connectionBehaviorGroup);
  cbLanguageSync->setObjectName(QStringLiteral("cbLanguageSync"));
  connectionBehaviorLayout->addWidget(cbLanguageSync);
  auto *retryHint = new QLabel(connectionBehaviorGroup);
  retryHint->setObjectName(QStringLiteral("retryRangeHint"));
  retryHint->setProperty("muted", true);
  retryHint->setWordWrap(true);
  connectionBehaviorLayout->addWidget(retryHint);

  pageLayout->addWidget(connectionBehaviorGroup);

  // 滚动修饰符
  groupBox = new QGroupBox(scrollAreaWidgetContents);
  groupBox->setObjectName(QStringLiteral("groupBox"));
  groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

  gridLayout = new QFormLayout(groupBox);
  gridLayout->setObjectName(QStringLiteral("gridLayout"));
  gridLayout->setHorizontalSpacing(10);
  gridLayout->setVerticalSpacing(8);
  gridLayout->setContentsMargins(12, 26, 12, 12);
  gridLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
  gridLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  gridLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  gridLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

  // 水平滚动
  lblHScroll = new QLabel(groupBox);
  lblHScroll->setObjectName(QStringLiteral("lblHScroll"));
  lblHScroll->setMinimumSize(120, 0);
  lblHScroll->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  lblHScroll->setWordWrap(false);
  lblHScroll->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  gridLayout->setWidget(0, QFormLayout::LabelRole, lblHScroll);

  scrollHorizontalField = new QWidget(groupBox);
  scrollHorizontalField->setObjectName(QStringLiteral("scrollHorizontalField"));
  scrollHorizontalField->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

  horizontalLayout_3 = new QHBoxLayout(scrollHorizontalField);
  horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
  horizontalLayout_3->setSpacing(8);
  horizontalLayout_3->setContentsMargins(0, 0, 0, 0);

  cbXScrollInvert = new QCheckBox(scrollHorizontalField);
  cbXScrollInvert->setObjectName(QStringLiteral("cbXScrollInvert"));
  horizontalLayout_3->addWidget(cbXScrollInvert);

  lblXScrollSpeed = new QLabel(scrollHorizontalField);
  lblXScrollSpeed->setObjectName(QStringLiteral("lblXScrollSpeed"));
  lblXScrollSpeed->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  horizontalLayout_3->addWidget(lblXScrollSpeed);

  sbXScrollScale = new QDoubleSpinBox(scrollHorizontalField);
  sbXScrollScale->setObjectName(QStringLiteral("sbXScrollScale"));
  sbXScrollScale->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  sbXScrollScale->setAccelerated(true);
  sbXScrollScale->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
  sbXScrollScale->setMinimum(0.1);
  sbXScrollScale->setMaximum(10.0);
  sbXScrollScale->setSingleStep(0.1);
  sbXScrollScale->setValue(1.0);
  sbXScrollScale->setFixedWidth(88);
  horizontalLayout_3->addWidget(sbXScrollScale);

  gridLayout->setWidget(0, QFormLayout::FieldRole, scrollHorizontalField);

  // 垂直滚动
  lblVScroll = new QLabel(groupBox);
  lblVScroll->setObjectName(QStringLiteral("lblVScroll"));
  lblVScroll->setMinimumSize(120, 0);
  lblVScroll->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  lblVScroll->setWordWrap(false);
  lblVScroll->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  gridLayout->setWidget(1, QFormLayout::LabelRole, lblVScroll);

  scrollVerticalField = new QWidget(groupBox);
  scrollVerticalField->setObjectName(QStringLiteral("scrollVerticalField"));
  scrollVerticalField->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

  horizontalLayout_2 = new QHBoxLayout(scrollVerticalField);
  horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
  horizontalLayout_2->setSpacing(8);
  horizontalLayout_2->setContentsMargins(0, 0, 0, 0);

  cbYScrollInvert = new QCheckBox(scrollVerticalField);
  cbYScrollInvert->setObjectName(QStringLiteral("cbYScrollInvert"));
  horizontalLayout_2->addWidget(cbYScrollInvert);

  lblScrollSpeed = new QLabel(scrollVerticalField);
  lblScrollSpeed->setObjectName(QStringLiteral("lblScrollSpeed"));
  lblScrollSpeed->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  horizontalLayout_2->addWidget(lblScrollSpeed);

  sbYScrollScale = new QDoubleSpinBox(scrollVerticalField);
  sbYScrollScale->setObjectName(QStringLiteral("sbYScrollScale"));
  sbYScrollScale->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  sbYScrollScale->setAccelerated(true);
  sbYScrollScale->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
  sbYScrollScale->setMinimum(0.1);
  sbYScrollScale->setMaximum(10.0);
  sbYScrollScale->setSingleStep(0.1);
  sbYScrollScale->setValue(1.0);
  sbYScrollScale->setFixedWidth(88);
  horizontalLayout_2->addWidget(sbYScrollScale);

  gridLayout->setWidget(1, QFormLayout::FieldRole, scrollVerticalField);

  pageLayout->addWidget(groupBox);

  scrollArea->setWidget(scrollAreaWidgetContents);
  verticalLayout->addWidget(scrollArea);

  buttonBox = new QDialogButtonBox(dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(
      QDialogButtonBox::Cancel | QDialogButtonBox::Reset | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Save
  );
  verticalLayout->addWidget(buttonBox);

  retranslateUi(dialog);
}

void Ui::ClientConfigDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(QCoreApplication::translate("ClientConfigDialog", "Client Configuration"));
  connectionBehaviorGroup->setTitle(QCoreApplication::translate("ClientConfigDialog", "Connection Behaviour"));
  cbDynamicConnectTime->setText(
      QCoreApplication::translate("ClientConfigDialog", "Increase delay after failed connection attempts")
  );
  // 单行字面量：lupdate 才能把整段 tooltip 当作一个可翻译源串；
  // 禁止 clang-format 折行，否则提取出的源串会被截断。
  // clang-format off
  cbDynamicConnectTime->setToolTip(QCoreApplication::translate("ClientConfigDialog", "<html><head/><body><p>Allow the client to slow the rate it attempts to reconnect to the server when connections attempts are failing. The delay between connection attempts will  start at 1 second intervals and can be a maxium of 5 minutes between connection attempts.</p></body></html>"));
  // clang-format on
  cbLanguageSync->setText(QCoreApplication::translate("ClientConfigDialog", "Use the server keyboard language"));
  dialog->findChild<QLabel *>(QStringLiteral("retryRangeHint"))
      ->setText(
          QCoreApplication::translate(
              "ClientConfigDialog", "Retries start at 1 second and increase to a maximum of 5 minutes."
          )
      );
  groupBox->setTitle(QCoreApplication::translate("ClientConfigDialog", "Scroll Modifiers"));
  lblHScroll->setText(QCoreApplication::translate("ClientConfigDialog", "Horizontal Scroll"));
  cbXScrollInvert->setText(QCoreApplication::translate("ClientConfigDialog", "Invert"));
  lblXScrollSpeed->setText(QCoreApplication::translate("ClientConfigDialog", "Scale"));
  lblVScroll->setText(QCoreApplication::translate("ClientConfigDialog", "Vertical Scroll"));
  cbYScrollInvert->setText(QCoreApplication::translate("ClientConfigDialog", "Invert"));
  lblScrollSpeed->setText(QCoreApplication::translate("ClientConfigDialog", "Scale"));
  synergy::gui::compact::normalizeCards(dialog);
  synergy::gui::compact::styleButtons(buttonBox);
}
