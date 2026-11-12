/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateDownloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace synergy::gui::update {
namespace {

constexpr qint64 kMaxPackageSize = 512LL * 1024 * 1024;

const auto kSha256Pattern = QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"));

} // namespace

UpdateDownloader::UpdateDownloader(QObject *parent, const DownloadPolicy &policy)
    : QObject(parent), m_network(new QNetworkAccessManager(this)), m_policy(policy)
{
}

UpdateDownloader::~UpdateDownloader()
{
  cleanup();
}

bool UpdateDownloader::isAllowedDownloadUrl(const QUrl &url) const
{
  return url.scheme() == m_policy.scheme && url.host() == m_policy.host &&
         url.path().startsWith(m_policy.pathPrefix);
}

QString UpdateDownloader::safeFileName(const QString &name)
{
  const auto baseName = QFileInfo(name).fileName();
  if (baseName.isEmpty() || baseName == QLatin1String(".") || baseName == QLatin1String("..")) {
    return {};
  }
  return baseName;
}

void UpdateDownloader::start(const UpdateAsset &asset, const QString &directory)
{
  cleanup();

  if (!isAllowedDownloadUrl(asset.url)) {
    emit failed(tr("Refusing to download a package outside this repository."));
    return;
  }
  if (!kSha256Pattern.match(asset.sha256).hasMatch()) {
    emit failed(tr("Refusing to download a package without a SHA-256 digest."));
    return;
  }
  if (asset.size > kMaxPackageSize) {
    emit failed(tr("Update package is larger than the allowed limit."));
    return;
  }

  const auto fileName = safeFileName(asset.name);
  if (fileName.isEmpty()) {
    emit failed(tr("Update package has an unsafe file name."));
    return;
  }

  m_asset = asset;
  m_path = QDir(directory).filePath(fileName);
  m_received = 0;
  m_completed = false;
  m_hash.reset();

  m_file = std::make_unique<QFile>(m_path);
  if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    emit failed(tr("Could not create the update file: %1").arg(m_file->errorString()));
    cleanup();
    return;
  }

  QNetworkRequest request(asset.url);
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Synergy-UpdateDownloader"));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setTransferTimeout(300000);

  auto *reply = m_network->get(request);
  m_reply = reply;
  // Capture the reply so an aborted or superseded download cannot handle the
  // bytes of a newer one.
  connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
    if (m_reply != reply || m_file == nullptr) {
      return;
    }
    const auto chunk = reply->readAll();
    m_received += chunk.size();
    if (m_received > kMaxPackageSize || (m_asset.size > 0 && m_received > m_asset.size)) {
      reply->abort();
      return;
    }
    m_hash.addData(chunk);
    if (m_file->write(chunk) != chunk.size()) {
      reply->abort();
      return;
    }
    emit progress(m_received, m_asset.size);
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (m_reply != reply) {
      return;
    }
    finishDownload();
  });
}

void UpdateDownloader::finishDownload()
{
  auto *reply = m_reply;
  if (reply == nullptr) {
    return;
  }
  m_reply = nullptr;
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    emit failed(tr("Could not download the update: %1").arg(reply->errorString()));
    cleanup();
    return;
  }

  if (m_file == nullptr || !m_file->flush()) {
    emit failed(tr("Could not finish writing the update file."));
    cleanup();
    return;
  }
  m_file->close();

  if (m_asset.size > 0 && m_received != m_asset.size) {
    emit failed(tr("Update package size does not match the release metadata."));
    cleanup();
    return;
  }

  if (m_hash.result().toHex() != m_asset.sha256) {
    emit failed(tr("Update package failed SHA-256 verification."));
    cleanup();
    return;
  }

  m_completed = true;
  const auto path = m_path;
  m_file.reset();
  emit finished(path);
}

void UpdateDownloader::cleanup()
{
  if (m_reply != nullptr) {
    auto *reply = m_reply;
    m_reply = nullptr;
    reply->abort();
    reply->deleteLater();
  }
  if (m_file != nullptr) {
    m_file->close();
    m_file.reset();
  }
  if (!m_completed && !m_path.isEmpty()) {
    QFile::remove(m_path);
  }
}

} // namespace synergy::gui::update
