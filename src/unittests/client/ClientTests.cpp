/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "ClientTests.h"
#include "ProtocolTestSupport.h"

using namespace client_test;

void ClientTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
  initializeProcess();
}

// Mutation: omit either suspend/resume add/remove. Exact keys, not just count.
void ClientTests::C01_idleOwnership()
{
  Fixture f;
  QVERIFY(f.idleHandlersOnly());
  QVERIFY(f.events.state.timers.empty());
  QCOMPARE(f.factoryState->creates, 0);
  f.destroyClient();
  QVERIFY(f.events.state.handlers.empty());
  QCOMPARE(f.screenState->enabled, 0);
  QVERIFY(!f.events.dispatchEvent(Event(EventTypes::ScreenResume, f.screenTarget)));
}
void ClientTests::C02_connectSelection_data()
{
  QTest::addColumn<bool>("secure");
  QTest::newRow("plain") << false;
  QTest::newRow("synthetic-peer-auth") << true;
}
// Mutation: remove connect call or duplicate-connection guard.
void ClientTests::C02_connectSelection()
{
  QFETCH(bool, secure);
  Fixture f(secure);
  f.begin();
  QCOMPARE(f.factoryState->creates, 1);
  QCOMPARE(f.socket()->connects, 1);
  QCOMPARE(f.factoryState->levels.front(), secure ? SecurityLevel::PeerAuth : SecurityLevel::PlainText);
  QCOMPARE(f.factoryState->families.front(), IArchNetwork::AddressFamily::INet);
  QVERIFY(f.events.state.timerFor(5.0) != nullptr);
  QCOMPARE(f.events.state.timers.size(), size_t(1));
  QCOMPARE(f.screenState->enabled, 0);
  f.client->connect();
  QCOMPARE(f.factoryState->creates, 1);
  f.connected();
  QVERIFY(!f.events.state.has(EventTypes::DataSocketConnected, f.filterTarget));
  QVERIFY(!f.events.state.has(EventTypes::DataSocketSecureConnected, f.filterTarget));
  QVERIFY(!f.events.state.has(EventTypes::DataSocketConnectionFailed, f.filterTarget));
  f.send(hello());
  QCOMPARE(f.screenState->enabled, 0);
  f.send("DSOP" + options({}));
  f.client->connect();
  QCOMPARE(f.factoryState->creates, 1);
  QCOMPARE(f.screenState->enabled, 1);
  f.stop();
  f.assertReleased(true, false);
}
// Mutation: suppress connected/hello/DSOP registered callback.
void ClientTests::C03_registeredHandshake()
{
  Fixture f;
  f.begin();
  f.connected();
  QVERIFY(f.client->isConnecting());
  QVERIFY(!f.client->isConnected());
  f.send(hello());
  const auto expected = packet(hello() + text("synthetic-client"));
  QCOMPARE(f.socket()->output, expected);
  QVERIFY(!f.client->isConnecting());
  QVERIFY(f.client->isConnected());
  QCOMPARE(f.screenState->enabled, 0);
  f.send("QINF");
  const auto info = std::string("DINF") + integer(-7, 2) + integer(9, 2) + integer(800, 2) + integer(600, 2) +
                    integer(0, 2) + integer(31, 2) + integer(43, 2);
  QCOMPARE(f.socket()->output, expected + packet(info));
  f.send("CIAK");
  f.send("DSOP" + options({}));
  QCOMPARE(f.screenState->enabled, 1);
  QCOMPARE(f.events.state.count(EventTypes::ClientConnected), size_t(1));
  QCOMPARE(
      std::count(
          f.events.state.observedKeys.begin(), f.events.state.observedKeys.end(),
          HandlerKey{EventTypes::ClientConnected, f.screenTarget}
      ),
      1
  );
  f.send("DMMV" + integer(12, 2) + integer(34, 2));
  QVERIFY((f.screenState->calls.back() == Call{"move", 12, 34}));
  QCOMPARE(f.socket()->output, expected + packet(info) + packet("CNOP"));
  f.stop();
  f.assertReleased(true, false);
}
void ClientTests::C04_failureOwnership_data()
{
  QTest::addColumn<int>("kind");
  QTest::newRow("factory-exception") << 0;
  QTest::newRow("connect-exception") << 1;
  QTest::newRow("typed-socket-failure") << 2;
}
void ClientTests::C04_failureOwnership()
{
  QFETCH(int, kind);
  Fixture f;
  f.factoryState->throwCreate = kind == 0;
  f.factoryState->throwConnect = kind == 1;
  f.begin();
  if (kind == 2) {
    QVERIFY(f.events.state.has(EventTypes::DataSocketConnectionFailed, f.filterTarget));
    // Production consumes this new non-POD info; synchronous borrowed Event
    // and fixture never free/delete it a second time.
    auto info = std::make_unique<IDataSocket::ConnectionFailedInfo>("synthetic socket failure");
    Event e(EventTypes::DataSocketConnectionFailed, f.filterTarget, info.get(), Event::EventFlags::DontFreeData);
    info.release();
    QVERIFY(f.events.dispatchEvent(e));
  }
  f.drain();
  QCOMPARE(f.events.state.failures.size(), size_t(1));
  const auto expected = kind == 0   ? "synthetic factory failure"
                        : kind == 1 ? "synthetic connect failure"
                                    : "synthetic socket failure";
  QCOMPARE(f.events.state.failures.front().what, QString::fromLatin1(expected));
  QVERIFY(f.events.state.failures.front().retry);
  QCOMPARE(f.events.state.failures.front().target, f.screenTarget);
  QCOMPARE(f.events.state.failureDisposals, 1);
  QCOMPARE(f.screenState->enabled, 0);
  QVERIFY(f.events.state.timers.empty());
  QVERIFY(f.idleHandlersOnly());
  if (kind != 0)
    QCOMPARE(f.socket()->destroyed, 1);
}
void ClientTests::C05_timeout_data()
{
  QTest::addColumn<bool>("transportConnected");
  QTest::newRow("connecting") << false;
  QTest::newRow("waiting-hello") << true;
}
void ClientTests::C05_timeout()
{
  QFETCH(bool, transportConnected);
  Fixture f;
  f.begin();
  if (transportConnected)
    f.connected();
  auto *timer = f.events.state.timerFor(5.0);
  QVERIFY(timer != nullptr);
  QVERIFY(f.events.dispatchEvent(Event(EventTypes::Timer, timer)));
  f.drain();
  f.assertReleased(false);
  QVERIFY(!QTest::currentTestFailed());
  QCOMPARE(f.events.state.failures.front().what, QStringLiteral("Timed out"));
  f.ready();
  QCOMPARE(f.factoryState->creates, 2);
  QCOMPARE(f.screenState->enabled, 1);
}
void ClientTests::C06_refusal_data()
{
  QTest::addColumn<QByteArray>("message");
  QTest::addColumn<QString>("reason");
  QTest::newRow("busy") << QByteArray("EBSY") << QStringLiteral("server already has a connected client with our name");
  QTest::newRow("complete-incompatible") << QByteArray::fromStdString("EICV" + integer(1, 2) + integer(99, 2))
                                         << QStringLiteral("server has incompatible version");
}
void ClientTests::C06_refusal()
{
  QFETCH(QByteArray, message);
  QFETCH(QString, reason);
  Fixture f;
  f.awaitingHandshake();
  const auto before = f.socket()->output;
  f.send(message.toStdString());
  f.assertReleased(false);
  QVERIFY(!QTest::currentTestFailed());
  QCOMPARE(f.events.state.failures.front().type, EventTypes::ClientConnectionRefused);
  QCOMPARE(f.events.state.failures.front().what, reason);
  QCOMPARE(f.socket()->output, before);
  QCOMPARE(f.events.state.count(EventTypes::ClientConnected), size_t(0));
  QCOMPARE(f.factoryState->creates, 1);
}
void ClientTests::C07_disconnectEvents_data()
{
  QTest::addColumn<int>("kind");
  const char *names[] = {"socket-disconnected", "input-shutdown", "output-shutdown", "output-error", "server-close"};
  for (int i = 0; i < 5; ++i)
    QTest::newRow(names[i]) << i;
}
void ClientTests::C07_disconnectEvents()
{
  QFETCH(int, kind);
  Fixture f;
  f.ready();
  const auto before = f.socket()->output;
  const EventTypes types[] = {
      EventTypes::SocketDisconnected, EventTypes::StreamInputShutdown, EventTypes::StreamOutputShutdown,
      EventTypes::StreamOutputError
  };
  if (kind == 4)
    f.send("CBYE");
  else
    QVERIFY(f.events.dispatchEvent(Event(types[kind], f.filterTarget)));
  f.drain();
  f.assertReleased(true, false);
  QCOMPARE(f.events.state.count(EventTypes::ClientDisconnected), size_t(1));
  QCOMPARE(
      std::count(
          f.events.state.observedKeys.begin(), f.events.state.observedKeys.end(),
          HandlerKey{EventTypes::ClientDisconnected, f.screenTarget}
      ),
      1
  );
  QCOMPARE(f.socket()->output, before);
  QVERIFY(f.events.state.failures.empty());
}
void ClientTests::C08_cleanupStages_data()
{
  QTest::addColumn<int>("stage");
  QTest::newRow("connecting") << 0;
  QTest::newRow("waiting-hello") << 1;
  QTest::newRow("proxy-before-ready") << 2;
  QTest::newRow("ready") << 3;
  QTest::newRow("ready-disconnect-twice") << 4;
}
void ClientTests::C08_cleanupStages()
{
  QFETCH(int, stage);
  Fixture f;
  f.begin();
  if (stage >= 1)
    f.connected();
  if (stage >= 2)
    f.send(hello());
  if (stage >= 3)
    f.send("DSOP" + options({}));
  const auto target = f.filterTarget;
  if (stage == 4) {
    f.stop();
    f.client->disconnect(nullptr);
    f.drain();
    f.assertReleased(true, false);
    QCOMPARE(f.events.state.count(EventTypes::ClientDisconnected), size_t(2));
  }
  f.destroyClient();
  QVERIFY(f.events.state.handlers.empty());
  QVERIFY(f.events.state.timers.empty());
  QCOMPARE(f.socket()->destroyed, 1);
  QCOMPARE(f.screenState->disabled, stage >= 3 ? 1 : 0);
  QCOMPARE(f.events.state.timerCreates, f.events.state.timerDeletes);
  QCOMPARE(f.events.state.duplicateTimerDeletes, 0);
  QVERIFY(!f.events.dispatchEvent(Event(EventTypes::StreamInputReady, target)));
}
void ClientTests::C09_suspendResume()
{
  {
    Fixture f;
    f.ready();
    QVERIFY(f.events.dispatchEvent(Event(EventTypes::ScreenSuspend, f.screenTarget)));
    f.drain();
    QCOMPARE(f.socket()->destroyed, 1);
    QCOMPARE(f.screenState->disabled, 1);
    QVERIFY(f.events.dispatchEvent(Event(EventTypes::ScreenResume, f.screenTarget)));
    QCOMPARE(f.factoryState->creates, 2);
    f.connected();
    f.send(hello());
    f.send("DSOP" + options({}));
    QCOMPARE(f.screenState->enabled, 2);
    QVERIFY(f.events.dispatchEvent(Event(EventTypes::ScreenResume, f.screenTarget)));
    QCOMPARE(f.factoryState->creates, 2);
  }
  {
    Fixture f;
    QVERIFY(f.events.dispatchEvent(Event(EventTypes::ScreenSuspend, f.screenTarget)));
    f.client->connect();
    QCOMPARE(f.factoryState->creates, 0);
    QVERIFY(f.events.dispatchEvent(Event(EventTypes::ScreenResume, f.screenTarget)));
    QCOMPARE(f.factoryState->creates, 1);
    f.connected();
    f.send(hello());
    f.send("DSOP" + options({}));
    QCOMPARE(f.screenState->enabled, 1);
  }
}
void ClientTests::C10_ownedHandlers_data()
{
  QTest::addColumn<bool>("secure");
  QTest::addColumn<bool>("drop");
  QTest::newRow("plain-live") << false << false;
  QTest::newRow("plain-pending-drop") << false << true;
  QTest::newRow("secure-live") << true << false;
  QTest::newRow("secure-pending-drop") << true << true;
}
void ClientTests::C10_ownedHandlers()
{
  QFETCH(bool, secure);
  QFETCH(bool, drop);
  Fixture f(secure);
  f.ready();
  QVERIFY(!f.events.state.has(EventTypes::DataSocketSecureConnected, f.filterTarget));
  void *proxyTarget = nullptr;
  for (const auto &[key, h] : f.events.state.handlers) {
    Q_UNUSED(h);
    if (key.first == EventTypes::ClipboardSending)
      proxyTarget = key.second;
  }
  QVERIFY(proxyTarget != nullptr);
  IScreen::ClipboardInfo grabbed{0, 7};
  QVERIFY(f.events.dispatchEvent(
      Event(EventTypes::ClipboardGrabbed, f.screenTarget, &grabbed, Event::EventFlags::DontFreeData)
  ));
  QCOMPARE(f.events.state.chunks.size(), size_t(3));
  const auto before = f.socket()->output;
  if (drop) {
    f.retireOwners();
    f.client->disconnect(nullptr);
    // Must fail before any potential dangling callback is invoked.
    QVERIFY(!f.events.state.has(EventTypes::ClipboardSending, proxyTarget));
    QVERIFY(!f.events.state.has(EventTypes::Unknown, proxyTarget));
  }
  f.drain();
  for (const auto &r : f.events.state.chunks) {
    QCOMPARE(r.disposed, 1);
    QCOMPARE(r.dispatched, drop ? 0 : 1);
    QCOMPARE(r.dropped, drop ? 1 : 0);
  }
  QCOMPARE(f.events.state.unsafeDrops, 0);
  if (drop) {
    QCOMPARE(f.socket()->output, before);
    QVERIFY(f.idleHandlersOnly());
    QVERIFY(!f.events.dispatchEvent(Event(EventTypes::ClipboardSending, proxyTarget)));
  } else {
    const auto data = syntheticClipboard();
    const auto expected = packet("DCLP" + clipboardBody(0, 0, 1, "15")) +
                          packet("DCLP" + clipboardBody(0, 0, 2, data)) + packet("DCLP" + clipboardBody(0, 0, 3, ""));
    QCOMPARE(f.socket()->output, before + expected);
    f.stop();
    QVERIFY(f.idleHandlersOnly());
  }
  // Separate fixture disposal control: this subclass is NOT substituted for
  // the three production StreamChunker payloads above. It verifies the typed
  // Event::deleteData path actually invokes a virtual chunk destructor once.
  class TrackedChunk final : public ClipboardChunk
  {
  public:
    explicit TrackedChunk(int &count) : ClipboardChunk(7), destroyed(count)
    {
    }
    ~TrackedChunk() override
    {
      ++destroyed;
    }
    int &destroyed;
  };
  int destroyed = 0;
  int unregisteredTarget = 0;
  f.events.addEvent(Event(EventTypes::ClipboardSending, &unregisteredTarget, new TrackedChunk(destroyed)));
  f.drain();
  QCOMPARE(destroyed, 1);
  QCOMPARE(f.events.state.chunks.back().disposed, 1);
  QCOMPARE(f.events.state.chunks.back().dropped, 1);
}
void ClientTests::C11_realQueueReentry_data()
{
  QTest::addColumn<bool>("handshake");
  QTest::newRow("actual-map-normal-DMMV") << false;
  QTest::newRow("actual-map-handshake-DSOP") << true;
}
void ClientTests::C11_realQueueReentry()
{
  QFETCH(bool, handshake);
  Fixture<RealDispatchQueue> f;
  if (handshake)
    f.awaitingHandshake();
  else
    f.ready();
  const auto target = f.filterTarget;
  const auto before = f.socket()->output;
  const auto inputs = f.inputCalls();
  f.send(handshake ? "DSOP" + integer(2, 4) + integer(kOptionHeartbeat, 4) : "DMMV" + integer(12, 2));
  f.assertReleased(!handshake);
  QVERIFY(!QTest::currentTestFailed());
  QCOMPARE(f.inputCalls(), inputs);
  QCOMPARE(f.socket()->output, before);
  QCOMPARE(f.events.state.failures.front().what, QStringLiteral("truncated message"));
  QVERIFY(!f.events.state.has(EventTypes::StreamInputReady, target));
  QVERIFY(!f.events.dispatchEvent(Event(EventTypes::StreamInputReady, target)));
  QCOMPARE(f.events.state.unsafeDrops, 0);
}
void ClientTests::N01_unknownBatchRecovery()
{
  Fixture f;
  f.ready();
  const auto inputs = f.inputCalls();
  f.send("ZZZZdiscard");
  QVERIFY(f.client->isConnected());
  QVERIFY(f.events.state.failures.empty());
  QCOMPARE(f.inputCalls(), inputs);
  QVERIFY(f.socket()->input.empty());
  // Valid next frame is withheld until unknown batch has actually drained.
  f.send("DMMV" + integer(12, 2) + integer(34, 2));
  QCOMPARE(f.inputCalls(), inputs + 1);
}
void ClientTests::N02_shortCodes_data()
{
  QTest::addColumn<int>("length");
  QTest::newRow("one-byte") << 1;
  QTest::newRow("two-bytes") << 2;
  QTest::newRow("three-bytes") << 3;
}
void ClientTests::N02_shortCodes()
{
  QFETCH(int, length);
  Fixture f;
  f.ready();
  const auto before = f.socket()->output;
  f.send(std::string("DMMV").substr(0, length));
  f.assertReleased(true);
  QVERIFY(!QTest::currentTestFailed());
  QCOMPARE(f.events.state.failures.front().what, QStringLiteral("incomplete message from server"));
  QCOMPARE(f.socket()->output, before);
  QCOMPARE(f.inputCalls(), size_t(0));
}
void ClientTests::N03_helloBoundaries_data()
{
  QTest::addColumn<int>("kind");
  for (int n = 1; n < 11; ++n)
    QTest::newRow(qPrintable(QString("complete-short-%1").arg(n))) << n;
  QTest::newRow("zero-payload-times-out") << 0;
  QTest::newRow("valid-length-prefix-fragment") << -1;
  QTest::newRow("valid-version-fragment") << -2;
}
void ClientTests::N03_helloBoundaries()
{
  QFETCH(int, kind);
  Fixture f;
  f.begin();
  f.connected();
  if (kind > 0) {
    f.send(hello().substr(0, kind));
    f.assertReleased(false);
    QVERIFY(!QTest::currentTestFailed());
    QCOMPARE(f.events.state.failures.front().what, QStringLiteral("got invalid hello message from server"));
    QVERIFY(f.socket()->output.empty());
    QCOMPARE(f.screenState->enabled, 0);
  } else if (kind == 0) {
    f.raw(packet(""));
    QVERIFY(f.events.state.failures.empty());
    QVERIFY(!f.client->isConnected());
    auto *timer = f.events.state.timerFor(5.0);
    QVERIFY(timer != nullptr);
    QVERIFY(f.events.dispatchEvent(Event(EventTypes::Timer, timer)));
    f.drain();
    f.assertReleased(false);
    QVERIFY(!QTest::currentTestFailed());
  } else {
    const auto frame = packet(hello());
    const size_t split = kind == -1 ? 2 : 13;
    f.raw(frame.substr(0, split));
    QVERIFY(f.events.state.failures.empty());
    QVERIFY(f.socket()->output.empty());
    QVERIFY(!f.client->isConnected());
    f.raw(frame.substr(split));
    QCOMPARE(f.socket()->output, packet(hello() + text("synthetic-client")));
    f.send("DSOP" + options({}));
    QCOMPARE(f.screenState->enabled, 1);
  }
}
static void bodyData(const std::vector<BodyRow> &rows)
{
  QTest::addColumn<QByteArray>("code");
  QTest::addColumn<QByteArray>("body");
  QTest::addColumn<QByteArray>("prelude");
  QTest::addColumn<bool>("complete");
  for (size_t row = 0; row < rows.size(); ++row) {
    const auto &r = rows[row];
    for (size_t n = 0; n <= r.body.size(); ++n) {
      const auto name =
          QString::fromStdString(r.code) + QString("-stage%1-body%2-of%3").arg(row).arg(n).arg(r.body.size());
      QTest::newRow(qPrintable(name)) << QByteArray::fromStdString(r.code)
                                      << QByteArray::fromStdString(r.body.substr(0, n))
                                      << QByteArray::fromStdString(r.prelude) << (n == r.body.size());
    }
  }
}
void ClientTests::N04_normalBodies_data()
{
  bodyData(normalBodies());
}
void ClientTests::N04_normalBodies()
{
  QFETCH(QByteArray, code);
  QFETCH(QByteArray, body);
  QFETCH(QByteArray, prelude);
  QFETCH(bool, complete);
  Fixture f;
  f.ready();
  if (!prelude.isEmpty())
    f.send(prelude.toStdString());
  const auto before = f.socket()->output;
  const auto inputs = f.inputCalls();
  const auto clipboardClears = f.screenState->clipboardClears;
  const auto optionCalls =
      std::count_if(f.screenState->calls.begin(), f.screenState->calls.end(), [](const Call &call) {
        return call.name == "options";
      });
  f.send(code.toStdString() + body.toStdString());
  if (complete) {
    QVERIFY(f.events.state.failures.empty());
    QVERIFY(f.client->isConnected());
    if (code == "CCLP")
      QCOMPARE(f.screenState->clipboardClears, clipboardClears + 1);
    if (code == "DSOP")
      QCOMPARE(
          std::count_if(
              f.screenState->calls.begin(), f.screenState->calls.end(),
              [](const Call &call) { return call.name == "options"; }
          ),
          optionCalls + 1
      );
    const auto count = f.inputCalls();
    f.send("DMMV" + integer(12, 2) + integer(34, 2));
    QCOMPARE(f.inputCalls(), count + 1);
    if (code == "DCLP" && !prelude.isEmpty() && static_cast<uint8_t>(body.at(5)) == 3) {
      QCOMPARE(f.screenState->clipboardSets, 1);
      QCOMPARE(f.screenState->clipboardText, std::string("abc"));
    }
  } else {
    f.assertReleased(true);
    QVERIFY(!QTest::currentTestFailed());
    QCOMPARE(f.inputCalls(), inputs);
    QCOMPARE(f.socket()->output, before);
    QCOMPARE(f.screenState->clipboardSets, 0);
    // grabClipboard clears the platform clipboard without entering inputCalls;
    // setOptions has its own observable boundary as well. Neither may happen
    // partially before a malformed message causes terminal disconnect.
    QCOMPARE(f.screenState->clipboardClears, clipboardClears);
    QCOMPARE(
        std::count_if(
            f.screenState->calls.begin(), f.screenState->calls.end(),
            [](const Call &call) { return call.name == "options"; }
        ),
        optionCalls
    );
    QCOMPARE(
        f.events.state.failures.front().what,
        code == "DCLP" ? QStringLiteral("invalid clipboard data from server") : QStringLiteral("truncated message")
    );
  }
}
void ClientTests::N05_handshakeBodies_data()
{
  bodyData(handshakeBodies());
}
void ClientTests::N05_handshakeBodies()
{
  QFETCH(QByteArray, code);
  QFETCH(QByteArray, body);
  QFETCH(QByteArray, prelude);
  QFETCH(bool, complete);
  Q_UNUSED(prelude);
  Fixture f;
  f.awaitingHandshake();
  const auto before = f.socket()->output;
  const auto optionCalls =
      std::count_if(f.screenState->calls.begin(), f.screenState->calls.end(), [](const Call &call) {
        return call.name == "options";
      });
  f.send(code.toStdString() + body.toStdString());
  if (complete && code == "DSOP") {
    QVERIFY(f.events.state.failures.empty());
    QCOMPARE(f.screenState->enabled, 1);
    QCOMPARE(f.events.state.count(EventTypes::ClientConnected), size_t(1));
    QCOMPARE(
        std::count_if(
            f.screenState->calls.begin(), f.screenState->calls.end(),
            [](const Call &call) { return call.name == "options"; }
        ),
        optionCalls + 1
    );
  } else if (complete && code == "LSYN") {
    QVERIFY(f.events.state.failures.empty());
    QCOMPARE(f.screenState->enabled, 0);
    f.send("DSOP" + options({}));
    QCOMPARE(f.screenState->enabled, 1);
  } else {
    f.assertReleased(false);
    QVERIFY(!QTest::currentTestFailed());
    QCOMPARE(f.socket()->output, before);
    QCOMPARE(f.events.state.count(EventTypes::ClientConnected), size_t(0));
    QCOMPARE(
        std::count_if(
            f.screenState->calls.begin(), f.screenState->calls.end(),
            [](const Call &call) { return call.name == "options"; }
        ),
        optionCalls
    );
    QCOMPARE(
        f.events.state.failures.front().type,
        complete ? EventTypes::ClientConnectionRefused : EventTypes::ClientConnectionFailed
    );
  }
}
void ClientTests::N06_recoveryStress()
{
  Fixture f;
  // Recover the SAME owning Client after a representative of every malformed
  // decoder family, including each DCLP assembly stage and handshake reader.
  auto recoverBody = [&](const BodyRow &row, bool handshake) {
    const auto failures = f.events.state.failures.size();
    if (handshake)
      f.awaitingHandshake();
    else
      f.ready();
    if (!row.prelude.empty())
      f.send(row.prelude);
    f.send(row.code + row.body.substr(0, row.body.size() - 1));
    if (!f.idleHandlersOnly() || !f.events.state.timers.empty() || f.events.state.failures.size() != failures + 1)
      return false;
    f.ready();
    const auto inputs = f.inputCalls();
    f.send("DMMV" + integer(12, 2) + integer(34, 2));
    const bool forwarded = f.inputCalls() == inputs + 1;
    f.stop();
    return forwarded && f.idleHandlersOnly() && f.events.state.timers.empty() &&
           f.events.state.timerCreates == f.events.state.timerDeletes;
  };
  for (const auto &row : normalBodies())
    QVERIFY(recoverBody(row, false));
  for (const auto &row : handshakeBodies())
    QVERIFY(recoverBody(row, true));
  for (int cycle = 0; cycle < 32; ++cycle) {
    const auto failureStart = f.events.state.failures.size();
    const auto connectedStart = f.events.state.count(EventTypes::ClientConnected);
    const auto enabledStart = f.screenState->enabled;
    if (cycle % 4 == 0) {
      f.ready();
      f.stop();
    } else if (cycle % 4 == 1) {
      f.begin();
      auto *t = f.events.state.timerFor(5.0);
      QVERIFY(t != nullptr);
      QVERIFY(f.events.dispatchEvent(Event(EventTypes::Timer, t)));
      f.drain();
    } else if (cycle % 4 == 2) {
      f.ready();
      f.send("DMMV" + integer(12, 2));
    } else {
      f.awaitingHandshake();
      f.send("EBSY");
    }
    QVERIFY(f.idleHandlersOnly());
    QVERIFY(f.events.state.timers.empty());
    QVERIFY(f.events.state.pending.empty());
    QCOMPARE(f.events.state.timerCreates, f.events.state.timerDeletes);
    QCOMPARE(f.events.state.failureDisposals, static_cast<int>(f.events.state.failures.size()));
    QCOMPARE(f.events.state.failures.size(), failureStart + (cycle % 4 == 0 ? 0 : 1));
    QCOMPARE(f.events.state.unsafeDrops, 0);
    // A fresh public connect after EVERY outcome must complete and forward input.
    f.ready();
    const auto inputs = f.inputCalls();
    f.send("DMMV" + integer(12, 2) + integer(34, 2));
    QCOMPARE(f.inputCalls(), inputs + 1);
    f.stop();
    QCOMPARE(f.screenState->enabled, enabledStart + (cycle % 4 == 0 || cycle % 4 == 2 ? 2 : 1));
    QCOMPARE(
        f.events.state.count(EventTypes::ClientConnected), connectedStart + (cycle % 4 == 0 || cycle % 4 == 2 ? 2 : 1)
    );
    QVERIFY(f.idleHandlersOnly());
    QVERIFY(f.events.state.timers.empty());
    for (const auto &s : f.factoryState->sockets)
      QCOMPARE(s->destroyed, 1);
    QCOMPARE(f.events.state.timerCreates, f.events.state.timerDeletes);
  }
}
QTEST_MAIN(ClientTests)
