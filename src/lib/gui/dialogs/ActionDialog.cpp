/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ActionDialog.h"
#include "ActionDialogDesign.h"

#include "Action.h"
#include "Hotkey.h"
#include "KeySequence.h"
#include "common/I18N.h"
#include "config/ServerConfig.h"
#include "gui/StyleUtils.h"

#include <QComboBox>
#include <QLayout>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

ActionDialog::ActionDialog(QWidget *parent, const synergy::gui::ServerConfig &config, Hotkey &hotkey, Action &action)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ActionDialog>()},
      m_hotkey(hotkey),
      m_action(action)
{
  ui->setupUi(this);
  connect(ui->keySequenceWidget, &KeySequenceWidget::keySequenceChanged, this, &ActionDialog::keySequenceChanged);
  connect(
      ui->comboActionType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionDialog::actionTypeChanged
  );
  connect(ui->keySequenceWidget, &KeySequenceWidget::recordingChanged, this, &ActionDialog::keySequenceChanged);
  connect(ui->comboRecipients, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
    actionTypeChanged(ui->comboActionType->currentIndex());
  });
  connect(ui->listScreens, &QListWidget::itemChanged, this, &ActionDialog::itemToggled);
  for (auto *field : {ui->comboSwitchToScreen, ui->comboSwitchInDirection, ui->comboLockCursorToScreen})
    connect(field, qOverload<int>(&QComboBox::currentIndexChanged), this, &ActionDialog::itemToggled);
  connect(ui->comboSwitchToScreen, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
    ui->comboSwitchToScreen->setToolTip(ui->comboSwitchToScreen->currentText());
  });
  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ActionDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ActionDialog::reject);

  ui->keySequenceWidget->setKeySequence(m_action.keySequence());

  ui->comboSwitchInDirection->setCurrentIndex(ui->comboSwitchInDirection->findData(m_action.switchDirection()));
  ui->comboLockCursorToScreen->setCurrentIndex(ui->comboLockCursorToScreen->findData(m_action.lockCursorMode()));

  const int typeRow = ui->comboActionType->findData(m_action.type());
  m_readOnly = typeRow < 0;
  if (m_readOnly)
    ui->comboActionType->addItem(tr("Unsupported action (%1) — read-only").arg(m_action.type()), m_action.type());
  ui->comboActionType->setCurrentIndex(ui->comboActionType->findData(m_action.type()));
  auto *readOnlyNotice = new QLabel(ui->actionCard);
  readOnlyNotice->setObjectName(QStringLiteral("unsupportedActionNotice"));
  readOnlyNotice->setTextFormat(Qt::PlainText);
  readOnlyNotice->setWordWrap(true);
  readOnlyNotice->setText(tr("Unsupported action (%1) — read-only").arg(m_action.type()));
  readOnlyNotice->setVisible(m_readOnly);
  static_cast<QVBoxLayout *>(ui->actionCard->layout())->insertWidget(1, readOnlyNotice);
  ui->comboTriggerOn->setCurrentIndex(ui->comboTriggerOn->findData(m_action.activeOnRelease()));

  for (const synergy::gui::Screen &screen : config.screens()) {
    if (screen.isNull())
      continue;
    auto *newListItem = new QListWidgetItem(screen.name());
    newListItem->setIcon(synergy::gui::appIcon(QStringLiteral("video-display")));
    newListItem->setCheckState(Qt::Checked);
    if ((m_action.typeScreenNames().indexOf(screen.name()) == -1) &&
        (m_action.haveScreens() && !m_action.typeScreenNames().isEmpty()))
      newListItem->setCheckState(Qt::Unchecked);

    ui->listScreens->addItem(newListItem);

    ui->comboSwitchToScreen->addItem(screen.name(), screen.name());
    ui->comboSwitchToScreen->setItemData(ui->comboSwitchToScreen->count() - 1, true, Qt::UserRole + 1);
    if (screen.name() == m_action.switchScreenName())
      ui->comboSwitchToScreen->setCurrentIndex(ui->comboSwitchToScreen->count() - 1);
  }
  if (ui->comboSwitchToScreen->findData(m_action.switchScreenName()) < 0 &&
      (!m_action.switchScreenName().isEmpty() || m_action.type() == ActionTypes::SwitchTo)) {
    const auto label = m_action.switchScreenName().isEmpty() ? tr("Choose a computer")
                                                             : tr("%1 (unavailable)").arg(m_action.switchScreenName());
    ui->comboSwitchToScreen->addItem(label, m_action.switchScreenName());
    const int row = ui->comboSwitchToScreen->count() - 1;
    ui->comboSwitchToScreen->setItemData(row, false, Qt::UserRole + 1);
    ui->comboSwitchToScreen->setCurrentIndex(row);
    ui->comboSwitchToScreen->setToolTip(label);
  }

  ui->keySequenceWidget->setVisible(false);
  ui->groupScreens->setVisible(false);
  ui->listScreens->setEnabled(!ui->keySequenceWidget->keySequence().isMouseButton());
  ui->comboSwitchToScreen->setVisible(false);
  ui->comboSwitchInDirection->setVisible(false);
  ui->comboLockCursorToScreen->setVisible(false);

  ui->comboRecipients->setCurrentIndex(m_action.haveScreens() && !m_action.typeScreenNames().isEmpty() ? 1 : 0);
  actionTypeChanged(ui->comboActionType->currentIndex());
  if (m_readOnly) {
    for (auto *control : findChildren<QComboBox *>())
      control->setEnabled(false);
    ui->keySequenceWidget->setEnabled(false);
    ui->listScreens->setEnabled(false);
  }
  connect(I18N::instance(), &I18N::languageChanged, this, [this, readOnlyNotice] {
    const QSignalBlocker type(ui->comboActionType), trigger(ui->comboTriggerOn), recipients(ui->comboRecipients),
        target(ui->comboSwitchToScreen), direction(ui->comboSwitchInDirection), lock(ui->comboLockCursorToScreen);
    ui->retranslateUi(this);
    if (m_readOnly)
      ui->comboActionType->setItemText(
          ui->comboActionType->currentIndex(), tr("Unsupported action (%1) — read-only").arg(m_action.type())
      );
    readOnlyNotice->setText(tr("Unsupported action (%1) — read-only").arg(m_action.type()));
    for (int row = 0; row < ui->comboSwitchToScreen->count(); ++row) {
      if (ui->comboSwitchToScreen->itemData(row, Qt::UserRole + 1).toBool())
        continue;
      const auto name = ui->comboSwitchToScreen->itemData(row).toString();
      ui->comboSwitchToScreen->setItemText(
          row, name.isEmpty() ? tr("Choose a computer") : tr("%1 (unavailable)").arg(name)
      );
    }
    ui->comboSwitchToScreen->setToolTip(ui->comboSwitchToScreen->currentText());
  });
}

