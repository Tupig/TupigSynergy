/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/win32/MSWindowsPolicies.h"

#include <QTest>

using namespace synergy::windows;

class MSWindowsDesksTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void unchangedDesktopDoesNothing()
  {
    QCOMPARE(desktopAction(false, false), DesktopAction::None);
    QCOMPARE(desktopAction(false, true), DesktopAction::None);
  }

  void screensaverPreventsDesktopSwitch()
  {
    QCOMPARE(desktopAction(true, true), DesktopAction::NotifyScreensaver);
    QCOMPARE(desktopAction(true, false), DesktopAction::Switch);
  }

  void keysSyncOnlyOnAccessibleSwitch()
  {
    QVERIFY(shouldSyncDesktopKeys(DesktopAction::Switch, true));
    QVERIFY(!shouldSyncDesktopKeys(DesktopAction::Switch, false));
    QVERIFY(!shouldSyncDesktopKeys(DesktopAction::None, true));
    QVERIFY(!shouldSyncDesktopKeys(DesktopAction::NotifyScreensaver, true));
  }
};

QTEST_GUILESS_MAIN(MSWindowsDesksTests)
#include "MSWindowsDesksTests.moc"
