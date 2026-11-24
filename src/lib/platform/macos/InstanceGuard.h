/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <memory>
#include <utility>

namespace synergy::platform::macos {

enum class InstanceStatus
{
  Primary,
  Duplicate,
  Busy,
  Failure,
  Bypassed
};

struct InstanceGuard
{
  InstanceGuard(InstanceStatus result, const char *message = nullptr, std::unique_ptr<QLockFile> ownedLock = {})
      : status(result),
        detail(message),
        lock(std::move(ownedLock))
  {
  }

  InstanceStatus status;
  const char *detail;
  std::unique_ptr<QLockFile> lock;
  std::unique_ptr<QLocalServer> activationServer;
};

InstanceGuard acquireGuiInstance(const QString &socketName);
InstanceGuard acquireCoreInstance(bool singleInstanceOnly);

namespace detail {
int activationConnectError(int socket, int initialError);
}

} // namespace synergy::platform::macos