void ActionDialog::accept()
{
  if (!canSave())
    return;

  const int type = ui->comboActionType->currentData().toInt();
  if (isKeyAction(type))
    m_action.setKeySequence(ui->keySequenceWidget->keySequence());
  m_action.setType(type);

  if (isKeyAction(type)) {
    m_action.typeScreenNames().clear();

    int screenCount = ui->listScreens->count();

    for (int i = 0; i < ui->listScreens->count(); i++) {
      const auto &item = ui->listScreens->item(i);
      m_action.typeScreenNames().append(item->text());
      if (ui->comboRecipients->currentIndex() == 1 && item->checkState() == Qt::Unchecked) {
        screenCount--;
        m_action.typeScreenNames().removeLast();
      }
    }

    if (ui->comboRecipients->currentIndex() == 0)
      m_action.typeScreenNames().clear();

    m_action.setHaveScreens(ui->comboRecipients->currentData().toInt() == 1 && screenCount > 0);
  }

  if (type == ActionTypes::SwitchTo)
    m_action.setSwitchScreenName(ui->comboSwitchToScreen->currentData().toString());
  if (type == ActionTypes::SwitchInDirection)
    m_action.setSwitchDirection(ui->comboSwitchInDirection->currentData().toInt());
  if (type == ActionTypes::ModifyCursorLock)
    m_action.setLockCursorMode(ui->comboLockCursorToScreen->currentData().toInt());
  m_action.setActiveOnRelease(ui->comboTriggerOn->currentData().toBool());
  m_action.setRestartServer(type == ActionTypes::RestartServer);

  QDialog::accept();
}

