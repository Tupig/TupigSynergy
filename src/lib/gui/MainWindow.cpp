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
#include "widgets/PageHost.h"
#include "widgets/StatusBar.h"

#ifdef SYNERGY_EXTRA_HEADER
#include "synergy/hooks/gui_hook.h"
#endif

#include <QApplication>
#include <QBoxLayout>
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
#include <QVariant>

#include <memory>
#include <thread>

#if defined(Q_OS_MACOS)
#include "platform/macos/OSXSDKHeaders.h"
#endif

using namespace synergy::gui;

MainWindow::MainWindow(const QString &instanceSocketName)
    : ui{std::make_unique<Ui::MainWindow>()},
      m_guiSocketName(instanceSocketName),
      m_coreProcess(m_serverConfig),
      m_trayIcon{new QSystemTrayIcon(this)},
      m_guiDupeChecker{new QLocalServer(this)},
      m_daemonIpcClient{new ipc::DaemonIpcClient(this)},
      m_actionAbout{new QAction(this)},
      m_actionCheckForUpdates{new QAction(this)},
      m_actionClearSettings{new QAction(this)},
      m_actionReportBug{new QAction(this)},
      m_actionMinimize{new QAction(this)},
      m_actionQuit{new QAction(this)},
      m_actionLog{new QAction(this)},
      m_actionRestore{new QAction(this)},
      m_actionSettings{new QAction(this)},
      m_actionStartCore{new QAction(this)},
      m_actionRestartCore{new QAction(this)},
      m_actionStopCore{new QAction(this)},
      m_networkMonitor{new NetworkMonitor(this)}
{
  ui->setupUi(this);
  setMinimumWidth(540);

  setWindowIcon(QIcon(QStringLiteral(":/image/app-icon.png")));

  m_logDock = ui->logPanel;
  m_statusBar = ui->statusBar;
  m_pageHost = ui->pageHost;
  connect(m_pageHost, &PageHost::contentSizeChanged, this, &MainWindow::fitToCurrentPage);

  // 纯图标按钮：返回与导出；其余按钮使用设计稿的小号 26px 图标+文字。
  for (auto *button : {ui->btnBack, ui->btnSaveServerConfig}) {
    button->setProperty("iconButton", true);
  }
  // 主页没有上一级，只有子页面显示返回箭头。
  ui->btnBack->setVisible(false);

  ui->spinPort->setValue(Settings::value(Settings::Core::Port).toInt());
  ui->spinPort->setEnabled(Settings::isWritable());
  connect(ui->spinPort, &QSpinBox::editingFinished, this, [this] {
    if (!Settings::isWritable())
      return;
    const int port = ui->spinPort->value();
    if (port == Settings::value(Settings::Core::Port).toInt())
      return;
    Settings::setValue(Settings::Core::Port, port);
    Settings::save();
    if (m_coreProcess.isStarted())
      m_coreProcess.restart();
  });

  connect(ui->btnBack, &QPushButton::clicked, this, [this] {
    // PageHost 把编辑器包在 QScrollArea 的容器里，必须按属性取回 QDialog。
    auto *area = qobject_cast<QScrollArea *>(ui->pageStack->currentWidget());
    auto *dialog = area ? area->property("pageEditor").value<QDialog *>() : nullptr;
    if (dialog != nullptr)
      dialog->reject();
  });
  connect(ui->pageHost, &PageHost::editorVisibilityChanged, this, [this](bool visible) {
    ui->btnBack->setVisible(visible);
    ui->logPanel->setVisible(!visible);
  });
  connect(ui->pageHost, &PageHost::titleChanged, this, [this](const QString &title) {
    ui->pageTitle->setText(title.isEmpty() ? tr("Devices & Connection") : title);
  });
  connect(ui->btnSettingsShortcut, &QPushButton::clicked, this, [this] { m_actionSettings->trigger(); });
  connect(ui->btnUpdatesShortcut, &QPushButton::clicked, this, [this] { m_actionCheckForUpdates->trigger(); });
  connect(ui->btnAboutShortcut, &QPushButton::clicked, this, [this] { m_actionAbout->trigger(); });

  // Setup Actions
  m_actionAbout->setMenuRole(QAction::AboutRole);
  m_actionAbout->setIcon(synergy::gui::appIcon(QStringLiteral("help-about")));

  m_actionMinimize->setIcon(synergy::gui::appIcon(QStringLiteral("window-minimize-pip")));
  m_actionRestore->setIcon(synergy::gui::appIcon(QStringLiteral("window-restore-pip")));

  if (!synergy::platform::isWindows()) {
    m_actionQuit->setShortcut(QKeySequence::Quit);
  }

  m_actionQuit->setIcon(synergy::gui::appIcon("application-exit"));
  m_actionQuit->setMenuRole(QAction::QuitRole);

  m_actionClearSettings->setIcon(synergy::gui::appIcon(QStringLiteral("edit-clear-all")));
  // ApplicationSpecificRole is macOS-only: Qt merges these actions into the
  // application menu (updates and maintenance belong next to "Preferences").
  // Other platforms ignore the role and keep them in the Help menu.
  m_actionClearSettings->setMenuRole(QAction::ApplicationSpecificRole);

  m_actionCheckForUpdates->setIcon(synergy::gui::appIcon(QStringLiteral("system-software-update")));
  m_actionCheckForUpdates->setMenuRole(QAction::ApplicationSpecificRole);

  m_actionSettings->setIcon(synergy::gui::appIcon(QStringLiteral("configure")));
  m_actionSettings->setMenuRole(QAction::PreferencesRole);

  m_actionStartCore->setIcon(synergy::gui::appIcon(QStringLiteral("system-run")));
  m_actionStartCore->setMenuRole(QAction::NoRole);

  m_actionRestartCore->setEnabled(false);
  m_actionRestartCore->setIcon(synergy::gui::appIcon(QStringLiteral("view-refresh")));
  m_actionRestartCore->setMenuRole(QAction::NoRole);

  m_actionStopCore->setIcon(synergy::gui::appIcon(QStringLiteral("process-stop")));
  m_actionStopCore->setMenuRole(QAction::NoRole);

  m_actionReportBug->setIcon(synergy::gui::appIcon(QStringLiteral("tools-report-bug")));
  m_actionReportBug->setMenuRole(QAction::NoRole);

  // Setup the Instance Checking
  // In case of a previous crash remove first
  QLocalServer::removeServer(m_guiSocketName);
  m_guiDupeChecker->listen(m_guiSocketName);

  createMenuBar();
  setupControls();
  updateText();
  connectSlots();
  setupTrayIcon();
  updateScreenName();

  qDebug().noquote() << "active settings path:" << Settings::settingsPath();

  // Force generation of a certificate on the host
  if (TlsUtility::isEnabled()) {
    if (Settings::value(Settings::Security::KeySize).toInt() < kMinimumRsaBits) {
      QTimer::singleShot(0, this, [this] {
        showPage(new QMessageBox(
            QMessageBox::Information, kAppName,
            tr("Your current TLS key is smaller than the minimum allowed size, A new key 2048-bit key will be "
               "generated."),
            QMessageBox::Ok, this
        ));
      });
      Settings::setValue(Settings::Security::KeySize, kMinimumRsaBits);
    }
    if (!TlsUtility::isCertValid()) {
      generateCertificateAsync();
    } else {
      m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
    }
  }

  applyConfig();
  m_statusBar->setSecurityIcon(TlsUtility::isEnabled());
  restoreWindow();

  // Check GitHub Releases after the window is up; the check is non-blocking and
  // only surfaces a dialog when a newer version exists or the user asks.
  QTimer::singleShot(3000, this, [this]() {
    if (Settings::value(Settings::Gui::CheckForUpdates).toBool()) {
      checkForUpdates(false);
    }
  });

#ifdef SYNERGY_EXTRA_HEADER
  synergy::hooks::onMainWindow(this, &m_coreProcess);
#endif
}
MainWindow::~MainWindow()
{
  // Stop network monitoring
  if (m_networkMonitor) {
    m_networkMonitor->stopMonitoring();
  }

  m_guiDupeChecker->close();
  m_coreProcess.cleanup();
}

