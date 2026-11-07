/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsKeyState.h"
#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"

#include <QTest>

class MSWindowsKeyStateTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase()
  {
    m_arch.init();
  }

  void repeatTracksPressReleaseSequence()
  {
    EventQueue events;
    synergy::KeyMap map;
    MSWindowsKeyState state(nullptr, nullptr, &events, map, {"en"}, false);
    QVERIFY(!state.testAutoRepeat(true, false, 30));
    QVERIFY(state.testAutoRepeat(true, false, 30));
    QVERIFY(!state.testAutoRepeat(true, false, 31));
    QVERIFY(!state.testAutoRepeat(false, false, 31));
    QVERIFY(!state.testAutoRepeat(true, false, 31));
    QVERIFY(state.testAutoRepeat(true, true, 32));
    state.disable();
    QVERIFY(!state.testAutoRepeat(true, false, 32));
  }

  void savedModifiersMergeOnlyChangedBits()
  {
    EventQueue events;
    synergy::KeyMap map;
    MSWindowsKeyState state(nullptr, nullptr, &events, map, {"en"}, false);
    state.onKey(30, true, KeyModifierShift | KeyModifierControl);
    state.saveModifiers();
    QCOMPARE(state.getSavedModifiers(), KeyModifierMask(KeyModifierShift | KeyModifierControl));
    state.useSavedModifiers(true);
    state.setSavedModifiers(KeyModifierControl | KeyModifierAlt);
    // A newly observed modifier must survive restoring the saved state.
    state.onKey(31, true, KeyModifierShift | KeyModifierControl | KeyModifierSuper);
    state.useSavedModifiers(false);
    QCOMPARE(state.getActiveModifiers(), KeyModifierMask(KeyModifierControl | KeyModifierAlt | KeyModifierSuper));
    state.useSavedModifiers(false);
    QCOMPARE(state.getActiveModifiers(), KeyModifierMask(KeyModifierControl | KeyModifierAlt | KeyModifierSuper));
  }

private:
  Arch m_arch;
  Log m_log;
};

QTEST_GUILESS_MAIN(MSWindowsKeyStateTests)
#include "MSWindowsKeyStateTests.moc"
