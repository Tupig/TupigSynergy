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
#include <QNetworkAccessManager>
#include <QNetworkReply>
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

void UpdateDownloaderTests::disablesHttp2ForPackageTransfers_data()
{
  QTest::addColumn<int>("size");
  QTest::newRow("single") << 256;
  QTest::newRow("range-fallback") << 9 * 1024 * 1024;
}

void UpdateDownloaderTests::disablesHttp2ForPackageTransfers()
{
  QFETCH(int, size);
  const QByteArray bytes(size, 'a');
  TestHttpServer server;
  server.setBinary(bytes);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  QList<bool> ordinaryRequestsAllowHttp2;
  UpdateDownloader downloader(nullptr, loopbackPolicy());
  auto *network = downloader.findChild<QNetworkAccessManager *>();
  QVERIFY(network);
  connect(network, &QNetworkAccessManager::finished, this, [&ordinaryRequestsAllowHttp2](const QNetworkReply *reply) {
    const auto request = reply->request();
    if (!request.hasRawHeader("Range"))
      ordinaryRequestsAllowHttp2.append(request.attribute(QNetworkRequest::Http2AllowedAttribute, true).toBool());
  });
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  const auto digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), bytes.size(), QString::fromLatin1(digest)}, directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
  QCOMPARE(ordinaryRequestsAllowHttp2, QList<bool>{false});
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

void UpdateDownloaderTests::retriesAfterPartialDisconnect()
{
  TestHttpServer server;
  const auto bytes = QByteArrayLiteral("recoverable package");
  server.setOrdinaryResponses({
      {200, bytes, bytes.size(), 3, true},
      {200, bytes, bytes.size(), -1, false},
  });
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  QSignalSpy progress(&downloader, &UpdateDownloader::progress);
  const auto digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), bytes.size(), QString::fromLatin1(digest)}, directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count() + failed.count(), 1, 5000);
  QCOMPARE(finished.count(), 1);
  QCOMPARE(failed.count(), 0);
  QCOMPARE(server.ordinaryRequests(), 2);
  QCOMPARE(server.partialRequests(), 0);
  bool sawPositive = false;
  bool sawResetAfterPositive = false;
  for (const auto &row : progress) {
    if (row.at(0).toLongLong() > 0)
      sawPositive = true;
    if (sawPositive && row.at(0).toLongLong() == 0)
      sawResetAfterPositive = true;
  }
  QVERIFY(sawPositive);
  QVERIFY(sawResetAfterPositive);
  QFile file(finished.first().first().toString());
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), bytes);
}

void UpdateDownloaderTests::boundsPersistentDisconnect()
{
  TestHttpServer server;
  const auto bytes = QByteArrayLiteral("stalled package");
  server.setOrdinaryResponses({{200, bytes, bytes.size(), 2, true}, {200, bytes, bytes.size(), 2, true}});
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), bytes.size(),
       QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
      directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count() + failed.count(), 1, 5000);
  QCOMPARE(finished.count(), 0);
  QCOMPARE(failed.count(), 1);
  QCOMPARE(server.ordinaryRequests(), 2);
  QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("package.msi"))));
}

void UpdateDownloaderTests::doesNotRetryHttpFailure()
{
  TestHttpServer server;
  server.setOrdinaryResponses({{500, QByteArrayLiteral("no"), 2, -1, false}});
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  const QByteArray bytes("no");
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), bytes.size(),
       QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
      directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count() + failed.count(), 1, 5000);
  QCOMPARE(finished.count(), 0);
  QCOMPARE(failed.count(), 1);
  QCOMPARE(server.ordinaryRequests(), 1);
  QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("package.msi"))));
}

void UpdateDownloaderTests::retriesRangeFallbackAfterDisconnect()
{
  const auto bytes = QByteArray(9 * 1024 * 1024, 'r');
  TestHttpServer server;
  server.setByteRanges(false);
  server.setOrdinaryResponses({{200, bytes, bytes.size(), 4096, true}, {200, bytes, bytes.size(), -1, false}});
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), bytes.size(),
       QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
      directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count() + failed.count(), 1, 5000);
  QCOMPARE(finished.count(), 1);
  QCOMPARE(failed.count(), 0);
  QVERIFY(server.partialRequests() >= 1 && server.partialRequests() <= 3);
  QCOMPARE(server.ordinaryRequests(), 2);
  QFile file(finished.first().first().toString());
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), bytes);
}

