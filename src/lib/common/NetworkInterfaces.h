/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QStringList>

namespace synergy::network {

bool isVirtualInterface(const QString &interfaceName);

//! Return eligible local addresses in order of preference.
QStringList availableAddresses();

//! Select the configured address, the current preferred address or loopback.
QString selectServerBindAddress(const QString &configured, const QStringList &available);

} // namespace synergy::network
