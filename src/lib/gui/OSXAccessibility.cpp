/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXHelpers.h"

#include <ApplicationServices/ApplicationServices.h>

bool checkOSXAccessibilityPermission(bool prompt)
{
  if (!prompt)
    return AXIsProcessTrusted();

  const void *keys[] = {kAXTrustedCheckOptionPrompt};
  const void *values[] = {kCFBooleanTrue};
  const auto options = CFDictionaryCreate(nullptr, keys, values, 1, nullptr, nullptr);
  const bool trusted = AXIsProcessTrustedWithOptions(options);
  CFRelease(options);
  return trusted;
}
