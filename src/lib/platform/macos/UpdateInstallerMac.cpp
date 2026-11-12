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
  // `open` hands the DMG to the Finder, which mounts it and shows the app
  // bundle. Replacing the running bundle requires code signing and user
  // consent, so the update deliberately stops at the visible install step.
  if (QProcess::startDetached(QStringLiteral("/usr/bin/open"), {packagePath})) {
    return UpdateInstallResult::Launched;
  }

  if (error != nullptr) {
    *error = QStringLiteral("could not open %1 in the Finder").arg(packagePath);
  }
  return UpdateInstallResult::Failed;
}

} // namespace synergy::platform
