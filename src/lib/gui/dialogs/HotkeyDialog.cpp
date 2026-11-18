/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "HotkeyDialog.h"
#include "ActionDialog.h"
#include "HotkeyDialogDesign.h"
#include "common/I18N.h"
#include "gui/ActionPresentation.h"
#include "widgets/PageHost.h"
#include <QCoreApplication>
#include <QPushButton>
#include <utility>

HotkeyDialog::HotkeyDialog(QWidget *parent, Hotkey &hotkey, const synergy::gui::ServerConfig *config)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::HotkeyDialog>()},
      m_Hotkey(hotkey),
      m_draft(hotkey),
      m_serverConfig(config)
{
  ui->setupUi(this);

  ui->m_pKeySequenceWidgetHotkey->setKeySequence(m_Hotkey.keySequence());
  const auto refresh = [this] {
    const auto *keys = sequenceWidget();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(keys->valid() && !keys->isRecording());
    ui->label->setText(
        keys->isRecording() ? tr("Press a key combination. Escape restores the previous hotkey.")
                            : tr("Click the field to record a keyboard or mouse shortcut.")
    );
  };
  connect(ui->m_pKeySequenceWidgetHotkey, &KeySequenceWidget::recordingChanged, this, refresh);
  connect(ui->m_pKeySequenceWidgetHotkey, &KeySequenceWidget::keySequenceChanged, this, refresh);
  refresh();
  connect(I18N::instance(), &I18N::languageChanged, this, [this, refresh] {
    ui->retranslateUi(this);
    refresh();
    for (int row = 0; row < ui->listAssociatedActions->count() && row < m_draft.actions().size(); ++row)
      synergy::gui::presentAction(ui->listAssociatedActions->item(row), m_draft.actions().at(row));
  });
  connect(ui->btnAddAction, &QPushButton::clicked, this, [this] { configureAction(true); });
  connect(ui->btnConfigureAction, &QPushButton::clicked, this, [this] { configureAction(false); });
  connect(ui->listAssociatedActions, &QListWidget::itemDoubleClicked, this, [this] { configureAction(false); });
  connect(ui->listAssociatedActions, &QListWidget::currentRowChanged, this, [this] {
    const bool selected = ui->listAssociatedActions->currentRow() >= 0;
    ui->btnConfigureAction->setEnabled(selected && m_serverConfig);
    ui->btnRemoveAction->setEnabled(selected && m_serverConfig);
  });
  connect(ui->btnRemoveAction, &QPushButton::clicked, this, [this] {
    const int row = ui->listAssociatedActions->currentRow();
    if (row < 0 || row >= m_draft.actions().size())
      return;
    m_draft.actions().removeAt(row);
    refreshActions();
  });
  refreshActions();
}

HotkeyDialog::~HotkeyDialog() = default;

void HotkeyDialog::accept()
{
  if (!sequenceWidget()->valid() || sequenceWidget()->isRecording())
    return;

  m_draft.setKeySequence(sequenceWidget()->keySequence());
  m_Hotkey = m_draft;
  QDialog::accept();
}

void HotkeyDialog::refreshActions()
{
  const int previous = ui->listAssociatedActions->currentRow();
  ui->listAssociatedActions->clear();
  for (const auto &action : std::as_const(m_draft).actions()) {
    auto *item = new QListWidgetItem(ui->listAssociatedActions);
    synergy::gui::presentAction(item, action);
  }
  ui->listAssociatedActions->setCurrentRow(qMin(qMax(0, previous), ui->listAssociatedActions->count() - 1));
  ui->btnAddAction->setEnabled(m_serverConfig);
  const bool selected = ui->listAssociatedActions->currentRow() >= 0;
  ui->btnConfigureAction->setEnabled(selected && m_serverConfig);
  ui->btnRemoveAction->setEnabled(selected && m_serverConfig);
}

void HotkeyDialog::configureAction(bool create)
{
  if (!m_serverConfig || sequenceWidget()->isRecording())
    return;
  const int row = ui->listAssociatedActions->currentRow();
  if (!create && (row < 0 || row >= m_draft.actions().size()))
    return;
  auto action = std::make_shared<Action>(create ? Action() : m_draft.actions().at(row));
  auto *editor = new ActionDialog(this, *m_serverConfig, m_draft, *action);
  connect(editor, &QDialog::accepted, this, [this, action, create, row] {
    if (create)
      m_draft.actions().append(*action);
    else if (row >= 0 && row < m_draft.actions().size())
      m_draft.actions()[row] = *action;
    refreshActions();
  });
  PageHost::present(this, editor);
}

const KeySequenceWidget *HotkeyDialog::sequenceWidget() const
{
  return ui->m_pKeySequenceWidgetHotkey;
}
