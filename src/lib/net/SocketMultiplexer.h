/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <memory>
#include <mutex>
#include <vector>

template <class T> class CondVar;
class Mutex;
class Thread;
class ISocket;
class ISocketMultiplexerJob;

//! Socket multiplexer
/*!
A socket multiplexer services multiple sockets simultaneously.
*/
class SocketMultiplexer
{
public:
  SocketMultiplexer();
  SocketMultiplexer(SocketMultiplexer const &) = delete;
  SocketMultiplexer(SocketMultiplexer &&) = delete;
  ~SocketMultiplexer();

  SocketMultiplexer &operator=(SocketMultiplexer const &) = delete;
  SocketMultiplexer &operator=(SocketMultiplexer &&) = delete;

  //! @name manipulators
  //@{

  void addSocket(ISocket *, ISocketMultiplexerJob *);

  // Wait for any active callback before returning. Callers must not hold
  // socket-owned locks needed by their callback. Safe from a callback itself.
  void removeSocket(ISocket *);

  //@}
  //! @name accessors
  //@{

  // maybe belongs on ISocketMultiplexer
  static SocketMultiplexer *getInstance();

  //@}

private:
  // Service job entry: pairs a socket key with its current job.
  struct SocketJobEntry
  {
    ISocket *socket = nullptr;
    ISocketMultiplexerJob *job = nullptr;
  };

  // Snapshot of job list; job metadata stays alive through deferred deletion.
  using JobSnapshot = std::vector<SocketJobEntry>;

  // Result of executing one job during a poll iteration.  The next job may be
  // the same pointer (job continues), a new job (ownership transfers to the
  // multiplexer), or nullptr (socket no longer serviced).
  struct JobTransition
  {
    ISocket *socket = nullptr;
    ISocketMultiplexerJob *previousJob = nullptr;
    ISocketMultiplexerJob *nextJob = nullptr;
  };

  // Service sockets in a loop.
  [[noreturn]] void serviceThread(const void *);

  // Build a snapshot of active jobs under lock, then unlock.
  JobSnapshot buildJobSnapshot();

  // Apply the results of job execution to m_jobs under lock.
  void updateJobState(const std::vector<JobTransition> &transitions);

private:
  // Lock order: execution, registry, then socket-owned locks after releasing
  // the registry. Recursive so callbacks can unregister themselves.
  std::recursive_mutex m_executionMutex;
  std::unique_ptr<Mutex> m_mutex;
  std::unique_ptr<Thread> m_thread;

  // Condition variable: signaled when m_jobsReady changes or on shutdown.
  std::unique_ptr<CondVar<bool>> m_jobsReady;

  // Flat list of active jobs (no cursor markers).  m_mutex protects all access.
  std::vector<SocketJobEntry> m_jobs;

  // Pending removals: sockets to erase from m_jobs after execution.
  std::vector<ISocket *> m_pendingRemovals;

  // Jobs displaced by a concurrent addSocket() while serviceThread may still
  // be executing them from its snapshot.  Deleted by updateJobState() once
  // the current poll iteration has finished.
  std::vector<ISocketMultiplexerJob *> m_retiredJobs;
};
