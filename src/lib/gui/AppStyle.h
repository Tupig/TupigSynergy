/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

namespace synergy::gui {

/**
 * @brief Application-wide stylesheet for the card-based dark design.
 *
 * Geometry (heights, radii, paddings) is shared by both themes so switching
 * between light and dark never moves a control. Only colours differ.
 */
QString appStyleSheet();

} // namespace synergy::gui