void MainWindow::restoreWindow()
{
  auto windowGeometry = Settings::value(Settings::Gui::WindowGeometry).toRect();
  const auto totalGeometry = QGuiApplication::primaryScreen()->availableGeometry();
  if (!windowGeometry.isValid()) {
    resize(Ui::MainWindow::kWindowWidth, height());
    windowGeometry = geometry();
  } else {
    setGeometry(windowGeometry);
  }
  m_pageHost->requestContentFit();
  m_expandedSize = geometry().size();

  if (!totalGeometry.contains(windowGeometry)) {
    QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
    move(screenGeometry.center() - rect().center());
  }
}

void MainWindow::setupControls()
{
  secureSocket(false);

  ui->btnConfigureServer->setIcon(synergy::gui::appIcon(QStringLiteral("configure")));
  ui->btnConfigureClient->setIcon(synergy::gui::appIcon(QStringLiteral("configure")));

  if (Settings::value(Settings::Core::LastVersion).toString() != kVersion) {
    Settings::setValue(Settings::Core::LastVersion, kVersion);
  }

  m_logDock->setCollapsed(!Settings::value(Settings::Gui::LogExpanded).toBool());

  ui->serverOptions->setVisible(false);
  ui->clientOptions->setVisible(false);

  const auto coreMode = Settings::value(Settings::Core::CoreMode).value<Settings::CoreMode>();
  ui->rbModeClient->setChecked(coreMode == Settings::CoreMode::Client);
  ui->rbModeServer->setChecked(coreMode == Settings::CoreMode::Server);

  ui->lineEditName->setValidator(new QRegularExpressionValidator(m_nameRegEx, this));
  ui->lineEditName->setVisible(false);
  ui->btnSaveName->hide();
  ui->btnCancelName->hide();
  ui->lineEditName->installEventFilter(this);
  ui->btnSaveName->installEventFilter(this);
  ui->btnCancelName->installEventFilter(this);

  if (synergy::platform::isMac()) {
    ui->rbModeServer->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->rbModeClient->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->btnSaveServerConfig->setFixedWidth(ui->btnSaveServerConfig->height());
  } else {
    ui->btnSaveServerConfig->setIconSize(QSize(22, 22));
  }
}

