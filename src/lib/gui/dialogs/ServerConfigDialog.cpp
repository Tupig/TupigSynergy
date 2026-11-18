/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialog.h"
#include "ServerConfigDialogDesign.h"

#include "common/Constants.h"
#include "common/I18N.h"
#include "common/NetworkProtocol.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "dialogs/ActionDialog.h"
#include "dialogs/HotkeyDialog.h"
#include "dialogs/ScreenSettingsDialog.h"
#include "gui/ActionPresentation.h"
#include "gui/StyleUtils.h"
#include "widgets/PageHost.h"

#include <QApplication>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

using enum synergy::gui::ScreenConfig::SwitchCorner;

namespace {
// One visible Add control retains both click and the existing drag payload.
class AddComputerButton final : public QPushButton
{
public:
  AddComputerButton(const QString &text, NewScreenWidget *source, QWidget *parent)
      : QPushButton(text, parent),
        m_source(source)
  {
  }

protected:
  void mousePressEvent(QMouseEvent *event) override
  {
    m_origin = event->pos();
    m_dragged = false;
    QPushButton::mousePressEvent(event);
  }
  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (!m_dragged && m_source && (event->buttons() & Qt::LeftButton) &&
        (event->pos() - m_origin).manhattanLength() >= QApplication::startDragDistance()) {
      m_dragged = true;
      setDown(false);
      m_source->startDrag(this);
      return;
    }
    QPushButton::mouseMoveEvent(event);
  }
  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (m_dragged)
      event->accept();
    else
      QPushButton::mouseReleaseEvent(event);
  }

private:
  QPointer<NewScreenWidget> m_source;
  QPoint m_origin;
  bool m_dragged = false;
};

class TrashDropProxy final : public QObject
{
public:
  TrashDropProxy(QPushButton *button, TrashScreenWidget *target) : QObject(button), m_target(target)
  {
    button->setAcceptDrops(true);
    button->installEventFilter(this);
  }

protected:
  bool eventFilter(QObject *object, QEvent *event) override
  {
    if (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove && event->type() != QEvent::Drop)
      return QObject::eventFilter(object, event);
    // The existing selected-screen guard also protects the visible drop target.
    if (!static_cast<QWidget *>(object)->isEnabled() || !m_target) {
      event->ignore();
      return true;
    }
    if (event->type() == QEvent::DragMove) {
      auto *move = static_cast<QDragMoveEvent *>(event);
      if (move->mimeData()->hasFormat(ScreenSetupModel::mimeType())) {
        move->setDropAction(Qt::MoveAction);
        move->accept();
      } else {
        move->ignore();
      }
    } else {
      QCoreApplication::sendEvent(m_target, event);
    }
    return true;
  }

private:
  QPointer<TrashScreenWidget> m_target;
};
} // namespace

