/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

class QNetworkReply;

namespace synergy::gui::update {

/**
 * @brief A release asset selected for this platform.
 */
struct UpdateAsset
{
  QString name;
  QUrl url;
  qint64 size = -1;
  QString sha256; //!< Lowercase hex digest, never empty.
};

/**
 * @brief A newer GitHub Release and its platform package.
 */
struct UpdateInfo
{
  QString version; //!< Tag without the leading `v`.
  QString tag;
  QString notes;
  QUrl releaseUrl;
  UpdateAsset asset;
};

/**
 * @brief Queries the GitHub Releases API for a newer version.
 *
 * Only HTTPS URLs under `github.com/Tupig/TupigSynergy/releases` are accepted,
 * and a release is usable only when the selected asset publishes a SHA-256
 * digest. A missing digest is a hard failure: an unverifiable package must
 * never reach the installer.
 */
class UpdateChecker : public QObject
{
  Q_OBJECT
public:
  explicit UpdateChecker(QObject *parent = nullptr);
  ~UpdateChecker() override;

  //! Test seam: override the API endpoint (defaults to this repository).
  void setApiUrl(const QUrl &url);
  //! Test seam: compare against this version instead of the compiled one.
  void setCurrentVersion(const QString &version);
  //! Test seam: override the platform package suffixes in priority order.
  void setAssetSuffixes(const QStringList &suffixes);

  void checkForUpdates();

signals:
  void updateAvailable(const UpdateInfo &info);
  void noUpdate(const QString &currentVersion);
  void failed(const QString &reason);

private:
  void handleResponse(QNetworkReply *reply);
  static QStringList defaultAssetSuffixes();
  static bool isAllowedReleaseUrl(const QUrl &url);
  static bool isAllowedAssetUrl(const QUrl &url);
  static QString sanitizedNotes(const QString &notes);

  QNetworkAccessManager *m_network = nullptr;
  QUrl m_apiUrl;
  QString m_currentVersion;
  QStringList m_assetSuffixes;
  QNetworkReply *m_reply = nullptr;
};

} // namespace synergy::gui::update

Q_DECLARE_METATYPE(synergy::gui::update::UpdateInfo)
