/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

class QDialog;

namespace synergy::gui {

/**
 * @brief Applies the compact dialog rhythm used by the design review.
 *
 * Every dialog keeps one vertical scale: 12px outer margins, 8px between rows,
 * 10px between groups, 28px control height. Doing it in code (instead of each
 * .ui) keeps every page identical and makes the numbers easy to audit.
 */
void applyCompactDialogStyle(QDialog *dialog);

} // namespace synergy::gui
