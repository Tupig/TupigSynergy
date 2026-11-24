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
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHideEvent>
#include <QHostAddress>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollBar>
#include <QShowEvent>
#include <QVariant>

#include <memory>
#include <thread>

#if defined(Q_OS_MACOS)
#include "platform/macos/OSXSDKHeaders.h"
#endif

using namespace synergy::gui;

namespace {

//! Set a busy cursor without using the platform system-cursor image.
/*!
Qt 6.11's Cocoa wait cursor builds a null QImage on macOS 26/27 and crashes in
CGImageCreate. A valid transparent pixmap keeps the window marked busy without
that platform path; other platforms keep the native wait cursor.
*/
void setBusyCursor()
{
#if defined(Q_OS_MACOS)
  QPixmap pixmap(16, 16);
  pixmap.fill(Qt::transparent);
  QApplication::setOverrideCursor(QCursor(pixmap));
#else
  QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
}

} // namespace

void MainWindow::coreProcessError(CoreProcess::Error error)
{
  if (error == CoreProcess::Error::AddressMissing) {
    showPage(new QMessageBox(
        QMessageBox::Warning, tr("Address missing"),
        tr("Please enter the hostname or IP address of the other computer."), QMessageBox::Ok, this
    ));
  } else if (error == CoreProcess::Error::StartFailed) {
    show();
    auto message = tr("The Core executable could not be started.\n"
                      "Please check if you have sufficient permissions to run %1.")
                       .arg(kCoreBinName);

    if (Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Server) {
      const auto mode =
          Settings::value(Settings::Server::ExternalConfigFile).toBool() ? tr("read") : tr("read and write");
      message.append(tr("\nAdditionally, check you are able to %1 the server config file: %2")
                         .arg(mode, Settings::serverConfigFile()));
    }
    showPage(new QMessageBox(QMessageBox::Warning, kAppName, message, QMessageBox::Ok, this));
  }
}

void MainWindow::startCore()
{
  // Let macOS request accessibility permission before starting the core.
#if defined(Q_OS_MACOS)
  if (!checkOSXAccessibilityPermission()) {
    qWarning().noquote() << tr("Start blocked: macOS has not granted Accessibility permission.");
    return;
  }
#endif

#ifdef SYNERGY_EXTRA_HEADER
  if (!synergy::hooks::onCoreStart()) {
    return;
  }
#endif

  m_actionStartCore->setEnabled(false);
  m_actionRestartCore->setEnabled(true);

  m_coreProcess.start();
}

void MainWindow::stopCore()
{
  qDebug() << "stopping core process";
  m_coreProcess.stop();
  toggleCanRunCore(canRunCore());
}

void MainWindow::handleLogLine(const QString &line)
{
  m_logDock->appendLine(line);
}

void MainWindow::handleUnrecognisedClient(const QString &clientName)
{
  if (m_ignoredClients.contains(clientName)) {
    qDebug("ignoring %s:", qPrintable(clientName));
    return;
  }

  if (m_newClientPromptShowing || m_serverConfigDialogVisible)
    return;

  if (Settings::value(Settings::Server::ExternalConfig).toBool())
    return;

  if (m_serverConfig.isFull() || m_serverConfig.screenExists(clientName))
    return;

  m_newClientPromptShowing = true;

  const bool authenticated = Settings::value(Settings::Security::TlsEnabled).toBool() &&
                             Settings::value(Settings::Security::CheckPeers).toBool();
  auto *prompt = new QMessageBox(this);
  prompt->setWindowTitle(tr("%1 - New Client").arg(kAppName));
  prompt->setTextFormat(Qt::PlainText);
  if (authenticated) {
    prompt->setText(
        tr("A new client called '%1' has been accepted. You'll need to add it to your server's screen layout.")
            .arg(clientName)
    );
    prompt->setStandardButtons(QMessageBox::Ok);
  } else {
    prompt->setText(tr("A new client called '%1' wants to connect").arg(clientName));
    prompt->addButton(tr("Ignore"), QMessageBox::RejectRole);
    prompt->addButton(tr("Add client"), QMessageBox::AcceptRole);
  }
  connect(prompt, &QDialog::finished, this, [this, prompt, clientName, authenticated](int result) {
    const bool accepted = authenticated ? result == QMessageBox::Ok
                                        : prompt->buttonRole(prompt->clickedButton()) == QMessageBox::AcceptRole;
    if (accepted) {
      QTimer::singleShot(0, this, [this, clientName] {
        m_newClientPromptShowing = false;
        if (m_coreProcess.mode() == CoreMode::Server && !m_serverConfig.screenExists(clientName) &&
            !m_serverConfig.isFull() && !Settings::value(Settings::Server::ExternalConfig).toBool())
          serverConnectionConfigureClient(clientName);
      });
    } else {
      m_newClientPromptShowing = false;
      m_ignoredClients.insert(clientName);
    }
  });
  showPage(prompt);
}

