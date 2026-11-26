/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2024 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QKeySequence>
#include <QLocalServer>
#include <QMainWindow>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSystemTrayIcon>
#include <QTimer>
#include <memory>

#include "common/Constants.h"
#include "config/ServerConfig.h"
#include "gui/core/CoreProcess.h"
#include "gui/core/NetworkMonitor.h"
#include "net/Fingerprint.h"

#ifdef Q_OS_MACOS
#include "gui/OSXHelpers.h"
#endif

class QAction;
class QMenu;

class LogPanel;
class MessagePage;
class PageHost;
class QDialog;
class StatusBar;

namespace Ui {
class MainWindow;
}

namespace synergy::gui::update {
class UpdateChecker;
struct UpdateInfo;
} // namespace synergy::gui::update

namespace synergy::gui::ipc {
class DaemonIpcClient;
}

class MainWindow : public QMainWindow
{
  friend class MainWindowFlowTests;

  using ConnectionState = synergy::core::ConnectionState;
  using CoreMode = Settings::CoreMode;
  using CoreProcess = synergy::gui::CoreProcess;
  using NetworkMonitor = synergy::gui::NetworkMonitor;
  using ProcessState = synergy::core::ProcessState;

  Q_OBJECT

public:
  enum class LogLevel
  {
    Error,
    Info
  };

public:
  explicit MainWindow(
      const QString &instanceSocketName = QString::fromUtf8(kGuiSocketName),
      std::unique_ptr<QLocalServer> activationServer = {}
  );
  ~MainWindow() override;

  [[nodiscard]] CoreMode coreMode() const
  {
    return m_coreProcess.mode();
  }
  void open();
  synergy::gui::ServerConfig &serverConfig()
  {
    return m_serverConfig;
  }

  void hide();

protected:
  virtual synergy::gui::update::UpdateChecker *createUpdateChecker();
  void changeEvent(QEvent *e) override;
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  /**
   * @brief updateText Update all text not in the UI
   */
  void updateText();
  void showPage(QDialog *editor);
  void fitToCurrentPage();
  bool m_fittingPage = false;
  PageHost *m_pageHost = nullptr;
  void toggleLogVisible(bool visible);

  void settingsChanged(const QString &key = QString());
  void serverConfigSaving();
  void coreProcessError(CoreProcess::Error error);
  void coreConnectionStateChanged(ConnectionState state);
  void coreProcessStateChanged(ProcessState state);
  void serverBindAddressChanged(const QString &address);

  void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void serverConnectionConfigureClient(const QString &clientName);

  void clearSettings();
  void openAboutDialog();
  void openHelpUrl() const;
  void openSettings();
  enum class CoreStartSource
  {
    Manual,
    Automatic
  };
  void startCore(CoreStartSource source);
  void stopCore();
  bool saveServerConfig();
  void resetCore();
  void clearDeclinedPeerFingerprints();

  void showMyFingerprint();
  void updateSecurityIcon(bool visible);
  void updateNetworkInfo();

  void coreModeToggled(bool checked);
  void updateModeControls();
  void updateModeControlLabels();
  std::unique_ptr<Ui::MainWindow> ui;

