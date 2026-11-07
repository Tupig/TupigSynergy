/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenConfig.h"
namespace synergy::gui {

const char *ScreenConfig::m_modifierNames[] = {"shift", "ctrl", "alt", "meta", "super", "altgr", "none"};

const char *ScreenConfig::m_fixNames[] = {
    "halfDuplexCapsLock", "halfDuplexNumLock", "halfDuplexScrollLock", "xtestIsXineramaUnaware"
};

const char *ScreenConfig::m_switchCornerNames[] = {"top-left", "top-right", "bottom-left", "bottom-right"};

} // namespace synergy::gui
