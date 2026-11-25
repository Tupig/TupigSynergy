/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QEvent>
#include <QThread>
#include <functional>
#include <utility>

namespace synergy::platform {

// Keep the main event loop running while the worker can still need to query
// the input source. The worker's finished signal will quit Qt after cleanup.
class OSXCoreQuitFilter final : public QObject
{
public:
  OSXCoreQuitFilter(QThread &worker, std::function<void()> requestStop)
      : m_worker(worker),
        m_requestStop(std::move(requestStop))
  {
  }

protected:
  bool eventFilter(QObject *, QEvent *event) override
  {
    if (event->type() != QEvent::Quit || !m_worker.isRunning())
      return false;

    if (!std::exchange(m_stopRequested, true))
      m_requestStop();
    event->ignore();
    return true;
  }

private:
  QThread &m_worker;
  std::function<void()> m_requestStop;
  bool m_stopRequested = false;
};

} // namespace synergy::platform
