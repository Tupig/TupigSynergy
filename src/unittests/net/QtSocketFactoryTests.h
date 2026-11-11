/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <memory>

class EventQueue;

class QtSocketFactoryTests : public QObject
{
  Q_OBJECT
public:
  ~QtSocketFactoryTests() override;

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void cleanup();

  void legacyNetworkFlagHonorsEnv();
  void plaintextRoundTrip();
  void tlsRejectsUntrustedServer();
  void tlsTrustedServerRoundTrip();
  void tlsRejectsWrongFingerprint();
  void tlsRejectsSmallRsaKey();

private:
  bool pump(const std::function<bool()> &done, int timeoutMs = 10000);

  Log m_log;
  EventQueue *m_events = nullptr;
  QString m_certPath;
  QTemporaryDir m_directory;
  const QByteArray m_legacyNetwork = qgetenv("USE_LEGACY_NETWORK");
  const bool m_hadLegacyNetwork = qEnvironmentVariableIsSet("USE_LEGACY_NETWORK");
  QString m_testDir;
};
