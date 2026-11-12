/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateCheckerTests.h"

#include "TestHttpServer.h"
#include "gui/update/UpdateChecker.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using synergy::gui::update::UpdateChecker;
using synergy::gui::update::UpdateInfo;

namespace {

const auto kDigest = QStringLiteral("sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
const auto kReleaseUrl = QStringLiteral("https://github.com/Tupig/TupigSynergy/releases/tag/v1.2.4");

QByteArray releaseJson(
    const QString &tag, const QString &assetName, const QString &assetUrl, const QString &digest,
    const QString &releaseUrl = kReleaseUrl, bool prerelease = false
)
{
  QJsonObject asset{
      {QStringLiteral("name"), assetName},
      {QStringLiteral("browser_download_url"), assetUrl},
      {QStringLiteral("size"), 1024},
  };
  if (!digest.isEmpty()) {
    asset.insert(QStringLiteral("digest"), digest);
  }

  const QJsonObject root{
      {QStringLiteral("tag_name"), tag},
      {QStringLiteral("html_url"), releaseUrl},
      {QStringLiteral("body"), QStringLiteral("release notes")},
      {QStringLiteral("draft"), false},
      {QStringLiteral("prerelease"), prerelease},
      {QStringLiteral("assets"), QJsonArray{asset}},
  };
  return QJsonDocument(root).toJson();
}

QString downloadUrl(const QString &name)
{
  return QStringLiteral("https://github.com/Tupig/TupigSynergy/releases/download/v1.2.4/%1").arg(name);
}

} // namespace

void UpdateCheckerTests::newerVersionEmitsUpdate()
{
  TestHttpServer server;
  server.setJson(releaseJson(
      QStringLiteral("v1.2.4"), QStringLiteral("synergy_1.2.4_windows_x64.msi"),
      downloadUrl(QStringLiteral("synergy_1.2.4_windows_x64.msi")), kDigest
  ));

  UpdateChecker checker;
  checker.setApiUrl(server.url(QStringLiteral("/releases/latest")));
  checker.setCurrentVersion(QStringLiteral("1.2.3"));
  checker.setAssetSuffixes({QStringLiteral("_windows_x64.msi")});

  QSignalSpy available(&checker, &UpdateChecker::updateAvailable);
  checker.checkForUpdates();
  QTRY_COMPARE_WITH_TIMEOUT(available.count(), 1, 5000);

  const auto info = available.first().first().value<UpdateInfo>();
  QCOMPARE(info.version, QStringLiteral("1.2.4"));
  QCOMPARE(info.asset.name, QStringLiteral("synergy_1.2.4_windows_x64.msi"));
  QCOMPARE(info.asset.sha256, kDigest.mid(7));
}

void UpdateCheckerTests::sameVersionEmitsNoUpdate()
{
  TestHttpServer server;
  server.setJson(releaseJson(
      QStringLiteral("v1.2.3"), QStringLiteral("synergy_1.2.3_windows_x64.msi"),
      downloadUrl(QStringLiteral("synergy_1.2.3_windows_x64.msi")), kDigest
  ));

  UpdateChecker checker;
  checker.setApiUrl(server.url());
  checker.setCurrentVersion(QStringLiteral("1.2.3"));
  checker.setAssetSuffixes({QStringLiteral("_windows_x64.msi")});

  QSignalSpy noUpdate(&checker, &UpdateChecker::noUpdate);
  checker.checkForUpdates();
  QTRY_COMPARE_WITH_TIMEOUT(noUpdate.count(), 1, 5000);
}

void UpdateCheckerTests::missingDigestFails()
{
  TestHttpServer server;
  server.setJson(releaseJson(
      QStringLiteral("v1.2.4"), QStringLiteral("synergy_1.2.4_windows_x64.msi"),
      downloadUrl(QStringLiteral("synergy_1.2.4_windows_x64.msi")), QString()
  ));

  UpdateChecker checker;
  checker.setApiUrl(server.url());
  checker.setCurrentVersion(QStringLiteral("1.2.3"));
  checker.setAssetSuffixes({QStringLiteral("_windows_x64.msi")});

  QSignalSpy failed(&checker, &UpdateChecker::failed);
  checker.checkForUpdates();
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
}

void UpdateCheckerTests::nonGithubAssetFails()
{
  TestHttpServer server;
  server.setJson(releaseJson(
      QStringLiteral("v1.2.4"), QStringLiteral("synergy_1.2.4_windows_x64.msi"),
      QStringLiteral("https://example.com/synergy_1.2.4_windows_x64.msi"), kDigest
  ));

  UpdateChecker checker;
  checker.setApiUrl(server.url());
  checker.setCurrentVersion(QStringLiteral("1.2.3"));
  checker.setAssetSuffixes({QStringLiteral("_windows_x64.msi")});

  QSignalSpy failed(&checker, &UpdateChecker::failed);
  checker.checkForUpdates();
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
}

void UpdateCheckerTests::malformedJsonFails()
{
  TestHttpServer server;
  server.setJson(QByteArrayLiteral("{not json"));

  UpdateChecker checker;
  checker.setApiUrl(server.url());
  checker.setCurrentVersion(QStringLiteral("1.2.3"));

  QSignalSpy failed(&checker, &UpdateChecker::failed);
  checker.checkForUpdates();
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
}

void UpdateCheckerTests::selectsPlatformAsset()
{
  TestHttpServer server;
  QJsonObject macAsset{
      {QStringLiteral("name"), QStringLiteral("synergy_1.2.4_mac_arm64.dmg")},
      {QStringLiteral("browser_download_url"), downloadUrl(QStringLiteral("synergy_1.2.4_mac_arm64.dmg"))},
      {QStringLiteral("size"), 2048},
      {QStringLiteral("digest"), kDigest},
  };
  QJsonObject winAsset{
      {QStringLiteral("name"), QStringLiteral("synergy_1.2.4_windows_x64.msi")},
      {QStringLiteral("browser_download_url"), downloadUrl(QStringLiteral("synergy_1.2.4_windows_x64.msi"))},
      {QStringLiteral("size"), 1024},
      {QStringLiteral("digest"), kDigest},
  };
  const QJsonObject root{
      {QStringLiteral("tag_name"), QStringLiteral("v1.2.4")},
      {QStringLiteral("html_url"), kReleaseUrl},
      {QStringLiteral("body"), QStringLiteral("notes")},
      {QStringLiteral("assets"), QJsonArray{macAsset, winAsset}},
  };
  server.setJson(QJsonDocument(root).toJson());

  UpdateChecker checker;
  checker.setApiUrl(server.url());
  checker.setCurrentVersion(QStringLiteral("1.2.3"));
  checker.setAssetSuffixes({QStringLiteral("_windows_x64.msi")});

  QSignalSpy available(&checker, &UpdateChecker::updateAvailable);
  checker.checkForUpdates();
  QTRY_COMPARE_WITH_TIMEOUT(available.count(), 1, 5000);

  const auto info = available.first().first().value<UpdateInfo>();
  QCOMPARE(info.asset.name, QStringLiteral("synergy_1.2.4_windows_x64.msi"));
}

void UpdateCheckerTests::prereleaseIsIgnored()
{
  TestHttpServer server;
  server.setJson(releaseJson(
      QStringLiteral("v1.2.4"), QStringLiteral("synergy_1.2.4_windows_x64.msi"),
      downloadUrl(QStringLiteral("synergy_1.2.4_windows_x64.msi")), kDigest, kReleaseUrl, true
  ));

  UpdateChecker checker;
  checker.setApiUrl(server.url());
  checker.setCurrentVersion(QStringLiteral("1.2.3"));
  checker.setAssetSuffixes({QStringLiteral("_windows_x64.msi")});

  QSignalSpy noUpdate(&checker, &UpdateChecker::noUpdate);
  checker.checkForUpdates();
  QTRY_COMPARE_WITH_TIMEOUT(noUpdate.count(), 1, 5000);
}

QTEST_GUILESS_MAIN(UpdateCheckerTests)
