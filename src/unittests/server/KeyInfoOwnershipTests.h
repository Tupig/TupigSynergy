/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTest>

class KeyInfoOwnershipTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void conditionDestroysAdoptedObjectAndString();
  void parsedConditionAndActionsPreserveFields();
  void serverDefaultScrollLockStillToggles();
  void actionCloneAndReplacementOwnIndependentStrings();
  void selfAdoptionKeepsPayloadAlive();
  void repeatedActionsBorrowPayload_data();
  void repeatedActionsBorrowPayload();
  void keyStateProducersReleaseObjects_data();
  void keyStateProducersReleaseObjects();
  void mouseModifiersReleaseOwnedPayload_data();
  void mouseModifiersReleaseOwnedPayload();
  void typedEventMovesTransferDisposalRight();
  void legacyAndBorrowedPayloadsKeepTheirRules();
  void realQueueDisposesPayload_data();
  void realQueueDisposesPayload();
  void filterForwardingBorrowsOriginalPayload();

private:
  Arch m_arch;
  Log m_log;
};
