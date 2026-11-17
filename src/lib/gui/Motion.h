/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

class QWidget;

namespace synergy::gui {

enum class MotionMode
{
  System,
  Reduced,
  Animated
};

// Internal deterministic check seam; there is no production settings/CLI override.
void setMotionModeForTesting(MotionMode mode);
bool reducedMotion();
void animateContentTransition(QWidget *target);
void installInteractionMotion();

} // namespace synergy::gui
