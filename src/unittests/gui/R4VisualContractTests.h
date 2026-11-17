/* SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception */
#pragma once
#include <QString>
class MainWindow;
namespace synergy::testing {
bool prepareR4VisualFixture(const QString &root);
void observeR4VisualMain(MainWindow *window, const QString &root);
} // namespace synergy::testing