ServerConfigDialog::ServerConfigDialog(QWidget *parent, synergy::gui::ServerConfig &config)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ServerConfigDialog>()},
      m_originalServerConfig(config),
      m_originalServerConfigIsExternal(config.useExternalConfig()),
      m_originalServerConfigUsesExternalFile(config.configFile()),
      m_serverConfig(config),
      m_screenSetupModel(m_serverConfig.screens(), m_serverConfig.numColumns(), m_serverConfig.numRows())
{
  ui->setupUi(this);
  connect(ui->cbExternalConfig, &QCheckBox::toggled, ui->groupExternalConfig, &QGroupBox::setChecked);
  connect(ui->groupExternalConfig, &QGroupBox::toggled, ui->cbExternalConfig, &QCheckBox::setChecked);

  ui->labelProtocol->setWhatsThis(ui->labelProtocol->whatsThis().arg(kAppName));

  m_originalProtocol = networkProtocolFromValue(Settings::value(Settings::Server::Protocol));
  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ServerConfigDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ServerConfigDialog::reject);

  ui->lblRemoveScreen->setPixmap(synergy::gui::appIcon("user-trash").pixmap(QSize(24, 24)));
  connect(ui->lblRemoveScreen, &TrashScreenWidget::screenRemoved, this, &ServerConfigDialog::onScreenRemoved);

  ui->lblNewScreen->setEnabled(!model().isFull());
  auto *addComputerButton = new AddComputerButton(tr("Add computer"), ui->lblNewScreen, ui->tabComputers);
  addComputerButton->setObjectName(QStringLiteral("addComputerButton"));
  addComputerButton->setIcon(synergy::gui::appIcon(QStringLiteral("list-add")));
  addComputerButton->setIconSize(QSize(15, 15));
  addComputerButton->setAutoDefault(false);
  addComputerButton->setDefault(false);
  addComputerButton->setEnabled(!model().isFull());
  connect(addComputerButton, &QPushButton::clicked, this, [this] { addClient(); });
  connect(&m_screenSetupModel, &ScreenSetupModel::screensChanged, addComputerButton, [this, addComputerButton] {
    addComputerButton->setEnabled(!model().isFull());
  });
  if (auto *tools = ui->groupComputerLayout->findChild<QHBoxLayout *>(QStringLiteral("layoutComputers")))
    tools->insertWidget(0, addComputerButton);
  ui->lblNewScreen->setPixmap(synergy::gui::appIcon("video-display").pixmap(QSize(24, 24)));

  // 死角复选框按标签宽度收缩，避免网格拉伸后只剩指示框。
  for (auto *corner : {ui->cbCornerTopLeft, ui->cbCornerTopRight, ui->cbCornerBottomLeft, ui->cbCornerBottomRight}) {
    corner->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    corner->setMinimumWidth(corner->fontMetrics().horizontalAdvance(corner->text()) + 32);
  }
  // 网格随页签可用高度伸缩；为说明、拖拽工具和底部操作保留空间。
  ui->screenSetupView->setFixedHeight(180);
  ui->lblRemoveScreen->setFixedSize(32, 32);
  ui->lblRemoveScreen->setScaledContents(true);
  ui->lblNewScreen->setFixedSize(32, 32);
  ui->lblNewScreen->setScaledContents(true);

  connect(ui->btnNewHotkey, &QPushButton::clicked, this, &ServerConfigDialog::addHotkey);
  connect(ui->btnEditHotkey, &QPushButton::clicked, this, &ServerConfigDialog::editHotkey);
  connect(ui->btnRemoveHotkey, &QPushButton::clicked, this, &ServerConfigDialog::removeHotkey);
  connect(ui->listHotkeys, &QListView::doubleClicked, this, &ServerConfigDialog::editHotkey);
  connect(
      ui->listHotkeys->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      &ServerConfigDialog::listHotkeysSelectionChanged
  );

  connect(ui->btnNewAction, &QPushButton::clicked, this, &ServerConfigDialog::addAction);
  connect(ui->btnEditAction, &QPushButton::clicked, this, &ServerConfigDialog::editAction);
  connect(ui->btnRemoveAction, &QPushButton::clicked, this, &ServerConfigDialog::removeAction);
  connect(ui->listActions, &QListView::doubleClicked, this, &ServerConfigDialog::editAction);
  connect(
      ui->listActions->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      &ServerConfigDialog::listActionsSelectionChanged
  );

  // force the first tab, since qt creator sets the active tab as the last one
  // the developer was looking at, and it's easy to accidentally save that.
  ui->tabWidget->setCurrentIndex(0);

  ui->btnBrowseConfigFile->setIcon(synergy::gui::appIcon(QStringLiteral("document-open")));
  ui->lineConfigFile->setText(serverConfig().configFile());

  ui->rbProtocolSynergy->setChecked(serverConfig().protocol() == NetworkProtocol::Synergy);
  ui->rbProtocolBarrier->setChecked(serverConfig().protocol() == NetworkProtocol::Barrier);
  ui->comboProtocol->setCurrentIndex(ui->comboProtocol->findData(QVariant::fromValue(serverConfig().protocol())));
  connect(ui->rbProtocolBarrier, &QRadioButton::toggled, this, &ServerConfigDialog::toggleProtocol);
  connect(ui->comboProtocol, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
    if (ui->comboProtocol->currentIndex() < 0)
      return;
    const auto protocol = ui->comboProtocol->currentData().value<NetworkProtocol>();
    if (protocol == NetworkProtocol::Synergy)
      ui->rbProtocolSynergy->setChecked(true);
    else if (protocol == NetworkProtocol::Barrier)
      ui->rbProtocolBarrier->setChecked(true);
  });
  connect(ui->rbProtocolBarrier, &QRadioButton::toggled, this, [this](bool barrier) {
    const QSignalBlocker blocker(ui->comboProtocol);
    ui->comboProtocol->setCurrentIndex(
        ui->comboProtocol->findData(QVariant::fromValue(barrier ? NetworkProtocol::Barrier : NetworkProtocol::Synergy))
    );
  });

  ui->cbHeartbeat->setChecked(serverConfig().hasHeartbeat());
  connect(ui->cbHeartbeat, &QCheckBox::toggled, this, &ServerConfigDialog::toggleHeartbeat);

  ui->sbHeartbeat->setEnabled(ui->cbHeartbeat->isChecked());
  ui->sbHeartbeat->setValue(serverConfig().heartbeat());
  connect(ui->sbHeartbeat, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setHeartbeat);

  ui->cbRelativeMouseMoves->setChecked(serverConfig().relativeMouseMoves());

  if (!synergy::platform::isWindows())
    ui->cbWin32KeepForeground->setVisible(false);

  ui->cbWin32KeepForeground->setChecked(serverConfig().win32KeepForeground());
  connect(ui->cbWin32KeepForeground, &QCheckBox::toggled, this, &ServerConfigDialog::toggleWin32Foreground);

  ui->cbSwitchDelay->setChecked(serverConfig().hasSwitchDelay());
  connect(ui->cbSwitchDelay, &QCheckBox::toggled, this, &ServerConfigDialog::toggleSwitchDelay);

  ui->sbSwitchDelay->setEnabled(ui->cbSwitchDelay->isChecked());
  ui->sbSwitchDelay->setValue(serverConfig().switchDelay());
  connect(ui->sbSwitchDelay, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setSwitchDelay);

  ui->cbSwitchDoubleTap->setChecked(serverConfig().hasSwitchDoubleTap());
  connect(ui->cbSwitchDoubleTap, &QCheckBox::toggled, this, &ServerConfigDialog::toggleSwitchDoubleTap);

  ui->sbSwitchDoubleTap->setEnabled(ui->cbSwitchDoubleTap->isChecked());
  ui->sbSwitchDoubleTap->setValue(serverConfig().switchDoubleTap());
  connect(
      ui->sbSwitchDoubleTap, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setSwitchDoubleTap
  );

  connect(ui->cbRelativeMouseMoves, &QCheckBox::toggled, this, &ServerConfigDialog::toggleRelativeMouseMoves);
  connect(ui->cbEnableClipboard, &QCheckBox::toggled, this, &ServerConfigDialog::toggleClipboard);

  connect(ui->btnBrowseConfigFile, &QPushButton::clicked, this, &ServerConfigDialog::browseConfigFile);

  ui->groupExternalConfig->setChecked(serverConfig().useExternalConfig());
  ui->widgetExternalConfigControls->setEnabled(ui->groupExternalConfig->isChecked());
  ui->tabWidget->setTabEnabled(0, !ui->groupExternalConfig->isChecked());
  ui->tabWidget->setTabEnabled(1, !ui->groupExternalConfig->isChecked());
  ui->tabWidget->setTabEnabled(2, !ui->groupExternalConfig->isChecked());
  connect(ui->groupExternalConfig, &QGroupBox::toggled, this, &ServerConfigDialog::toggleExternalConfig);

  connect(
      ui->sbSwitchCornerSize, QOverload<int>::of(&QSpinBox::valueChanged), this,
      &ServerConfigDialog::setSwitchCornerSize
  );
  connect(
      ui->sbClipboardSizeLimit, QOverload<int>::of(&QSpinBox::valueChanged), this,
      &ServerConfigDialog::setClipboardLimit
  );

  ui->cbCornerTopLeft->setChecked(serverConfig().switchCorner(static_cast<int>(TopLeft)));
  connect(ui->cbCornerTopLeft, &QCheckBox::toggled, this, &ServerConfigDialog::toggleCornerTopLeft);

  ui->cbCornerTopRight->setChecked(serverConfig().switchCorner(static_cast<int>(TopRight)));
  connect(ui->cbCornerTopRight, &QCheckBox::toggled, this, &ServerConfigDialog::toggleCornerTopRight);

  ui->cbCornerBottomLeft->setChecked(serverConfig().switchCorner(static_cast<int>(BottomLeft)));
  connect(ui->cbCornerBottomLeft, &QCheckBox::toggled, this, &ServerConfigDialog::toggleCornerBottomLeft);

  ui->cbCornerBottomRight->setChecked(serverConfig().switchCorner(static_cast<int>(BottomRight)));
  connect(ui->cbCornerBottomRight, &QCheckBox::toggled, this, &ServerConfigDialog::toggleCornerBottomRight);

  ui->sbSwitchCornerSize->setValue(serverConfig().switchCornerSize());

  ui->cbDefaultLockToScreenState->setChecked(serverConfig().defaultLockToScreenState());
  connect(
      ui->cbDefaultLockToScreenState, &QCheckBox::toggled, this, &ServerConfigDialog::toggleDefaultLockToScreenState
  );

  ui->cbDisableLockToScreen->setChecked(serverConfig().disableLockToScreen());
  connect(ui->cbDisableLockToScreen, &QCheckBox::toggled, this, &ServerConfigDialog::toggleLockToScreen);

  ui->cbEnableClipboard->setChecked(serverConfig().clipboardSharing());
  auto clipboardSharingSizeM = static_cast<int>(serverConfig().clipboardSharingSize() / 1024);
  ui->sbClipboardSizeLimit->setValue(clipboardSharingSizeM);
  ui->sbClipboardSizeLimit->setEnabled(serverConfig().clipboardSharing());

  for (const Hotkey &hotkey : std::as_const(serverConfig().hotkeys()))
    synergy::gui::appendHotkey(ui->listHotkeys, hotkey);

  ui->screenSetupView->setModel(&m_screenSetupModel);

  auto *deleteComputer = new QPushButton(tr("Delete selected"), ui->groupComputerLayout);
  deleteComputer->setObjectName(QStringLiteral("deleteSelectedComputerButton"));
  deleteComputer->setIcon(synergy::gui::appIcon(QStringLiteral("user-trash")));
  deleteComputer->setProperty("danger", true);
  deleteComputer->setIconSize(QSize(15, 15));
  deleteComputer->setAutoDefault(false);
  deleteComputer->setDefault(false);
  deleteComputer->setEnabled(false);
  new TrashDropProxy(deleteComputer, ui->lblRemoveScreen);
  ui->lblNewScreen->hide();
  ui->lblRemoveScreen->hide();
  if (auto *tools = ui->groupComputerLayout->findChild<QHBoxLayout *>(QStringLiteral("layoutComputers")))
    tools->insertWidget(1, deleteComputer);
  const auto updateDelete = [this, deleteComputer] {
    const auto selected = ui->screenSetupView->currentIndex();
    const bool removable = selected.isValid() && !model().screen(selected).isNull() &&
                           !model().screen(selected).isPrimary() && !serverConfig().useExternalConfig();
    deleteComputer->setEnabled(removable);
  };
  connect(ui->screenSetupView->selectionModel(), &QItemSelectionModel::currentChanged, this, updateDelete);
  connect(&m_screenSetupModel, &ScreenSetupModel::screensChanged, this, updateDelete);
  connect(ui->groupExternalConfig, &QGroupBox::toggled, this, updateDelete);
  connect(deleteComputer, &QPushButton::clicked, this, [this, updateDelete] {
    const auto selected = ui->screenSetupView->currentIndex();
    if (!selected.isValid() || model().screen(selected).isNull() || model().screen(selected).isPrimary() ||
        serverConfig().useExternalConfig())
      return;
    model().screen(selected) = synergy::gui::Screen();
    Q_EMIT m_screenSetupModel.dataChanged(selected, selected);
    Q_EMIT m_screenSetupModel.screensChanged();
    onScreenRemoved();
    updateDelete();
  });

  auto &screens = serverConfig().screens();
  auto server = std::ranges::find_if(screens, [this](const synergy::gui::Screen &screen) {
    return (screen.name() == serverConfig().getServerName());
  });

  if (server == screens.end()) {
    synergy::gui::Screen serverScreen(serverConfig().getServerName());
    serverScreen.markAsPrimary();
    model().screen(serverConfig().numColumns() / 2, serverConfig().numRows() / 2) = serverScreen;
  } else {
    server->markAsPrimary();
  }

  onChange();

  // computers
  connect(&m_screenSetupModel, &ScreenSetupModel::screensChanged, this, &ServerConfigDialog::onChange);
  connect(I18N::instance(), &I18N::languageChanged, this, [this, addComputerButton, deleteComputer] {
    ui->retranslateUi(this);
    ui->labelProtocol->setWhatsThis(ui->labelProtocol->whatsThis().arg(kAppName));
    addComputerButton->setText(tr("Add computer"));
    deleteComputer->setText(tr("Delete selected"));
    for (int row = 0; row < ui->listHotkeys->count() && row < serverConfig().hotkeys().size(); ++row)
      synergy::gui::presentHotkey(ui->listHotkeys->item(row), serverConfig().hotkeys().at(row));
    const int hotkeyRow = ui->listHotkeys->currentRow();
    if (hotkeyRow >= 0 && hotkeyRow < serverConfig().hotkeys().size()) {
      const auto &actions = std::as_const(serverConfig()).hotkeys().at(hotkeyRow).actions();
      for (int row = 0; row < ui->listActions->count() && row < actions.size(); ++row)
        synergy::gui::presentAction(ui->listActions->item(row), actions.at(row));
    }
  });
}

