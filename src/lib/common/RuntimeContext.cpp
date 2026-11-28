/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "RuntimeContext.h"

#include "common/Constants.h"
#include <QDir>
#include <QFileInfo>
#include <optional>

namespace synergy::runtime {
namespace {
std::optional<IsolatedRuntimeContext> s_context;
bool s_sealed = false;

bool safeDescendant(const QString &root, const QString &relative)
{
  QString path = root;
  const auto parts = relative.split(QLatin1Char('/'));
  for (const auto &part : parts) {
    path += QLatin1Char('/') + part;
    const QFileInfo info(path);
    if (info.isSymLink())
      return false;
    if (info.exists()) {
      const auto canonical = info.canonicalFilePath();
      if (!canonical.startsWith(root + QLatin1Char('/')))
        return false;
    }
  }
  return true;
}
} // namespace

bool configureIsolatedRuntime(const IsolatedRuntimeContext &context, QString *error)
{
  const auto fail = [error](const char *message) {
    if (error)
      *error = QString::fromLatin1(message);
    return false;
  };
  if (s_context || s_sealed)
    return fail("Runtime context is already configured or in use.");
  const QFileInfo rootInfo(context.root);
  if (context.root.isEmpty() || !QDir::isAbsolutePath(context.root) || !rootInfo.isDir() || rootInfo.isSymLink())
    return fail("An existing absolute disposable directory is required.");
  const auto root = rootInfo.canonicalFilePath();
  const auto tempRoot = QFileInfo(QDir::tempPath()).canonicalFilePath();
  if (root.isEmpty() || root != QDir::cleanPath(context.root) || tempRoot.isEmpty() ||
      !root.startsWith(tempRoot + QLatin1Char('/')))
    return fail("Runtime root must be a canonical child of the temporary directory.");
  const auto key = context.instanceKey;
  if (key.isEmpty() || key.size() > 72 || key == QString::fromUtf8(kGuiSocketName) ||
      key == QStringLiteral("%1-gui").arg(kAppId))
    return fail("An independent short instance identity is required.");
  for (const auto character : key) {
    const auto value = character.unicode();
    if (value < 33 || value > 126 || character == QLatin1Char('/') || character == QLatin1Char('\\'))
      return fail("Instance identity must be printable ASCII without path characters.");
  }
  const QStringList descendants{
      QStringLiteral("user/%1.conf").arg(kAppName),
      QStringLiteral("system/%1.conf").arg(kAppName),
      QStringLiteral("user/%1.extra.conf").arg(kAppName),
      QStringLiteral("system/%1.extra.conf").arg(kAppName),
      QStringLiteral("state/app.state"),
      QStringLiteral("legacy/user.ini"),
      QStringLiteral("legacy/system.ini"),
      QStringLiteral("test-settings.ini"),
      QStringLiteral("logs/app.log"),
      QStringLiteral("logs/gui"),
      QStringLiteral("logs/core"),
      QStringLiteral("logs/service"),
      QStringLiteral("logs/update"),
      QStringLiteral("user/tls"),
      QStringLiteral("system/tls")
  };
  for (const auto &relative : descendants) {
    if (!safeDescendant(root, relative))
      return fail("Runtime paths may not follow symlinks or escape the disposable root.");
  }
  s_context = IsolatedRuntimeContext{root, key};
  if (error)
    error->clear();
  return true;
}

bool isIsolatedRuntime()
{
  return s_context.has_value();
}

void sealRuntimeContext()
{
  s_sealed = true;
}

QString runtimeRoot()
{
  sealRuntimeContext();
  return s_context ? s_context->root : QString();
}

QString runtimeStateFile()
{
  const auto root = runtimeRoot();
  return root.isEmpty() ? QString() : root + QStringLiteral("/state/app.state");
}

QString runtimeTestSettingsFile()
{
  const auto root = runtimeRoot();
  return root.isEmpty() ? QString() : root + QStringLiteral("/test-settings.ini");
}

QString runtimeInstanceKey(const QString &productionDefault)
{
  sealRuntimeContext();
  return s_context ? s_context->instanceKey : productionDefault;
}

std::unique_ptr<QSettings> legacySettings(QSettings::Scope scope)
{
  const auto root = runtimeRoot();
  if (root.isEmpty())
    return std::make_unique<QSettings>(QSettings::NativeFormat, scope, kAppName, kAppName);
  const auto relative =
      scope == QSettings::SystemScope ? QStringLiteral("/legacy/system.ini") : QStringLiteral("/legacy/user.ini");
  auto settings = std::make_unique<QSettings>(root + relative, QSettings::IniFormat);
  settings->setFallbacksEnabled(false);
  return settings;
}
} // namespace synergy::runtime
