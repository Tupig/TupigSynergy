/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <algorithm>

namespace synergy::windows {

// Side-effect-free decisions shared by the native desktop, hook and watchdog
// implementations. Keeping OS calls at the call sites makes these boundaries
// testable without installing hooks or switching the user's active desktop.
enum class DesktopAction
{
  None,
  NotifyScreensaver,
  Switch
};

constexpr DesktopAction desktopAction(bool changed, bool screensaverActive)
{
  if (!changed)
    return DesktopAction::None;
  return screensaverActive ? DesktopAction::NotifyScreensaver : DesktopAction::Switch;
}

constexpr bool shouldSyncDesktopKeys(DesktopAction action, bool accessible)
{
  return action == DesktopAction::Switch && accessible;
}

constexpr bool shouldIgnoreInjectedInput(bool primary, bool injected)
{
  return !primary && injected;
}

constexpr unsigned restartDelaySeconds(unsigned failures)
{
  if (failures <= 1)
    return 0;
  const unsigned exponent = (std::min)(failures - 2, 6u);
  return (std::min)(1u << exponent, 60u);
}

constexpr bool hasStableUptime(double seconds)
{
  return seconds >= 10.0;
}

} // namespace synergy::windows