void MainWindow::handleConnectionRefused(synergy::core::ConnectionRefusal reason)
{
  if (reason != synergy::core::ConnectionRefusal::AlreadyConnected)
    return;

  if (!isVisible() || m_clientErrorVisible)
    return;

  m_clientErrorVisible = true;
  showAndActivate();

  const auto address = Settings::value(Settings::Client::RemoteHost).toString();
  auto *message = new QMessageBox(
      QMessageBox::Warning, tr("%1 Connection Error").arg(kAppName),
      tr("<p>Failed to connect to the server '%1'.</p>"
         "<p>A Client with your name is already connected to the server.</p>"
         "Please ensure that you're using a unique name and that only a "
         "single instance of the client process is running.</p>")
          .arg(address.toHtmlEscaped()),
      QMessageBox::Ok, this
  );
  connect(message, &QDialog::finished, this, [this] { m_clientErrorVisible = false; });
  showPage(message);
}

void MainWindow::handleMissingKeyboardLayouts(const QString &layouts)
{
  if (Settings::value(Settings::Gui::IgnoreMissingKeyboardLayouts).toBool())
    return;

  auto *msgBox = new QMessageBox(this);
  msgBox->setIcon(QMessageBox::Warning);
  msgBox->setWindowTitle(tr("Missing Keyboard Layouts"));
  msgBox->setText(tr("<p>Keyboard layout support requires matching layouts on all computers. "
                     "The following layouts from the other computer are not installed on this computer:</p>"
                     "<p><b>%1</b></p>"
                     "<p>Please install them to enable support for these layouts.</p>")
                      .arg(layouts.toHtmlEscaped()));

  auto *checkBox = new QCheckBox(tr("Don't show this again"), msgBox);
  msgBox->setCheckBox(checkBox);
  connect(msgBox, &QDialog::finished, this, [checkBox] {
    if (checkBox->isChecked())
      Settings::setValue(Settings::Gui::IgnoreMissingKeyboardLayouts, true);
  });
  showPage(msgBox);
}

void MainWindow::handlePeerFingerprint(const QString &fingerprint)
{
  const auto sha256Text = QString(fingerprint).remove(':');
  const Fingerprint sha256 = {QCryptographicHash::Sha256, QByteArray::fromHex(sha256Text.toLatin1())};

  const bool isClient = m_coreProcess.mode() == CoreMode::Client;
  if ((isClient && m_checkedServers.contains(sha256Text)) || (!isClient && m_checkedClients.contains(sha256Text))) {
    qDebug("ignoring fingerprint, already handled");
    return;
  }

  FingerprintDatabase db;
  db.read(trustedFingerprintDatabase());

  if (db.isTrusted(sha256)) {
    qDebug("fingerprint is trusted");
    return;
  }

  if (isClient) {
    m_checkedServers.append(sha256Text);
    m_coreProcess.stop();
  } else {
    m_checkedClients.append(sha256Text);
  }

  const auto mode = isClient ? FingerprintDialogMode::Client : FingerprintDialogMode::Server;
  const auto databasePath = trustedFingerprintDatabase();
  const auto remoteHost = Settings::value(Settings::Client::RemoteHost).toString();
  auto *dialog = new FingerprintDialog(this, m_fingerprint, mode, sha256);
  connect(dialog, &QDialog::accepted, this, [this, sha256, sha256Text, isClient, databasePath, remoteHost] {
    // 绑定显示时的信任库；并重新读取以免覆盖另一条已确认的指纹。
    FingerprintDatabase current;
    current.read(databasePath);
    current.addTrusted(sha256);
    if (!current.write(databasePath)) {
      qCritical().noquote() << QCoreApplication::translate("GuiLogMessages", "unable to write fingerprint to db:")
                            << databasePath;
      m_coreProcess.stop();
      return;
    }
    if (isClient) {
      m_checkedServers.removeAll(sha256Text);
      if (m_coreProcess.mode() == CoreMode::Client &&
          Settings::value(Settings::Client::RemoteHost).toString() == remoteHost)
        m_coreProcess.start();
    } else {
      m_checkedClients.removeAll(sha256Text);
    }
  });
  showPage(dialog);
}

void MainWindow::updateStatus()
{
  using enum ProcessState;
  const auto connection = m_coreProcess.connectionState();
  const auto process = m_coreProcess.processState();
  const bool isServer = (m_coreProcess.mode() == CoreMode::Server);
  if (process != Stopped && !ui->lineEditName->isHidden())
    cancelHostNameEdit();
  ui->btnEditName->setVisible(process == Stopped && ui->lineEditName->isHidden());
  ui->btnEditName->setEnabled(Settings::isWritable());
  if (process == Stopped || process == Started)
    updateNetworkInfo();
  m_statusBar->setStatus(connection, process, isServer);
}

