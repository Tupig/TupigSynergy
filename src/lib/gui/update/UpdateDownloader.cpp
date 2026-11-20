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

#include <utility>

namespace synergy::gui::update {
namespace {

constexpr qint64 kMaxPackageSize = 512LL * 1024 * 1024;
constexpr qint64 kParallelThreshold = 8LL * 1024 * 1024;
constexpr int kRangeCount = 3;

const auto kSha256Pattern = QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"));

} // namespace

UpdateDownloader::UpdateDownloader(QObject *parent, const DownloadPolicy &policy)
    : QObject(parent),
      m_network(new QNetworkAccessManager(this)),
      m_policy(policy)
{
}

UpdateDownloader::~UpdateDownloader()
{
  cleanup();
}

bool UpdateDownloader::isAllowedDownloadUrl(const QUrl &url) const
{
  return url.scheme() == m_policy.scheme && url.host() == m_policy.host && url.userInfo().isEmpty() &&
         (m_policy.scheme != QLatin1String("https") || url.port(443) == 443) &&
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
    Q_EMIT failed(tr("Refusing to download a package outside this repository."));
    return;
  }
  if (!kSha256Pattern.match(asset.sha256).hasMatch()) {
    Q_EMIT failed(tr("Refusing to download a package without a SHA-256 digest."));
    return;
  }
  if (asset.size > kMaxPackageSize) {
    Q_EMIT failed(tr("Update package is larger than the allowed limit."));
    return;
  }

  const auto fileName = safeFileName(asset.name);
  if (fileName.isEmpty()) {
    Q_EMIT failed(tr("Update package has an unsafe file name."));
    return;
  }

  m_asset = asset;
  m_path = QDir(directory).filePath(fileName);
  m_received = 0;
  m_completed = false;
  m_hash.reset();

  m_file = std::make_unique<QFile>(m_path);
  if (!m_file->open(QIODevice::ReadWrite | QIODevice::Truncate)) {
    const auto reason = tr("Could not create the update file: %1").arg(m_file->errorString());
    cleanup();
    Q_EMIT failed(reason);
    return;
  }

  if (asset.size >= kParallelThreshold)
    startRanges();
  else
    startSingleDownload();
}

void UpdateDownloader::startSingleDownload()
{
  QNetworkRequest request(m_asset.url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(300000);
#endif

  auto *reply = startRequest(request);
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
    Q_EMIT progress(m_received, m_asset.size);
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (m_reply != reply) {
      return;
    }
    finishDownload();
  });
}

void UpdateDownloader::startRanges()
{
  m_ranges.reserve(kRangeCount);
  for (int index = 0; index < kRangeCount; ++index) {
    const auto first = m_asset.size * index / kRangeCount;
    const auto last = m_asset.size * (index + 1) / kRangeCount - 1;
    QNetworkRequest request(m_asset.url);
    // Give each range an independent connection on slow or lossy links.
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setRawHeader("Range", "bytes=" + QByteArray::number(first) + '-' + QByteArray::number(last));
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif

    auto *reply = startRequest(request);
    reply->setReadBufferSize(64 * 1024);
    m_ranges.append({reply, first, last, 0});
    connect(reply, &QNetworkReply::readyRead, this, [this, index] {
      if (readRange(m_ranges[index]))
        Q_EMIT progress(m_received, m_asset.size);
    });
    connect(reply, &QNetworkReply::finished, this, [this, index] { finishRange(m_ranges[index]); });
  }
}

QNetworkReply *UpdateDownloader::startRequest(QNetworkRequest request)
{
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Synergy-UpdateDownloader"));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
  auto *reply = m_network->get(request);
  connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl &url) {
    const auto target = reply->url().resolved(url);
    const int defaultPort = m_policy.scheme == QLatin1String("https") ? 443 : 80;
    const bool sameRepository =
        isAllowedDownloadUrl(target) && target.port(defaultPort) == m_asset.url.port(defaultPort);
    const bool releaseCdn = m_policy.scheme == QLatin1String("https") && m_policy.host == QLatin1String("github.com") &&
                            target.scheme() == QLatin1String("https") && target.port(443) == 443 &&
                            target.host() == QLatin1String("release-assets.githubusercontent.com") &&
                            target.userInfo().isEmpty();
    if (sameRepository || releaseCdn) {
      Q_EMIT reply->redirectAllowed();
      return;
    }
    cleanup();
    Q_EMIT failed(tr("Refusing to download a package outside this repository."));
  });
  return reply;
}

bool UpdateDownloader::readRange(Range &range)
{
  auto *reply = range.reply;
  const auto expectedRange = "bytes " + QByteArray::number(range.first) + '-' + QByteArray::number(range.last) + '/' +
                             QByteArray::number(m_asset.size);
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 206 ||
      reply->rawHeader("Content-Range") != expectedRange) {
    fallBackToSingleDownload();
    return false;
  }
  if (reply->bytesAvailable() == 0)
    return true;
  const auto chunk = reply->readAll();
  if (chunk.size() > range.last - range.first + 1 - range.received || !m_file->seek(range.first + range.received) ||
      m_file->write(chunk) != chunk.size()) {
    fallBackToSingleDownload();
    return false;
  }
  range.received += chunk.size();
  m_received += chunk.size();
  return true;
}

void UpdateDownloader::finishRange(Range &range)
{
  auto *reply = range.reply;
  if (reply->error() != QNetworkReply::NoError) {
    fallBackToSingleDownload();
    return;
  }
  if (!readRange(range))
    return;
  if (range.received != range.last - range.first + 1) {
    fallBackToSingleDownload();
    return;
  }
  range.reply = nullptr;
  reply->deleteLater();
  for (const auto &part : std::as_const(m_ranges)) {
    if (part.reply != nullptr)
      return;
  }
  m_ranges.clear();
  if (!m_file->flush() || !m_file->seek(0) || !m_hash.addData(m_file.get())) {
    cleanup();
    Q_EMIT failed(tr("Could not finish writing the update file."));
    return;
  }
  completeDownload();
}

void UpdateDownloader::fallBackToSingleDownload()
{
  abortRanges();
  m_received = 0;
  if (!m_file->resize(0) || !m_file->seek(0)) {
    cleanup();
    Q_EMIT failed(tr("Could not finish writing the update file."));
    return;
  }
  startSingleDownload();
  Q_EMIT progress(0, m_asset.size);
}

void UpdateDownloader::abortRanges()
{
  for (const auto &range : std::as_const(m_ranges)) {
    if (range.reply != nullptr) {
      range.reply->disconnect(this);
      range.reply->abort();
      range.reply->deleteLater();
    }
  }
  m_ranges.clear();
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
    const auto reason = tr("Could not download the update: %1").arg(reply->errorString());
    cleanup();
    Q_EMIT failed(reason);
    return;
  }

  if (m_file == nullptr || !m_file->flush()) {
    cleanup();
    Q_EMIT failed(tr("Could not finish writing the update file."));
    return;
  }
  completeDownload();
}

void UpdateDownloader::completeDownload()
{
  m_file->close();

  if (m_asset.size > 0 && m_received != m_asset.size) {
    cleanup();
    Q_EMIT failed(tr("Update package size does not match the release metadata."));
    return;
  }

  if (m_hash.result().toHex() != m_asset.sha256) {
    cleanup();
    Q_EMIT failed(tr("Update package failed SHA-256 verification."));
    return;
  }

  m_completed = true;
  const auto path = m_path;
  m_file.reset();
  Q_EMIT finished(path);
}

void UpdateDownloader::cleanup()
{
  abortRanges();
  if (m_reply != nullptr) {
    auto *reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);
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
