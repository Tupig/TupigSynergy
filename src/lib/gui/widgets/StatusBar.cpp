/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/StyleUtils.h"

#include "StatusBar.h"
#include "common/Constants.h"
#include "common/Settings.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVariant>

StatusBar::StatusBar(QWidget *parent)
    : QWidget{parent},
      m_btnFingerprint{new QPushButton(this)},
      m_lblSecurityIcon{new QLabel(this)},
      m_lblStatus{new QLabel(this)},
      m_lblSecurity{new QLabel(this)},
      m_retryTimer{new QTimer(this)}
{
  setObjectName(QStringLiteral("statusBar"));
  setAttribute(Qt::WA_StyledBackground);
  setFixedHeight(32);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(12, 0, 12, 0);
  layout->setSpacing(9);

  m_btnFingerprint->setProperty("iconButton", true);
  m_btnFingerprint->setIcon(synergy::gui::appIcon(QStringLiteral("fingerprint")));
  m_btnFingerprint->setIconSize(QSize(16, 16));
  connect(m_btnFingerprint, &QPushButton::clicked, this, &StatusBar::requestShowMyFingerprints);

  m_lblSecurityIcon->setVisible(false);
  m_lblSecurityIcon->setFixedSize(14, 14);
  m_lblSecurityIcon->setScaledContents(true);

  m_lblStatus->setObjectName(QStringLiteral("statusText"));
  m_lblStatus->setText(tr("%1 is not running").arg(kAppName));

  m_lblSecurity->setObjectName(QStringLiteral("statusRight"));

  layout->addWidget(m_btnFingerprint);
  layout->addWidget(m_lblSecurityIcon);
  layout->addWidget(m_lblStatus, 1);
  layout->addWidget(m_lblSecurity);

  m_retryTimer->setInterval(1000);
  m_retryTimer->setSingleShot(false);
  connect(m_retryTimer, &QTimer::timeout, this, &StatusBar::updateTimerLabel);

  updateText();
}

// clang-format off
void StatusBar::setStatus(ConnectionState connectionState, ProcessState processState, bool isServer)
{
  if (m_retryTimer->isActive())
    m_retryTimer->stop();
  setSecurityIconVisible(false);
  switch (processState) {
    using enum ProcessState;
    case Starting:
      m_connectionInterval = -1;
      m_lblStatus->setText(tr("%1 is starting...").arg(kAppName));
      break;

    case RetryPending:
      m_connectionInterval = -1;
      m_lblStatus->setText(tr("%1 will retry in a moment...").arg(kAppName));
      break;

    case Stopping:
      m_connectionInterval = -1;
      m_lblStatus->setText(tr("%1 is stopping...").arg(kAppName));
      break;

    case Stopped:
      m_connectionInterval = -1;
      m_lblStatus->setText(tr("%1 is not running").arg(kAppName));
      break;

    case Started: {
      switch (connectionState) {
        using enum ConnectionState;

        case Listening: {
          if (isServer) {
            setSecurityIconVisible(true);
            m_lblStatus->setText(tr("%1 is waiting for clients").arg(kAppName));
          }
          break;
        }

        case Connecting:
          updateTimerLabel();
          if (Settings::value(Settings::Client::DynamicConnectionRetry).toBool())
            m_retryTimer->start();
          break;

        case Connected: {
          setSecurityIconVisible(true);
          m_connectionInterval = -1;
          if (!isServer) {
            m_lblStatus->setText(tr("%1 is connected as client of %2")
                                     .arg(kAppName, Settings::value(Settings::Client::RemoteHost).toString()));
          }
          break;
        }

        case Disconnected:
          m_lblStatus->setText(tr("%1 is disconnected").arg(kAppName));
          m_connectionInterval = -1;
          break;
      }
    }
  }
}
// clang-format on
void StatusBar::setServerClients(const QStringList &clients)
{
  if (clients.isEmpty()) {
    m_lblStatus->setText(tr("%1 is waiting for clients").arg(kAppName));
    m_lblStatus->setToolTip("");
    return;
  }
  const auto clientCount = static_cast<int>(clients.size());
  static const auto comma = QStringLiteral(", ");
  static const auto newLine = QStringLiteral("\n");
  //: Shown when in server mode and at least 1 client is connected
  //: %1 is replaced by the app name
  //: %2 will be a list of at least one client
  //: %n will be replaced by the number of clients (n is >=1), it is not requried to be in the translation
  const auto text = tr("%1 is connected, with %n client(s): %2", "", clientCount).arg(kAppName, clients.join(comma));
  m_lblStatus->setText(text);

  const auto toolTipString = clientCount == 1 ? "" : tr("Clients:\n %1").arg(clients.join(newLine));
  m_lblStatus->setToolTip(toolTipString);
}

void StatusBar::setSecurityIconVisible(bool visible)
{
  m_lblSecurityIcon->setVisible(visible);
}

void StatusBar::setConnectionInterval(int newInterval)
{
  m_connectionInterval = newInterval;
}

bool StatusBar::securityIconVisible() const
{
  return m_lblSecurityIcon->isVisible();
}

void StatusBar::setBtnFingerprintVisible(bool visible)
{
  m_btnFingerprint->setVisible(visible);
}

void StatusBar::changeEvent(QEvent *e)
{
  QWidget::changeEvent(e);
  if (e->type() == QEvent::LanguageChange)
    updateText();
}

void StatusBar::updateText()
{
  m_btnFingerprint->setToolTip(tr("View local fingerprint"));
  m_btnFingerprint->setAccessibleName(tr("View local fingerprint"));
  setSecurityLevel(m_securityLevel);
}

void StatusBar::updateTimerLabel()
{
  QString text;
  if (m_connectionInterval < 2 || !Settings::value(Settings::Client::DynamicConnectionRetry).toBool()) {
    text = tr("%1 is connecting...").arg(kAppName);
  } else {
    text = tr("%1 is waiting %2 seconds before the next retry").arg(kAppName, QString::number(m_connectionInterval));
    m_connectionInterval--;
  }
  m_lblStatus->setText(text);
}

void StatusBar::updateSecurityInfo(bool encrypted)
{
  setSecurityIcon(encrypted);
}

void StatusBar::setSecurityIcon(bool encrypted)
{
  const auto icon = synergy::gui::appIcon(encrypted ? QStringLiteral("security-high") : QStringLiteral("security-low"));
  m_lblSecurityIcon->setPixmap(icon.pixmap(QSize(32, 32)));
  m_encrypted = encrypted;
  setSecurityLevel(m_securityLevel);
}

void StatusBar::setSecurityLevel(const QString &securityLevel)
{
  m_securityLevel = securityLevel;
  const auto txt = m_encrypted ? tr("TLS enabled") : tr("TLS disabled");
  m_lblSecurityIcon->setToolTip(txt);
  m_lblSecurity->setText(securityLevel.isEmpty() ? txt : securityLevel);
}