//////////////////////////////////////////////////////////////////////////////
// Begin slots
//////////////////////////////////////////////////////////////////////////////

// remember: using queued connection allows the render loop to run before
// executing the slot. the default is to instantly call the slot when the
// signal is emitted from the thread that owns the receiver's object.
void MainWindow::connectSlots()
{
  connect(Settings::instance(), &Settings::serverSettingsChanged, this, &MainWindow::serverConfigSaving);
  connect(Settings::instance(), &Settings::settingsChanged, this, &MainWindow::settingsChanged);

  connect(&m_coreProcess, &CoreProcess::error, this, &MainWindow::coreProcessError);
  connect(&m_coreProcess, &CoreProcess::logLine, this, &MainWindow::handleLogLine);
  connect(
      &m_coreProcess, &CoreProcess::processStateChanged, this, &MainWindow::coreProcessStateChanged,
      Qt::QueuedConnection
  );
  connect(&m_coreProcess, &CoreProcess::connectionStateChanged, this, &MainWindow::coreConnectionStateChanged);
  connect(&m_coreProcess, &CoreProcess::secureSocket, this, &MainWindow::secureSocket);
  connect(
      &m_coreProcess, &CoreProcess::daemonIpcClientConnectionFailed, this, &MainWindow::daemonIpcClientConnectionFailed
  );
  connect(&m_coreProcess, &CoreProcess::securityLevelChanged, m_statusBar, &StatusBar::setSecurityLevel);

  connect(m_actionAbout, &QAction::triggered, this, &MainWindow::openAboutDialog);
  connect(m_actionCheckForUpdates, &QAction::triggered, this, [this]() { checkForUpdates(true); });
  connect(m_actionClearSettings, &QAction::triggered, this, &MainWindow::clearSettings);
  connect(m_actionReportBug, &QAction::triggered, this, &MainWindow::openHelpUrl);
  connect(m_actionMinimize, &QAction::triggered, this, &MainWindow::hide);

  connect(m_actionQuit, &QAction::triggered, this, &MainWindow::close);
  connect(m_actionRestore, &QAction::triggered, this, &MainWindow::showAndActivate);
  connect(m_actionSettings, &QAction::triggered, this, &MainWindow::openSettings);
  connect(m_actionStartCore, &QAction::triggered, this, &MainWindow::startCore);
  connect(m_actionRestartCore, &QAction::triggered, this, &MainWindow::resetCore);
  connect(m_actionStopCore, &QAction::triggered, this, &MainWindow::stopCore);

  // Mac os tray will only show a menu
  if (!synergy::platform::isMac())
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayIconActivated);

  connect(&m_coreProcess, &CoreProcess::connectedClientsChanged, this, &MainWindow::serverClientsChanged);
  connect(&m_coreProcess, &CoreProcess::unrecognisedClient, this, &MainWindow::handleUnrecognisedClient);
  connect(&m_coreProcess, &CoreProcess::connectionRefused, this, &MainWindow::handleConnectionRefused);
  connect(&m_coreProcess, &CoreProcess::retryIn, this, &MainWindow::updateTimeoutDelay);
  connect(&m_coreProcess, &CoreProcess::peerFingerprint, this, &MainWindow::handlePeerFingerprint);
  connect(&m_coreProcess, &CoreProcess::missingKeyboardLayouts, this, &MainWindow::handleMissingKeyboardLayouts);

  if (Settings::value(Settings::Gui::AutoStartCore).toBool()) {
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStopCore, &QAction::trigger, Qt::UniqueConnection);
  } else {
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger, Qt::UniqueConnection);
  }

  connect(ui->btnRestartCore, &QPushButton::clicked, this, &MainWindow::resetCore);

  connect(ui->lineHostname, &QLineEdit::returnPressed, ui->btnRestartCore, &QPushButton::click);
  connect(ui->lineHostname, &QLineEdit::textChanged, this, &MainWindow::remoteHostChanged);
  // debounce the settings write so typing a host name does not rewrite the
  // whole settings file on every keystroke
  m_remoteHostSaveTimer.setSingleShot(true);
  m_remoteHostSaveTimer.setInterval(400);
  connect(&m_remoteHostSaveTimer, &QTimer::timeout, this, &MainWindow::saveRemoteHost);
  connect(ui->lineHostname, &QLineEdit::editingFinished, this, &MainWindow::saveRemoteHost);

  connect(ui->btnSaveServerConfig, &QPushButton::clicked, this, &MainWindow::saveServerConfig);
  connect(ui->btnConfigureServer, &QPushButton::clicked, this, [this] { showConfigureServer(""); });
  connect(ui->btnConfigureClient, &QPushButton::clicked, this, [this] { showConfigureClient(); });
  connect(ui->lblComputerName, &QLabel::linkActivated, this, &MainWindow::openSettings);

  connect(ui->rbModeServer, &QRadioButton::toggled, this, &MainWindow::coreModeToggled);
  connect(ui->rbModeClient, &QRadioButton::toggled, this, &MainWindow::coreModeToggled);

  connect(m_logDock, &LogPanel::collapsedChanged, this, [this](bool collapsed) {
    Settings::setValue(Settings::Gui::LogExpanded, !collapsed);
    Settings::setValue(Settings::Gui::WindowGeometry, geometry());
  });

  connect(m_statusBar, &StatusBar::requestShowMyFingerprints, this, &MainWindow::showMyFingerprint);

  connect(m_guiDupeChecker, &QLocalServer::newConnection, this, &MainWindow::showAndActivate);

  connect(ui->btnEditName, &QPushButton::clicked, this, &MainWindow::showHostNameEditor);

  // Focus loss keeps the draft. Only the explicit Save/Enter path can commit it.
  connect(ui->lineEditName, &QLineEdit::returnPressed, this, &MainWindow::setHostName);
  connect(ui->btnSaveName, &QPushButton::clicked, this, &MainWindow::setHostName);
  connect(ui->btnCancelName, &QPushButton::clicked, this, &MainWindow::cancelHostNameEdit);

  connect(m_networkMonitor, &NetworkMonitor::ipAddressesChanged, this, &MainWindow::updateIpLabel);
}