ServerConfigDialog::~ServerConfigDialog() = default;

bool ServerConfigDialog::addClient(const QString &clientName)
{
  return addComputer(clientName, true);
}

void ServerConfigDialog::accept()
{
  if (ui->groupExternalConfig->isChecked() && !QFile::exists(ui->lineConfigFile->text())) {

    auto *prompt = new QMessageBox(
        QMessageBox::Warning, tr("Filename invalid"), tr("Please select a valid configuration file."),
        QMessageBox::Ok | QMessageBox::Ignore, this
    );
    connect(prompt, &QDialog::finished, this, [this](int result) {
      if (result == QMessageBox::Ok)
        QTimer::singleShot(0, this, [this] {
          if (browseConfigFile())
            accept();
        });
    });
    PageHost::present(this, prompt);
    return;
  }

  // now that the dialog has been accepted, copy the new server config to the
  // original one, which is a reference to the one in MainWindow.
  setOriginalServerConfig(serverConfig());

  QDialog::accept();
}

void ServerConfigDialog::reject()
{
  serverConfig().setUseExternalConfig(m_originalServerConfigIsExternal);
  serverConfig().setConfigFile(m_originalServerConfigUsesExternalFile);
  serverConfig().setProtocol(m_originalProtocol);
  Settings::setValue(Settings::Server::Protocol, networkProtocolToOption(m_originalProtocol));

  QDialog::reject();
}

