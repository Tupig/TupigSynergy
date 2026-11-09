/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QTest>

class ServerProxyTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void unknownMessageReturnsUnknown();
  void serverNoopReturnsOkay();
  void serverCloseDisconnects();
  void keepAliveIsEchoed();
  void mouseMoveForwardsToClient();
  void enterAndLeaveForwardToClient();
  void P01_keyDispatch_data();
  void P01_keyDispatch();
  void P01_blockedPressPreservesRelease();
  void P02_pointerDispatch_data();
  void P02_pointerDispatch();
  void P03_motionOrder_data();
  void P03_motionOrder();
  void P04_infoAcknowledgment();
  void P05_optionsAndTranslation();
  void P06_controlBodies();
  void P07_clipboardAssembly_data();
  void P07_clipboardAssembly();
  void P08_disabledTransferConsumes();
  void N04_screensaverForwarding_data();
  void N04_screensaverForwarding();

private:
  Log m_log;
};
