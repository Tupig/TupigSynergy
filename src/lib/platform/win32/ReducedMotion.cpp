/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "platform/ReducedMotion.h"
#include <Windows.h>

namespace synergy::platform {
ReduceMotionPreference readMotionPreference()
{
  BOOL enabled = TRUE;
  if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
    return ReduceMotionPreference::Unknown;
  return enabled ? ReduceMotionPreference::Full : ReduceMotionPreference::Reduced;
}
} // namespace synergy::platform
