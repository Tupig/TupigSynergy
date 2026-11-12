/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateChecker.h"

#include "UpdateVersion.h"
#include "common/VersionInfo.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>

#include <algorithm>

namespace synergy::gui::update {
namespace {

constexpr auto kApiUrl = "https://api.github.com/repos/Tupig/TupigSynergy/releases/latest";
constexpr auto kReleasePath = "/Tupig/TupigSynergy/releases/";
constexpr auto kDownloadPath = "/Tupig/TupigSynergy/releases/download/";
constexpr auto kNotesLimit = 64 * 1024;
constexpr auto kResponseLimit = 4 * 1024 * 1024;

QHash<QString, QString> readOsRelease()
{
  QHash<QString, QString> values;
  QFile file(QStringLiteral("/etc/os-release"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return values;
  }

  for (const auto &line : QString::fromUtf8(file.readAll()).split(QChar(u'\n'))) {
    const auto equals = line.indexOf(QChar(u'='));
    if (equals <= 0) {
      continue;
    }
    auto value = line.mid(equals + 1).trimmed();
    if (value.size() >= 2 && value.startsWith(QChar(u'"')) && value.endsWith(QChar(u'"'))) {
      value = value.mid(1, value.size() - 2);
    }
    values.insert(line.left(equals).trimmed(), value);
  }
  return values;
}

QString linuxArchitecture()
{
  return QSysInfo::currentCpuArchitecture() == QLatin1String("arm64") ? QStringLiteral("aarch64")
                                                                      : QStringLiteral("x86_64");
}

} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent),
      m_network(new QNetworkAccessManager(this)),
      m_apiUrl(QString::fromLatin1(kApiUrl)),
      m_currentVersion(QString::fromLatin1(kVersion)),
      m_assetSuffixes(defaultAssetSuffixes())
{
}

UpdateChecker::~UpdateChecker()
{
  if (m_reply != nullptr) {
    m_reply->abort();
  }
}

void UpdateChecker::setApiUrl(const QUrl &url)
{
  m_apiUrl = url;
}

void UpdateChecker::setCurrentVersion(const QString &version)
{
  m_currentVersion = version;
}

void UpdateChecker::setAssetSuffixes(const QStringList &suffixes)
{
  m_assetSuffixes = suffixes;
}

QStringList UpdateChecker::defaultAssetSuffixes()
{
#if defined(Q_OS_WIN)
  const auto arch =
      QSysInfo::currentCpuArchitecture() == QLatin1String("arm64") ? QStringLiteral("arm64") : QStringLiteral("x64");
  return {QStringLiteral("_windows_%1.msi").arg(arch)};
#elif defined(Q_OS_MACOS)
  const auto arch =
      QSysInfo::currentCpuArchitecture() == QLatin1String("arm64") ? QStringLiteral("arm64") : QStringLiteral("x64");
  return {QStringLiteral("_mac_%1.dmg").arg(arch)};
#elif defined(Q_OS_LINUX)
  const auto arch = linuxArchitecture();
  const auto os = readOsRelease();
  const auto id = os.value(QStringLiteral("ID")).toLower();
  auto version = os.value(QStringLiteral("VERSION_ID"));
  version.replace(QChar(u'.'), QChar(u'-'));

  QStringList suffixes;
  const auto add = [&suffixes, &arch](const QString &distribution, const QString &extension) {
    suffixes << QStringLiteral("_%1_%2%3").arg(distribution, arch, extension);
  };

  if (id == QLatin1String("ubuntu")) {
    add(QStringLiteral("ubuntu-%1").arg(version), QStringLiteral(".deb"));
  } else if (id == QLatin1String("debian")) {
    add(QStringLiteral("debian-%1").arg(version.section(QChar(u'-'), 0, 0)), QStringLiteral(".deb"));
  } else if (id == QLatin1String("fedora")) {
    add(QStringLiteral("fedora-%1").arg(version.section(QChar(u'-'), 0, 0)), QStringLiteral(".rpm"));
  } else if (id == QLatin1String("rocky") || id == QLatin1String("rhel") || id == QLatin1String("centos")) {
    const auto major = version.section(QChar(u'-'), 0, 0);
    if (major == QLatin1String("8")) {
      add(QStringLiteral("rocky-8-9"), QStringLiteral(".rpm"));
    } else {
      add(QStringLiteral("rocky-9-3"), QStringLiteral(".rpm"));
    }
  } else if (id.startsWith(QLatin1String("opensuse"))) {
    add(QStringLiteral("opensuse-tumbleweed"), QStringLiteral(".rpm"));
  } else if (id == QLatin1String("arch")) {
    add(QStringLiteral("arch-linux"), QStringLiteral(".pkg.tar.zst"));
  }

  // A flatpak is architecture-portable and is the fallback for unknown distros.
  add(QStringLiteral("linux"), QStringLiteral(".flatpak"));
  return suffixes;
#else
  return {};
#endif
}

bool UpdateChecker::isAllowedReleaseUrl(const QUrl &url)
{
  return url.scheme() == QLatin1String("https") && url.host() == QLatin1String("github.com") &&
         url.path().startsWith(QLatin1String(kReleasePath));
}

bool UpdateChecker::isAllowedAssetUrl(const QUrl &url)
{
  return url.scheme() == QLatin1String("https") && url.host() == QLatin1String("github.com") &&
         url.path().startsWith(QLatin1String(kDownloadPath));
}

QString UpdateChecker::sanitizedNotes(const QString &notes)
{
  if (notes.size() <= kNotesLimit) {
    return notes;
  }
  return notes.left(kNotesLimit) + QStringLiteral("\n\n[release notes truncated]");
}

void UpdateChecker::checkForUpdates()
{
  if (m_reply != nullptr) {
    auto *previous = m_reply;
    m_reply = nullptr;
    previous->abort();
    previous->deleteLater();
  }

  QNetworkRequest request(m_apiUrl);
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Synergy-UpdateChecker"));
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(30000);
#endif

  auto *reply = m_network->get(request);
  m_reply = reply;
  // Capture the reply so a superseded request can never handle a newer one.
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (m_reply != reply) {
      return;
    }
    m_reply = nullptr;
    reply->deleteLater();
    handleResponse(reply);
  });
  connect(reply, &QNetworkReply::downloadProgress, this, [this, reply](qint64 received, qint64 total) {
    if (m_reply != reply) {
      return;
    }
    if (received > kResponseLimit || total > kResponseLimit) {
      reply->abort();
    }
  });
}

