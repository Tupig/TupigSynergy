/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QCoreApplication>
#include <pthread.h>
#include <stdexcept>
#include <utility>

namespace synergy::platform {

// AppKit also queries the input source. A mutex around our own calls cannot
// serialize those queries, so run TIS work on the same native main thread.
template <typename Action> void onInputSourceThread(Action &&action)
{
  if (pthread_main_np()) {
    std::forward<Action>(action)();
    return;
  }

  auto *app = QCoreApplication::instance();
  if (!app || !QMetaObject::invokeMethod(app, std::forward<Action>(action), Qt::BlockingQueuedConnection))
    throw std::runtime_error("could not dispatch the macOS input source query");
}

} // namespace synergy::platform
