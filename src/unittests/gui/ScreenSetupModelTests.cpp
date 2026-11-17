/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSetupModelTests.h"

#include "gui/ScreenSetupModel.h"
#include "gui/config/Screen.h"
#include "gui/config/ScreenList.h"

void ScreenSetupModelTests::clampsRowsToScreenList()
{
  synergy::gui::ScreenList screens;
  screens.append(synergy::gui::Screen(QStringLiteral("a")));
  screens.append(synergy::gui::Screen(QStringLiteral("b")));
  screens.append(synergy::gui::Screen(QStringLiteral("c")));
  screens.append(synergy::gui::Screen(QStringLiteral("d")));

  // a 3x9 grid cannot hold only 4 screens; rows clamp instead of aborting
  ScreenSetupModel model(screens, 3, 9);

  QCOMPARE(model.columnCount(), 3);
  QCOMPARE(model.rowCount(), 1);
  QVERIFY(model.rowCount() * model.columnCount() <= screens.size());
}

void ScreenSetupModelTests::clampsGridSize()
{
  synergy::gui::ScreenList screens;
  screens.append(synergy::gui::Screen(QStringLiteral("a")));

  ScreenSetupModel model(screens, 200, 200);

  QCOMPARE(model.columnCount(), 100);
  QVERIFY(model.rowCount() * model.columnCount() <= screens.size());
}

void ScreenSetupModelTests::outOfRangeIndexesAreInvalid()
{
  synergy::gui::ScreenList screens;
  for (int i = 0; i < 9; ++i) {
    screens.append(synergy::gui::Screen(QStringLiteral("screen%1").arg(i)));
  }

  ScreenSetupModel model(screens, 3, 3);

  QVERIFY(!model.data(model.index(3, 0)).isValid());
  QVERIFY(!model.data(model.index(0, 3)).isValid());
  QVERIFY(!model.data(model.index(-1, 0)).isValid());
}

void ScreenSetupModelTests::inRangeIndexesHaveNames()
{
  synergy::gui::ScreenList screens;
  for (int i = 0; i < 9; ++i) {
    screens.append(synergy::gui::Screen(QStringLiteral("screen%1").arg(i)));
  }

  ScreenSetupModel model(screens, 3, 3);

  QCOMPARE(model.data(model.index(2, 2)).toString(), QStringLiteral("screen8"));
}

// Additive M-C02 keyboard/empty-cell contract; occupied drag behavior retained.
void ScreenSetupModelTests::emptyCellsAreSelectableDropTargetsWithoutDrag()
{
  synergy::gui::ScreenList screens;
  for (int i = 0; i < 9; ++i)
    screens.append(i == 0 ? synergy::gui::Screen(QStringLiteral("Primary-Fixture")) : synergy::gui::Screen());
  ScreenSetupModel model(screens, 3, 3);
  const auto empty = model.flags(model.index(0, 1));
  QVERIFY(empty.testFlag(Qt::ItemIsEnabled));
  QVERIFY(empty.testFlag(Qt::ItemIsSelectable));
  QVERIFY(empty.testFlag(Qt::ItemIsDropEnabled));
  QVERIFY(!empty.testFlag(Qt::ItemIsDragEnabled));
  const auto occupied = model.flags(model.index(0, 0));
  QVERIFY(occupied.testFlag(Qt::ItemIsDragEnabled));
  QCOMPARE(model.flags(QModelIndex()), Qt::NoItemFlags);
}

QTEST_MAIN(ScreenSetupModelTests)
