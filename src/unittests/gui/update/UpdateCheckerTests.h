/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class UpdateCheckerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void newerVersionEmitsUpdate();
  void sameVersionEmitsNoUpdate();
  void missingDigestFails();
  void nonGithubAssetFails();
  void malformedJsonFails();
  void selectsPlatformAsset();
  void prereleaseIsIgnored();
};
