/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QtGlobal>

namespace synergy::gui::diagnostic {

void clearSettings(bool enableRestart);
#ifdef Q_OS_MACOS
bool launchPendingRestart();
#endif
} // namespace synergy::gui::diagnostic
