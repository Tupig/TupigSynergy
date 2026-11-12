/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace synergy::gui::update {

/**
 * @brief A parsed semantic version.
 *
 * Accepted forms: `1.2.3`, `v1.2.3`, `1.2.3-dev`, `1.2.3-dev+sha`. A leading
 * `v` and build metadata after `+` are ignored for ordering. Missing minor or
 * patch components are treated as zero so release tags and the compiled
 * `kVersion` compare consistently.
 */
struct ParsedVersion
{
  QList<int> numbers;
  QStringList prerelease;
  bool valid = false;
};

ParsedVersion parseVersion(QStringView text);

/**
 * @brief Compare two versions.
 * @return A negative value when `lhs` is older, zero when equal, positive when
 *         newer. An invalid version sorts before every valid one.
 */
int compareVersions(QStringView lhs, QStringView rhs);

} // namespace synergy::gui::update
