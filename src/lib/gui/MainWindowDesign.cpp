/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MainWindowDesign.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

QPushButton *smallButton(QWidget *parent, const QString &name, const QString &text, const QString &icon)
{
  auto *button = new QPushButton(text, parent);
  button->setObjectName(name);
  button->setProperty("small", true);
  button->setIcon(QIcon::fromTheme(icon));
  return button;
}

QPushButton *iconButton(QWidget *parent, const QString &name, const QString &icon)
{
  auto *button = new QPushButton(parent);
  button->setObjectName(name);
  button->setProperty("iconButton", true);
  button->setIcon(QIcon::fromTheme(icon));
  return button;
}

} // namespace

void Ui::MainWindow::setupUi(QMainWindow *window)
{
  window->setObjectName(QStringLiteral("MainWindow"));
  window->resize(kWindowWidth, 470);

  topLevelWidget = new QWidget(window);
  topLevelWidget->setObjectName(QStringLiteral("appRoot"));
  auto *root = new QVBoxLayout(topLevelWidget);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  // 页面条 36px：返回 + 标题 + 右侧快捷入口
  pageBar = new QWidget(topLevelWidget);
  pageBar->setObjectName(QStringLiteral("pageBar"));
  pageBar->setFixedHeight(kPageBarHeight);
  auto *pageBarLayout = new QHBoxLayout(pageBar);
  pageBarLayout->setContentsMargins(12, 4, 12, 4);
  pageBarLayout->setSpacing(kGap);

  btnBack = iconButton(pageBar, QStringLiteral("btnBack"), QStringLiteral("go-previous"));
  btnBack->setToolTip(QObject::tr("Back"));
  btnBack->setVisible(false);
  pageBarLayout->addWidget(btnBack);

  pageTitle = new QLabel(QObject::tr("Devices & Connection"), pageBar);
  pageTitle->setObjectName(QStringLiteral("pageTitle"));
  pageBarLayout->addWidget(pageTitle);
  pageBarLayout->addStretch(1);

  btnSettingsShortcut = smallButton(
      pageBar, QStringLiteral("btnSettingsShortcut"), QObject::tr("Preferences"), QStringLiteral("configure")
  );
  btnUpdatesShortcut = smallButton(
      pageBar, QStringLiteral("btnUpdatesShortcut"), QObject::tr("Updates"), QStringLiteral("system-software-update")
  );
  btnAboutShortcut =
      smallButton(pageBar, QStringLiteral("btnAboutShortcut"), QObject::tr("About"), QStringLiteral("help-about"));
  pageBarLayout->addWidget(btnSettingsShortcut);
  pageBarLayout->addWidget(btnUpdatesShortcut);
  pageBarLayout->addWidget(btnAboutShortcut);
  root->addWidget(pageBar);

  // 页面宿主：主页与子页面共用同一内容区
  pageHost = new PageHost(topLevelWidget);
  auto *pageHostLayout = new QVBoxLayout(pageHost);
  pageHostLayout->setContentsMargins(0, 0, 0, 0);
  pageHostLayout->setSpacing(0);
  pageStack = new QStackedWidget(pageHost);
  pageStack->setObjectName(QStringLiteral("pageStack"));
  pageHostLayout->addWidget(pageStack);
  root->addWidget(pageHost, 1);

  workspace = new QScrollArea(pageStack);
  workspace->setObjectName(QStringLiteral("workspace"));
  workspace->setFrameShape(QFrame::NoFrame);
  workspace->setWidgetResizable(true);
  workspace->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto *workspaceContents = new QWidget(workspace);
  workspaceContents->setObjectName(QStringLiteral("workspaceContents"));
  auto *workspaceLayout = new QVBoxLayout(workspaceContents);
  workspaceLayout->setContentsMargins(kWorkspacePadding, kWorkspacePadding, kWorkspacePadding, kWorkspacePadding);
  workspaceLayout->setSpacing(kGap);
  workspace->setWidget(workspaceContents);
  pageStack->addWidget(workspace);

  // 名称行
  auto *identity = new QWidget(workspaceContents);
  identity->setObjectName(QStringLiteral("identity"));
  auto *identityLayout = new QHBoxLayout(identity);
  identityLayout->setContentsMargins(0, 0, 0, 0);
  identityLayout->setSpacing(kGap);
  lblComputerName = new QLabel(identity);
  lblComputerName->setObjectName(QStringLiteral("lblComputerName"));
  lblComputerName->setTextFormat(Qt::PlainText);
  identityLayout->addWidget(lblComputerName);
  lineEditName = new QLineEdit(identity);
  lineEditName->setObjectName(QStringLiteral("lineEditName"));
  lineEditName->setMaximumWidth(200);
  lineEditName->setMaxLength(255);
  lineEditName->setToolTip(QObject::tr("Use Enter to save the name change."));
  identityLayout->addWidget(lineEditName);
  btnEditName =
      smallButton(identity, QStringLiteral("btnEditName"), QObject::tr("Edit"), QStringLiteral("document-edit"));
  btnEditName->setToolTip(QObject::tr("Edit Computer Name"));
  identityLayout->addWidget(btnEditName);
  identityLayout->addStretch(1);
  lblIpAddresses = new QLabel(identity);
  lblIpAddresses->setObjectName(QStringLiteral("lblIpAddresses"));
  lblIpAddresses->setTextFormat(Qt::RichText);
  identityLayout->addWidget(lblIpAddresses);
  workspaceLayout->addWidget(identity);

  // 连接卡片
  auto *card = new QFrame(workspaceContents);
  card->setObjectName(QStringLiteral("card"));
  groupBox = card;
  auto *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(12, 10, 12, 10);
  cardLayout->setSpacing(kGap);

  auto *modes = new QHBoxLayout;
  modes->setSpacing(6);
  rbModeServer = new ModeCard(card);
  rbModeServer->setObjectName(QStringLiteral("rbModeServer"));
  rbModeServer->setText(QCoreApplication::translate("ModeCard", "Server"));
  rbModeClient = new ModeCard(card);
  rbModeClient->setObjectName(QStringLiteral("rbModeClient"));
  rbModeClient->setText(QCoreApplication::translate("ModeCard", "Client"));
  modes->addWidget(rbModeServer);
  modes->addWidget(rbModeClient);
  cardLayout->addLayout(modes);

  clientOptions = new QWidget(card);
  clientOptions->setObjectName(QStringLiteral("clientOptions"));
  auto *clientLayout = new QHBoxLayout(clientOptions);
  clientLayout->setContentsMargins(0, 0, 0, 0);
  clientLayout->setSpacing(kGap);
  m_pLabelServerName = new QLabel(QObject::tr("Connect to:"), clientOptions);
  m_pLabelServerName->setObjectName(QStringLiteral("m_pLabelServerName"));
  clientLayout->addWidget(m_pLabelServerName);
  lineHostname = new QLineEdit(clientOptions);
  lineHostname->setObjectName(QStringLiteral("lineHostname"));
  lineHostname->setToolTip(
      QObject::tr("Hostname or IP address of the server computer. May contain a comma separated list.")
  );
  clientLayout->addWidget(lineHostname, 1);
  spinPort = new QSpinBox(clientOptions);
  spinPort->setObjectName(QStringLiteral("spinPort"));
  spinPort->setAccessibleName(QObject::tr("Port"));
  spinPort->setRange(1, 65535);
  spinPort->setValue(22211);
  spinPort->setMaximumWidth(88);
  clientLayout->addWidget(spinPort);
  cardLayout->addWidget(clientOptions);

  lblNoMode = new QLabel(QObject::tr("You must select a mode"), card);
  lblNoMode->setObjectName(QStringLiteral("lblNoMode"));
  cardLayout->addWidget(lblNoMode);

  auto *actions = new QHBoxLayout;
  actions->setSpacing(kGap);
  serverOptions = new QWidget(card);
  serverOptions->setObjectName(QStringLiteral("serverOptions"));
  auto *serverLayout = new QHBoxLayout(serverOptions);
  serverLayout->setContentsMargins(0, 0, 0, 0);
  serverLayout->setSpacing(kGap);
  btnConfigureServer = new QPushButton(QObject::tr("&Configure Server"), serverOptions);
  btnConfigureServer->setObjectName(QStringLiteral("btnConfigureServer"));
  btnConfigureServer->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
  btnSaveServerConfig =
      iconButton(serverOptions, QStringLiteral("btnSaveServerConfig"), QStringLiteral("document-save-as"));
  btnSaveServerConfig->setToolTip(QObject::tr("Export server configuration"));
  serverLayout->addWidget(btnConfigureServer);
  serverLayout->addWidget(btnSaveServerConfig);
  actions->addWidget(serverOptions);

  btnConfigureClient = new QPushButton(QObject::tr("&Configure Client"), card);
  btnConfigureClient->setObjectName(QStringLiteral("btnConfigureClient"));
  btnConfigureClient->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
  actions->addWidget(btnConfigureClient);
  actions->addStretch(1);

  btnToggleCore = new QPushButton(QObject::tr("&Start"), card);
  btnToggleCore->setObjectName(QStringLiteral("btnToggleCore"));
  btnToggleCore->setEnabled(false);
  actions->addWidget(btnToggleCore);

  btnRestartCore = new QPushButton(QObject::tr("&Restart"), card);
  btnRestartCore->setObjectName(QStringLiteral("btnRestartCore"));
  btnRestartCore->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
  btnRestartCore->setEnabled(false);
  btnRestartCore->setToolTip(QObject::tr("Restart"));
  actions->addWidget(btnRestartCore);
  cardLayout->addLayout(actions);
  workspaceLayout->addWidget(card);

  logPanel = new LogPanel(workspaceContents);
  logPanel->setObjectName(QStringLiteral("logPanel"));
  workspaceLayout->addWidget(logPanel);
  workspaceLayout->addStretch(1);

  statusBar = new StatusBar(topLevelWidget);
  statusBar->setObjectName(QStringLiteral("statusBar"));
  root->addWidget(statusBar);

  window->setCentralWidget(topLevelWidget);
}

void Ui::MainWindow::retranslateUi(QMainWindow *window)
{
  Q_UNUSED(window)
  pageTitle->setText(QObject::tr("Devices & Connection"));
  btnBack->setToolTip(QObject::tr("Back"));
  btnSettingsShortcut->setText(QObject::tr("Preferences"));
  btnUpdatesShortcut->setText(QObject::tr("Updates"));
  btnAboutShortcut->setText(QObject::tr("About"));
  btnEditName->setText(QObject::tr("Edit"));
  m_pLabelServerName->setText(QObject::tr("Connect to:"));
  lblNoMode->setText(QObject::tr("You must select a mode"));
  btnConfigureServer->setText(QObject::tr("&Configure Server"));
  btnConfigureClient->setText(QObject::tr("&Configure Client"));
  btnRestartCore->setText(QObject::tr("&Restart"));
}
