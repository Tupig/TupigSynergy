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
#include <string>
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
  std::atomic<int> snapshots = 0;
  std::atomic<int> destructions = 0;
  std::atomic<bool> read = false;
  std::atomic<bool> write = false;
  std::atomic<bool> error = false;
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
    bool constructed = false;
    const auto cleanup = synergy::finally([&] {
      if (!constructed)
        close();
    });
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
    constructed = true;
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
  GatedJob(
      ReadySocket &owner, std::shared_ptr<JobState> state, Gate gate, std::shared_ptr<JobState> next = {},
      ArchSocket selected = nullptr, bool readable = true
  )
      : owner(owner),
        state(std::move(state)),
        gate(gate),
        next(std::move(next)),
        socket(ARCH->copySocket(selected ? selected : owner.listener)),
        readable(readable)
  {
  }
  ~GatedJob() override
  {
    ARCH->closeSocket(socket);
    ++state->destructions;
    state->destroyed.set_value();
  }
  ArchSocket getSocket() const override
  {
    return socket;
  }
  bool isReadable() const override
  {
    ++state->snapshots;
    if (gate == Gate::Snapshot && !state->enteredOnce.exchange(true)) {
      state->entered.set_value();
      state->released.wait();
    }
    return readable;
  }
  bool isWritable() const override
  {
    return false;
  }
  ISocketMultiplexerJob *run(bool read, bool write, bool error) override
  {
    state->read = read;
    state->write = write;
    state->error = error;
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
  bool readable;
};

void reportJobState(const char *phase, const JobState &old, const JobState &replacement)
{
  qInfo(
      "%s old snapshots=%d calls=%d destroyed=%d flags=%d/%d/%d replacement snapshots=%d calls=%d "
      "destroyed=%d flags=%d/%d/%d",
      phase, old.snapshots.load(), old.calls.load(), old.destructions.load(), int(old.read.load()),
      int(old.write.load()), int(old.error.load()), replacement.snapshots.load(), replacement.calls.load(),
      replacement.destructions.load(), int(replacement.read.load()), int(replacement.write.load()),
      int(replacement.error.load())
  );
}

#if defined(Q_OS_WIN)
int nativeReady(ArchSocket socket, bool read, int milliseconds)
{
  fd_set selected;
  FD_ZERO(&selected);
  FD_SET(socket->m_socket, &selected);
  timeval timeout{milliseconds / 1000, (milliseconds % 1000) * 1000};
  const int result = select(0, read ? &selected : nullptr, read ? nullptr : &selected, nullptr, &timeout);
  if (result == SOCKET_ERROR)
    throw std::runtime_error("native select failed: " + std::to_string(WSAGetLastError()));
  return result;
}

class ConnectedLoopback
{
public:
  explicit ConnectedLoopback(SocketMultiplexer &multiplexer) : owner(multiplexer)
  {
    if (nativeReady(owner.listener, true, 5000) != 1 || nativeReady(owner.peer, false, 5000) != 1)
      throw std::runtime_error("loopback connection did not become ready");
    ARCH->throwErrorOnSocket(owner.peer);
    accepted = ARCH->acceptSocket(owner.listener, nullptr);
    if (!accepted)
      throw std::runtime_error("ready listener had no connection to accept");
  }
  ~ConnectedLoopback()
  {
    ARCH->closeSocket(accepted);
  }
  ReadySocket owner;
  ArchSocket accepted = nullptr;
};

struct PollObservation
{
  int status;
  unsigned short events;
};

PollObservation readPoll(ArchSocket socket)
{
  IArchNetwork::PollEntry entry{};
  entry.m_socket = socket;
  entry.m_events = IArchNetwork::PollEventMask::In;
  const auto status = ARCH->pollSocket(&entry, 1, 0.0);
  return {status, entry.m_revents};
}

void reportPolls(const char *phase, int before, PollObservation first, PollObservation second, int after)
{
  qInfo(
      "%s select-before=%d poll-first=%d/%u poll-second=%d/%u select-after=%d", phase, before, first.status,
      unsigned(first.events), second.status, unsigned(second.events), after
  );
}
#endif
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
    auto oldDestroyed = old->destroyed.get_future();
    auto finished = replacement->destroyed.get_future();
    const auto release = synergy::finally([&] { old->unblock(); });
    multiplexer.addSocket(&socket, new GatedJob(socket, old, Gate::Snapshot));
    QVERIFY(entered.wait_for(5s) == std::future_status::ready);
    reportJobState("C1 old metadata gated", *old, *replacement);
