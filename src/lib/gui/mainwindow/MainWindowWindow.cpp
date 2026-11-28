/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2024 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MainWindow.h"
#include "MainWindowDesign.h"
#include "gui/AppMenus.h"
#include "gui/Logger.h"

#include "Diagnostic.h"
#include "StyleUtils.h"

#include "dialogs/AboutDialog.h"
#include "dialogs/ClientConfigDialog.h"
#include "dialogs/FingerprintDialog.h"
#include "dialogs/ServerConfigDialog.h"
#include "dialogs/SettingsDialog.h"

#include "common/PlatformInfo.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "gui/Messages.h"
#include "gui/TlsUtility.h"
#include "gui/core/CoreProcess.h"
#include "gui/ipc/DaemonIpcClient.h"
#include "gui/update/UpdateChecker.h"
#include "gui/update/UpdateDialog.h"
#include "gui/widgets/LogPanel.h"
#include "gui/widgets/MessagePage.h"

#include "net/FingerprintDatabase.h"
#include "net/TlsPolicy.h"
#include "widgets/StatusBar.h"
#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>

#ifdef SYNERGY_EXTRA_HEADER
#include "synergy/hooks/gui_hook.h"
#endif

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHideEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollBar>
#include <QShowEvent>
#include <QVBoxLayout>

#include <memory>
#include <thread>
#include <utility>

#if defined(Q_OS_MACOS)
#include "platform/macos/OSXSDKHeaders.h"
#endif

using namespace synergy::gui;

void MainWindow::createMenuBar()
{
  m_actionLog->setCheckable(true);
  m_actionLog->setChecked(Settings::value(Settings::Gui::LogExpanded).toBool());
  connect(m_actionLog, &QAction::toggled, this, &MainWindow::toggleLogVisible);
  connect(m_logDock, &LogPanel::collapsedChanged, m_actionLog, [this](bool collapsed) {
    QSignalBlocker blocker(m_actionLog);
    m_actionLog->setChecked(!collapsed);
  });

  auto *bar = new QMenuBar(this);
  const AppMenuActions actions{m_actionRestore, m_actionStartCore, m_actionRestartCore,   m_actionStopCore,
                               m_actionQuit,    m_actionSettings,  m_actionClearSettings, m_actionCheckForUpdates,
                               m_actionAbout,   m_actionLog,       m_actionReportBug};
  populateMainMenus(bar, actions, synergy::platform::isMac() ? MenuPlatform::MacOS : MenuPlatform::WindowsLinux);
  m_menuFile = bar->findChild<QMenu *>(QStringLiteral("menuFile"));
  m_menuEdit = bar->findChild<QMenu *>(QStringLiteral("menuEdit"));
  m_menuView = bar->findChild<QMenu *>(QStringLiteral("menuView"));
  m_menuHelp = bar->findChild<QMenu *>(QStringLiteral("menuHelp"));
  setMenuBar(bar);
}

void MainWindow::fitToCurrentPage()
{
  if (m_fittingPage || !m_pageHost)
    return;
  const QScopedValueRollback<bool> fitting(m_fittingPage, true);
  ui->updateShortcutPresentation(width());
  const int preferred = m_pageHost->preferredContentHeight(m_pageHost->width());
  const auto *currentScreen = screen() ? screen() : QGuiApplication::primaryScreen();
  const int frame = qMax(0, frameGeometry().height() - height());
  const int menuHeight = menuBar()->isNativeMenuBar() ? 0 : menuBar()->sizeHint().height();
  const int chrome = ui->pageBar->height() + ui->statusBar->height() + menuHeight;
  int available = 800;
  if (currentScreen) {
    const QRect workArea = currentScreen->availableGeometry();
    available = workArea.bottom() + 1 - qMax(workArea.top(), frameGeometry().top());
  }
  const int bodyCap = qMax(1, qMin(660, available - chrome - frame));
  const int fittedHeight = chrome + qMin(preferred, bodyCap);
  const int defaultHeight = Ui::MainWindow::kWindowHeight;
  const bool compactPage = m_pageHost->hasEditor() || m_logDock->isCollapsed();
  const int minimum = compactPage ? qMin(defaultHeight, fittedHeight) : defaultHeight;
  setMinimumHeight(minimum);
  const int target = qMax(minimum, fittedHeight);
  if (height() != target)
    resize(width(), target);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);
  ui->updateShortcutPresentation(event->size().width());
  if (m_pageHost)
    m_pageHost->requestContentFit();
}

