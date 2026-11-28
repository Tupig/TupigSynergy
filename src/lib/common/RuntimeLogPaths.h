/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QString>

namespace synergy {

enum class RuntimeLogRole
{
  Gui,
  Core,
  Daemon,
  Update
};

// Managed paths are mandatory for native installations and isolated probes.
bool usesManagedRuntimeLogs();
QString managedRuntimeLogDirectory(RuntimeLogRole role);

} // namespace synergy
