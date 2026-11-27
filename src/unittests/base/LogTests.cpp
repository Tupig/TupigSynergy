/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogTests.h"
#include "base/LogOutputters.h"

#include <QFile>
#include <QTemporaryDir>

#include <clocale>
#include <iostream>
#include <sstream>

#define LEVEL_PRINT "%z\057"
#define LEVEL_ERR "%z\061"
#define LEVEL_INFO "%z\063"

QString sanitizeBuffer(const std::stringstream &in)
{
  static QRegularExpression timestampRegex("\\[\\S+\\] ");
  QString rtn = QString::fromStdString(in.str()).simplified();
  rtn.remove(timestampRegex);
  return rtn;
}

void LogTests::initTestCase()
{
  std::setlocale(LC_NUMERIC, "C");
  m_log.setFilter(LogLevel::Level::Debug);
}

void LogTests::printWithErrorValidOutput()
{
  std::stringstream buffer;
  std::streambuf *old = std::cerr.rdbuf(buffer.rdbuf());

  m_log.print(nullptr, 0, LEVEL_ERR "test message");

  auto string = sanitizeBuffer(buffer);
  std::cerr.rdbuf(old);

  QCOMPARE(string, "ERROR: test message");
}

void LogTests::printTestPrintLevel()
{
  std::stringstream buffer;
  std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());

  m_log.print(nullptr, 0, LEVEL_PRINT "test message");

  auto string = sanitizeBuffer(buffer);
  std::cout.rdbuf(old);

  QCOMPARE(string, "test message");
}

void LogTests::printTestWithArgs()
{
  std::stringstream buffer;
  std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());

  m_log.print(nullptr, 0, LEVEL_INFO "test %s", "IamARG");

  auto string = sanitizeBuffer(buffer);
  std::cout.rdbuf(old);

  QCOMPARE(string, "INFO: test IamARG");
}

void LogTests::printTestLogString()
{
  std::stringstream buffer;
  std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());

  auto longString = QString(10000, 'a');
  m_log.print(nullptr, 0, LEVEL_INFO "%s", qPrintable(longString));

  auto string = sanitizeBuffer(buffer);
  std::cout.rdbuf(old);

  QCOMPARE(string, QString("INFO: %1").arg(longString));
}

void LogTests::printLevelToHigh()
{
  std::stringstream buffer;
  std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());

  m_log.print(CLOG_VERBOSE "test message");

  auto string = sanitizeBuffer(buffer);
  std::cout.rdbuf(old);

  QCOMPARE(string, QString{});
}

void LogTests::printInfoWithFileAndLine()
{
  std::stringstream buffer;
  std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());

  m_log.print("test file", 123, LEVEL_INFO "test message");

  auto string = sanitizeBuffer(buffer);
  std::cout.rdbuf(old);

  QCOMPARE(string, "INFO: test message test file:123");
}

void LogTests::printErrWithFileAndLine()
{
  std::stringstream buffer;
  std::streambuf *old = std::cerr.rdbuf(buffer.rdbuf());

  m_log.print("test file", 123, LEVEL_ERR "test message");

  auto string = sanitizeBuffer(buffer);
  std::cerr.rdbuf(old);

  QCOMPARE(string, "ERROR: test message test file:123");
}

void LogTests::fileLogAppendsAndChangesDestination()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto firstPath = directory.filePath(QStringLiteral("first.log"));
  const auto secondPath = directory.filePath(QStringLiteral("second.log"));

  FileLogOutputter log(firstPath);
  const auto firstMessage = QString::fromUtf8("连接成功");
  QVERIFY(log.write(LogLevel::Level::Info, firstMessage));
  log.close();
  QVERIFY(log.write(LogLevel::Level::Info, QStringLiteral("second entry")));
  log.setLogFilename({});
  QVERIFY(!log.write(LogLevel::Level::Info, QStringLiteral("disabled destination")));
  log.setLogFilename(secondPath);
  QVERIFY(log.write(LogLevel::Level::Info, QStringLiteral("new destination")));
  log.close();

  QFile first(firstPath);
  QVERIFY(first.open(QIODevice::ReadOnly));
  QCOMPARE(first.readAll(), firstMessage.toUtf8() + "\nsecond entry\n");

  QFile second(secondPath);
  QVERIFY(second.open(QIODevice::ReadOnly));
  QCOMPARE(second.readAll(), QByteArray("new destination\n"));
}

void LogTests::fileLogRotatesAndReopens()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto logPath = directory.filePath(QStringLiteral("core.log"));

  FileLogOutputter log(logPath);
  const QString largeMessage(1100000, QLatin1Char('x'));
  QVERIFY(log.write(LogLevel::Level::Info, largeMessage));
  QVERIFY(log.write(LogLevel::Level::Info, QStringLiteral("after rotation")));
  log.close();

  QFile rotated(logPath + QStringLiteral(".1"));
  QVERIFY(rotated.open(QIODevice::ReadOnly));
  QCOMPARE(rotated.readAll(), largeMessage.toUtf8() + '\n');

  QFile current(logPath);
  QVERIFY(current.open(QIODevice::ReadOnly));
  QCOMPARE(current.readAll(), QByteArray("after rotation\n"));
}

QTEST_MAIN(LogTests)