void UpdateDownloaderTests::resetsStateForSecondOperation()
{
  TestHttpServer server;
  const auto first = QByteArrayLiteral("first package");
  const auto second = QByteArrayLiteral("second package");
  server.setOrdinaryResponses({
      {200, first, first.size(), 2, true},
      {200, first, first.size(), -1, false},
      {200, second, second.size(), 2, true},
      {200, second, second.size(), -1, false},
  });
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), first.size(),
       QString::fromLatin1(QCryptographicHash::hash(first, QCryptographicHash::Sha256).toHex())},
      directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), second.size(),
       QString::fromLatin1(QCryptographicHash::hash(second, QCryptographicHash::Sha256).toHex())},
      directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 5000);
  QCOMPARE(failed.count(), 0);
  QCOMPARE(server.ordinaryRequests(), 4);
  QFile file(finished.at(1).first().toString());
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), second);
}

void UpdateDownloaderTests::retriesAfterTimeout()
{
  TestHttpServer server;
  const auto bytes = QByteArrayLiteral("timeout package");
  server.setOrdinaryResponses({{200, bytes, bytes.size(), 2, false, true}, {200, bytes, bytes.size(), -1, false}});
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto policy = loopbackPolicy();
  policy.singleRequestTimeoutMs = 350;
  UpdateDownloader downloader(nullptr, policy);
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), bytes.size(),
       QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
      directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count() + failed.count(), 1, 5000);
  QCOMPARE(finished.count(), 1);
  QCOMPARE(failed.count(), 0);
  QCOMPARE(server.ordinaryRequests(), 2);
}

void UpdateDownloaderTests::rejectsOversizeResponse()
{
  TestHttpServer server;
  const auto bytes = QByteArrayLiteral("oversize package");
  server.setBinary(bytes);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  UpdateDownloader downloader(nullptr, loopbackPolicy());
  QSignalSpy finished(&downloader, &UpdateDownloader::finished);
  QSignalSpy failed(&downloader, &UpdateDownloader::failed);
  downloader.start(
      {QStringLiteral("package.msi"), server.url(), bytes.size() - 1,
       QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
      directory.path()
  );
  QTRY_COMPARE_WITH_TIMEOUT(finished.count() + failed.count(), 1, 5000);
  QCOMPARE(finished.count(), 0);
  QCOMPARE(failed.count(), 1);
  QCOMPARE(server.ordinaryRequests(), 1);
  QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("package.msi"))));
}

void UpdateDownloaderTests::destroyingDownloadCancelsAndCleansUp()
{
  TestHttpServer server;
  const auto bytes = QByteArrayLiteral("cancel package");
  server.setOrdinaryResponses({{200, bytes, bytes.size(), 3, false, true}});
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto policy = loopbackPolicy();
  policy.singleRequestTimeoutMs = 350;
  auto *downloader = new UpdateDownloader(nullptr, policy);
  QObject observer;
  int progressCount = 0;
  int finishedCount = 0;
  int failedCount = 0;
  QObject::connect(downloader, &UpdateDownloader::progress, &observer, [&progressCount](qint64 received, qint64) {
    if (received > 0)
      ++progressCount;
  });
  QObject::connect(downloader, &UpdateDownloader::finished, &observer, [&finishedCount] { ++finishedCount; });
  QObject::connect(downloader, &UpdateDownloader::failed, &observer, [&failedCount] { ++failedCount; });
  downloader->start(
      {QStringLiteral("package.msi"), server.url(), bytes.size(),
       QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
      directory.path()
  );
  QTRY_VERIFY_WITH_TIMEOUT(progressCount > 0, 5000);
  delete downloader;
  QTest::qWait(500);
  QCOMPARE(server.ordinaryRequests(), 1);
  QCOMPARE(finishedCount, 0);
  QCOMPARE(failedCount, 0);
  QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("package.msi"))));
}

QTEST_GUILESS_MAIN(UpdateDownloaderTests)