#if defined(Q_OS_WIN)
    qInfo("C1 original listener select-ready=%d", nativeReady(socket.listener, true, 0));
#endif
    multiplexer.removeSocket(&socket);
    multiplexer.addSocket(&socket, new GatedJob(socket, replacement, Gate::None));
    reportJobState("C1 replacement registered before poll", *old, *replacement);
    old->unblock();
    const auto outcome = finished.wait_for(5s);
    reportJobState("C1 replacement completion before teardown", *old, *replacement);
    QVERIFY(outcome == std::future_status::ready);
    QCOMPARE(old->calls.load(), 0);
    QCOMPARE(replacement->calls.load(), 1);
    QVERIFY(oldDestroyed.wait_for(5s) == std::future_status::ready);
    QCOMPARE(old->destructions.load(), 1);
    QCOMPARE(replacement->destructions.load(), 1);
  }

#if defined(Q_OS_WIN)
  void windowsRepeatedPollRetainsPendingAccept()
  {
    SocketMultiplexer multiplexer;
    ReadySocket socket(multiplexer);
    QCOMPARE(nativeReady(socket.listener, true, 5000), 1);
    const auto before = nativeReady(socket.listener, true, 0);
    const auto first = readPoll(socket.listener);
    const auto second = readPoll(socket.listener);
    const auto after = nativeReady(socket.listener, true, 0);
    reportPolls("C2 original pending accept", before, first, second, after);

    // Drain only after both observations; never introduce another connection.
    ArchSocket accepted = ARCH->acceptSocket(socket.listener, nullptr);
    const auto close = synergy::finally([&] {
      if (accepted)
        ARCH->closeSocket(accepted);
    });
    QVERIFY(accepted != nullptr);
    const auto drained = readPoll(socket.listener);
    qInfo("C2 accept drained poll=%d/%u", drained.status, unsigned(drained.events));
    QCOMPARE(nativeReady(socket.listener, true, 0), 0);
    QCOMPARE(drained.status, 0);
    QCOMPARE(drained.events, static_cast<unsigned short>(0));
    QCOMPARE(before, 1);
    QCOMPARE(after, 1);
    QCOMPARE(first.status, 1);
    QCOMPARE(first.events, static_cast<unsigned short>(IArchNetwork::PollEventMask::In));
    QCOMPARE(second.status, 1);
    QCOMPARE(second.events, static_cast<unsigned short>(IArchNetwork::PollEventMask::In));
  }

  void windowsRepeatedPollRetainsOriginalByte()
  {
    SocketMultiplexer multiplexer;
    ConnectedLoopback socket(multiplexer);
    QCOMPARE(nativeReady(socket.accepted, true, 0), 0);
    const auto idle = readPoll(socket.accepted);
    QCOMPARE(idle.status, 0);
    QCOMPARE(idle.events, static_cast<unsigned short>(0));
    const char expected = 'Q';
    QCOMPARE(ARCH->writeSocket(socket.owner.peer, &expected, 1), size_t(1));
    QCOMPARE(nativeReady(socket.accepted, true, 5000), 1);
    const auto before = nativeReady(socket.accepted, true, 0);
    const auto first = readPoll(socket.accepted);
    const auto second = readPoll(socket.accepted);
    const auto after = nativeReady(socket.accepted, true, 0);
    reportPolls("C2 original pending byte", before, first, second, after);
    char received = 0;
    QCOMPARE(recv(socket.accepted->m_socket, &received, 1, 0), 1);
    QCOMPARE(received, expected);
    QCOMPARE(nativeReady(socket.accepted, true, 0), 0);
    const auto drained = readPoll(socket.accepted);
    QCOMPARE(drained.status, 0);
    QCOMPARE(drained.events, static_cast<unsigned short>(0));
    const auto empty = recv(socket.accepted->m_socket, &received, 1, 0);
    const auto emptyError = empty == SOCKET_ERROR ? WSAGetLastError() : 0;
    QCOMPARE(empty, SOCKET_ERROR);
    QCOMPARE(emptyError, WSAEWOULDBLOCK);
    QCOMPARE(before, 1);
    QCOMPARE(after, 1);
    QCOMPARE(first.status, 1);
    QCOMPARE(first.events, static_cast<unsigned short>(IArchNetwork::PollEventMask::In));
    QCOMPARE(second.status, 1);
    QCOMPARE(second.events, static_cast<unsigned short>(IArchNetwork::PollEventMask::In));
  }

  void windowsReplacementReceivesOriginalByte()
  {
    SocketMultiplexer multiplexer;
    ConnectedLoopback socket(multiplexer);
    const char expected = 'R';
    QCOMPARE(ARCH->writeSocket(socket.owner.peer, &expected, 1), size_t(1));
    QCOMPARE(nativeReady(socket.accepted, true, 5000), 1);
    auto old = std::make_shared<JobState>();
    auto replacement = std::make_shared<JobState>();
    auto entered = old->entered.get_future();
    auto oldDestroyed = old->destroyed.get_future();
    auto finished = replacement->destroyed.get_future();
    std::atomic<int> receivedCount = -2;
    std::atomic<int> receivedError = 0;
    std::atomic<char> receivedByte = 0;
    replacement->afterGate = [&] {
      char value = 0;
      const auto count = recv(socket.accepted->m_socket, &value, 1, 0);
      receivedCount = count;
      receivedError = count == SOCKET_ERROR ? WSAGetLastError() : 0;
      receivedByte = value;
    };
    const auto release = synergy::finally([&] {
      old->unblock();
      // The callback captures result variables below this socket's lifetime.
      // Unregister before those variables leave scope, including assertion exits.
      multiplexer.removeSocket(&socket.owner);
    });
    multiplexer.addSocket(&socket.owner, new GatedJob(socket.owner, old, Gate::Snapshot, {}, socket.accepted));
    QVERIFY(entered.wait_for(5s) == std::future_status::ready);
    QCOMPARE(nativeReady(socket.accepted, true, 0), 1);
    reportJobState("C4 old metadata gated with original byte", *old, *replacement);
    multiplexer.removeSocket(&socket.owner);
    multiplexer.addSocket(&socket.owner, new GatedJob(socket.owner, replacement, Gate::None, {}, socket.accepted));
    old->unblock();
    const auto outcome = finished.wait_for(5s);
    reportJobState("C4 replacement completion before teardown", *old, *replacement);
    qInfo(
        "C4 recv count=%d error=%d byte=%d select-after=%d", receivedCount.load(), receivedError.load(),
        int(receivedByte.load()), nativeReady(socket.accepted, true, 0)
    );
    QVERIFY(outcome == std::future_status::ready);
    QCOMPARE(old->calls.load(), 0);
    QCOMPARE(replacement->calls.load(), 1);
    QVERIFY(replacement->read.load());
    QVERIFY(!replacement->write.load());
    QVERIFY(!replacement->error.load());
    QCOMPARE(receivedCount.load(), 1);
    QCOMPARE(receivedError.load(), 0);
    QCOMPARE(receivedByte.load(), expected);
    QCOMPARE(nativeReady(socket.accepted, true, 0), 0);
    char extra = 0;
    const auto empty = recv(socket.accepted->m_socket, &extra, 1, 0);
    const auto emptyError = empty == SOCKET_ERROR ? WSAGetLastError() : 0;
    QCOMPARE(empty, SOCKET_ERROR);
    QCOMPARE(emptyError, WSAEWOULDBLOCK);
    QVERIFY(oldDestroyed.wait_for(5s) == std::future_status::ready);
    QCOMPARE(old->destructions.load(), 1);
    QCOMPARE(replacement->destructions.load(), 1);
    ARCH->closeSocketForWrite(socket.owner.peer);
    QCOMPARE(nativeReady(socket.accepted, true, 5000), 1);
    QCOMPARE(recv(socket.accepted->m_socket, &extra, 1, 0), 0);
  }

  void windowsReplacementWithoutReadInterestIgnoresPendingAccept()
  {
    SocketMultiplexer multiplexer;
    ReadySocket socket(multiplexer);
    ReadySocket control(multiplexer);
    QCOMPARE(nativeReady(socket.listener, true, 5000), 1);
    QCOMPARE(nativeReady(control.listener, true, 5000), 1);
    auto old = std::make_shared<JobState>();
    auto replacement = std::make_shared<JobState>();
    auto controlState = std::make_shared<JobState>();
    auto entered = old->entered.get_future();
    auto oldDestroyed = old->destroyed.get_future();
    auto finished = replacement->destroyed.get_future();
    auto controlFinished = controlState->destroyed.get_future();
    const auto release = synergy::finally([&] { old->unblock(); });
    multiplexer.addSocket(&socket, new GatedJob(socket, old, Gate::Snapshot));
    QVERIFY(entered.wait_for(5s) == std::future_status::ready);
    multiplexer.removeSocket(&socket);
    multiplexer.addSocket(&socket, new GatedJob(socket, replacement, Gate::None, {}, nullptr, false));
    // A separate current entry makes this poll return. The replacement must
    // receive false flags despite the original listener still being ready.
    multiplexer.addSocket(&control, new GatedJob(control, controlState, Gate::None));
    old->unblock();
    const auto outcome = finished.wait_for(5s);
    reportJobState("C5 no read interest before teardown", *old, *replacement);
    QVERIFY(outcome == std::future_status::ready);
    QVERIFY(controlFinished.wait_for(5s) == std::future_status::ready);
    QVERIFY(oldDestroyed.wait_for(5s) == std::future_status::ready);
    QCOMPARE(old->calls.load(), 0);
    QCOMPARE(replacement->calls.load(), 1);
    QVERIFY(!replacement->read.load());
    QVERIFY(!replacement->write.load());
    QVERIFY(!replacement->error.load());
    QCOMPARE(controlState->calls.load(), 1);
    QVERIFY(controlState->read.load());
    QCOMPARE(old->destructions.load(), 1);
    QCOMPARE(replacement->destructions.load(), 1);
    QCOMPARE(controlState->destructions.load(), 1);
    QCOMPARE(nativeReady(socket.listener, true, 0), 1);
  }

  void windowsReplacementOnDifferentNativeSocketIgnoresOldReadiness()
  {
    SocketMultiplexer multiplexer;
    ReadySocket original(multiplexer);
    ConnectedLoopback idle(multiplexer);
    ReadySocket control(multiplexer);
    QCOMPARE(nativeReady(original.listener, true, 5000), 1);
    QCOMPARE(nativeReady(idle.accepted, true, 0), 0);
    QCOMPARE(nativeReady(control.listener, true, 5000), 1);
    auto old = std::make_shared<JobState>();
    auto replacement = std::make_shared<JobState>();
    auto controlState = std::make_shared<JobState>();
    auto entered = old->entered.get_future();
    auto oldDestroyed = old->destroyed.get_future();
    auto finished = replacement->destroyed.get_future();
    auto controlFinished = controlState->destroyed.get_future();
    const auto release = synergy::finally([&] { old->unblock(); });
    multiplexer.addSocket(&original, new GatedJob(original, old, Gate::Snapshot));
    QVERIFY(entered.wait_for(5s) == std::future_status::ready);
    multiplexer.removeSocket(&original);
    multiplexer.addSocket(&original, new GatedJob(original, replacement, Gate::None, {}, idle.accepted));
    multiplexer.addSocket(&control, new GatedJob(control, controlState, Gate::None));
    old->unblock();
    const auto outcome = finished.wait_for(5s);
    reportJobState("C5 different native socket before teardown", *old, *replacement);
    QVERIFY(outcome == std::future_status::ready);
    QVERIFY(controlFinished.wait_for(5s) == std::future_status::ready);
    QVERIFY(oldDestroyed.wait_for(5s) == std::future_status::ready);
    QCOMPARE(old->calls.load(), 0);
    QCOMPARE(replacement->calls.load(), 1);
    QVERIFY(!replacement->read.load());
    QVERIFY(!replacement->write.load());
    QVERIFY(!replacement->error.load());
    QCOMPARE(controlState->calls.load(), 1);
    QVERIFY(controlState->read.load());
    QCOMPARE(old->destructions.load(), 1);
    QCOMPARE(replacement->destructions.load(), 1);
    QCOMPARE(controlState->destructions.load(), 1);
    QCOMPARE(nativeReady(original.listener, true, 0), 1);
    QCOMPARE(nativeReady(idle.accepted, true, 0), 0);
  }
#endif

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
    QVERIFY_THROWS_EXCEPTION(IOClosedException, listener.close());
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
