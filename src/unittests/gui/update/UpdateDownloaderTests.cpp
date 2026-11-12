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

QTEST_GUILESS_MAIN(UpdateDownloaderTests)
