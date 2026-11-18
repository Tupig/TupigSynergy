/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QString>

namespace synergy::gui {

/// Translates display text only; uncertain matches retain the original message.
QString translateEngineLogMessage(const QString &message);

} // namespace synergy::gui
