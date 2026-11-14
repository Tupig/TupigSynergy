/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

class QWidget;

void requestOSXNotificationPermission();
bool isOSXDevelopmentBuild();
bool showOSXNotification(const QString &title, const QString &body);
bool isOSXInterfaceStyleDark();
void forceAppActive();
void macOSNativeHide();

// True when this process may use the macOS accessibility API. When it may not,
// registers the app with the system and shows a modeless reminder pointing at
// the Privacy & Security pane. Input sharing is the only feature that needs the
// grant, so callers decide whether to continue without it.
bool checkOSXAccessibilityPermission(QWidget *parent = nullptr);
