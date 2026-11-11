/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/win32/MSWindowsPolicies.h"

#include <QTest>

#include <limits>

class MSWindowsWatchdogTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void retriesBackOffAndSaturate()
  {
    const unsigned expected[] = {0, 0, 1, 2, 4, 8, 16, 32, 60, 60};
    for (unsigned failures = 0; failures < sizeof(expected) / sizeof(expected[0]); ++failures)
      QCOMPARE(synergy::windows::restartDelaySeconds(failures), expected[failures]);
    QCOMPARE(synergy::windows::restartDelaySeconds(std::numeric_limits<unsigned>::max()), 60u);
  }

  void onlyHealthyUptimeClearsFailureHistory()
  {
    QVERIFY(!synergy::windows::hasStableUptime(0.0));
    QVERIFY(!synergy::windows::hasStableUptime(9.999));
    QVERIFY(synergy::windows::hasStableUptime(10.0));
    QVERIFY(synergy::windows::hasStableUptime(60.0));
  }
};

QTEST_GUILESS_MAIN(MSWindowsWatchdogTests)
#include "MSWindowsWatchdogTests.moc"
