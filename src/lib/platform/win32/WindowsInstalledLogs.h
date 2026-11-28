/* SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception */
#pragma once

#include <QString>

namespace synergy::platform {

//! True only when the executable is inside an MSI layout with a valid marker.
bool isWindowsMsiLayout();

//! Return the protected installed log directory, or empty if unavailable.
//! Service callers receive logs/service; account callers receive logs/accounts/<SID>.
QString windowsInstalledLogDirectory(bool forService);

} // namespace synergy::platform
