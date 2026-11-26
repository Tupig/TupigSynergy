/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "UpdateChecker.h"

#include <QCryptographicHash>
#include <QList>
#include <QObject>
#include <QUrl>

#include <memory>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

namespace synergy::gui::update {

/**
 * @brief Where a package may be downloaded from.
 *
 * Production uses this repository's GitHub Release download URL and Releases
 * Asset API. Only test callers can supply a different policy.
 */
struct DownloadPolicy
{
  QString scheme = QStringLiteral("https");
  QString host = QStringLiteral("github.com");
  QString pathPrefix = QStringLiteral("/Tupig/TupigSynergy/releases/download/");
  // Internal policy seam; preserve the existing five-minute request timeout.
  int singleRequestTimeoutMs = 300000;
  // Only the primary route without any received package bytes uses this limit.
  int firstByteTimeoutMs = 15000;
  QUrl assetApiRoot = QUrl(QStringLiteral("https://api.github.com/repos/Tupig/TupigSynergy/releases/assets/"));
  QUrl releaseContentOrigin = QUrl(QStringLiteral("https://release-assets.githubusercontent.com"));
};

/**
 * @brief Streams a release asset to a temporary file and verifies its digest.
 *
 * The SHA-256 is computed while the bytes are written, so the package is never
 * trusted before verification. A mismatch deletes the file and reports an
 * error; the installer is never started with unverified bytes.
 */
class UpdateDownloader : public QObject
{
  Q_OBJECT
public:
  explicit UpdateDownloader(QObject *parent = nullptr, const DownloadPolicy &policy = {});
  ~UpdateDownloader() override;

  void start(const UpdateAsset &asset, const QString &directory);

Q_SIGNALS:
  void progress(qint64 received, qint64 total);
  void usingAlternateSource();
  void finished(const QString &filePath);
  void failed(const QString &reason);

private:
  struct Range
  {
    QNetworkReply *reply = nullptr;
    qint64 first = 0;
    qint64 last = 0;
    qint64 received = 0;
  };

  void startSingleDownload();
  void restartSingleDownload();
  void startRanges();
  QNetworkReply *startRequest(QNetworkRequest request);
  bool readRange(Range &range);
  void finishRange(Range &range);
  void fallBackToSingleDownload();
  bool tryAssetApi();
  void abortSingleRequest();
  void abortRanges();
  void completeDownload();
  void finishDownload();
  void cleanup();
  bool isAllowedDownloadUrl(const QUrl &url) const;
  static QString safeFileName(const QString &name);

  QNetworkAccessManager *m_network = nullptr;
  QNetworkReply *m_reply = nullptr;
  QList<Range> m_ranges;
  std::unique_ptr<QFile> m_file;
  QCryptographicHash m_hash{QCryptographicHash::Sha256};
  DownloadPolicy m_policy;
  UpdateAsset m_asset;
  QUrl m_assetApiUrl;
  QString m_path;
  qint64 m_received = 0;
  bool m_completed = false;
  bool m_singleRetryUsed = false;
  bool m_usingAssetApi = false;
};

} // namespace synergy::gui::update
