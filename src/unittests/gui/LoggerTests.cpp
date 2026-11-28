/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LoggerTests.h"
#include "common/Settings.h"

#include "gui/Logger.h"

#include "common/RuntimeContext.h"
#include "common/RuntimeLogPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>

using namespace synergy::gui;

void LoggerTests::initTestCase()
{
  QVERIFY(m_directory.isValid());
  const auto root = QFileInfo(m_directory.path()).canonicalFilePath();
  QVERIFY(synergy::runtime::configureIsolatedRuntime({root, QStringLiteral("synergy-logger-unit-test")}));
  Settings::setSettingsFile(m_directory.filePath(QStringLiteral("Synergy.conf")));
  Settings::setStateFile(m_directory.filePath(QStringLiteral("Synergy.state")));
}

void LoggerTests::newLine()
{
  QSignalSpy spy(Logger::instance(), &Logger::newLine);
  QVERIFY(spy.isValid());

  Settings::setValue(Settings::Log::GuiDebug, true);
  Logger::instance()->handleMessage(QtDebugMsg, "stub", "test");

  QCOMPARE(spy.count(), 1);
  QVERIFY(qvariant_cast<QString>(spy.takeFirst().at(0)).contains("test"));
  Settings::setValue(Settings::Log::GuiDebug, false);

  const QDir logs(synergy::managedRuntimeLogDirectory(synergy::RuntimeLogRole::Gui));
  const auto files = logs.entryList({QStringLiteral("*.log")}, QDir::Files);
  QCOMPARE(files.size(), 1);
  QFile log(logs.absoluteFilePath(files.first()));
  QVERIFY(log.open(QIODevice::ReadOnly));
  QVERIFY(log.readAll().contains("test"));
}

void LoggerTests::noNewLine()
{
  QSignalSpy spy(Logger::instance(), &Logger::newLine);
  QVERIFY(spy.isValid());

  Settings::setValue(Settings::Log::GuiDebug, false);
  Logger::instance()->handleMessage(QtDebugMsg, "stub", "test");
  QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(LoggerTests)