void UpdateChecker::handleResponse(QNetworkReply *reply)
{
  if (reply->error() != QNetworkReply::NoError) {
    Q_EMIT failed(tr("Could not reach GitHub Releases: %1").arg(reply->errorString()));
    return;
  }

  const auto payload = reply->readAll();
  if (payload.size() > kResponseLimit) {
    Q_EMIT failed(tr("GitHub Releases response is unexpectedly large."));
    return;
  }

  QJsonParseError parseError{};
  const auto document = QJsonDocument::fromJson(payload, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    Q_EMIT failed(tr("GitHub Releases returned an unreadable response."));
    return;
  }

  const auto root = document.object();
  if (root.value(QStringLiteral("draft")).toBool() || root.value(QStringLiteral("prerelease")).toBool()) {
    Q_EMIT noUpdate(m_currentVersion);
    return;
  }

  const auto tag = root.value(QStringLiteral("tag_name")).toString();
  if (!parseVersion(tag).valid) {
    Q_EMIT failed(tr("GitHub Release tag is not a semantic version."));
    return;
  }

  const auto releaseUrl = QUrl(root.value(QStringLiteral("html_url")).toString());
  if (!isAllowedReleaseUrl(releaseUrl)) {
    Q_EMIT failed(tr("GitHub Release URL is outside this repository."));
    return;
  }

  UpdateAsset selected;
  const auto assets = root.value(QStringLiteral("assets")).toArray();
  for (const auto &value : assets) {
    const auto asset = value.toObject();
    const auto name = asset.value(QStringLiteral("name")).toString();
    const auto matches = std::any_of(m_assetSuffixes.cbegin(), m_assetSuffixes.cend(), [&name](const QString &suffix) {
      return name.endsWith(suffix, Qt::CaseInsensitive);
    });
    if (!matches) {
      continue;
    }

    const auto url = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
    if (!isAllowedAssetUrl(url)) {
      Q_EMIT failed(tr("Release asset URL is outside this repository."));
      return;
    }

    const auto digest = asset.value(QStringLiteral("digest")).toString();
    static const auto digestPattern = QRegularExpression(QStringLiteral("^sha256:([0-9a-fA-F]{64})$"));
    const auto match = digestPattern.match(digest);
    if (!match.hasMatch()) {
      Q_EMIT failed(tr("Release asset does not publish a SHA-256 digest; refusing to install it."));
      return;
    }

    selected = {
        name, url, static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble(-1)), match.captured(1).toLower()
    };
    break;
  }

  if (selected.name.isEmpty()) {
    Q_EMIT failed(tr("No release package is available for this platform."));
    return;
  }

  if (compareVersions(tag, m_currentVersion) <= 0) {
    Q_EMIT noUpdate(m_currentVersion);
    return;
  }

  auto version = tag;
  if (version.startsWith(QChar(u'v')) || version.startsWith(QChar(u'V'))) {
    version.remove(0, 1);
  }

  Q_EMIT updateAvailable(
      {version, tag, sanitizedNotes(root.value(QStringLiteral("body")).toString()), releaseUrl, selected}
  );
}

} // namespace synergy::gui::update
