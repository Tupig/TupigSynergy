/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogTests.h"
#include "base/LogOutputters.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

void LogTests::printPreservesBoundaryAndBoundsLongRecords()
{
  std::stringstream buffer;
  auto *old = std::cout.rdbuf(buffer.rdbuf());
  const std::string boundary(1024, 'x');
  const std::string large(300 * 1024, 'y');
  m_log.print(nullptr, 0, LEVEL_PRINT "%s", boundary.c_str());
  m_log.print(nullptr, 0, LEVEL_PRINT "%s", large.c_str());
  std::cout.rdbuf(old);

  const auto lines = QString::fromStdString(buffer.str()).split(u'\n');
  QCOMPARE(lines.value(0), QString::fromStdString(boundary));
  QCOMPARE(lines.value(1).size(), 256 * 1024 - 1);
  QVERIFY(lines.value(1).endsWith(QStringLiteral("...")));
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

void LogTests::segmentedLogRotatesAtSizeAndMidnight()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto now = QDateTime::fromString(QStringLiteral("2026-11-28T23:59:59"), Qt::ISODate);
  FileLogOutputter log(directory.path(), FileLogOutputter::Mode::SegmentedDirectory, [&now] { return now; });
  constexpr qsizetype limit = 10 * 1024 * 1024;
  QVERIFY(log.write(LogLevel::Level::Info, QString(limit - 1, u'x')));
  QCOMPARE(QFileInfo(directory.filePath(QStringLiteral("20261128235959.log"))).size(), limit);

  const auto next = QStringLiteral("连接完成");
  QVERIFY(log.write(LogLevel::Level::Info, next));
  const auto secondPath = directory.filePath(QStringLiteral("20261128235959-01.log"));
  QFile second(secondPath);
  QVERIFY(second.open(QIODevice::ReadOnly));
  QCOMPARE(second.readAll(), next.toUtf8() + '\n');
  second.close();

  now = now.addSecs(1);
  QVERIFY(log.write(LogLevel::Level::Warning, QStringLiteral("new day")));
  QVERIFY(!log.write(LogLevel::Level::Debug, QString(limit, u'x')));
  QCOMPARE(QFileInfo(directory.filePath(QStringLiteral("20261129000000.log"))).size(), 8);
}

void LogTests::segmentedLogPrunesOnlyManagedHistory()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  for (const auto &name :
       {QStringLiteral("keep.log"), QStringLiteral("20180101120000.log"), QStringLiteral("20261128120000-ab.log")}) {
    QFile old(directory.filePath(name));
    QVERIFY(old.open(QIODevice::WriteOnly));
  }
  const auto now = QDateTime::fromString(QStringLiteral("2026-11-28T12:00:00"), Qt::ISODate);
  FileLogOutputter log(directory.path(), FileLogOutputter::Mode::SegmentedDirectory, [now] { return now; });
  for (int i = 0; i < 12; ++i) {
    QVERIFY(log.write(LogLevel::Level::Info, QStringLiteral("session")));
    log.close();
  }
  QVERIFY(!QFileInfo::exists(directory.filePath(QStringLiteral("20180101120000.log"))));
  QVERIFY(QFileInfo::exists(directory.filePath(QStringLiteral("keep.log"))));
  QVERIFY(QFileInfo::exists(directory.filePath(QStringLiteral("20261128120000-ab.log"))));
  const QDir logs(directory.path());
  QCOMPARE(logs.entryList({QStringLiteral("20261128120000-??.log")}, QDir::Files).size(), 11);
  QVERIFY(QFileInfo::exists(directory.filePath(QStringLiteral("20261128120000-11.log"))));
}

QTEST_MAIN(LogTests)