synergy::gui::update::UpdateChecker *MainWindow::createUpdateChecker()
{
  return new synergy::gui::update::UpdateChecker(this);
}

bool MainWindow::activateUpdatePage()
{
  using synergy::gui::update::UpdateDialog;
  if (m_pageHost->activateEditor(m_updateCheckPage, true)) {
    showAndActivate();
    return true;
  }
  if (m_pageHost->activateEditor(UpdateDialog::staticMetaObject, true)) {
    showAndActivate();
    return true;
  }
  if (auto *update = m_pageHost->findChild<UpdateDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
    showPage(update);
    return true;
  }
  return false;
}

void MainWindow::showAvailableUpdate()
{
  if (activateUpdatePage())
    return;
  auto *update = new synergy::gui::update::UpdateDialog(*m_availableUpdate, this);
#ifdef Q_OS_MACOS
  m_resumeSharingAfterUpdate = false;
  update->setInstallPreparation([this] {
    // A retry uses the state before the updater stopped sharing.
    m_resumeSharingAfterUpdate |= m_coreProcess.isStarted();
    return m_coreProcess.stopDesktopForUpdate();
  });
  connect(update, &synergy::gui::update::UpdateDialog::installationStarted, this, [this] {
    Settings::setValue(Settings::Gui::WindowGeometry, geometry());
    Settings::setValue(Settings::Gui::AutoStartCore, m_resumeSharingAfterUpdate);
    m_saveOnExit = false;
  });
#endif
  showPage(update);
}

bool MainWindow::finishUpdateCheck()
{
  auto *checker = std::exchange(m_updateChecker, nullptr);
  disconnect(checker, nullptr, this, nullptr);
  checker->deleteLater();
  const auto checking = std::exchange(m_updateCheckPage, {});
  if (!checking)
    return false;
  const bool showResult = checking->isVisibleTo(m_pageHost);
  checking->accept();
  return showResult;
}

