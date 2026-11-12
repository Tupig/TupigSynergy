/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

namespace synergy::platform {

/**
 * @brief Result of handing a verified update package to the OS installer.
 */
enum class UpdateInstallResult
{
  Launched,    //!< The system installer was started; the caller must exit.
  Unsupported, //!< This platform has no automated installer hand-off.
  Failed       //!< Starting the installer failed; `error` describes why.
};

/**
 * @brief Hand a downloaded, digest-verified package to the platform installer.
 *
 * Windows opens the MSI through ShellExecute so UAC owns the elevation prompt;
 * macOS replaces the .app bundle through a detached helper and relaunches it;
 * Linux opens the package with xdg-open so the distribution's package manager
 * performs the upgrade. The caller must quit once the result is Launched.
 */
UpdateInstallResult launchUpdateInstaller(const QString &packagePath, QString *error = nullptr);

} // namespace synergy::platform
