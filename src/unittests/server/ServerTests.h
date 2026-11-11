/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "arch/Arch.h"
#include "base/Log.h"
#include <QTest>

class ServerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void motionInsideScreenDoesNotSwitch();
  void crossingConnectedEdgeEntersSecondary_data();
  void crossingConnectedEdgeEntersSecondary();
  void crossingMissingNeighborDoesNotSwitch();
  void deniedLeaveDoesNotEnterSecondary();
  void pressedMousePreventsEdgeSwitch();
  void explicitSwitchReturnsToPrimary();
  void unknownTargetDoesNotSwitch();
  void configurationWithoutPrimaryIsRejected();
  void SwitchToScreenInfo_alloc_screen();
  void KeyboardBroadcastInfo_alloc_stateAndSceens();

private:
  Arch m_arch;
  Log m_log;
};
