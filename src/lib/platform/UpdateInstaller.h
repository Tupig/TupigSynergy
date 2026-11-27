/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

class QTemporaryDir;

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
 * Windows opens the MSI through ShellExecute so UAC owns the elevation prompt.
 * macOS rechecks the package digest in the detached installer, replaces the
 * application contents and relaunches it. On success the helper releases the
 * package in the caller-owned temporary download directory.
 * Linux opens the package with xdg-open so the distribution's package manager
 * performs the upgrade. The caller must quit once the result is Launched.
 */
UpdateInstallResult launchUpdateInstaller(
    const QString &packagePath, const QString &expectedSha256, const QTemporaryDir &downloadDirectory,
    QString *error = nullptr
);

#ifdef Q_OS_MACOS
//! Private temporary download directory template for one release digest.
QString macUpdateDownloadTemplate(const QString &sha256);

//! Find an owned, size-matched download left ready by the Mac helper after status 75.
//! The caller must verify its digest again before installation.
QString findMacUpdateRetry(const QString &assetName, const QString &sha256, qint64 size);

//! Stop offering a rejected download for retry without removing the package.
void forgetMacUpdateRetry(const QString &packagePath, const QString &sha256);
#endif

} // namespace synergy::platform
