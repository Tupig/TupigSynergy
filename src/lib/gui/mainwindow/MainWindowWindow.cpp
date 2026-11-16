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

#include "Diagnostic.h"
#include "StyleUtils.h"

#include "dialogs/AboutDialog.h"
#include "dialogs/ClientConfigDialog.h"
#include "dialogs/FingerprintDialog.h"
#include "dialogs/ServerConfigDialog.h"
#include "dialogs/SettingsDialog.h"

#include "common/PlatformInfo.h"
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
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollBar>
#include <QShowEvent>

#include <memory>
#include <thread>

#if defined(Q_OS_MACOS)
#include "platform/macos/OSXSDKHeaders.h"
#endif

using namespace synergy::gui;

void MainWindow::createMenuBar()
{
  m_menuFile->addAction(m_actionStartCore);
  m_menuFile->addAction(m_actionRestartCore);
  m_menuFile->addAction(m_actionStopCore);
  m_menuFile->addSeparator();
  m_menuFile->addAction(m_actionQuit);

  auto *logAction = new QAction(tr("Show &Log"), this);
  logAction->setCheckable(true);
  logAction->setChecked(Settings::value(Settings::Gui::LogExpanded).toBool());
  connect(logAction, &QAction::toggled, this, &MainWindow::toggleLogVisible);
  connect(m_logDock, &LogPanel::collapsedChanged, logAction, [logAction](bool collapsed) {
    QSignalBlocker blocker(logAction);
    logAction->setChecked(!collapsed);
  });
  m_menuView->addAction(logAction);

  // macOS moves actions that carry a menu role into the application menu.
  // A menu holding only such actions renders empty, so macOS gets no Edit
  // menu at all; its Preferences action rides on the Help menu and is merged
  // out, leaving "Get help" as the only remaining Help item. Qt prepends each
  // merged action, hence the reversed order: the application menu shows
  // "Check for Updates" above "Clear settings".
  if (synergy::platform::isMac()) {
    m_menuHelp->addAction(m_actionReportBug);
    m_menuHelp->addAction(m_actionSettings);
    m_menuHelp->addAction(m_actionAbout);
    m_menuHelp->addAction(m_actionClearSettings);
    m_menuHelp->addAction(m_actionCheckForUpdates);
  } else {
    m_menuEdit->addAction(m_actionSettings);
    m_menuHelp->addAction(m_actionCheckForUpdates);
    m_menuHelp->addAction(m_actionAbout);
    m_menuHelp->addAction(m_actionReportBug);
    m_menuHelp->addSeparator();
    m_menuHelp->addAction(m_actionClearSettings);
  }

  auto menuBar = new QMenuBar(this);
  menuBar->addMenu(m_menuFile);
  if (!synergy::platform::isMac())
    menuBar->addMenu(m_menuEdit);
  menuBar->addMenu(m_menuView);
  menuBar->addMenu(m_menuHelp);

  setMenuBar(menuBar);
}

void MainWindow::checkForUpdates(bool manual)
{
  auto *checker = new synergy::gui::update::UpdateChecker(this);
  const auto release = [checker]() { checker->deleteLater(); };

  connect(
      checker, &synergy::gui::update::UpdateChecker::updateAvailable, this,
      [this, release](const synergy::gui::update::UpdateInfo &info) {
        showPage(new synergy::gui::update::UpdateDialog(info, this));
        release();
      }
  );
  connect(
      checker, &synergy::gui::update::UpdateChecker::noUpdate, this,
      [this, manual, release](const QString &version) {
        if (manual) {
          showPage(new QMessageBox(
              QMessageBox::Information, tr("No Updates"), tr("You are running the latest version (%1).").arg(version),
              QMessageBox::Ok, this
          ));
        }
        release();
      }
  );
  connect(checker, &synergy::gui::update::UpdateChecker::failed, this, [this, manual, release](const QString &reason) {
    if (manual) {
      auto *page = new MessagePage(this, MessagePage::Kind::Warning, tr("Update Check Failed"), reason);
      auto *ok = page->addButton(tr("OK"), true);
      connect(ok, &QPushButton::clicked, page, &QDialog::accept);
      showPage(page);
    } else {
      qWarning().noquote() << "update check failed:" << reason;
    }
    release();
  });

  checker->checkForUpdates();
}

void MainWindow::setupTrayIcon()
{
  auto trayMenu = new QMenu(this);
  trayMenu->addActions(
      {m_actionStartCore, m_actionRestartCore, m_actionStopCore, m_actionMinimize, m_actionRestore, m_actionTrayQuit}
  );
  trayMenu->insertSeparator(m_actionMinimize);
  trayMenu->insertSeparator(m_actionTrayQuit);
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
    if (Settings::value(Settings::Gui::CloseReminder).toBool()) {
      event->ignore();
      Settings::setValue(Settings::Gui::CloseReminder, false);
      auto *message = new QMessageBox(
          QMessageBox::Information, kAppName,
          tr("%1 will continue to run in the background. Use the tray icon to reopen the window.").arg(kAppName),
          QMessageBox::Ok, this
      );
      connect(message, &QDialog::finished, this, [this] {
        Settings::setValue(Settings::Gui::WindowGeometry, geometry());
        hide();
      });
      showPage(message);
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
  if (obj != ui->lineEditName || event->type() != QEvent::KeyPress)
    return false;
  if (const auto keyEvent = static_cast<QKeyEvent *>(event); keyEvent->key() != Qt::Key_Escape)
    return false;
  ui->lineEditName->hide();
  ui->lblComputerName->show();
  ui->btnEditName->show();
  ui->lineEditName->setText(Settings::value(Settings::Core::ComputerName).toString());
  toggleCanRunCore(canRunCore());
  return true;
}

void MainWindow::showAndActivate()
{
  const auto wasVisible = isVisible();
#ifdef Q_OS_MACOS
  forceAppActive();
#endif
  showNormal();
  raise();
  activateWindow();
  m_actionRestore->setVisible(false);
  m_actionMinimize->setVisible(true);
  if (!wasVisible)
    restoreWindow();
}
