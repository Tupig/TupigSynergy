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
#include "gui/widgets/LogPanel.h"
#include "net/FingerprintDatabase.h"
#include "net/TlsPolicy.h"
#include "widgets/StatusBar.h"

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
#include <QStyle>
#include <QVariant>

#include <memory>
#include <thread>

#if defined(Q_OS_MACOS)
#include "platform/macos/OSXSDKHeaders.h"
#endif

using namespace synergy::gui;

void MainWindow::serverConfigSaving()
{
  m_serverConfig.commit();
}

void MainWindow::clearSettings()
{
  auto *prompt = new QMessageBox(
      QMessageBox::Question, tr("%1 Clear Settings").arg(kAppName),
      tr("<p>Are you sure you want to clear all settings and restart %1?</p>"
         "<p>This action cannot be undone.</p>")
          .arg(kAppName),
      QMessageBox::Yes | QMessageBox::No, this
  );
  prompt->setDefaultButton(QMessageBox::No);
  connect(prompt, &QDialog::finished, this, [this](int result) {
    if (result != QMessageBox::Yes)
      return;
    m_networkMonitor->stopMonitoring();

    disconnect(&m_coreProcess, nullptr, this, nullptr);
    disconnect(m_guiDupeChecker, nullptr, this, nullptr);
    disconnect(m_trayIcon, nullptr, this, nullptr);

    m_coreProcess.stop();
    m_coreProcess.clearSettings();

    m_saveOnExit = false;
    diagnostic::clearSettings(true);
  });
  showPage(prompt);
}

bool MainWindow::saveServerConfig()
{
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save server configuration as..."));

  if (!fileName.isEmpty() && !m_serverConfig.save(fileName)) {
    showPage(new QMessageBox(
        QMessageBox::Warning, tr("Save failed"), tr("Could not save server configuration to file."), QMessageBox::Ok,
        this
    ));
    return true;
  }

  return false;
}

void MainWindow::coreModeToggled(bool checked)
{
  // this method is called when rbClient or rbServer toggles
  // with both being in the same group one must be turned on if the other is turned off
  // only react to toggle on to avoid calling everything twice when the user switches modes
  if (!checked)
    return;

  Settings::CoreMode mode = Settings::CoreMode::None;

  if (ui->rbModeServer->isChecked())
    mode = Settings::CoreMode::Server;
  if (ui->rbModeClient->isChecked())
    mode = Settings::CoreMode::Client;

  qDebug() << QStringLiteral("change mode to: %1").arg(QVariant::fromValue(mode).toString());

  if (m_coreProcess.isStarted() && m_coreProcess.mode() != mode)
    m_coreProcess.stop();
  m_coreProcess.setMode(mode);

  Settings::setValue(Settings::Core::CoreMode, mode);
  Settings::save();

  updateModeControls();
}

void MainWindow::updateModeControls()
{
  const auto mode = m_coreProcess.mode();
  const bool isServer = mode == Settings::CoreMode::Server;
  const bool isClient = mode == Settings::CoreMode::Client;

  ui->serverOptions->setVisible(isServer);
  ui->clientOptions->setVisible(isClient);
  ui->btnConfigureClient->setVisible(isClient);
  ui->lblNoMode->setVisible(!isServer && !isClient);
  toggleCanRunCore(canRunCore());

  ui->lblIpAddresses->setVisible(
      (isClient && !Settings::value(Settings::Core::Interface).toString().isEmpty()) || isServer
  );

  if (ui->lblIpAddresses->isVisible())
    updateNetworkInfo();

  if (isServer) {
    m_networkMonitor->startMonitoring();
  } else {
    m_networkMonitor->stopMonitoring();
  }

  if (isServer || isClient)
    updateModeControlLabels();
}

void MainWindow::updateModeControlLabels()
{
  const bool isServer = m_coreProcess.mode() == CoreMode::Server;
  const bool isStarted = m_coreProcess.isStarted();

  QString startText;
  QString stopText;
  QIcon startIcon;
  QIcon stopIcon;

  if (isServer) {
    startText = tr("Start");
    stopText = tr("Stop");
    startIcon = QIcon::fromTheme(QStringLiteral("system-run"));
    stopIcon = QIcon::fromTheme(QStringLiteral("process-stop"));
  } else {
    startText = tr("Connect");
    stopText = tr("Disconnect");
    startIcon = QIcon::fromTheme(QStringLiteral("network-connect"));
    stopIcon = QIcon::fromTheme(QStringLiteral("network-disconnect"));
  }

  m_actionStartCore->setText(startText);
  m_actionStartCore->setIcon(startIcon);
  m_actionStopCore->setText(stopText);
  m_actionStopCore->setIcon(stopIcon);

  if (isStarted) {
    ui->btnToggleCore->setText(stopText);
    ui->btnToggleCore->setIcon(stopIcon);
  } else {
    ui->btnToggleCore->setText(startText);
    ui->btnToggleCore->setIcon(startIcon);
  }

  // 设计稿：运行中为红色「停止」，未运行为强调色「连接」（客户端）。
  ui->btnToggleCore->setProperty("danger", isStarted && isServer);
  ui->btnToggleCore->setProperty("primary", !isStarted && !isServer);
  ui->btnToggleCore->style()->unpolish(ui->btnToggleCore);
  ui->btnToggleCore->style()->polish(ui->btnToggleCore);
}

