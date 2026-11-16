/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSettingsDialogDesign.h"

#include <QCoreApplication>
#include <QFrame>
#include <QStringList>

namespace {

QString translate(const char *text)
{
  return QCoreApplication::translate("ScreenSettingsDialog", text);
}

void configureFormLayout(QFormLayout *form)
{
  form->setHorizontalSpacing(10);
  form->setVerticalSpacing(8);
  form->setContentsMargins(0, 0, 0, 0);
  form->setRowWrapPolicy(QFormLayout::DontWrapRows);
  form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
}

QLabel *createFormLabel(QWidget *parent)
{
  auto *label = new QLabel(parent);
  label->setMinimumSize(120, 0);
  label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  label->setWordWrap(false);
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  return label;
}

QComboBox *createModifierCombo(QWidget *parent)
{
  auto *combo = new QComboBox(parent);
  combo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  combo->setMaximumSize(140, 16777215);
  combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  for (int i = 0; i < 7; ++i) {
    combo->addItem(QString());
  }
  return combo;
}

} // namespace

void Ui::ScreenSettingsDialog::setupUi(QDialog *dialog)
{
  dialog->setObjectName(QStringLiteral("ScreenSettingsDialog"));
  dialog->resize(640, 640);

  _2 = new QVBoxLayout(dialog);
  _2->setObjectName(QStringLiteral("_2"));
  _2->setSpacing(8);
  _2->setContentsMargins(10, 10, 10, 10);

  scrollArea = new QScrollArea(dialog);
  scrollArea->setObjectName(QStringLiteral("scrollArea"));
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  scrollAreaWidgetContents = new QWidget();
  scrollAreaWidgetContents->setObjectName(QStringLiteral("scrollAreaWidgetContents"));

  layoutOptions = new QHBoxLayout(scrollAreaWidgetContents);
  layoutOptions->setObjectName(QStringLiteral("layoutOptions"));
  layoutOptions->setSpacing(8);
  layoutOptions->setContentsMargins(0, 0, 0, 0);

  // 左列：计算机信息 + 修饰键
  layoutLeftSide = new QVBoxLayout;
  layoutLeftSide->setObjectName(QStringLiteral("layoutLeftSide"));
  layoutLeftSide->setSpacing(8);
  layoutLeftSide->setContentsMargins(0, 0, 0, 0);

  groupComputerName = new QGroupBox(scrollAreaWidgetContents);
  groupComputerName->setObjectName(QStringLiteral("groupComputerName"));
  groupComputerName->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

  _3 = new QVBoxLayout(groupComputerName);
  _3->setObjectName(QStringLiteral("_3"));
  _3->setSpacing(8);
  _3->setContentsMargins(0, 0, 0, 0);

  _4 = new QFormLayout;
  _4->setObjectName(QStringLiteral("_4"));
  configureFormLayout(_4);

  label_7 = createFormLabel(groupComputerName);
  label_7->setObjectName(QStringLiteral("label_7"));
  _4->setWidget(0, QFormLayout::LabelRole, label_7);

  lineNameEdit = new QLineEdit(groupComputerName);
  lineNameEdit->setObjectName(QStringLiteral("lineNameEdit"));
  lineNameEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  lineNameEdit->setMaximumSize(180, 16777215);
  lineNameEdit->setMaxLength(255);
  _4->setWidget(0, QFormLayout::FieldRole, lineNameEdit);
  label_7->setBuddy(lineNameEdit);

  _3->addLayout(_4);

  lblNameError = new QLabel(groupComputerName);
  lblNameError->setObjectName(QStringLiteral("lblNameError"));
  lblNameError->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  lblNameError->setText(QStringLiteral("lblNameError"));
  lblNameError->setWordWrap(true);
  lblNameError->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  _3->addWidget(lblNameError);

  layoutLeftSide->addWidget(groupComputerName);

  groupModifers = new QGroupBox(scrollAreaWidgetContents);
  groupModifers->setObjectName(QStringLiteral("groupModifers"));

  formLayout_2 = new QFormLayout(groupModifers);
  formLayout_2->setObjectName(QStringLiteral("formLayout_2"));
  configureFormLayout(formLayout_2);

  label_2 = createFormLabel(groupModifers);
  label_2->setObjectName(QStringLiteral("label_2"));
  comboShift = createModifierCombo(groupModifers);
  comboShift->setObjectName(QStringLiteral("comboShift"));
  formLayout_2->setWidget(0, QFormLayout::LabelRole, label_2);
  formLayout_2->setWidget(0, QFormLayout::FieldRole, comboShift);
  label_2->setBuddy(comboShift);

  label_3 = createFormLabel(groupModifers);
  label_3->setObjectName(QStringLiteral("label_3"));
  comboCtrl = createModifierCombo(groupModifers);
  comboCtrl->setObjectName(QStringLiteral("comboCtrl"));
  comboCtrl->setCurrentIndex(1);
  formLayout_2->setWidget(1, QFormLayout::LabelRole, label_3);
  formLayout_2->setWidget(1, QFormLayout::FieldRole, comboCtrl);
  label_3->setBuddy(comboCtrl);

  label_4 = createFormLabel(groupModifers);
  label_4->setObjectName(QStringLiteral("label_4"));
  comboAlt = createModifierCombo(groupModifers);
  comboAlt->setObjectName(QStringLiteral("comboAlt"));
  comboAlt->setCurrentIndex(2);
  formLayout_2->setWidget(2, QFormLayout::LabelRole, label_4);
  formLayout_2->setWidget(2, QFormLayout::FieldRole, comboAlt);
  label_4->setBuddy(comboAlt);

  label_5 = createFormLabel(groupModifers);
  label_5->setObjectName(QStringLiteral("label_5"));
  comboMeta = createModifierCombo(groupModifers);
  comboMeta->setObjectName(QStringLiteral("comboMeta"));
  comboMeta->setCurrentIndex(3);
  formLayout_2->setWidget(3, QFormLayout::LabelRole, label_5);
  formLayout_2->setWidget(3, QFormLayout::FieldRole, comboMeta);
  label_5->setBuddy(comboMeta);

  label_6 = createFormLabel(groupModifers);
  label_6->setObjectName(QStringLiteral("label_6"));
  comboSuper = createModifierCombo(groupModifers);
  comboSuper->setObjectName(QStringLiteral("comboSuper"));
  comboSuper->setCurrentIndex(4);
  formLayout_2->setWidget(4, QFormLayout::LabelRole, label_6);
  formLayout_2->setWidget(4, QFormLayout::FieldRole, comboSuper);
  label_6->setBuddy(comboSuper);

  labelAltGr = createFormLabel(groupModifers);
  labelAltGr->setObjectName(QStringLiteral("labelAltGr"));
  comboAltGr = createModifierCombo(groupModifers);
  comboAltGr->setObjectName(QStringLiteral("comboAltGr"));
  comboAltGr->setCurrentIndex(5);
  formLayout_2->setWidget(5, QFormLayout::LabelRole, labelAltGr);
  formLayout_2->setWidget(5, QFormLayout::FieldRole, comboAltGr);
  labelAltGr->setBuddy(comboAltGr);

  layoutLeftSide->addWidget(groupModifers);

  layoutOptions->addLayout(layoutLeftSide);

  // 右列：死角 + 修正 + 别名
  layoutRightSide = new QVBoxLayout;
  layoutRightSide->setObjectName(QStringLiteral("layoutRightSide"));
  layoutRightSide->setSpacing(8);
  layoutRightSide->setContentsMargins(0, 0, 0, 0);

  groupDeadCorners = new QGroupBox(scrollAreaWidgetContents);
  groupDeadCorners->setObjectName(QStringLiteral("groupDeadCorners"));

  verticalLayout_2 = new QVBoxLayout(groupDeadCorners);
  verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
  verticalLayout_2->setSpacing(8);
  verticalLayout_2->setContentsMargins(0, 0, 0, 0);

  layoutDeadCornersTop = new QHBoxLayout;
  layoutDeadCornersTop->setObjectName(QStringLiteral("layoutDeadCornersTop"));
  layoutDeadCornersTop->setSpacing(8);
  layoutDeadCornersTop->setContentsMargins(0, 0, 0, 0);

  chkDeadTopLeft = new QCheckBox(groupDeadCorners);
  chkDeadTopLeft->setObjectName(QStringLiteral("chkDeadTopLeft"));
  chkDeadTopLeft->setAutoRepeatInterval(100);
  layoutDeadCornersTop->addWidget(chkDeadTopLeft);

  chkDeadTopRight = new QCheckBox(groupDeadCorners);
  chkDeadTopRight->setObjectName(QStringLiteral("chkDeadTopRight"));
  chkDeadTopRight->setLayoutDirection(Qt::RightToLeft);
  layoutDeadCornersTop->addWidget(chkDeadTopRight);

  verticalLayout_2->addLayout(layoutDeadCornersTop);

  layoutCornerSize = new QFormLayout;
  layoutCornerSize->setObjectName(QStringLiteral("layoutCornerSize"));
  configureFormLayout(layoutCornerSize);

  label = createFormLabel(groupDeadCorners);
  label->setObjectName(QStringLiteral("label"));
  layoutCornerSize->setWidget(0, QFormLayout::LabelRole, label);

  sbSwitchCornerSize = new QSpinBox(groupDeadCorners);
  sbSwitchCornerSize->setObjectName(QStringLiteral("sbSwitchCornerSize"));
  sbSwitchCornerSize->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  sbSwitchCornerSize->setMinimumSize(70, 0);
  sbSwitchCornerSize->setMaximumSize(90, 16777215);
  layoutCornerSize->setWidget(0, QFormLayout::FieldRole, sbSwitchCornerSize);
  label->setBuddy(sbSwitchCornerSize);

  verticalLayout_2->addLayout(layoutCornerSize);

  layoutDeadCornersBottom = new QHBoxLayout;
  layoutDeadCornersBottom->setObjectName(QStringLiteral("layoutDeadCornersBottom"));
  layoutDeadCornersBottom->setSpacing(8);
  layoutDeadCornersBottom->setContentsMargins(0, 0, 0, 0);

  chkDeadBottomLeft = new QCheckBox(groupDeadCorners);
  chkDeadBottomLeft->setObjectName(QStringLiteral("chkDeadBottomLeft"));
  layoutDeadCornersBottom->addWidget(chkDeadBottomLeft);

  chkDeadBottomRight = new QCheckBox(groupDeadCorners);
  chkDeadBottomRight->setObjectName(QStringLiteral("chkDeadBottomRight"));
  chkDeadBottomRight->setLayoutDirection(Qt::RightToLeft);
  layoutDeadCornersBottom->addWidget(chkDeadBottomRight);

  verticalLayout_2->addLayout(layoutDeadCornersBottom);

  layoutRightSide->addWidget(groupDeadCorners);

  groupFixes = new QGroupBox(scrollAreaWidgetContents);
  groupFixes->setObjectName(QStringLiteral("groupFixes"));

  verticalLayout_3 = new QVBoxLayout(groupFixes);
  verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
  verticalLayout_3->setSpacing(8);
  verticalLayout_3->setContentsMargins(0, 0, 0, 0);

  chkFixScrollLock = new QCheckBox(groupFixes);
  chkFixScrollLock->setObjectName(QStringLiteral("chkFixScrollLock"));
  chkFixScrollLock->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  verticalLayout_3->addWidget(chkFixScrollLock);

  chkFixCapsLock = new QCheckBox(groupFixes);
  chkFixCapsLock->setObjectName(QStringLiteral("chkFixCapsLock"));
  chkFixCapsLock->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  verticalLayout_3->addWidget(chkFixCapsLock);

  chkFixNumLock = new QCheckBox(groupFixes);
  chkFixNumLock->setObjectName(QStringLiteral("chkFixNumLock"));
  chkFixNumLock->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  verticalLayout_3->addWidget(chkFixNumLock);

  chkFixXTest = new QCheckBox(groupFixes);
  chkFixXTest->setObjectName(QStringLiteral("chkFixXTest"));
  chkFixXTest->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  chkFixXTest->setChecked(false);
  verticalLayout_3->addWidget(chkFixXTest);

  layoutRightSide->addWidget(groupFixes);

  groupAliases = new QGroupBox(scrollAreaWidgetContents);
  groupAliases->setObjectName(QStringLiteral("groupAliases"));
  groupAliases->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  verticalLayout = new QVBoxLayout(groupAliases);
  verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
  verticalLayout->setSpacing(8);
  verticalLayout->setContentsMargins(0, 0, 0, 0);

  horizontalLayout = new QHBoxLayout;
  horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
  horizontalLayout->setSpacing(8);
  horizontalLayout->setContentsMargins(0, 0, 0, 0);

  lineAddAlias = new QLineEdit(groupAliases);
  lineAddAlias->setObjectName(QStringLiteral("lineAddAlias"));
  lineAddAlias->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  lineAddAlias->setMinimumSize(120, 0);
  lineAddAlias->setMaxLength(255);
  horizontalLayout->addWidget(lineAddAlias);

  btnAddAlias = new QPushButton(groupAliases);
  btnAddAlias->setObjectName(QStringLiteral("btnAddAlias"));
  btnAddAlias->setEnabled(true);
  horizontalLayout->addWidget(btnAddAlias);

  verticalLayout->addLayout(horizontalLayout);

  lblAliasError = new QLabel(groupAliases);
  lblAliasError->setObjectName(QStringLiteral("lblAliasError"));
  lblAliasError->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
  lblAliasError->setText(QStringLiteral("lblAliasError"));
  lblAliasError->setWordWrap(true);
  lblAliasError->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  verticalLayout->addWidget(lblAliasError);

  listAliases = new QListWidget(groupAliases);
  listAliases->setObjectName(QStringLiteral("listAliases"));
  listAliases->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  listAliases->setMinimumSize(120, 72);
  listAliases->setMaximumSize(16777215, 100);
  listAliases->setSelectionMode(QAbstractItemView::ExtendedSelection);
  verticalLayout->addWidget(listAliases);

  btnRemoveAlias = new QPushButton(groupAliases);
  btnRemoveAlias->setObjectName(QStringLiteral("btnRemoveAlias"));
  btnRemoveAlias->setEnabled(false);
  btnRemoveAlias->setMaximumSize(140, 16777215);
  verticalLayout->addWidget(btnRemoveAlias, 0, Qt::AlignLeft);

  layoutRightSide->addWidget(groupAliases);

  layoutOptions->addLayout(layoutRightSide);

  scrollArea->setWidget(scrollAreaWidgetContents);
  _2->addWidget(scrollArea);

  buttonBox = new QDialogButtonBox(dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
  _2->addWidget(buttonBox);

  QWidget::setTabOrder(lineNameEdit, comboMeta);
  QWidget::setTabOrder(comboMeta, comboShift);
  QWidget::setTabOrder(comboShift, comboCtrl);
  QWidget::setTabOrder(comboCtrl, comboSuper);
  QWidget::setTabOrder(comboSuper, comboAltGr);
  QWidget::setTabOrder(comboAltGr, comboAlt);
  QWidget::setTabOrder(comboAlt, chkDeadTopLeft);
  QWidget::setTabOrder(chkDeadTopLeft, chkDeadTopRight);
  QWidget::setTabOrder(chkDeadTopRight, sbSwitchCornerSize);
  QWidget::setTabOrder(sbSwitchCornerSize, chkDeadBottomLeft);
  QWidget::setTabOrder(chkDeadBottomLeft, chkDeadBottomRight);
  QWidget::setTabOrder(chkDeadBottomRight, chkFixScrollLock);
  QWidget::setTabOrder(chkFixScrollLock, chkFixCapsLock);
  QWidget::setTabOrder(chkFixCapsLock, chkFixNumLock);
  QWidget::setTabOrder(chkFixNumLock, chkFixXTest);
  QWidget::setTabOrder(chkFixXTest, lineAddAlias);
  QWidget::setTabOrder(lineAddAlias, btnAddAlias);
  QWidget::setTabOrder(btnAddAlias, listAliases);
  QWidget::setTabOrder(listAliases, btnRemoveAlias);

  retranslateUi(dialog);
}

void Ui::ScreenSettingsDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(translate("Computer settings"));

  groupComputerName->setTitle(translate("Computer Info"));
  label_7->setText(translate("Name"));

  groupModifers->setTitle(translate("Modifier Keys"));
  label_2->setText(translate("&Shift"));
  label_3->setText(translate("&Ctrl"));
  label_4->setText(translate("Al&t"));
  label_5->setText(translate("M&eta"));
  label_6->setText(translate("S&uper"));
  labelAltGr->setText(translate("Alt &Gr"));

  const QStringList modifiers{
      translate("Shift"), translate("Ctrl"),   translate("Alt"),  translate("Meta"),
      translate("Super"), translate("Alt Gr"), translate("None"),
  };
  for (auto *combo : {comboShift, comboCtrl, comboAlt, comboMeta, comboSuper, comboAltGr}) {
    for (int i = 0; i < modifiers.size(); ++i) {
      combo->setItemText(i, modifiers[i]);
    }
  }

  groupDeadCorners->setTitle(translate("Dead Corners"));
  chkDeadTopLeft->setToolTip(translate("Top Left"));
  chkDeadTopRight->setToolTip(translate("Top Right"));
  label->setText(translate("Corner si&ze"));
  chkDeadBottomLeft->setToolTip(translate("Bottom Left"));
  chkDeadBottomRight->setToolTip(translate("Bottom Right"));

  groupFixes->setTitle(translate("Fixes"));
  chkFixScrollLock->setText(translate("SCROLL LOCK key"));
  chkFixCapsLock->setText(translate("CAPS LOCK key"));
  chkFixNumLock->setText(translate("NUM LOCK key"));
  chkFixXTest->setText(translate("XTest for Xinerama"));

  groupAliases->setTitle(translate("Aliases"));
  btnAddAlias->setText(translate("&Add"));
  btnRemoveAlias->setText(translate("&Remove"));
}