void MainWindow::coreProcessStateChanged(ProcessState state)
{
  using enum ProcessState;
  updateStatus();
  if (state == Started) {
    qDebug() << "recording that core has started";
    Settings::setValue(Settings::Gui::AutoStartCore, true);
  }

  if (state == Started || state == Starting || state == RetryPending) {
    disconnect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger);
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStopCore, &QAction::trigger, Qt::UniqueConnection);

    ui->btnRestartCore->setEnabled(true);
    m_actionStartCore->setEnabled(false);
    m_actionRestartCore->setEnabled(true);
    m_actionStopCore->setEnabled(true);

    if (state == Starting) {
      saveSettings();
    }

  } else {
    disconnect(ui->btnToggleCore, &QPushButton::clicked, m_actionStopCore, &QAction::trigger);
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger, Qt::UniqueConnection);

    ui->btnRestartCore->setEnabled(false);
    m_actionStartCore->setEnabled(canRunCore());
    m_actionRestartCore->setEnabled(false);
    m_actionStopCore->setEnabled(false);
  }
  updateModeControlLabels();
  toggleCanRunCore(canRunCore());
}

void MainWindow::coreConnectionStateChanged(ConnectionState state)
{
  qDebug() << "core connection state changed:" << static_cast<int>(state);

  updateStatus();

  // always assume connection is not secure when connection changes
  // to anything except connected. the only way the padlock shows is
  // when the correct TLS version string is detected.
  if (state != ConnectionState::Connected) {
    secureSocket(false);
  } else if (isVisible()) {
    showFirstConnectedMessage();
  }
}

void MainWindow::serverBindAddressChanged(const QString &address)
{
  if (m_coreProcess.mode() != CoreMode::Server || (!address.isEmpty() && !m_coreProcess.isStarted()) ||
      address == m_serverBindAddress)
    return;
  m_serverBindAddress = address;
  updateNetworkInfo();

  const QHostAddress boundAddress(address);
  if (boundAddress.isNull() || boundAddress.isLoopback() ||
      Settings::value(Settings::Gui::ShownServerFirstStartMessage).toBool())
    return;
  // The core reports the address only after the listener starts successfully.
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  messages::showFirstServerStartMessage(this);
}

void MainWindow::updateLocalFingerprint()
{
  m_statusBar->setBtnFingerprintVisible(TlsUtility::isEnabled() && !m_fingerprint.data.isEmpty());
}

QString MainWindow::trustedFingerprintDatabase() const
{
  const bool isClient = m_coreProcess.mode() == CoreMode::Client;
  return isClient ? Settings::tlsTrustedServersDb() : Settings::tlsTrustedClientsDb();
}

bool MainWindow::generateCertificate()
{
  const auto certificate = Settings::value(Settings::Security::Certificate).toString();
  if (!QFile::exists(certificate) && !TlsUtility::generateCertificate()) {
    return false;
  }

  m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};

  updateLocalFingerprint();
  return true;
}

void MainWindow::generateCertificateAsync()
{
  m_certificateReady = false;

  // capture the inputs on the GUI thread; RSA key generation can block for
  // seconds and must not freeze window construction
  const auto keyLength = std::max(kMinimumRsaBits, Settings::value(Settings::Security::KeySize).toInt());
  const auto certPath = Settings::value(Settings::Security::Certificate).toString();

  setBusyCursor();
  QPointer<MainWindow> self(this);
  std::thread([self, keyLength, certPath] {
    const bool ok = TlsUtility::generateCertificateAt(certPath, keyLength);
    if (!self) {
      return;
    }

    QMetaObject::invokeMethod(
        self.data(),
        [self, ok] {
          if (!self) {
            return;
          }
          QApplication::restoreOverrideCursor();
          self->m_certificateReady = true;
          if (ok && TlsUtility::isCertValid()) {
            self->m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
            self->updateLocalFingerprint();
          } else {
            qCritical().noquote(
            ) << QCoreApplication::translate("GuiLogMessages", "failed to generate a tls certificate");
          }
        },
        Qt::QueuedConnection
    );
  }).detach();
}

void MainWindow::serverClientsChanged(const QStringList &clients)
{
  if (m_coreProcess.mode() != CoreMode::Server || !m_coreProcess.isStarted())
    return;
  m_statusBar->setServerClients(clients);
}

void MainWindow::daemonIpcClientConnectionFailed()
{
  if (property("daemonNoticeVisible").toBool())
    return;
  setProperty("daemonNoticeVisible", true);
  auto *message = new QMessageBox(this);
  message->setIcon(QMessageBox::Warning);
  message->setWindowTitle(tr("Background service offline"));
  auto *retry = message->addButton(tr("Retry"), QMessageBox::AcceptRole);
  message->addButton(tr("Ignore"), QMessageBox::RejectRole);
  auto *disable = message->addButton(tr("Disable"), QMessageBox::NoRole);
  message->setText(
      tr("<p>There was a problem finding the %1 background service (daemon).</p>"
         "<p>The background service makes %1 work with UAC prompts and the login screen.</p>"
         "<p>If %1 is installed, the service may have stopped. Restart it from Windows Services, then retry.</p>"
         "<p>If it was never installed, install %1 using its installer.</p>"
         "<p>Disable stops using the background service.</p>")
          .arg(kAppName)
  );
  connect(message, &QDialog::finished, this, [this, message, retry, disable] {
    setProperty("daemonNoticeVisible", false);
    if (message->clickedButton() == retry)
      m_coreProcess.retryDaemon();
    else if (message->clickedButton() == disable)
      Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);
  });
  showPage(message);
}
