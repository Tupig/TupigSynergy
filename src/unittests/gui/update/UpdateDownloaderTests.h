/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class UpdateDownloaderTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void verifiesDigest();
  void rejectsMismatch();
  void rejectsUnsafeUrl();
  void rejectsSizeMismatch();
  void downloadsLargePackage_data();
  void downloadsLargePackage();
  void disablesHttp2ForPackageTransfers_data();
  void disablesHttp2ForPackageTransfers();
  void rejectsUnrelatedRedirect_data();
  void rejectsUnrelatedRedirect();
  void retriesAfterPartialDisconnect();
  void boundsPersistentDisconnect();
  void doesNotRetryHttpFailure_data();
  void doesNotRetryHttpFailure();
  void retriesRangeFallbackAfterDisconnect();
  void resetsStateForSecondOperation();
  void retriesAfterTimeout();
  void fallsBackToOfficialAsset_data();
  void fallsBackToOfficialAsset();
  void firstByteDeadlineDoesNotInterruptProgress();
  void stopsWhileUsingOfficialAsset();
  void rejectsOversizeResponse();
  void destroyingDownloadCancelsAndCleansUp();
};
