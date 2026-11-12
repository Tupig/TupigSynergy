/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "UpdateChecker.h"

#include <QCryptographicHash>
#include <QObject>
#include <QUrl>

#include <memory>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace synergy::gui::update {

/**
 * @brief Where a package may be downloaded from.
 *
 * Production always uses the GitHub Release download prefix. Tests inject a
 * loopback server; there is no settings or configuration path that can change
 * this at runtime.
 */
struct DownloadPolicy
{
  QString scheme = QStringLiteral("https");
  QString host = QStringLiteral("github.com");
  QString pathPrefix = QStringLiteral("/Tupig/TupigSynergy/releases/download/");
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
  void finished(const QString &filePath);
  void failed(const QString &reason);

private:
  void finishDownload();
  void cleanup();
  bool isAllowedDownloadUrl(const QUrl &url) const;
  static QString safeFileName(const QString &name);

  QNetworkAccessManager *m_network = nullptr;
  QNetworkReply *m_reply = nullptr;
  std::unique_ptr<QFile> m_file;
  QCryptographicHash m_hash{QCryptographicHash::Sha256};
  DownloadPolicy m_policy;
  UpdateAsset m_asset;
  QString m_path;
  qint64 m_received = 0;
  bool m_completed = false;
};

} // namespace synergy::gui::update
