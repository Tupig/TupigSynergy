/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsTests.h"

#include "common/Constants.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>

void SettingsTests::initTestCase()
{
  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();
}

void SettingsTests::setSettingsFile()
{
  Settings::setSettingsFile(m_settingsFile);
}

void SettingsTests::setStateFile()
{
  Settings::setStateFile(m_stateFile);
}

void SettingsTests::settingsFile()
{
  QVERIFY(Settings::settingsFile().endsWith(m_settingsFile));
}

void SettingsTests::settingsPath()
{
  QVERIFY(Settings::settingsPath().endsWith(m_settingsPath));
}

void SettingsTests::tlsDir()
{
  QVERIFY(Settings::tlsDir().endsWith(m_expectedTlsDir));
}

void SettingsTests::tlsTrustedServersDb()
{
  QVERIFY(Settings::tlsTrustedServersDb().endsWith(m_expectedTlsServerDB));
}

void SettingsTests::tlsTrustedClientsDb()
{
  QVERIFY(Settings::tlsTrustedClientsDb().endsWith(m_expectedTlsClientDB));
}

void SettingsTests::checkValidSettings()
{
  QSignalSpy spy(Settings::instance(), &Settings::settingsChanged);
  QVERIFY(spy.isValid());

  const auto keysToCheck = QRegularExpression(QLatin1String("[^%1]").arg(Settings::Core::ComputerName));
  const auto validKeys = Settings::validKeys().filter(keysToCheck);
  for (const auto &setting : validKeys) {
    const auto value = Settings::value(setting).toString();
    QCOMPARE(Settings::defaultValue(setting).toString(), value);

    Settings::setValue(setting, "NEW_VALUE");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<QString>(spy.first().at(0)), setting);
    QCOMPARE(Settings::value(setting).toString(), "NEW_VALUE");

    Settings::setValue(setting);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(Settings::value(setting).toString(), value);

    // Reset the spy for the next loop
    spy.clear();
    QCOMPARE(spy.count(), 0);
  }
}

void SettingsTests::checkCleanScreenName()
{
  const auto input = QStringLiteral("--!_ _-S@c#r$e%e^&*(n)= +Name\n[1]2|3?4--5>6<,7`~/8*90\\.lan--..    ..");
  const auto expected = QStringLiteral("Screen_Name_1234--567890.lan");

  Settings::setValue(Settings::Core::ComputerName, input);

  QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), expected);
}

void SettingsTests::checkCleanScreenName_LongName()
{
  QString input;
  input.fill('f', 300);
  input.prepend('.');

  QString expected;
  expected.fill('f', 255);

  Settings::setValue(Settings::Core::ComputerName, input);

  QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), expected);
}

void SettingsTests::emptyBlockedComboSurvivesClean()
{
  const auto path = QDir(m_settingsPathTemp).filePath(QStringLiteral("blocked.conf"));
  QDir().mkpath(m_settingsPathTemp);
  QFile::remove(path);
  {
    QFile primed(path);
    QVERIFY(primed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    primed.write("[security]\nblockedKeyCombos=\n");
    primed.close();
  }

  Settings::setSettingsFile(path);

  const auto blocked = Settings::value(Settings::Security::BlockedKeyCombos).toStringList();
  QVERIFY(!blocked.contains(QStringLiteral("0xEFFF:0x0006")));
}

void SettingsTests::missingKeysReturnDefaults()
{
  const auto path = QDir(m_settingsPathTemp).filePath(QStringLiteral("empty.conf"));
  QDir().mkpath(m_settingsPathTemp);
  QFile::remove(path);
  Settings::setSettingsFile(path);

  QCOMPARE(Settings::value(Settings::Core::Port).toInt(), Settings::defaultValue(Settings::Core::Port).toInt());
  QCOMPARE(Settings::value(Settings::Log::Level).toString(), Settings::defaultValue(Settings::Log::Level).toString());
  QCOMPARE(
      Settings::value(Settings::Client::RemoteHost).toString(),
      Settings::defaultValue(Settings::Client::RemoteHost).toString()
  );
}

void SettingsTests::corruptedConfigFallsBackToDefaults()
{
  const auto path = QDir(m_settingsPathTemp).filePath(QStringLiteral("corrupt.conf"));
  QDir().mkpath(m_settingsPathTemp);
  QFile::remove(path);
  {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("not an ini at all [broken\n%%%\n\x01\x02\x03\n");
    file.close();
  }

  Settings::setSettingsFile(path);

  // loading a file repairs the computer name by design (setupComputerName);
  // every other missing key falls back to its default
  QVERIFY(!Settings::value(Settings::Core::ComputerName).toString().isEmpty());
  QCOMPARE(Settings::value(Settings::Core::Port).toInt(), Settings::defaultValue(Settings::Core::Port).toInt());
}

void SettingsTests::wrongTypeValueIsReturnedAsStored()
{
  const auto path = QDir(m_settingsPathTemp).filePath(QStringLiteral("wrongtype.conf"));
  QDir().mkpath(m_settingsPathTemp);
  QFile::remove(path);
  {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("[core]\nport=not-a-number\n");
    file.close();
  }

  Settings::setSettingsFile(path);

  // the stored string is preserved (no crash, no silent default); numeric
  // conversion of the malformed value yields zero
  QCOMPARE(Settings::value(Settings::Core::Port).toString(), QStringLiteral("not-a-number"));
  QCOMPARE(Settings::value(Settings::Core::Port).toInt(), 0);
}

void SettingsTests::portableModeFollowsMarker()
{
  const auto dir = QStringLiteral("%1/settings").arg(QCoreApplication::applicationDirPath());
  const auto settingsFile = QStringLiteral("%1/%2.conf").arg(dir, kAppName);
  const auto marker = QStringLiteral("%1/%2.portable").arg(dir, kAppId);
  if (QFile::exists(settingsFile) || QFile::exists(marker))
    QSKIP("portable files already exist next to the test binary");

  QVERIFY(!Settings::isPortableMode());

  // The archive ships the marker, not the settings file: extracting an update
  // must not overwrite user data, while a fresh unpack still enters portable
  // mode before the settings file has been created.
  QVERIFY(QDir().mkpath(dir));
  QFile file(marker);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("marker");
  file.close();
  QVERIFY(Settings::isPortableMode());
  QVERIFY(QFile::remove(marker));
  QVERIFY(!Settings::isPortableMode());
  QDir().rmdir(dir);
}

QTEST_MAIN(SettingsTests)
