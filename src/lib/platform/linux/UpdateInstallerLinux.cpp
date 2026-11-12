/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/UpdateInstaller.h"

#include <QProcess>

namespace synergy::platform {

UpdateInstallResult launchUpdateInstaller(const QString &packagePath, QString *error)
{
  // xdg-open hands the .deb/.rpm/.pkg.tar.zst/.flatpak to the desktop's
  // package handler, so the distribution performs the upgrade.
  if (QProcess::startDetached(QStringLiteral("xdg-open"), {packagePath})) {
    return UpdateInstallResult::Launched;
  }

  if (error != nullptr) {
    *error = QStringLiteral("could not open %1 with xdg-open").arg(packagePath);
  }
  return UpdateInstallResult::Failed;
}

} // namespace synergy::platform