void MainWindow::toggleLogVisible(bool visible)
{
  // 视图菜单与工具条共用同一折叠状态；收起时只保留最新一条。
  m_logDock->setCollapsed(!visible);
}

void MainWindow::settingsChanged(const QString &key)
{
  if (key == Settings::Log::Level) {
    m_coreProcess.applyLogLevel();
    return;
  }

  if (key == Settings::Core::ComputerName)
    updateScreenName();

  if ((key == Settings::Security::Certificate) || (key == Settings::Security::KeySize) ||
      (key == Settings::Security::TlsEnabled) || (key == Settings::Security::CheckPeers)) {
    if (TlsUtility::isEnabled() && !TlsUtility::isCertValid()) {
      qWarning() << tr("invalid certificate, generating a new one");
      TlsUtility::generateCertificate();
    }
    updateSecurityIcon(m_statusBar->securityIconVisible());
    return;
  }
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
  if (reason != QSystemTrayIcon::Trigger)
    return;
  isVisible() ? hide() : showAndActivate();
}

void MainWindow::showPage(QDialog *editor)
{
  m_pageHost->push(editor);
  showAndActivate();
}

void MainWindow::openAboutDialog()
{
  showPage(new AboutDialog(this));
}

void MainWindow::openHelpUrl() const
{
  QDesktopServices::openUrl(QUrl(kUrlHelp));
}

