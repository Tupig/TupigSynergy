/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenTests.h"

#include "common/Settings.h"
#include "gui/config/Screen.h"

void ScreenTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();

  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void ScreenTests::basicFunctionality()
{
  synergy::gui::Screen screen;
  QVERIFY(screen.isNull());

  screen.setName("stub");
  QVERIFY(!screen.isNull());

  screen.saveSettings(Settings::proxy());
  screen.loadSettings(Settings::proxy());
  QCOMPARE("stub", screen.name());
}

void ScreenTests::binaryStreamPreservesLegacyLayout_data()
{
  QTest::addColumn<QString>("name");
  QTest::addColumn<bool>("primary");
  QTest::newRow("primary") << QStringLiteral("primary-test") << true;
  QTest::newRow("secondary") << QStringLiteral("secondary-test") << false;
  QTest::newRow("empty") << QString() << false;
}

void ScreenTests::binaryStreamPreservesLegacyLayout()
{
  QFETCH(QString, name);
  QFETCH(bool, primary);
  const QStringList aliases{QStringLiteral("alias-test")};
  const QList<int> modifiers{0, 2, 1, 3, 4, 5, 6};
  const QList<bool> corners{true, false, true, false};
  const QList<bool> fixes{false, true, false, true};

  // This is the existing drag/drop payload layout, independent of class names.
  QByteArray legacy;
  QDataStream legacyWriter(&legacy, QIODevice::WriteOnly);
  legacyWriter.setVersion(QDataStream::Qt_6_0);
  legacyWriter << name << 17 << aliases << modifiers << corners << fixes << primary;
  QCOMPARE(legacyWriter.status(), QDataStream::Ok);

  synergy::gui::Screen screen;
  QDataStream reader(legacy);
  reader.setVersion(QDataStream::Qt_6_0);
  reader >> screen;
  QCOMPARE(reader.status(), QDataStream::Ok);
  QVERIFY(reader.atEnd());
  QCOMPARE(screen.name(), name);
  QCOMPARE(screen.switchCornerSize(), 17);
  const synergy::gui::Screen &readOnlyScreen = screen;
  QCOMPARE(readOnlyScreen.aliases(), aliases);
  QCOMPARE(readOnlyScreen.modifiers(), modifiers);
  QCOMPARE(readOnlyScreen.switchCorners(), corners);
  QCOMPARE(readOnlyScreen.fixes(), fixes);
  QCOMPARE(screen.isPrimary(), primary);

  QByteArray encoded;
  QDataStream writer(&encoded, QIODevice::WriteOnly);
  writer.setVersion(QDataStream::Qt_6_0);
  writer << screen;
  QCOMPARE(writer.status(), QDataStream::Ok);
  QCOMPARE(encoded, legacy);
}

void ScreenTests::truncatedBinaryStreamReportsError()
{
  QByteArray truncated;
  QDataStream writer(&truncated, QIODevice::WriteOnly);
  writer.setVersion(QDataStream::Qt_6_0);
  writer << QStringLiteral("incomplete-test");

  synergy::gui::Screen screen;
  QDataStream reader(truncated);
  reader.setVersion(QDataStream::Qt_6_0);
  reader >> screen;
  QCOMPARE(reader.status(), QDataStream::ReadPastEnd);
}

QTEST_MAIN(ScreenTests)
