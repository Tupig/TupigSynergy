/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSettingsDialog.h"
#include "ScreenSettingsDialogDesign.h"
#include "widgets/PageHost.h"

#include "gui/StyleUtils.h"
#include "gui/config/Screen.h"
#include "validators/AliasValidator.h"
#include "validators/ScreenNameValidator.h"
#include "validators/ValidationError.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QMessageBox>

using enum synergy::gui::ScreenConfig::Modifier;
using enum synergy::gui::ScreenConfig::SwitchCorner;
using enum synergy::gui::ScreenConfig::Fix;

ScreenSettingsDialog::~ScreenSettingsDialog() = default;

ScreenSettingsDialog::ScreenSettingsDialog(
    QWidget *parent, synergy::gui::Screen *screen, const synergy::gui::ScreenList *screens
)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ScreenSettingsDialog>()},
      m_screen(screen)
{

  ui->setupUi(this);

  for (auto *form : findChildren<QFormLayout *>()) {
    int labelWidth = 120;
    for (int row = 0; row < form->rowCount(); ++row) {
      if (auto *item = form->itemAt(row, QFormLayout::LabelRole)) {
        if (auto *label = qobject_cast<QLabel *>(item->widget()))
          labelWidth = qMax(labelWidth, label->fontMetrics().horizontalAdvance(label->text()));
      }
    }
    for (int row = 0; row < form->rowCount(); ++row) {
      if (auto *item = form->itemAt(row, QFormLayout::LabelRole)) {
        if (auto *label = qobject_cast<QLabel *>(item->widget())) {
          label->setMaximumWidth(QWIDGETSIZE_MAX);
          label->setMinimumWidth(labelWidth);
          label->setWordWrap(false);
        }
      }
    }
  }
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setFocus();

  // 设计稿：四个死角复选框显示方位文字，宽度按文字取最小。
  const QList<QPair<QCheckBox *, QString>> corners{
      {ui->chkDeadTopLeft, tr("Top-left")},
      {ui->chkDeadTopRight, tr("Top-right")},
      {ui->chkDeadBottomLeft, tr("Bottom-left")},
      {ui->chkDeadBottomRight, tr("Bottom-right")},
  };
  for (const auto &[checkBox, text] : corners) {
    checkBox->setText(text);
    checkBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    checkBox->setMinimumWidth(checkBox->fontMetrics().horizontalAdvance(text) + 32);
  }

  ui->lineNameEdit->setText(m_screen->name());

  const auto valNameError = new validators::ValidationError(this, ui->lblNameError);
  const auto valName = new validators::ScreenNameValidator(ui->lineNameEdit, valNameError, screens);
  ui->lineNameEdit->setValidator(valName);

  const auto valAliasError = new validators::ValidationError(this, ui->lblAliasError);
  const auto valAlias = new validators::AliasValidator(ui->lineAddAlias, valAliasError);
  ui->lineAddAlias->setValidator(valAlias);

  // An empty validation message is not a blank form row. Keep the validator's
  // actual text and save rules, showing the row only when there is an error.
  const auto showNameError = [this] { ui->lblNameError->setVisible(!ui->lblNameError->text().isEmpty()); };
  const auto showAliasError = [this] { ui->lblAliasError->setVisible(!ui->lblAliasError->text().isEmpty()); };
  connect(ui->lineNameEdit, &QLineEdit::textChanged, this, showNameError);
  connect(ui->lineAddAlias, &QLineEdit::textChanged, this, showAliasError);
  showNameError();
  showAliasError();

  for (int i = 0; i < m_screen->aliases().count(); i++)
    new QListWidgetItem(
        synergy::gui::appIcon(QStringLiteral("video-display")), m_screen->aliases()[i], ui->listAliases
    );

  const QList<QComboBox *> modifiers{ui->comboShift, ui->comboCtrl,  ui->comboAlt,
                                     ui->comboMeta,  ui->comboSuper, ui->comboAltGr};
  for (int source = 0; source < modifiers.size(); ++source) {
    auto *field = modifiers[source];
    const int value = m_screen->modifier(source);
    if (field->findData(value) < 0) {
      field->addItem(tr("Unsupported modifier (%1) — read-only").arg(value), value);
      m_readOnly = true;
    }
    field->setCurrentIndex(field->findData(value));
  }
  if (m_readOnly) {
    for (auto *field : modifiers)
      field->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
  }

  ui->chkDeadTopLeft->setChecked(m_screen->switchCorner(static_cast<int>(TopLeft)));
  ui->chkDeadTopRight->setChecked(m_screen->switchCorner(static_cast<int>(TopRight)));
  ui->chkDeadBottomLeft->setChecked(m_screen->switchCorner(static_cast<int>(BottomLeft)));
  ui->chkDeadBottomRight->setChecked(m_screen->switchCorner(static_cast<int>(BottomRight)));
  ui->sbSwitchCornerSize->setValue(m_screen->switchCornerSize());

  ui->chkFixCapsLock->setChecked(m_screen->fix(CapsLock));
  ui->chkFixNumLock->setChecked(m_screen->fix(NumLock));
  ui->chkFixScrollLock->setChecked(m_screen->fix(ScrollLock));
  ui->chkFixXTest->setChecked(m_screen->fix(XTest));

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ScreenSettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ScreenSettingsDialog::reject);
  connect(ui->btnAddAlias, &QPushButton::clicked, this, &ScreenSettingsDialog::addAlias);
  connect(ui->btnRemoveAlias, &QPushButton::clicked, this, &ScreenSettingsDialog::removeAlias);
  connect(ui->lineAddAlias, &QLineEdit::textChanged, this, &ScreenSettingsDialog::checkNewAliasName);
  connect(ui->listAliases, &QListWidget::itemSelectionChanged, this, &ScreenSettingsDialog::aliasSelected);
}