void MainWindow::openSettings()
{
  if (m_pageHost->activateEditor(SettingsDialog::staticMetaObject)) {
    showAndActivate();
    return;
  }
  auto *dialog = new SettingsDialog(this, m_serverConfig);
  connect(dialog, &QDialog::accepted, this, [this] {
    Settings::save();
    applyConfig();
    ui->spinPort->setValue(Settings::value(Settings::Core::Port).toInt());
    if (m_coreProcess.isStarted())
      m_coreProcess.restart();
  });
  showPage(dialog);
}

void MainWindow::resetCore()
{
#ifdef SYNERGY_EXTRA_HEADER
  if (!synergy::hooks::onCoreStart()) {
    return;
  }
#endif

  m_coreProcess.restart();
}

void MainWindow::showMyFingerprint()
{
  showPage(new FingerprintDialog(this, m_fingerprint));
}

void MainWindow::updateSecurityIcon(bool visible)
{
  m_statusBar->setSecurityIconVisible(visible);
  if (!visible)
    return;
  m_statusBar->setSecurityIcon(TlsUtility::isEnabled());
}

void MainWindow::updateNetworkInfo()
{
  const auto mode = m_coreProcess.mode();
  if (mode == CoreMode::None) {
    return;
  }

  if (mode == CoreMode::Server)
    updateIpLabel(NetworkMonitor::validAddresses());
  else
    updateIpLabel({Settings::value(Settings::Core::Interface).toString()});
}

void MainWindow::serverConnectionConfigureClient(const QString &clientName)
{
  if (m_serverConfigDialogVisible)
    return;
  auto *dialog = new ServerConfigDialog(this, m_serverConfig);
  if (!dialog->addClient(clientName)) {
    dialog->deleteLater();
    return;
  }
  m_serverConfigDialogVisible = true;
  connect(dialog, &QDialog::accepted, this, [this] { m_coreProcess.restart(); });
  connect(dialog, &QDialog::finished, this, [this] { m_serverConfigDialogVisible = false; });
  showPage(dialog);
}

//////////////////////////////////////////////////////////////////////////////
// End slots
//////////////////////////////////////////////////////////////////////////////

void MainWindow::open()
{
  if (!Settings::value(Settings::Gui::Autohide).toBool())
    showAndActivate();
  else if (synergy::platform::isMac())
    // macOS to call hide after this function ends
    QTimer::singleShot(1, this, &MainWindow::hide);
  else
    hide();

  // if a critical error was shown just before the main window (i.e. on app
  // load), it will be hidden behind the main window. therefore we need to raise
  // it up in front of the main window.
  // HACK: because the `onShown` event happens just as the window is shown, the
  // message box has a chance of being raised under the main window. to solve
  // this we delay the error dialog raise by a split second. this seems a bit
  // hacky and fragile, so maybe there's a better approach.
  const auto kCriticalDialogDelay = 100;
  QTimer::singleShot(kCriticalDialogDelay, this, &messages::raiseCriticalDialog);

  if (Settings::value(Settings::Gui::AutoStartCore).toBool()) {
    if (!m_certificateReady) {
      qWarning() << "skipping core auto start, tls certificate is still being generated";
      return;
    }
    if (m_coreProcess.mode() == CoreMode::None) {
      qWarning() << "skipping core auto start, mode is not set";
      return;
    }
    if (ui->rbModeClient->isChecked() && ui->lineHostname->text().isEmpty())
      return;
    startCore();
  }
}

void MainWindow::showFirstConnectedMessage()
{
  if (Settings::value(Settings::Gui::ShownFirstConnectedMessage).toBool())
    return;
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  messages::showFirstConnectedMessage(this);
}

