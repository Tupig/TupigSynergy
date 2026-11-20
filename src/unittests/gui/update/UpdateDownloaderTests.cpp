/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateDownloaderTests.h"

#include "TestHttpServer.h"
#include "gui/update/UpdateDownloader.h"

#include <QCryptographicHash>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using synergy::gui::update::DownloadPolicy;
using synergy::gui::update::UpdateAsset;
using synergy::gui::update::UpdateDownloader;

namespace {

DownloadPolicy loopbackPolicy()
{
  return {QStringLiteral("http"), QStringLiteral("127.0.0.1"), QStringLiteral("/")};
}

QByteArray payload()
{
  return QByteArrayLiteral("synthetic update package bytes");
}

} // namespace

void UpdateDownloaderTests::verifiesDigest()
{
  TestHttpServer server;
  server.setBinary(payload());
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);

  UpdateAsset asset{
      QStringLiteral("package.msi"),
      server.url(QStringLiteral("/package.msi")),
      payload().size(),
      QString::fromLatin1(QCryptographicHash::hash(payload(), QCryptographicHash::Sha256).toHex()),
  };

  downloader.start(asset, directory.path());
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
  QCOMPARE(failed.count(), 0);

  QFile file(finished.first().first().toString());
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), payload());
}

void UpdateDownloaderTests::rejectsMismatch()
{
  TestHttpServer server;
  server.setBinary(payload());
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);

  UpdateAsset asset{
      QStringLiteral("package.msi"),
      server.url(QStringLiteral("/package.msi")),
      payload().size(),
      QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
  };

  downloader.start(asset, directory.path());
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
  QCOMPARE(finished.count(), 0);
  QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("package.msi"))));
}

void UpdateDownloaderTests::rejectsUnsafeUrl()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);

  UpdateAsset asset{
      QStringLiteral("package.msi"),
      QUrl(QStringLiteral("https://example.com/package.msi")),
      payload().size(),
      QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
  };

  downloader.start(asset, directory.path());
  QCOMPARE(failed.count(), 1);
}

void UpdateDownloaderTests::rejectsSizeMismatch()
{
  TestHttpServer server;
  server.setBinary(payload());
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);

  UpdateAsset asset{
      QStringLiteral("package.msi"),
      server.url(QStringLiteral("/package.msi")),
      payload().size() + 10,
      QString::fromLatin1(QCryptographicHash::hash(payload(), QCryptographicHash::Sha256).toHex()),
  };

  downloader.start(asset, directory.path());
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
}

void UpdateDownloaderTests::downloadsLargePackage_data()
{
  QTest::addColumn<bool>("ranges");
  QTest::addColumn<bool>("wrongRange");
  QTest::addColumn<bool>("wrongDigest");
  QTest::newRow("parallel") << true << false << false;
  QTest::newRow("fallback-no-range-support") << false << false << false;
  QTest::newRow("fallback-wrong-range") << true << true << false;
  QTest::newRow("corrupt-parallel-download") << true << false << true;
}

void UpdateDownloaderTests::downloadsLargePackage()
{
  QFETCH(bool, ranges);
  QFETCH(bool, wrongRange);
  QFETCH(bool, wrongDigest);
  const QByteArray bytes =
      QByteArray(3 * 1024 * 1024, 'a') + QByteArray(3 * 1024 * 1024, 'b') + QByteArray(3 * 1024 * 1024, 'c');
  TestHttpServer server;
  server.setBinary(bytes);
  server.setByteRanges(ranges, wrongRange);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  const auto digest =
      QCryptographicHash::hash(wrongDigest ? QByteArrayLiteral("wrong") : bytes, QCryptographicHash::Sha256);
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), bytes.size(), QString::fromLatin1(digest.toHex())}, directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count() + failed.count(), 1, 5000);
  QVERIFY(server.partialRequests() >= 1);
  if (ranges && !wrongRange) {
    QCOMPARE(server.partialRequests(), 3);
    QCOMPARE(server.ordinaryRequests(), 0);
  } else {
    QCOMPARE(server.ordinaryRequests(), 1);
  }
  if (wrongDigest) {
    QCOMPARE(failed.count(), 1);
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("package.msi"))));
    return;
  }
  QCOMPARE(failed.count(), 0);
  QFile file(finished.first().first().toString());
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), bytes);
}

void UpdateDownloaderTests::rejectsUnrelatedRedirect_data()
{
  QTest::addColumn<qint64>("size");
  QTest::newRow("single-request") << qint64(256);
  QTest::newRow("parallel-request") << qint64(9 * 1024 * 1024);
}

void UpdateDownloaderTests::rejectsUnrelatedRedirect()
{
  QFETCH(qint64, size);
  TestHttpServer source;
  TestHttpServer unrelated;
  source.setRedirect(unrelated.url());
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  downloader.start(
      {QStringLiteral("package.msi"), source.url(), size, QString(64, QLatin1Char('0'))}, directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
  QCOMPARE(finished.count(), 0);
  QCOMPARE(unrelated.partialRequests() + unrelated.ordinaryRequests(), 0);
  QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("package.msi"))));
}

QTEST_GUILESS_MAIN(UpdateDownloaderTests)