void ScreenSettingsDialog::accept()
{
  if (m_readOnly)
    return;
  if (ui->lineNameEdit->text().isEmpty()) {
    PageHost::present(
        this, new QMessageBox(
                  QMessageBox::Warning, tr("Screen name is empty"),
                  tr("The screen name cannot be empty. "
                     "Please either fill in a name or cancel the dialog."),
                  QMessageBox::Ok, this
              )
    );
    return;
  }
  if (!ui->lblNameError->text().isEmpty()) {
    return;
  }

  QStringList aliases;
  for (int i = 0; i < ui->listAliases->count(); i++) {
    QString alias(ui->listAliases->item(i)->text());
    if (alias == ui->lineNameEdit->text()) {
      PageHost::present(
          this, new QMessageBox(
                    QMessageBox::Warning, tr("Screen name matches alias"),
                    tr("The screen name cannot be the same as an alias. "
                       "Please either remove the alias or change the screen name."),
                    QMessageBox::Ok, this
                )
      );
      return;
    }
    aliases.append(alias);
  }
  // 全部校验通过再一次性提交；删除别名和反复保存不会留下旧项或重复项。
  m_screen->setName(ui->lineNameEdit->text());
  m_screen->aliases() = aliases;

  m_screen->setModifier(Shift, ui->comboShift->currentData().toInt());
  m_screen->setModifier(Ctrl, ui->comboCtrl->currentData().toInt());
  m_screen->setModifier(Alt, ui->comboAlt->currentData().toInt());
  m_screen->setModifier(Meta, ui->comboMeta->currentData().toInt());
  m_screen->setModifier(Super, ui->comboSuper->currentData().toInt());
  m_screen->setModifier(AltGr, ui->comboAltGr->currentData().toInt());

  m_screen->setSwitchCorner(TopLeft, ui->chkDeadTopLeft->isChecked());
  m_screen->setSwitchCorner(TopRight, ui->chkDeadTopRight->isChecked());
  m_screen->setSwitchCorner(BottomLeft, ui->chkDeadBottomLeft->isChecked());
  m_screen->setSwitchCorner(BottomRight, ui->chkDeadBottomRight->isChecked());
  m_screen->setSwitchCornerSize(ui->sbSwitchCornerSize->value());

  m_screen->setFix(CapsLock, ui->chkFixCapsLock->isChecked());
  m_screen->setFix(NumLock, ui->chkFixNumLock->isChecked());
  m_screen->setFix(ScrollLock, ui->chkFixScrollLock->isChecked());
  m_screen->setFix(XTest, ui->chkFixXTest->isChecked());

  QDialog::accept();
}

void ScreenSettingsDialog::addAlias()
{
  if (!ui->lineAddAlias->text().isEmpty() &&
      ui->listAliases->findItems(ui->lineAddAlias->text(), Qt::MatchFixedString).isEmpty()) {
    new QListWidgetItem(
        synergy::gui::appIcon(QStringLiteral("video-display")), ui->lineAddAlias->text(), ui->listAliases
    );
    ui->lineAddAlias->clear();
  }
}

void ScreenSettingsDialog::removeAlias() const
{
  QList<QListWidgetItem *> items = ui->listAliases->selectedItems();
  qDeleteAll(items);
}

void ScreenSettingsDialog::checkNewAliasName(const QString &text)
{
  ui->btnAddAlias->setEnabled(!text.isEmpty() && ui->lblAliasError->text().isEmpty());
}

void ScreenSettingsDialog::aliasSelected()
{
  ui->btnRemoveAlias->setEnabled(!ui->listAliases->selectedItems().isEmpty());
}