void MainWindow::updateText()
{
  m_menuFile->setTitle(tr("&File"));
  if (m_menuEdit)
    m_menuEdit->setTitle(tr("&Edit"));
  m_menuView->setTitle(tr("&View"));
#if defined(Q_OS_MACOS)
  // AppKit injects a search field into the menu titled "Help" every time it
  // opens. It drives the system help index, which this application does not
  // ship, and it alone makes the menu several times wider than its single
  // item. A different title keeps the menu as wide as its contents.
  m_menuHelp->setTitle(tr("&Support"));
#else
  m_menuHelp->setTitle(tr("&Help"));
#endif
  m_actionClearSettings->setText(tr("Clear settings"));
  m_actionCheckForUpdates->setText(tr("Check for &Updates"));
  m_actionReportBug->setText(tr("Get help"));
  m_actionMinimize->setText(tr("&Minimize to tray"));
  m_actionQuit->setText(tr("&Quit"));
  m_actionLog->setText(tr("Show &Log"));
  //: %1 will be the replaced with the appname
  m_actionRestore->setText(tr("&Open %1").arg(kAppName));
  m_actionSettings->setText(tr("&Preferences"));
  m_actionStartCore->setText(tr("&Start"));
  m_actionRestartCore->setText(tr("Rest&art"));
  m_actionStopCore->setText(tr("S&top"));
  //: %1 will be the replaced with the appname
  m_actionAbout->setText(tr("About %1...").arg(kAppName));
}

void MainWindow::toggleCanRunCore(bool enableButtons)
{
  enableButtons = enableButtons && ui->lineEditName->isHidden();
  const auto state = m_coreProcess.processState();
  const bool active =
      state == ProcessState::Starting || state == ProcessState::Started || state == ProcessState::RetryPending;
  const bool stopped = state == ProcessState::Stopped;
  ui->btnToggleCore->setEnabled(active || (enableButtons && stopped));
  ui->btnRestartCore->setEnabled(enableButtons && active);
  m_actionStartCore->setEnabled(enableButtons && stopped);
  m_actionRestartCore->setEnabled(enableButtons && active);
  m_actionStopCore->setEnabled(active);
}

void MainWindow::updateIpLabel(const QStringList &addresses)
{
  const auto mode = m_coreProcess.mode();
  if (mode == CoreMode::None) {
    return;
  }

  const bool serverStarted = m_coreProcess.isStarted();
  const bool fixedIP = !Settings::value(Settings::Core::Interface).toString().isEmpty();

  if ((!fixedIP && addresses.isEmpty() && !serverStarted) || (serverStarted && m_serverStartSuggestedIP.isEmpty())) {
    ui->lblIpAddresses->setText(tr("No IP Detected"));
    ui->lblIpAddresses->setToolTip(tr("Unable to detect an IP address. Check your network connection is active."));
    return;
  }

  QString labelText = tr("Local IP") + QStringLiteral("  ");
  QString toolTipText = tr("<p>If connecting via the hostname fails, try %1</p>");

  const bool filterIpList = (serverStarted || fixedIP);
  const QRegularExpression ipListFilter(filterIpList ? QStringLiteral("(%1)").arg(m_serverStartIPs.join("|")) : "");
  const QStringList ipList = addresses.filter(ipListFilter);

  bool IPValid = true;
  if ((filterIpList && (m_serverStartSuggestedIP != m_currentIpAddress)) ||
      !ipList.contains(m_serverStartSuggestedIP)) {
    IPValid = !ipList.isEmpty();
  }

  if (IPValid) {
    m_currentIpAddress = ipList.first();
    labelText.append(m_currentIpAddress);
  } else {
    labelText.append(m_serverStartSuggestedIP);
    toolTipText.append(tr("\nA bound IP is now invalid, you may need to restart the server."));
  }

  if (ipList.count() < 2 || fixedIP) {
    toolTipText = toolTipText.arg(tr("the suggested IP."));
  } else {
    toolTipText = toolTipText.arg(tr("one of the following IPs:<br/>%1").arg(ipList.join("<br/>")));
  }

  if (mode == CoreMode::Server && !fixedIP) {
    toolTipText.append(
        tr("\n<p>%1 listens on all network interfaces because no IP is selected in the settings.</p>").arg(kAppName)
    );
  }

  ui->lblIpAddresses->setText(labelText);
  ui->lblIpAddresses->setToolTip(mode == CoreMode::Server ? toolTipText : QString());
}

void MainWindow::updateTimeoutDelay(int newDelay)
{
  m_statusBar->setConnectionInterval(newDelay);
}

bool MainWindow::canRunCore() const
{
  const auto mode = m_coreProcess.mode();
  const bool isServer = mode == Settings::CoreMode::Server;
  const bool isClient = mode == Settings::CoreMode::Client;
  return (((isServer || isClient) && (isClient && !ui->lineHostname->text().isEmpty())) || isServer);
}
