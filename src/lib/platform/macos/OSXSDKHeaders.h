/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

// Apple's SDK headers still declare APIs that were deprecated years ago, and
// newer SDKs add availability attributes to more symbols. The project builds
// with -Werror, so suppress those warnings only while the SDK headers are
// parsed. Project code keeps every warning: the push/pop scope ends here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>

#pragma clang diagnostic pop