void ServerConfigDialog::addHotkey()
{
  auto hotkey = std::make_shared<Hotkey>();
  auto *dialog = new HotkeyDialog(this, *hotkey, &serverConfig());
  connect(dialog, &QDialog::accepted, this, [this, hotkey] {
    serverConfig().hotkeys().append(*hotkey);
    synergy::gui::appendHotkey(ui->listHotkeys, *hotkey);
    onChange();
  });
  PageHost::present(this, dialog);
}

void ServerConfigDialog::editHotkey()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to editing out of bounds hotkey row: " << row;
    return;
  }

  auto hotkey = std::make_shared<Hotkey>(serverConfig().hotkeys()[row]);
  auto *dialog = new HotkeyDialog(this, *hotkey, &serverConfig());
  connect(dialog, &QDialog::accepted, this, [this, hotkey, row] {
    serverConfig().hotkeys()[row] = *hotkey;
    synergy::gui::presentHotkey(ui->listHotkeys->item(row), *hotkey);
    listHotkeysSelectionChanged(ui->listHotkeys->selectionModel()->selection(), {});
    onChange();
  });
  PageHost::present(this, dialog);
}

void ServerConfigDialog::removeHotkey()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to remove out of bounds hotkey row: " << row;
    return;
  }

  serverConfig().hotkeys().removeAt(row);
  ui->listActions->clear();
  delete ui->listHotkeys->item(row);
  onChange();
}

