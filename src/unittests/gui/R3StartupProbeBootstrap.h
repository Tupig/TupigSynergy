/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once
#include <QStringList>
class QApplication;
class MainWindow;
namespace synergy::testing {
bool configureStartupProbe(const QStringList &arguments);
void startupProbePhase(const char *phase);
bool afterStartupProbePreInit();
void startupProbeApplicationReady(QApplication *application);
void startupProbeWindowReady(MainWindow *window);
void startupProbeFinished(int exitCode);
} // namespace synergy::testing
