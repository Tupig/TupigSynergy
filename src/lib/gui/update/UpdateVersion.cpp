/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateVersion.h"

#include <algorithm>

namespace synergy::gui::update {
namespace {

bool isNumeric(const QString &text)
{
  if (text.isEmpty()) {
    return false;
  }
  for (const auto character : text) {
    if (character < QChar(u'0') || character > QChar(u'9')) {
      return false;
    }
  }
  return true;
}

QString stripLeadingZeros(const QString &text)
{
  auto index = 0;
  while (index + 1 < text.size() && text.at(index) == QChar(u'0')) {
    ++index;
  }
  return text.mid(index);
}

int comparePrerelease(const QStringList &lhs, const QStringList &rhs)
{
  // A release outranks any prerelease of the same numeric version.
  if (lhs.isEmpty() || rhs.isEmpty()) {
    return lhs.isEmpty() == rhs.isEmpty() ? 0 : (lhs.isEmpty() ? 1 : -1);
  }

  const auto count = std::min(lhs.size(), rhs.size());
  for (auto index = 0; index < count; ++index) {
    const auto &left = lhs.at(index);
    const auto &right = rhs.at(index);
    if (left == right) {
      continue;
    }

    const auto leftNumeric = isNumeric(left);
    const auto rightNumeric = isNumeric(right);
    if (leftNumeric && rightNumeric) {
      // Avoid overflow on hostile identifiers: compare digit strings.
      const auto leftTrimmed = stripLeadingZeros(left);
      const auto rightTrimmed = stripLeadingZeros(right);
      if (leftTrimmed.size() != rightTrimmed.size()) {
        return leftTrimmed.size() < rightTrimmed.size() ? -1 : 1;
      }
      const auto order = leftTrimmed.compare(rightTrimmed);
      if (order != 0) {
        return order < 0 ? -1 : 1;
      }
      continue;
    }
    // Numeric identifiers sort before alphanumeric ones.
    if (leftNumeric != rightNumeric) {
      return leftNumeric ? -1 : 1;
    }
    const auto order = left.compare(right);
    if (order != 0) {
      return order < 0 ? -1 : 1;
    }
  }

  if (lhs.size() == rhs.size()) {
    return 0;
  }
  // A longer prerelease with an equal prefix is newer (1.0.0-rc.2 > 1.0.0-rc).
  return lhs.size() < rhs.size() ? -1 : 1;
}

} // namespace

ParsedVersion parseVersion(QStringView text)
{
  ParsedVersion version;
  auto value = text.trimmed().toString();
  if (value.startsWith(QChar(u'v')) || value.startsWith(QChar(u'V'))) {
    value.remove(0, 1);
  }

  // Drop build metadata; it never affects ordering.
  if (const auto plus = value.indexOf(QChar(u'+')); plus >= 0) {
    value.truncate(plus);
  }

  if (const auto dash = value.indexOf(QChar(u'-')); dash >= 0) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    version.prerelease = value.mid(dash + 1).split(QChar(u'.'), Qt::SkipEmptyParts);
#else
    version.prerelease = value.mid(dash + 1).split(QChar(u'.'), QString::SkipEmptyParts);
#endif
    value.truncate(dash);
  }

  if (value.isEmpty()) {
    return version;
  }

  for (const auto &component : value.split(QChar(u'.'))) {
    if (!isNumeric(component)) {
      return version;
    }
    bool ok = false;
    const auto number = component.toInt(&ok);
    if (!ok) {
      return version;
    }
    version.numbers.append(number);
  }

  if (version.numbers.isEmpty()) {
    return version;
  }
  while (version.numbers.size() < 3) {
    version.numbers.append(0);
  }
  version.valid = true;
  return version;
}

int compareVersions(QStringView lhs, QStringView rhs)
{
  const auto left = parseVersion(lhs);
  const auto right = parseVersion(rhs);

  if (!left.valid || !right.valid) {
    if (left.valid == right.valid) {
      return 0;
    }
    return left.valid ? 1 : -1;
  }

  const auto count = std::max(left.numbers.size(), right.numbers.size());
  for (auto index = 0; index < count; ++index) {
    const auto leftNumber = index < left.numbers.size() ? left.numbers.at(index) : 0;
    const auto rightNumber = index < right.numbers.size() ? right.numbers.at(index) : 0;
    if (leftNumber != rightNumber) {
      return leftNumber < rightNumber ? -1 : 1;
    }
  }

  return comparePrerelease(left.prerelease, right.prerelease);
}

} // namespace synergy::gui::update