void MainWindow::applyConfig()
{
  if (Settings::value(Settings::Gui::ShowVersionInTitle).toBool()) {
    setWindowTitle(QStringLiteral("%1 - %2").arg(kAppName, kDisplayVersion));
  } else {
    setWindowTitle(kAppName);
  }
#ifdef SYNERGY_EXTRA_HEADER
  synergy::hooks::onTitleApplied(this);
#endif

  if (const auto host = Settings::value(Settings::Client::RemoteHost).toString(); !host.isEmpty())
    ui->lineHostname->setText(host);

  updateLocalFingerprint();
  setTrayIcon();

  if (const auto ip = Settings::value(Settings::Core::Interface).toString(); !ip.isEmpty()) {
    m_serverStartIPs = QStringList{ip};
    m_serverStartSuggestedIP = ip;
  }

  coreModeToggled(true);
}

void MainWindow::saveSettings() const
{
  if (ui->rbModeClient->isChecked()) {
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  } else if (ui->rbModeServer->isChecked()) {
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Server);
  }
  if (!ui->lineHostname->text().isEmpty())
    Settings::setValue(Settings::Client::RemoteHost, ui->lineHostname->text());
  Settings::save();
}

void MainWindow::showConfigureServer(const QString &message)
{
  if (m_serverConfigDialogVisible)
    return;
  m_serverConfigDialogVisible = true;
  auto *dialog = new ServerConfigDialog(this, serverConfig());
  dialog->message(message);
  connect(dialog, &QDialog::accepted, this, [this] {
    if (m_coreProcess.isStarted())
      m_coreProcess.restart();
  });
  connect(dialog, &QDialog::finished, this, [this] { m_serverConfigDialogVisible = false; });
  showPage(dialog);
}

void MainWindow::showConfigureClient()
{
  auto *dialog = new ClientConfigDialog(this);
  connect(dialog, &QDialog::accepted, this, [this] {
    if (m_coreProcess.isStarted())
      m_coreProcess.restart();
  });
  showPage(dialog);
}

void MainWindow::secureSocket(bool secureSocket)
{
  m_secureSocket = secureSocket;
  updateSecurityIcon(m_statusBar->securityIconVisible());
}

void MainWindow::updateScreenName()
{
  const auto screenName = Settings::value(Settings::Core::ComputerName).toString();
  ui->lblComputerName->setText(screenName);
  ui->lineEditName->setText(screenName);
  m_serverConfig.updateServerName();
}

void MainWindow::showHostNameEditor()
{
  ui->lineEditName->show();
  ui->lblComputerName->hide();
  ui->btnEditName->hide();
  toggleCanRunCore(false);
  ui->lineEditName->setFocus();
}

void MainWindow::setHostName()
{
  ui->lineEditName->hide();
  ui->lblComputerName->show();
  ui->btnEditName->show();
  toggleCanRunCore(canRunCore());

  QString text = ui->lineEditName->text();
  const auto screenName = Settings::value(Settings::Core::ComputerName).toString();

  if (text == screenName)
    return;

  const bool isServer = ui->rbModeServer->isChecked();
  bool existingScreen = false;
  if (isServer)
    existingScreen = serverConfig().screenExists(text);

  if (!ui->lineEditName->hasAcceptableInput() || text.isEmpty() || existingScreen) {
    blockSignals(true);
    ui->lineEditName->setText(screenName);
    blockSignals(false);

    const auto title = tr("Invalid Screen Name");
    QString body;
    if (existingScreen) {
      body = tr("Screen name already exists");
    } else {
      body =
          tr("The name you have chosen is invalid.\n\n"
             "Valid names:\n"
             "• Use letters and numbers\n"
             "• May also use _ or -\n"
             "• Are between 1 and 255 characters");
    }
    showPage(new QMessageBox(QMessageBox::Information, title, body, QMessageBox::Ok, this));
    return;
  }

  ui->lblComputerName->setText(ui->lineEditName->text());
  Settings::setValue(Settings::Core::ComputerName, ui->lineEditName->text());
  if (isServer)
    serverConfig().updateServerName();
  applyConfig();
}

void MainWindow::remoteHostChanged(const QString &newRemoteHost)
{
  m_coreProcess.setAddress(newRemoteHost);
  toggleCanRunCore(!newRemoteHost.isEmpty() && ui->rbModeClient->isChecked());
  // debounce the settings write: textChanged fires per keystroke and each
  // save rewrites the whole settings file
  m_remoteHostSaveTimer.start();
}

void MainWindow::saveRemoteHost()
{
  if (m_remoteHostSaveTimer.isActive()) {
    m_remoteHostSaveTimer.stop();
  }
  const auto host = ui->lineHostname->text();
  if (host.isEmpty()) {
    Settings::setValue(Settings::Client::RemoteHost);
  } else {
    Settings::setValue(Settings::Client::RemoteHost, host);
  }
}