void ServerConfigDialog::listHotkeysSelectionChanged(const QItemSelection &selected, const QItemSelection &)
{
  bool itemsSelected = !selected.isEmpty();
  ui->btnEditHotkey->setEnabled(itemsSelected);
  ui->btnRemoveHotkey->setEnabled(itemsSelected);
  ui->btnNewAction->setEnabled(itemsSelected);
  ui->listActions->clear();
  ui->btnEditAction->setEnabled(false);
  ui->btnRemoveAction->setEnabled(false);

  if (itemsSelected && !serverConfig().hotkeys().isEmpty()) {
    const Hotkey &hotkey = serverConfig().hotkeys().at(selected.indexes().first().row());
    for (const Action &action : hotkey.actions())
      synergy::gui::appendAction(ui->listActions, action);
  }
}

void ServerConfigDialog::addAction()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to add action to out of bounds hotkey row: " << row;
    return;
  }

  auto hotkey = std::make_shared<Hotkey>(serverConfig().hotkeys()[row]);
  auto action = std::make_shared<Action>();
  auto *dialog = new ActionDialog(this, serverConfig(), *hotkey, *action);
  connect(dialog, &QDialog::accepted, this, [this, row, hotkey, action] {
    serverConfig().hotkeys()[row].actions().append(*action);
    synergy::gui::appendAction(ui->listActions, *action);
    onChange();
  });
  PageHost::present(this, dialog);
}

