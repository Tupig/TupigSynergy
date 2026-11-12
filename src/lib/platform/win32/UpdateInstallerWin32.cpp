/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateInstaller.h"

#include <QDir>

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <shellapi.h>

namespace synergy::platform {

UpdateInstallResult launchUpdateInstaller(const QString &packagePath, QString *error)
{
  const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
      nullptr, L"open", reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(packagePath).utf16()), nullptr, nullptr,
      SW_SHOWNORMAL
  ));

  // ShellExecute returns a value greater than 32 on success.
  if (result > 32) {
    return UpdateInstallResult::Launched;
  }

  if (error != nullptr) {
    *error = QStringLiteral("ShellExecuteW failed with code %1").arg(static_cast<qlonglong>(result));
  }
  return UpdateInstallResult::Failed;
}

} // namespace synergy::platform
