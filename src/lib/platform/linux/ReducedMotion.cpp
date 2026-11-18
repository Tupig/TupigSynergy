/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "platform/ReducedMotion.h"

namespace synergy::platform {
ReduceMotionPreference readMotionPreference()
{
  // Linux desktops do not expose a single preference through our current dependencies.
  // The central GUI policy may use Qt's effect flag, without calling it native support.
  return ReduceMotionPreference::Unknown;
}
} // namespace synergy::platform
