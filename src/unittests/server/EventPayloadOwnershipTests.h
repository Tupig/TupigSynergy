/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTest>

class EventPayloadOwnershipTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void actionFieldsAndCallbacks_data();
  void actionFieldsAndCallbacks();
  void actionOwnership_data();
  void actionOwnership();
  void connectionFieldsAndCallbacks_data();
  void connectionFieldsAndCallbacks();
  void connectionOwnership_data();
  void connectionOwnership();
  void correctScreenSwitchedEventDataKeepsItsContract();
  void borrowedConnectionRetainsOriginalOwner();

private:
  void checkAction(bool ownership);
  void checkConnection(bool ownership);
  Arch m_arch;
  Log m_log;
};
