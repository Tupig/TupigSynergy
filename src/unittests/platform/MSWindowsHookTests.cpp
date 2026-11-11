/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/win32/MSWindowsPolicies.h"

#include <QTest>

class MSWindowsHookTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void injectedEventsDoNotLoopOnSecondary()
  {
    QVERIFY(synergy::windows::shouldIgnoreInjectedInput(false, true));
    QVERIFY(!synergy::windows::shouldIgnoreInjectedInput(false, false));
    QVERIFY(!synergy::windows::shouldIgnoreInjectedInput(true, true));
    QVERIFY(!synergy::windows::shouldIgnoreInjectedInput(true, false));
  }
};

QTEST_GUILESS_MAIN(MSWindowsHookTests)
#include "MSWindowsHookTests.moc"
