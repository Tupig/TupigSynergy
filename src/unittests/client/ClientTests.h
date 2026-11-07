/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "arch/Arch.h"
#include "base/Log.h"
#include <QTest>

class ClientTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void handshakeEnablesScreenAndDisconnectCleansUp();
  void connectIsIgnoredWhileConnecting();
  void connectionTimeoutCleansUp();
  void serverRefusalDoesNotActivateScreen();
  void suspendAndResumeReconnects();
  void truncatedHelloCannotConnect();

private:
  Arch m_arch;
  Log m_log;
};
