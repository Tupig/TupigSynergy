/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateVersionTests.h"

#include "gui/update/UpdateVersion.h"

#include <QTest>

using synergy::gui::update::compareVersions;
using synergy::gui::update::parseVersion;

void UpdateVersionTests::parsesReleaseAndPrerelease()
{
  const auto release = parseVersion(QStringLiteral("v1.21.2"));
  QVERIFY(release.valid);
  QCOMPARE(release.numbers.size(), 3);
  QCOMPARE(release.numbers.at(0), 1);
  QCOMPARE(release.numbers.at(1), 21);
  QCOMPARE(release.numbers.at(2), 2);
  QVERIFY(release.prerelease.isEmpty());

  const auto prerelease = parseVersion(QStringLiteral("1.21.2-dev+8c36d07e"));
  QVERIFY(prerelease.valid);
  QCOMPARE(prerelease.prerelease, QStringList({QStringLiteral("dev")}));

  QVERIFY(!parseVersion(QStringLiteral("not-a-version")).valid);
}

void UpdateVersionTests::comparesNumericComponents()
{
  QVERIFY(compareVersions(QStringLiteral("1.2.3"), QStringLiteral("1.2.4")) < 0);
  QVERIFY(compareVersions(QStringLiteral("v1.2.3"), QStringLiteral("1.2.3")) == 0);
  QVERIFY(compareVersions(QStringLiteral("1.10.0"), QStringLiteral("1.9.9")) > 0);
  QVERIFY(compareVersions(QStringLiteral("2"), QStringLiteral("1.99.99")) > 0);
  QVERIFY(compareVersions(QStringLiteral("1.2"), QStringLiteral("1.2.1")) < 0);
}

void UpdateVersionTests::ordersPrereleases()
{
  QVERIFY(compareVersions(QStringLiteral("1.2.3-dev"), QStringLiteral("1.2.3")) < 0);
  QVERIFY(compareVersions(QStringLiteral("1.2.3-rc.1"), QStringLiteral("1.2.3-rc.2")) < 0);
  QVERIFY(compareVersions(QStringLiteral("1.2.3-rc"), QStringLiteral("1.2.3-rc.1")) < 0);
  QVERIFY(compareVersions(QStringLiteral("1.2.3-2"), QStringLiteral("1.2.3-alpha")) < 0);
}

void UpdateVersionTests::treatsInvalidAsOlder()
{
  QVERIFY(compareVersions(QStringLiteral("garbage"), QStringLiteral("0.0.1")) < 0);
  QVERIFY(compareVersions(QStringLiteral("1.0.0"), QStringLiteral("garbage")) > 0);
  QVERIFY(compareVersions(QStringLiteral("garbage"), QStringLiteral("nonsense")) == 0);
}

QTEST_GUILESS_MAIN(UpdateVersionTests)
