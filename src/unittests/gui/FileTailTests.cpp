/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/FileTail.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using synergy::gui::FileTail;

namespace {
void writeFile(const QString &path, const QString &text, QIODevice::OpenMode mode = QIODevice::WriteOnly)
{
  QFile file(path);
  QVERIFY2(file.open(mode | QIODevice::Text), qPrintable(file.errorString()));
  const auto bytes = text.toUtf8();
  QCOMPARE(file.write(bytes), bytes.size());
}

QStringList lines(const QSignalSpy &spy)
{
  QStringList result;
  for (const auto &arguments : spy)
    result.append(arguments.at(0).toString());
  return result;
}
} // namespace

class FileTailTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void followsFirstCreation();
  void followsRotationAndRecreation_data();
  void followsRotationAndRecreation();
  void handlesAppendAndTruncation();
  void switchesPath();
  void followsRotatingDirectory();
};

void FileTailTests::followsFirstCreation()
{
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const auto path = temp.filePath(QStringLiteral("service.log"));
  FileTail tail(path);
  QSignalSpy spy(&tail, &FileTail::newLine);

  writeFile(path, QStringLiteral("first\n"));
  const QStringList expected{QStringLiteral("first")};
  QTRY_COMPARE_WITH_TIMEOUT(lines(spy), expected, 2000);
}

void FileTailTests::followsRotationAndRecreation_data()
{
  QTest::addColumn<int>("pause");
  QTest::newRow("immediate") << 0;
  QTest::newRow("delayed") << 200;
}

void FileTailTests::followsRotationAndRecreation()
{
  QFETCH(int, pause);
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const QString path = temp.filePath(QStringLiteral("service.log"));
  writeFile(path, QStringLiteral("old\n"));
  FileTail tail(path);
  QSignalSpy spy(&tail, &FileTail::newLine);

  QVERIFY(QFile::rename(path, temp.filePath(QStringLiteral("service.log.1"))));
  if (pause > 0)
    QTest::qWait(pause);
  writeFile(path, QStringLiteral("replacement\n"));
  QTRY_COMPARE_WITH_TIMEOUT(lines(spy), QStringList({QStringLiteral("replacement")}), 2000);
  writeFile(path, QStringLiteral("later\n"), QIODevice::Append);
  const QStringList expected{QStringLiteral("replacement"), QStringLiteral("later")};
  QTRY_COMPARE_WITH_TIMEOUT(lines(spy), expected, 2000);
}

void FileTailTests::handlesAppendAndTruncation()
{
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const QString path = temp.filePath(QStringLiteral("service.log"));
  writeFile(path, QStringLiteral("old\n"));
  FileTail tail(path);
  QSignalSpy spy(&tail, &FileTail::newLine);

  writeFile(path, QStringLiteral("连接成功\n"), QIODevice::Append);
  QTRY_COMPARE_WITH_TIMEOUT(lines(spy), QStringList({QStringLiteral("连接成功")}), 2000);
  spy.clear();
  writeFile(path, QStringLiteral("reset\n"));
  QTRY_COMPARE_WITH_TIMEOUT(lines(spy), QStringList({QStringLiteral("reset")}), 2000);
}

void FileTailTests::switchesPath()
{
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const QString first = temp.filePath(QStringLiteral("first.log"));
  const QString second = temp.filePath(QStringLiteral("second.log"));
  writeFile(first, QStringLiteral("first\n"));
  writeFile(second, QStringLiteral("second\n"));
  FileTail tail(first);
  QSignalSpy spy(&tail, &FileTail::newLine);
  tail.setWatchedFile(second);
  writeFile(second, QStringLiteral("later\n"), QIODevice::Append);
  QTRY_COMPARE_WITH_TIMEOUT(lines(spy), QStringList({QStringLiteral("later")}), 2000);
}

void FileTailTests::followsRotatingDirectory()
{
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const auto first = temp.filePath(QStringLiteral("20261128120000.log"));
  writeFile(first, QStringLiteral("old\n"));
  FileTail tail(temp.path());
  QSignalSpy spy(&tail, &FileTail::newLine);
  writeFile(first, QStringLiteral("tail\n"), QIODevice::Append);
  QTRY_COMPARE_WITH_TIMEOUT(lines(spy), QStringList({QStringLiteral("tail")}), 2000);
  spy.clear();
  const auto second = temp.filePath(QStringLiteral("20261128120001.log"));
  writeFile(second, QStringLiteral("new\n"));
  writeFile(first, QStringLiteral("late\n"), QIODevice::Append);
  QTRY_COMPARE_WITH_TIMEOUT(lines(spy), QStringList({QStringLiteral("late"), QStringLiteral("new")}), 2000);
}

QTEST_GUILESS_MAIN(FileTailTests)
#include "FileTailTests.moc"
