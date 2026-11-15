/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QWidget>

#include "common/Enums.h"

class QPushButton;
class QLabel;
class QTimer;

using ProcessState = synergy::core::ProcessState;
using ConnectionState = synergy::core::ConnectionState;

/**
 * @brief 设计稿的底部状态栏：左侧指纹按钮与连接状态，右侧安全提示。
 */
class StatusBar : public QWidget
{
  Q_OBJECT
public:
  explicit StatusBar(QWidget *parent = nullptr);
  void setStatus(ConnectionState connectionState, ProcessState processState, bool isServer);
  void setServerClients(const QStringList &clients);
  void setSecurityIconVisible(bool visible);
  void setConnectionInterval(int newInterval);
  bool securityIconVisible() const;
  void updateSecurityInfo(bool encrypted);
  void setSecurityIcon(bool encrypted);
  void setSecurityLevel(const QString &securityLevel);
  void setBtnFingerprintVisible(bool visible);

Q_SIGNALS:
  void requestShowMyFingerprints();

protected:
  void changeEvent(QEvent *e) override;

private:
  void updateText();
  void updateTimerLabel();
  QPushButton *m_btnFingerprint = nullptr;
  QLabel *m_lblSecurityIcon = nullptr;
  QLabel *m_lblStatus = nullptr;
  QLabel *m_lblSecurity = nullptr;
  bool m_encrypted = false;
  QString m_securityLevel;
  int m_connectionInterval = -1;
  QTimer *m_retryTimer = nullptr;
};
