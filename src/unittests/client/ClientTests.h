/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once
#include "base/Log.h"
#include <QTest>

class ClientTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void C01_idleOwnership();
  void C02_connectSelection_data();
  void C02_connectSelection();
  void C03_registeredHandshake();
  void C04_failureOwnership_data();
  void C04_failureOwnership();
  void C05_timeout_data();
  void C05_timeout();
  void C06_refusal_data();
  void C06_refusal();
  void C07_disconnectEvents_data();
  void C07_disconnectEvents();
  void C08_cleanupStages_data();
  void C08_cleanupStages();
  void C09_suspendResume();
  void C10_ownedHandlers_data();
  void C10_ownedHandlers();
  void C11_realQueueReentry_data();
  void C11_realQueueReentry();
  void N01_unknownBatchRecovery();
  void N02_shortCodes_data();
  void N02_shortCodes();
  void N03_helloBoundaries_data();
  void N03_helloBoundaries();
  void N04_normalBodies_data();
  void N04_normalBodies();
  void N05_handshakeBodies_data();
  void N05_handshakeBodies();
  void N06_recoveryStress();

private:
  Log m_log;
};
