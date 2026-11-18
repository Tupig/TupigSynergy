/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "platform/ReducedMotion.h"
#import <AppKit/AppKit.h>

namespace synergy::platform {
ReduceMotionPreference readMotionPreference()
{
  if (@available(macOS 10.12, *)) {
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion ? ReduceMotionPreference::Reduced
                                                                              : ReduceMotionPreference::Full;
  }
  return ReduceMotionPreference::Unknown;
}
} // namespace synergy::platform
