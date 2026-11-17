/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QSettings>
#include <QString>
#include <memory>

namespace synergy::runtime {

// Internal dependency injection, installed only by the isolated test entry.
// Production has no command-line/environment path that configures this context.
struct IsolatedRuntimeContext
{
  QString root;
  QString instanceKey;
};

bool configureIsolatedRuntime(const IsolatedRuntimeContext &context, QString *error = nullptr);
bool isIsolatedRuntime();
void sealRuntimeContext();
QString runtimeRoot();
QString runtimeStateFile();
QString runtimeTestSettingsFile();
QString runtimeInstanceKey(const QString &productionDefault);
std::unique_ptr<QSettings> legacySettings(QSettings::Scope scope);

} // namespace synergy::runtime