void ServerConfigDialog::editAction()
{
  int hotkeyRow = ui->listHotkeys->currentRow();
  if (hotkeyRow < 0 || hotkeyRow >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to edit action from out of bounds hotkey row: " << hotkeyRow;
    return;
  }
  Hotkey &hotkey = serverConfig().hotkeys()[hotkeyRow];

  int actionRow = ui->listActions->currentRow();
  if (actionRow < 0 || actionRow >= hotkey.actions().size()) {
    qDebug() << "Attempt to remove out of bounds action row: " << actionRow;
    return;
  }
  auto hotkeyDraft = std::make_shared<Hotkey>(hotkey);
  auto action = std::make_shared<Action>(hotkey.actions()[actionRow]);
  auto *dialog = new ActionDialog(this, serverConfig(), *hotkeyDraft, *action);
  connect(dialog, &QDialog::accepted, this, [this, hotkeyRow, actionRow, hotkeyDraft, action] {
    serverConfig().hotkeys()[hotkeyRow].actions()[actionRow] = *action;
    synergy::gui::presentAction(ui->listActions->item(actionRow), *action);
    onChange();
  });
  PageHost::present(this, dialog);
}

void ServerConfigDialog::removeAction()
{
  int hotkeyRow = ui->listHotkeys->currentRow();
  if (hotkeyRow < 0 || hotkeyRow >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to remove action from out of bounds hotkey row: " << hotkeyRow;
    return;
  }
  Hotkey &hotkey = serverConfig().hotkeys()[hotkeyRow];

  int actionRow = ui->listActions->currentRow();
  if (actionRow < 0 || actionRow >= hotkey.actions().size()) {
    qDebug() << "Attempt to remove out of bounds action row: " << actionRow;
    return;
  }

  hotkey.actions().removeAt(actionRow);
  delete ui->listActions->currentItem();
  onChange();
}

void ServerConfigDialog::toggleClipboard(bool enabled)
{
  ui->sbClipboardSizeLimit->setEnabled(enabled);
  if (enabled && !ui->sbClipboardSizeLimit->value()) {
    auto size = static_cast<int>((synergy::gui::ServerConfig::defaultClipboardSharingSize() + 512) / 1024);
    ui->sbClipboardSizeLimit->setValue(size ? size : 1);
  }
  serverConfig().setClipboardSharing(enabled);
  onChange();
}

void ServerConfigDialog::setClipboardLimit(int limit)
{
  serverConfig().setClipboardSharingSize(limit * 1024);
  onChange();
}

void ServerConfigDialog::toggleHeartbeat(bool enabled)
{
  ui->sbHeartbeat->setEnabled(enabled);
  serverConfig().haveHeartbeat(enabled);
  onChange();
}

void ServerConfigDialog::setHeartbeat(int rate)
{
  serverConfig().setHeartbeat(rate);
  onChange();
}

void ServerConfigDialog::toggleRelativeMouseMoves(bool enabled)
{
  serverConfig().setRelativeMouseMoves(enabled);
  onChange();
}

void ServerConfigDialog::toggleProtocol()
{
  auto proto = ui->rbProtocolBarrier->isChecked() ? NetworkProtocol::Barrier : NetworkProtocol::Synergy;
  serverConfig().setProtocol(proto);
  Settings::setValue(Settings::Server::Protocol, networkProtocolToOption(proto));
  onChange();
}

void ServerConfigDialog::setSwitchCornerSize(int size)
{
  serverConfig().setSwitchCornerSize(size);
  onChange();
}

void ServerConfigDialog::toggleCornerBottomLeft(bool enable)
{
  serverConfig().setSwitchCorner(static_cast<int>(BottomLeft), enable);
  onChange();
}