void ActionDialog::updateSize()
{
  layout()->invalidate();
  updateGeometry();
  if (isWindow())
    adjustSize();
}

void ActionDialog::keySequenceChanged()
{
  ui->comboRecipients->setEnabled(!ui->keySequenceWidget->keySequence().isMouseButton());
  ui->listScreens->setEnabled(!ui->keySequenceWidget->keySequence().isMouseButton());
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(canSave());
}

void ActionDialog::itemToggled() const
{
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(canSave());
}

void ActionDialog::actionTypeChanged(int index)
{
  Q_UNUSED(index);
  index = ui->comboActionType->currentData().toInt();
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(canSave());
  ui->keySequenceWidget->setVisible(isKeyAction(index));
  ui->keySequenceField->setVisible(isKeyAction(index));
  ui->labelKeys->setVisible(isKeyAction(index));
  ui->labelTarget->setVisible(index == ActionTypes::SwitchTo);
  ui->labelDirection->setVisible(index == ActionTypes::SwitchInDirection);
  ui->labelCursor->setVisible(index == ActionTypes::ModifyCursorLock);
  ui->listScreens->setVisible(ui->comboRecipients->currentIndex() == 1);
  ui->comboRecipients->setEnabled(!ui->keySequenceWidget->keySequence().isMouseButton());
  ui->groupScreens->setVisible(isKeyAction(index));
  ui->listScreens->setEnabled(!ui->keySequenceWidget->keySequence().isMouseButton());
  ui->comboSwitchToScreen->setVisible(index == ActionTypes::SwitchTo);
  ui->comboSwitchInDirection->setVisible(index == ActionTypes::SwitchInDirection);
  ui->comboLockCursorToScreen->setVisible(index == ActionTypes::ModifyCursorLock);
  QTimer::singleShot(1, this, &ActionDialog::updateSize);
}

bool ActionDialog::isKeyAction(int index) const
{
  return ((index == ActionTypes::PressKey) || (index == ActionTypes::ReleaseKey) || (index == ActionTypes::ToggleKey));
}

bool ActionDialog::canSave() const
{
  if (m_readOnly || ui->comboActionType->currentIndex() < 0 || ui->keySequenceWidget->isRecording())
    return false;
  const int type = ui->comboActionType->currentData().toInt();
  if (type == ActionTypes::SwitchInDirection)
    return ui->comboSwitchInDirection->currentIndex() >= 0;
  if (type == ActionTypes::ModifyCursorLock)
    return ui->comboLockCursorToScreen->currentIndex() >= 0;
  if (type == ActionTypes::SwitchTo)
    return ui->comboSwitchToScreen->currentIndex() >= 0 &&
           ui->comboSwitchToScreen->currentData(Qt::UserRole + 1).toBool();
  if (isKeyAction(type)) {
    const QList<QListWidgetItem *> items = ui->listScreens->findItems("*", Qt::MatchWildcard);
    int totalChecked = 0;
    for (const auto &item : items) {
      if (item->checkState() == Qt::Checked)
        totalChecked++;
    }
    if (ui->comboRecipients->currentIndex() == 0)
      totalChecked = ui->listScreens->count();
    return (ui->keySequenceWidget->valid() && totalChecked > 0);
  }
  return true;
}

ActionDialog::~ActionDialog() = default;
