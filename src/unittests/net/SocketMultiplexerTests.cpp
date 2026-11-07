/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>
#if defined(Q_OS_WIN)
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "arch/Arch.h"
#include "base/FinalAction.h"
#include "base/Log.h"
#include "io/IOException.h"
#include "net/ISocket.h"
#include "net/ISocketMultiplexerJob.h"
#include "net/NetworkAddress.h"
#include "net/SocketMultiplexer.h"
#include "net/TCPListenSocket.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;

enum class Gate
{
  Snapshot,
  Run,
  SelfRemove,
  None
};

struct JobState
{
  std::promise<void> entered;
  std::promise<void> release;
  std::shared_future<void> released = release.get_future().share();
  std::promise<void> destroyed;
  std::atomic<bool> enteredOnce = false;
  std::atomic<bool> releasedOnce = false;
  std::atomic<int> calls = 0;
  std::function<void()> afterGate;

  void unblock()
  {
    if (!releasedOnce.exchange(true))
      release.set_value();
  }
};

class ReadySocket : public ISocket
{
public:
  explicit ReadySocket(SocketMultiplexer &multiplexer) : multiplexer(multiplexer)
  {
    listener = ARCH->newSocket(IArchNetwork::AddressFamily::INet, IArchNetwork::SocketType::Stream);
    NetworkAddress address("127.0.0.1", 0);
    address.resolve();
    ARCH->bindSocket(listener, address.getAddress());
    ARCH->listenOnSocket(listener);
    sockaddr_in bound{};
#if defined(Q_OS_WIN)
    int size = sizeof(bound);
    const auto result = getsockname(listener->m_socket, reinterpret_cast<sockaddr *>(&bound), &size);
#else
    socklen_t size = sizeof(bound);
    const auto result = getsockname(listener->m_fd, reinterpret_cast<sockaddr *>(&bound), &size);
#endif
    if (result != 0 || bound.sin_port == 0)
      throw std::runtime_error("cannot obtain test listener port");
    peer = ARCH->newSocket(IArchNetwork::AddressFamily::INet, IArchNetwork::SocketType::Stream);
    NetworkAddress remote("127.0.0.1", ntohs(bound.sin_port));
    remote.resolve();
    ARCH->connectSocket(peer, remote.getAddress());
  }
  ~ReadySocket() override
  {
    close();
  }
  void bind(const NetworkAddress &) override
  {
  }
  void close() override
  {
    multiplexer.removeSocket(this);
    if (peer) {
      ARCH->closeSocket(peer);
      peer = nullptr;
    }
    if (listener) {
      ARCH->closeSocket(listener);
      listener = nullptr;
    }
  }
  void *getEventTarget() const override
  {
    return const_cast<ReadySocket *>(this);
  }
  SocketMultiplexer &multiplexer;
  ArchSocket listener = nullptr;
  ArchSocket peer = nullptr;
};

class GatedJob : public ISocketMultiplexerJob
{
public:
  GatedJob(ReadySocket &owner, std::shared_ptr<JobState> state, Gate gate, std::shared_ptr<JobState> next = {})
      : owner(owner),
        state(std::move(state)),
        gate(gate),
        next(std::move(next)),
        socket(ARCH->copySocket(owner.listener))
  {
  }
  ~GatedJob() override
  {
    ARCH->closeSocket(socket);
    state->destroyed.set_value();
  }
  ArchSocket getSocket() const override
  {
    return socket;
  }
  bool isReadable() const override
  {
    if (gate == Gate::Snapshot && !state->enteredOnce.exchange(true)) {
      state->entered.set_value();
      state->released.wait();
    }
    return true;
  }
  bool isWritable() const override
  {
    return false;
  }
  ISocketMultiplexerJob *run(bool, bool, bool) override
  {
    ++state->calls;
    if (gate == Gate::Run) {
      state->entered.set_value();
      state->released.wait();
    } else if (gate == Gate::SelfRemove) {
      owner.multiplexer.removeSocket(&owner);
    }
    if (state->afterGate)
      state->afterGate();
    return next ? new GatedJob(owner, next, Gate::None) : nullptr;
  }

private:
  ReadySocket &owner;
  std::shared_ptr<JobState> state;
  Gate gate;
  std::shared_ptr<JobState> next;
  ArchSocket socket;
};
} // namespace

class SocketMultiplexerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase()
  {
    arch.init();
  }

  void removedSnapshotNeverRuns()
  {
    SocketMultiplexer multiplexer;
    ReadySocket socket(multiplexer);
    auto state = std::make_shared<JobState>();
    auto entered = state->entered.get_future();
    auto destroyed = state->destroyed.get_future();
    const auto release = synergy::finally([&] { state->unblock(); });
    multiplexer.addSocket(&socket, new GatedJob(socket, state, Gate::Snapshot));
    QVERIFY(entered.wait_for(5s) == std::future_status::ready);
    multiplexer.removeSocket(&socket);
    state->unblock();
    QVERIFY(destroyed.wait_for(5s) == std::future_status::ready);
    QCOMPARE(state->calls.load(), 0);
  }

  void removeWaitsForActiveCallback()
  {
    SocketMultiplexer multiplexer;
    ReadySocket socket(multiplexer);
    auto state = std::make_shared<JobState>();
    auto entered = state->entered.get_future();
    const auto release = synergy::finally([&] { state->unblock(); });
    multiplexer.addSocket(&socket, new GatedJob(socket, state, Gate::Run));
    QVERIFY(entered.wait_for(5s) == std::future_status::ready);
    std::promise<void> starting;
    std::promise<void> returned;
    auto started = starting.get_future();
    auto removed = returned.get_future();
    std::thread remover([&] {
      starting.set_value();
      multiplexer.removeSocket(&socket);
      returned.set_value();
    });
    const auto join = synergy::finally([&] {
      state->unblock();
      remover.join();
    });
    started.wait();
    // A bounded negative observation, not a sleep used to make teardown safe.
    const auto premature = removed.wait_for(100ms);
    state->unblock();
    QVERIFY(removed.wait_for(5s) == std::future_status::ready);
    QVERIFY(premature == std::future_status::timeout);
    QCOMPARE(state->calls.load(), 1);
  }

  void replacementSkipsStaleSnapshot()
  {
    SocketMultiplexer multiplexer;
    ReadySocket socket(multiplexer);
    auto old = std::make_shared<JobState>();
    auto replacement = std::make_shared<JobState>();
    auto entered = old->entered.get_future();
    auto finished = replacement->destroyed.get_future();
    const auto release = synergy::finally([&] { old->unblock(); });
    multiplexer.addSocket(&socket, new GatedJob(socket, old, Gate::Snapshot));
    QVERIFY(entered.wait_for(5s) == std::future_status::ready);
    multiplexer.removeSocket(&socket);
    multiplexer.addSocket(&socket, new GatedJob(socket, replacement, Gate::None));
    old->unblock();
    QVERIFY(finished.wait_for(5s) == std::future_status::ready);
    QCOMPARE(old->calls.load(), 0);
    QCOMPARE(replacement->calls.load(), 1);
  }

  void listenerDestructionWaitsForErrorCallback()
  {
    SocketMultiplexer multiplexer;
    ReadySocket ready(multiplexer);
    auto listener = std::make_unique<TCPListenSocket>(nullptr, &multiplexer, IArchNetwork::AddressFamily::INet);
    auto state = std::make_shared<JobState>();
    auto entered = state->entered.get_future();
    auto jobDestroyed = state->destroyed.get_future();
    auto *target = listener.get();
    state->afterGate = [target] { target->serviceListening(nullptr, false, false, true); };
    const auto release = synergy::finally([&] { state->unblock(); });
    multiplexer.addSocket(target, new GatedJob(ready, state, Gate::Run));
    QVERIFY(entered.wait_for(5s) == std::future_status::ready);
    std::promise<void> starting;
    std::promise<void> returned;
    auto started = starting.get_future();
    auto destroyed = returned.get_future();
    std::thread destroyer([&] {
      starting.set_value();
      listener.reset();
      returned.set_value();
    });
    const auto join = synergy::finally([&] {
      state->unblock();
      destroyer.join();
    });
    started.wait();
    const auto premature = destroyed.wait_for(100ms);
    state->unblock();
    QVERIFY(destroyed.wait_for(5s) == std::future_status::ready);
    QVERIFY(premature == std::future_status::timeout);
    QVERIFY(jobDestroyed.wait_for(5s) == std::future_status::ready);
    QCOMPARE(state->calls.load(), 1);
  }

  void listenerRepeatedCloseStillThrows()
  {
    SocketMultiplexer multiplexer;
    TCPListenSocket listener(nullptr, &multiplexer, IArchNetwork::AddressFamily::INet);
    listener.close();
    QVERIFY_EXCEPTION_THROWN(listener.close(), IOClosedException);
  }

  void callbackCanRemoveItselfAndDiscardNextJob()
  {
    SocketMultiplexer multiplexer;
    ReadySocket socket(multiplexer);
    auto state = std::make_shared<JobState>();
    auto next = std::make_shared<JobState>();
    auto finished = next->destroyed.get_future();
    multiplexer.addSocket(&socket, new GatedJob(socket, state, Gate::SelfRemove, next));
    QVERIFY(finished.wait_for(5s) == std::future_status::ready);
    QCOMPARE(state->calls.load(), 1);
    QCOMPARE(next->calls.load(), 0);
  }

private:
  Arch arch;
  Log log;
};

QTEST_GUILESS_MAIN(SocketMultiplexerTests)
#include "SocketMultiplexerTests.moc"
