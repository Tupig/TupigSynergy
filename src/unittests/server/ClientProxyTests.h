/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTest>

class ClientProxyTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void negotiatesSupportedVersions_data();
  void negotiatesSupportedVersions();
  void rejectsUnsupportedVersions_data();
  void rejectsUnsupportedVersions();
  void rejectsMalformedHello_data();
  void rejectsMalformedHello();
  void failedHandshakeCleansUp_data();
  void failedHandshakeCleansUp();

  void handshakeNoopReturnsTrue();
  void handshakeUnknownCodeReturnsFalse();
  void handshakeInfoCompletesHandshake();
  void parseMessageNoopReturnsTrue();
  void parseMessageUnknownReturnsFalse();
  void parseMessageInfoUpdatesShape();
  void parseMessageTruncatedInfoReturnsFalse();

private:
  Arch m_arch;
  Log m_log;
};