  void createMenuBar();
  void setupTrayIcon();
  void applyConfig();
  void setTrayIcon();
  void checkForUpdates(bool manual);
  bool finishUpdateCheck();
  bool activateUpdatePage();
  void showAvailableUpdate();
  synergy::gui::update::UpdateChecker *m_updateChecker = nullptr;
  QPointer<MessagePage> m_updateCheckPage;
  std::unique_ptr<synergy::gui::update::UpdateInfo> m_availableUpdate;
  void handleUnrecognisedClient(const QString &clientName);
  void handleConnectionRefused(synergy::core::ConnectionRefusal reason);
  void handlePeerFingerprint(const QString &fingerprint);
  void handleMissingKeyboardLayouts(const QString &layouts);
  void closeEvent(QCloseEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void secureSocket(bool secureSocket);
  void connectSlots();
  void handleLogLine(const QString &line);
  void updateLocalFingerprint();
  void updateScreenName();
  void saveSettings() const;
  void showConfigureServer(const QString &message);
  void showConfigureClient();
  void restoreWindow();
  void setupControls();
  void showFirstConnectedMessage();
  void updateStatus();
  void showAndActivate();
  void showHostNameEditor();
  void setHostName();
  void cancelHostNameEdit();
  void daemonIpcClientConnectionFailed();
  void toggleCanRunCore(bool enableButtons);
  void remoteHostChanged(const QString &newRemoteHost);
  void saveRemoteHost();
  void updateIpLabel(const QStringList &addresses);
  void updateTimeoutDelay(int newDelay);

  bool canRunCore() const;

  /**
   * @brief trustedFingerprintDatabase get the FingerprintDatabase for the trusted clients or trusted servers.
   * @return The path to the trusted fingerprint file
   */
  QString trustedFingerprintDatabase() const;

  /**
   * @brief generateCertificate Generate a new certificate
   * @return true when successful
   */
  bool generateCertificate();

  //! Generate the tls certificate on a worker thread so key generation does
  //! not freeze the GUI. The auto start of the core waits for completion.
  void generateCertificateAsync();

  Fingerprint m_fingerprint;

  //! single-shot debounce for the remote host settings write
  QTimer m_remoteHostSaveTimer;
  //! false while the tls certificate is being generated on a worker thread
  bool m_certificateReady = true;

  void serverClientsChanged(const QStringList &clients);

  // An isolated observer supplies its own endpoint; normal startup keeps the production name.
  const QString m_guiSocketName;
  inline static const auto m_nameRegEx = QRegularExpression(QStringLiteral("^[\\w\\-_\\.]{0,255}$"));

  bool m_secureSocket = false;
  bool m_saveOnExit = true;
#ifdef Q_OS_MACOS
  bool m_resumeSharingAfterUpdate = false;
#endif
  bool m_clientErrorVisible = false;
  synergy::gui::ServerConfig m_serverConfig;
  synergy::gui::CoreProcess m_coreProcess;
  QSet<QString> m_ignoredClients;
  bool m_newClientPromptShowing = false;
  bool m_serverConfigDialogVisible = false;
  struct PeerFingerprintPrompts
  {
    QSet<QString> pending;
    QSet<QString> declined;
  };
  PeerFingerprintPrompts m_clientFingerprintPrompts;
  PeerFingerprintPrompts m_serverFingerprintPrompts;
  QSystemTrayIcon *m_trayIcon = nullptr;
  QLocalServer *m_guiDupeChecker = nullptr;
  synergy::gui::ipc::DaemonIpcClient *m_daemonIpcClient = nullptr;

  LogPanel *m_logDock;
  StatusBar *m_statusBar = nullptr;

  // Window Menu
  QMenu *m_menuFile = nullptr;
  QMenu *m_menuEdit = nullptr;
  QMenu *m_menuView = nullptr;
  QMenu *m_menuHelp = nullptr;

  // Window Actions
  QAction *m_actionAbout = nullptr;
  QAction *m_actionCheckForUpdates = nullptr;
  QAction *m_actionClearSettings = nullptr;
  QAction *m_actionReportBug = nullptr;
  QAction *m_actionMinimize = nullptr;
  QAction *m_actionQuit = nullptr;
  QAction *m_actionLog = nullptr;
  QAction *m_actionRestore = nullptr;
  QAction *m_actionSettings = nullptr;
  QAction *m_actionStartCore = nullptr;
  QAction *m_actionRestartCore = nullptr;
  QAction *m_actionStopCore = nullptr;
  QKeySequence m_startCoreShortcut;
  QKeySequence m_restartCoreShortcut;

  // Network monitoring
  NetworkMonitor *m_networkMonitor = nullptr;
  QString m_serverBindAddress;
};
