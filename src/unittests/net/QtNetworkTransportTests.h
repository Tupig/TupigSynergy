/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once
#include "arch/Arch.h"
#include "base/Log.h"
#include <QTest>

class QtNetworkTransportTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void reconnectPreservesOnlyNewBytes_data();
  void reconnectPreservesOnlyNewBytes();
  void repeatedReconnect();
  void failedConnectionCanRetry();
  void nullableReadDiscards();

private:
  Arch m_arch;
  Log m_log;
};