void MainWindow::checkForUpdates(bool manual)
{
  using synergy::gui::update::UpdateChecker;
  using synergy::gui::update::UpdateDialog;
  using synergy::gui::update::UpdateInfo;

  if (manual ? activateUpdatePage() : m_pageHost->findChild<UpdateDialog *>() != nullptr)
    return;

  const bool newCheck = m_updateChecker == nullptr;
  if (newCheck) {
    m_updateChecker = createUpdateChecker();
    if (!m_updateChecker)
      return;
    m_updateChecker->setParent(this);
    connect(m_updateChecker, &UpdateChecker::updateAvailable, this, [this](const UpdateInfo &info) {
      qCInfo(synergy::gui::updateLog).noquote() << tr("New version available: %1").arg(info.version);
      m_availableUpdate = std::make_unique<UpdateInfo>(info);
      ui->setAvailableUpdateVersion(info.version);
      if (finishUpdateCheck())
        showAvailableUpdate();
    });
    connect(m_updateChecker, &UpdateChecker::noUpdate, this, [this](const QString &version) {
      qCDebug(synergy::gui::updateLog).noquote() << tr("You are running the latest version (%1).").arg(version);
      m_availableUpdate.reset();
      ui->setAvailableUpdateVersion(QString());
      if (!finishUpdateCheck())
        return;
      auto *page = new MessagePage(
          this, MessagePage::Kind::Information, tr("No Updates"),
          tr("You are running the latest version (%1).").arg(version)
      );
      auto *ok = page->addButton(tr("OK"), true);
      connect(ok, &QPushButton::clicked, page, &QDialog::accept);
      showPage(page);
    });
    connect(m_updateChecker, &UpdateChecker::failed, this, [this](const QString &reason) {
      qCWarning(synergy::gui::updateLog).noquote()
          << QCoreApplication::translate("GuiLogMessages", "update check failed:") << reason;
      if (!finishUpdateCheck()) {
        return;
      }
      auto *page = new MessagePage(this, MessagePage::Kind::Warning, tr("Update Check Failed"), reason);
      auto *close = page->addButton(tr("Close"));
      connect(close, &QPushButton::clicked, page, &QDialog::reject);
      auto *retry = page->addButton(tr("Retry"), true);
      connect(retry, &QPushButton::clicked, this, [this, page, retry] {
        retry->setEnabled(false);
        page->accept();
        checkForUpdates(true);
      });
      showPage(page);
    });
  }

  if (manual && !m_updateCheckPage) {
    auto *checking =
        new MessagePage(this, MessagePage::Kind::Information, tr("Check for Updates"), tr("Checking for updates…"));
    checking->setProperty("independentPage", true);
    m_updateCheckPage = checking;
    auto *progress = new QProgressBar(checking);
    progress->setRange(0, 0);
    checking->addContentWidget(progress);
    auto *cancel = checking->addButton(tr("Cancel"));
    connect(cancel, &QPushButton::clicked, checking, &QDialog::reject);
    const auto cancelCheck = [this, checking] {
      if (checking != m_updateCheckPage)
        return;
      m_updateCheckPage = nullptr;
      finishUpdateCheck();
    };
    connect(checking, &QDialog::rejected, this, cancelCheck);
    showPage(checking);
  }

  if (newCheck && m_updateChecker)
    m_updateChecker->checkForUpdates();
}

void MainWindow::setupTrayIcon()
{
  auto *trayMenu = new QMenu(this);
  const AppMenuActions actions{m_actionRestore, m_actionStartCore, m_actionRestartCore,   m_actionStopCore,
                               m_actionQuit,    m_actionSettings,  m_actionClearSettings, m_actionCheckForUpdates,
                               m_actionAbout,   m_actionLog,       m_actionReportBug};
  populateTrayMenu(trayMenu, actions);
  m_trayIcon->setContextMenu(trayMenu);

  setTrayIcon();
  m_trayIcon->show();
}

void MainWindow::setTrayIcon()
{
  static const auto fallbackPath = QStringLiteral(":/icons/%1-%2/apps/64/%3");

  if (synergy::platform::isMac()) {
    // macOS has no freedesktop icon theme, and the bundle ships the SVG icon
    // engine but not the qsvg image plugin, so fromTheme() and every SVG
    // fallback below are null; QSystemTrayIcon then logs "No Icon set" and
    // stays invisible. Use the bundled square app icon: the logo-*.png
    // wordmarks are 304x74, so scaling one into the menu bar renders a tiny
    // mark followed by the word "Synergy" instead of a square icon.
    m_trayIcon->setIcon(QIcon(QStringLiteral(":/image/app-icon.png")));
    return;
  }

  QString themeIcon = kRevFqdnName;
  if (!Settings::value(Settings::Gui::SymbolicTrayIcon).toBool()) {
#ifdef SYNERGY_EXTRA_HEADER
    m_trayIcon->setIcon(synergy::hooks::trayIcon(fallbackPath.arg(kAppId, QStringLiteral("dark"), themeIcon)));
#else
    m_trayIcon->setIcon(QIcon(fallbackPath.arg(kAppId, QStringLiteral("dark"), themeIcon)));
#endif
    return;
  }

  themeIcon.append(QStringLiteral("-symbolic"));

  if (synergy::platform::isWindows()) {
    QSettings settings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat
    );
    const QString theme = settings.value(QStringLiteral("SystemUsesLightTheme"), 1).toBool() ? QStringLiteral("light")
                                                                                             : QStringLiteral("dark");
    m_trayIcon->setIcon(QIcon(fallbackPath.arg(kAppId, theme, themeIcon)));
    return;
  }

  auto icon =
