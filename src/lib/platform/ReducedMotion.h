/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

namespace synergy::platform {

enum class ReduceMotionPreference
{
  Unknown,
  Reduced,
  Full
};

ReduceMotionPreference readMotionPreference();

} // namespace synergy::platform
