/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/RuntimeLogPaths.h"

#include "common/Constants.h"
#include "common/RuntimeContext.h"

#ifdef Q_OS_WIN
#include "platform/win32/WindowsInstalledLogs.h"
#endif

#include <QDir>

namespace synergy {

bool usesManagedRuntimeLogs()
{
#if defined(Q_OS_MACOS)
  return true;
#elif defined(Q_OS_WIN)
  return runtime::isIsolatedRuntime() || platform::isWindowsMsiLayout();
#else
  return runtime::isIsolatedRuntime();
#endif
}

QString managedRuntimeLogDirectory(const RuntimeLogRole role)
{
  QString directory;
  if (runtime::isIsolatedRuntime()) {
    directory = QDir(runtime::runtimeRoot()).filePath(QStringLiteral("logs"));
  } else {
#if defined(Q_OS_MACOS)
    directory = QDir::home().filePath(QStringLiteral("Library/Logs/%1").arg(kAppName));
#elif defined(Q_OS_WIN)
    directory = platform::windowsInstalledLogDirectory(role == RuntimeLogRole::Daemon);
#endif
  }
  if (directory.isEmpty()) {
    return {};
  }

  if (role == RuntimeLogRole::Daemon) {
#if defined(Q_OS_WIN)
    if (!runtime::isIsolatedRuntime()) {
      return directory;
    }
#endif
    return QDir(directory).filePath(QStringLiteral("service"));
  }
  if (role == RuntimeLogRole::Update) {
    return QDir(directory).filePath(QStringLiteral("update"));
  }
  return QDir(directory).filePath(role == RuntimeLogRole::Gui ? QStringLiteral("gui") : QStringLiteral("core"));
}

} // namespace synergy