#ifdef SYNERGY_EXTRA_HEADER
      synergy::hooks::trayIcon(fallbackPath.arg(kAppId, iconMode(), themeIcon));
#else
      QIcon::fromTheme(themeIcon, QIcon(fallbackPath.arg(kAppId, iconMode(), themeIcon)));
#endif
  icon.setIsMask(true);
  m_trayIcon->setIcon(icon);
}

void MainWindow::hideEvent(QHideEvent *event)
{
  // No UI is visible while hidden (tray or minimized); suspend the periodic
  // network poll and resume it in showEvent().
  m_networkMonitor->stopMonitoring();
  QMainWindow::hideEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
  QMainWindow::showEvent(event);
  if (m_coreProcess.mode() == Settings::CoreMode::Server) {
    // startMonitoring() runs an immediate update, which refreshes the IP label
    // if the network changed while the window was hidden.
    m_networkMonitor->startMonitoring();
  }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if (Settings::value(Settings::Gui::CloseToTray).toBool() && event->spontaneous()) {
    if (auto *reminder = findChild<QMessageBox *>(QStringLiteral("closeToTrayReminder"), Qt::FindDirectChildrenOnly)) {
      event->ignore();
      reminder->raise();
      return;
    }
    if (Settings::value(Settings::Gui::CloseReminder).toBool()) {
      event->ignore();
      auto *message = new QMessageBox(
          QMessageBox::NoIcon, kAppName,
          tr("%1 will continue to run in the background. Use the tray icon to reopen the window.").arg(kAppName),
          QMessageBox::Ok, this
      );
      message->setObjectName(QStringLiteral("closeToTrayReminder"));
      message->setAttribute(Qt::WA_DeleteOnClose);
      connect(message, &QDialog::finished, this, [this] {
        Settings::setValue(Settings::Gui::CloseReminder, false);
        Settings::setValue(Settings::Gui::WindowGeometry, geometry());
        hide();
      });
      message->open();
      return;
    }
    Settings::setValue(Settings::Gui::WindowGeometry, geometry());
    qDebug() << "hiding to tray";
    hide();
    event->ignore();
    return;
  }

  if (m_saveOnExit) {
    Settings::setValue(Settings::Gui::WindowGeometry, geometry());
    Settings::setValue(Settings::Gui::AutoStartCore, m_coreProcess.isStarted());
  }
  qDebug() << "quitting application";

  event->accept();
  QApplication::quit();
}

void MainWindow::hide()
{
#ifdef Q_OS_MACOS
  macOSNativeHide();
#else
  QMainWindow::hide();
#endif
  m_actionRestore->setVisible(true);
  m_actionMinimize->setVisible(false);
}

void MainWindow::changeEvent(QEvent *e)
{
  QMainWindow::changeEvent(e);
  if (e->type() == QEvent::PaletteChange) {
    updateIconTheme();
    setWindowIcon(QIcon(QStringLiteral(":/image/app-icon.png")));
    setTrayIcon();
  } else if (e->type() == QEvent::LanguageChange) {
    ui->retranslateUi(this);
    updateModeControlLabels();
    updateNetworkInfo();
    updateStatus();
    serverClientsChanged({});
    updateText();
  }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
  if ((obj != ui->lineEditName && obj != ui->btnSaveName && obj != ui->btnCancelName) || ui->lineEditName->isHidden() ||
      event->type() != QEvent::KeyPress)
    return false;
  if (const auto keyEvent = static_cast<QKeyEvent *>(event); keyEvent->key() != Qt::Key_Escape)
    return false;
  cancelHostNameEdit();
  return true;
}

void MainWindow::showAndActivate()
{
#ifdef Q_OS_MACOS
  if (!synergy::runtime::isIsolatedRuntime())
    forceAppActive();
#endif
  showNormal();
  raise();
  activateWindow();
  m_actionRestore->setVisible(true);
  m_actionMinimize->setVisible(true);
}