void ServerConfigDialog::toggleCornerTopLeft(bool enable)
{
  serverConfig().setSwitchCorner(static_cast<int>(TopLeft), enable);
  onChange();
}

void ServerConfigDialog::toggleCornerBottomRight(bool enable)
{
  serverConfig().setSwitchCorner(static_cast<int>(BottomRight), enable);
  onChange();
}

void ServerConfigDialog::toggleCornerTopRight(bool enable)
{
  serverConfig().setSwitchCorner(static_cast<int>(TopRight), enable);
  onChange();
}

void ServerConfigDialog::listActionsSelectionChanged(const QItemSelection &selected, const QItemSelection &)
{
  bool enabled = !selected.isEmpty();
  ui->btnEditAction->setEnabled(enabled);
  ui->btnRemoveAction->setEnabled(enabled);
}

void ServerConfigDialog::toggleSwitchDoubleTap(bool enable)
{
  ui->sbSwitchDoubleTap->setEnabled(enable);
  serverConfig().haveSwitchDoubleTap(enable);
  onChange();
}

void ServerConfigDialog::setSwitchDoubleTap(int within)
{
  serverConfig().setSwitchDoubleTap(within);
  onChange();
}

void ServerConfigDialog::toggleSwitchDelay(bool enable)
{
  ui->sbSwitchDelay->setEnabled(enable);
  serverConfig().haveSwitchDelay(enable);
  onChange();
}

void ServerConfigDialog::setSwitchDelay(int delay)
{
  serverConfig().setSwitchDelay(delay);
  onChange();
}

void ServerConfigDialog::toggleDefaultLockToScreenState(bool state)
{
  serverConfig().setDefaultLockToScreenState(state);
  onChange();
}

void ServerConfigDialog::toggleLockToScreen(bool disabled)
{
  serverConfig().setDisableLockToScreen(disabled);
  onChange();
}

void ServerConfigDialog::toggleWin32Foreground(bool enabled)
{
  serverConfig().setWin32KeepForeground(enabled);
  onChange();
}

void ServerConfigDialog::addClient()
{
  addComputer("", false);
}

void ServerConfigDialog::onScreenRemoved()
{
  ui->lblNewScreen->setEnabled(true);
  onChange();
}

void ServerConfigDialog::toggleExternalConfig(bool checked)
{
  ui->widgetExternalConfigControls->setEnabled(checked);
  ui->tabWidget->setTabEnabled(0, !checked);
  ui->tabWidget->setTabEnabled(1, !checked);
  ui->tabWidget->setTabEnabled(2, !checked);

  serverConfig().setUseExternalConfig(checked);
  onChange();
}

bool ServerConfigDialog::browseConfigFile()
{
  //: %1 is replaced with the application names
  //: (*.conf) and (*.*) should not be translated
  const auto configFilter = tr("%1 Configurations (*.conf);;All files (*.*)");

  QString fileName = QFileDialog::getOpenFileName(this, tr("Browse for a config file"), "", configFilter.arg(kAppName));

  if (!fileName.isEmpty()) {
    ui->lineConfigFile->setText(fileName);
    serverConfig().setConfigFile(ui->lineConfigFile->text());
    onChange();
    return true;
  }

  return false;
}

bool ServerConfigDialog::addComputer(const QString &clientName, bool doSilent)
{
  auto screen = std::make_shared<synergy::gui::Screen>(clientName);
  if (doSilent) {
    model().addScreen(*screen);
    ui->lblNewScreen->setEnabled(!model().isFull());
    return true;
  }
  auto *dialog = new ScreenSettingsDialog(this, screen.get(), &model().m_Screens);
  connect(dialog, &QDialog::accepted, this, [this, screen] {
    model().addScreen(*screen);
    ui->lblNewScreen->setEnabled(!model().isFull());
  });
  PageHost::present(this, dialog);
  return false;
}

void ServerConfigDialog::onChange()
{
  bool isAppConfigDataEqual =
      m_originalServerConfigIsExternal == serverConfig().useExternalConfig() &&
      m_originalServerConfigUsesExternalFile == serverConfig().configFile() &&
      m_originalProtocol == networkProtocolFromValue(Settings::value(Settings::Server::Protocol));
  ui->buttonBox->button(QDialogButtonBox::Ok)
      ->setEnabled(!isAppConfigDataEqual || !(m_originalServerConfig == m_serverConfig));
}
