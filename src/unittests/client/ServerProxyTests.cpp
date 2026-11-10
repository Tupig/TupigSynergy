/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerProxyTests.h"
#include "ProtocolTestSupport.h"

#include "../synergy/MockEventQueue.h"
#include "../synergy/TestStreams.h"

#include "ProtocolTypes.h"
#include "client/IServerProxyClient.h"
#include "client/ServerProxy.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

//! Records what the proxy forwarded, so tests can assert the client contract.
class FakeServerProxyClient : public IServerProxyClient
{
public:
  int disconnectCalls = 0;
  int enterCalls = 0;
  int leaveCalls = 0;
  int mouseMoveCalls = 0;
  int32_t lastX = 0;
  int32_t lastY = 0;

  void disconnect(const char *) override
  {
    ++disconnectCalls;
  }

  void refuseConnection(synergy::core::ConnectionRefusal, const char *) override
  {
  }

  void handshakeComplete() override
  {
  }

  void offerReceivedFiles(const std::vector<std::string> &) override
  {
  }

  size_t getMaximumClipboardReceiveSizeBytes() const override
  {
    return 1024 * 1024;
  }

  void getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const override
  {
    x = 0;
    y = 0;
    width = 1920;
    height = 1080;
  }

  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = 0;
    y = 0;
  }

  void enter(int32_t, int32_t, uint32_t, KeyModifierMask, bool) override
  {
    ++enterCalls;
  }

  bool leave() override
  {
    ++leaveCalls;
    return true;
  }

  void setClipboard(ClipboardID, const IClipboard *) override
  {
  }

  void grabClipboard(ClipboardID) override
  {
  }

  void keyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
  }

  void keyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
  }

  void keyUp(KeyID, KeyModifierMask, KeyButton) override
  {
  }

  void mouseDown(ButtonID) override
  {
  }

  void mouseUp(ButtonID) override
  {
  }

  void mouseMove(int32_t xAbs, int32_t yAbs) override
  {
    ++mouseMoveCalls;
    lastX = xAbs;
    lastY = yAbs;
  }

  void mouseRelativeMove(int32_t, int32_t) override
  {
  }

  void mouseWheel(int32_t, int32_t) override
  {
  }

  void screensaver(bool) override
  {
  }

  void resetOptions() override
  {
  }

  void setOptions(const OptionsList &) override
  {
  }

  std::string getName() const override
  {
    return "fake-client";
  }
};

//! Exposes the protected parser and its result type for direct testing.
class TestableServerProxy : public ServerProxy
{
public:
  using ServerProxy::ServerProxy;
  using ConnectionResult = ServerProxy::ConnectionResult;
  using ServerProxy::parseMessage;
};

void copyCode(const char *code, uint8_t out[4])
{
  std::memcpy(out, code, 4);
}

} // namespace

void ServerProxyTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
  client_test::initializeProcess();
}

void ServerProxyTests::unknownMessageReturnsUnknown()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});

  const uint8_t code[4] = {'Z', 'Z', 'Z', 'Z'};
  QCOMPARE(proxy.parseMessage(code), TestableServerProxy::ConnectionResult::Unknown);
}

void ServerProxyTests::serverNoopReturnsOkay()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});

  uint8_t code[4];
  copyCode(kMsgCNoop, code);
  QCOMPARE(proxy.parseMessage(code), TestableServerProxy::ConnectionResult::Okay);
  QCOMPARE(client.disconnectCalls, 0);
}

void ServerProxyTests::serverCloseDisconnects()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});

  uint8_t code[4];
  copyCode(kMsgCClose, code);
  QCOMPARE(proxy.parseMessage(code), TestableServerProxy::ConnectionResult::Disconnect);
  QCOMPARE(client.disconnectCalls, 1);
}

void ServerProxyTests::keepAliveIsEchoed()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream(MemoryStream::WriteMode::Loopback);
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});

  uint8_t code[4];
  copyCode(kMsgCKeepAlive, code);
  QCOMPARE(proxy.parseMessage(code), TestableServerProxy::ConnectionResult::Okay);

  // the echo the proxy wrote is visible through the loopback stream
  char buffer[8] = {};
  const uint32_t n = stream.read(buffer, sizeof(buffer));
  QCOMPARE(n, 4u);
  QCOMPARE(std::string(buffer, n), std::string(kMsgCKeepAlive));
}

void ServerProxyTests::mouseMoveForwardsToClient()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});

  // DMMV payload: x=12, y=34 as two 2-byte big-endian ints
  stream.push(std::string("\x00\x0c\x00\x22", 4));

  uint8_t code[4];
  copyCode(kMsgDMouseMove, code);
  QCOMPARE(proxy.parseMessage(code), TestableServerProxy::ConnectionResult::Okay);
  QCOMPARE(client.mouseMoveCalls, 1);
  QCOMPARE(client.lastX, 12);
  QCOMPARE(client.lastY, 34);
}

void ServerProxyTests::enterAndLeaveForwardToClient()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});

  // CINN payload: x=100, y=200, seqNum=7, mask=0
  stream.push(std::string("\x00\x64\x00\xc8", 4) + std::string("\x00\x00\x00\x07", 4) + std::string("\x00\x00", 2));

  uint8_t code[4];
  copyCode(kMsgCEnter, code);
  QCOMPARE(proxy.parseMessage(code), TestableServerProxy::ConnectionResult::Okay);
  QCOMPARE(client.enterCalls, 1);

  // COUT has no payload
  copyCode(kMsgCLeave, code);
  QCOMPARE(proxy.parseMessage(code), TestableServerProxy::ConnectionResult::Okay);
  QCOMPARE(client.leaveCalls, 1);
}

namespace {
using namespace client_test;
class DispatchClient final : public FakeServerProxyClient
{
public:
  std::vector<Call> calls;
  OptionsList lastOptions;
  int resets = 0, handshakes = 0, clipboardSets = 0, offers = 0;
  std::string clipboardText;
  void handshakeComplete() override
  {
    ++handshakes;
  }
  size_t getMaximumClipboardReceiveSizeBytes() const override
  {
    return 128;
  }
  void getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const override
  {
    x = -7;
    y = 9;
    w = 800;
    h = 600;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = 31;
    y = 43;
  }
  void enter(int32_t x, int32_t y, uint32_t seq, KeyModifierMask mask, bool screenSaver) override
  {
    calls.push_back({"enter", x, y, seq, mask, screenSaver ? "true" : "false"});
  }
  bool leave() override
  {
    calls.push_back({"leave"});
    return true;
  }
  void keyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang) override
  {
    calls.push_back({"keyDown", id, mask, button, 0, lang});
  }
  void keyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang) override
  {
    calls.push_back({"keyRepeat", id, mask, count, button, lang});
  }
  void keyUp(KeyID id, KeyModifierMask mask, KeyButton button) override
  {
    calls.push_back({"keyUp", id, mask, button});
  }
  void mouseDown(ButtonID id) override
  {
    calls.push_back({"button", id, 1});
  }
  void mouseUp(ButtonID id) override
  {
    calls.push_back({"button", id, 0});
  }
  void mouseMove(int32_t x, int32_t y) override
  {
    calls.push_back({"move", x, y});
  }
  void mouseRelativeMove(int32_t x, int32_t y) override
  {
    calls.push_back({"relative", x, y});
  }
  void mouseWheel(int32_t x, int32_t y) override
  {
    calls.push_back({"wheel", x, y});
  }
  void screensaver(bool on) override
  {
    calls.push_back({"screensaver", on});
  }
  void resetOptions() override
  {
    ++resets;
  }
  void setOptions(const OptionsList &values) override
  {
    lastOptions = values;
    calls.push_back({"options", static_cast<int64_t>(values.size())});
  }
  void grabClipboard(ClipboardID id) override
  {
    calls.push_back({"grabClipboard", id});
  }
  void setClipboard(ClipboardID id, const IClipboard *clipboard) override
  {
    ++clipboardSets;
    clipboard->open(0);
    clipboardText = clipboard->get(IClipboard::Format::Text);
    clipboard->close();
    calls.push_back({"clipboard", id});
  }
  void offerReceivedFiles(const std::vector<std::string> &) override
  {
    ++offers;
  }
};
class CapturedMemoryStream final : public MemoryStream
{
public:
  std::string output;
  void write(const void *bytes, uint32_t size) override
  {
    output.append(static_cast<const char *>(bytes), size);
  }
};
class DispatchProxy final : public ServerProxy
{
public:
  using ConnectionResult = ServerProxy::ConnectionResult;
  using ServerProxy::parseHandshakeMessage;
  using ServerProxy::parseMessage;
  using ServerProxy::ServerProxy;
};
struct DispatchFixture
{
  RecordingQueue events;
  DispatchClient client;
  CapturedMemoryStream stream;
  std::unique_ptr<DispatchProxy> proxy;
  explicit DispatchFixture(ServerProxyConfig config = {})
  {
    proxy = std::make_unique<DispatchProxy>(&client, &stream, &events, config);
    stream.push(options({}));
    const uint8_t code[] = {'D', 'S', 'O', 'P'};
    if (proxy->parseHandshakeMessage(code) != DispatchProxy::ConnectionResult::Okay)
      throw std::logic_error("cannot establish synthetic direct proxy");
    client.calls.clear();
  }
  ~DispatchFixture()
  {
    events.state.retire(proxy.get());
    proxy.reset();
    events.state.drain(events);
  }
  DispatchProxy::ConnectionResult message(const std::string &payload)
  {
    // A bodyless message must not enqueue a zero-byte MemoryStream segment:
    // the next decoder would otherwise observe a synthetic EOF before its body.
    if (payload.size() > 4)
      stream.push(payload.substr(4));
    uint8_t code[4];
    std::memcpy(code, payload.data(), 4);
    return proxy->parseMessage(code);
  }
  void batch(const std::string &payload)
  {
    stream.push(payload);
    events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget()));
    events.state.drain(events);
  }
};
} // namespace

// P01 mutation: omit forwarding, exchange repeat count/button, bypass sanitize.
void ServerProxyTests::P01_keyDispatch_data()
{
  QTest::addColumn<QByteArray>("message");
  QTest::addColumn<QString>("name");
  QTest::addColumn<int>("a");
  QTest::addColumn<int>("b");
  QTest::addColumn<int>("c");
  QTest::addColumn<int>("d");
  QTest::addColumn<QString>("lang");
  const auto scalar = integer(65, 2) + integer(0x8002, 2);
  QTest::newRow("DKDN-sanitize") << QByteArray::fromStdString("DKDN" + scalar + integer(7, 2)) << QString("keyDown")
                                 << 65 << 2 << 7 << 0 << QString("");
  QTest::newRow("DKDL-empty") << QByteArray::fromStdString("DKDL" + scalar + integer(7, 2) + text(""))
                              << QString("keyDown") << 65 << 2 << 7 << 0 << QString("");
  QTest::newRow("DKDL-en") << QByteArray::fromStdString("DKDL" + scalar + integer(7, 2) + text("en"))
                           << QString("keyDown") << 65 << 2 << 7 << 0 << QString("en");
  QTest::newRow("DKRP-count-button-language")
      << QByteArray::fromStdString("DKRP" + scalar + integer(3, 2) + integer(7, 2) + text("en")) << QString("keyRepeat")
      << 65 << 2 << 3 << 7 << QString("en");
  QTest::newRow("DKUP-sanitize") << QByteArray::fromStdString("DKUP" + scalar + integer(7, 2)) << QString("keyUp") << 65
                                 << 2 << 7 << 0 << QString("");
}
void ServerProxyTests::P01_keyDispatch()
{
  QFETCH(QByteArray, message);
  QFETCH(QString, name);
  QFETCH(int, a);
  QFETCH(int, b);
  QFETCH(int, c);
  QFETCH(int, d);
  QFETCH(QString, lang);
  DispatchFixture f;
  QCOMPARE(f.message(message.toStdString()), DispatchProxy::ConnectionResult::Okay);
  QCOMPARE(f.client.calls.size(), size_t(1));
  QVERIFY((f.client.calls.front() == Call{name.toStdString(), a, b, c, d, lang.toStdString()}));
  // Representative real Client -> real Screen route. Platform release is
  // intentionally button-only; direct decoder above owns full id/mask oracle.
  Fixture owner;
  owner.ready();
  owner.send(message.toStdString());
  QCOMPARE(owner.inputCalls(), size_t(1));
  const auto &actual = owner.screenState->calls.back();
  QVERIFY((actual == (name == "keyUp" ? Call{"keyUp", c} : Call{name.toStdString(), a, b, c, d, lang.toStdString()})));
}
void ServerProxyTests::P01_blockedPressPreservesRelease()
{
  ServerProxyConfig config;
  config.blockedKeyCombos = {"0x0041:0x0002"};
  config.blockedKeyCombosConfigured = true;
  DispatchFixture f(config);
  const auto scalar = integer(65, 2) + integer(2, 2);
  f.batch(
      "DKDN" + scalar + integer(7, 2) + "DKRP" + scalar + integer(3, 2) + integer(7, 2) + text("") + "DKUP" + scalar +
      integer(7, 2)
  );
  QCOMPARE(f.client.calls.size(), size_t(1));
  QVERIFY((f.client.calls.front() == Call{"keyUp", 65, 2, 7}));
}
void ServerProxyTests::P02_pointerDispatch_data()
{
  QTest::addColumn<QByteArray>("message");
  QTest::addColumn<QString>("name");
  QTest::addColumn<int>("a");
  QTest::addColumn<int>("b");
  QTest::addColumn<int>("c");
  QTest::addColumn<int>("d");
  auto row = [](const char *label, const std::string &bytes, const char *name, int a, int b = 0, int c = 0, int d = 0) {
    QTest::newRow(label) << QByteArray::fromStdString(bytes) << QString(name) << a << b << c << d;
  };
  row("down", "DMDN" + integer(1, 1), "button", 1, 1);
  row("up-extra-255", "DMUP" + integer(255, 1), "button", 255, 0);
  row("absolute-signed", "DMMV" + integer(-12, 2) + integer(34, 2), "move", -12, 34);
  row("relative-signed", "DMRM" + integer(-3, 2) + integer(4, 2), "relative", -3, 4);
  row("wheel-signed", "DMWM" + integer(-120, 2) + integer(240, 2), "wheel", -120, 240);
  row("enter", "CINN" + integer(-12, 2) + integer(34, 2) + integer(7, 4) + integer(2, 2), "enter", -12, 34, 7, 2);
  row("leave", "COUT", "leave", 0);
  row("screensaver-true", "CSEC" + integer(1, 1), "screensaver", 1);
  row("screensaver-false", "CSEC" + integer(0, 1), "screensaver", 0);
}
void ServerProxyTests::P02_pointerDispatch()
{
  QFETCH(QByteArray, message);
  QFETCH(QString, name);
  QFETCH(int, a);
  QFETCH(int, b);
  QFETCH(int, c);
  QFETCH(int, d);
  DispatchFixture f;
  QCOMPARE(f.message(message.toStdString()), DispatchProxy::ConnectionResult::Okay);
  QCOMPARE(f.client.calls.size(), size_t(1));
  QVERIFY((f.client.calls.front() == Call{name.toStdString(), a, b, c, d, name == "enter" ? "false" : ""}));
  Fixture owner;
  owner.ready();
  const auto old = owner.inputCalls();
  owner.send(message.toStdString());
  if (name == "screensaver")
    QCOMPARE(owner.inputCalls(), old); // real Screen::screensaver is a no-op.
  else if (name == "enter") {
    QCOMPARE(owner.inputCalls(), old + 2);
    const auto &calls = owner.screenState->calls;
    QVERIFY((calls[calls.size() - 2] == Call{"move", a, b}));
    QVERIFY((calls.back() == Call{"enter"}));
  } else if (name == "leave") {
    // Secondary screen leave releases fake keys, then fake platform leave.
    QVERIFY((owner.screenState->calls.back() == Call{"leave"}));
  } else {
    QVERIFY((owner.screenState->calls.back() == Call{name.toStdString(), a, b}));
  }
}
void ServerProxyTests::P03_motionOrder_data()
{
  QTest::addColumn<int>("kind");
  QTest::newRow("absolute-coalesce") << 0;
  QTest::newRow("relative-sum") << 1;
  QTest::newRow("absolute-relative-absolute") << 2;
  QTest::newRow("relative-absolute-relative") << 3;
  QTest::newRow("enter-discards-pending") << 4;
}
void ServerProxyTests::P03_motionOrder()
{
  QFETCH(int, kind);
  DispatchFixture f;
  const auto abs1 = "DMMV" + integer(10, 2) + integer(20, 2), abs2 = "DMMV" + integer(30, 2) + integer(40, 2);
  const auto rel1 = "DMRM" + integer(1, 2) + integer(2, 2), rel2 = "DMRM" + integer(3, 2) + integer(4, 2);
  const auto key = "DKDN" + integer(65, 2) + integer(0, 2) + integer(7, 2);
  std::vector<Call> want;
  if (kind == 0) {
    f.batch(abs1 + abs2 + key);
    want = {{"move", 30, 40}, {"keyDown", 65, 0, 7}};
  }
  if (kind == 1) {
    f.batch(rel1 + rel2 + key);
    want = {{"relative", 4, 6}, {"keyDown", 65, 0, 7}};
  }
  if (kind == 2) {
    f.batch(abs1 + rel2 + abs2 + key);
    want = {{"move", 30, 40}, {"keyDown", 65, 0, 7}};
  }
  if (kind == 3) {
    f.batch(rel1 + abs1 + rel2 + key);
    want = {{"move", 10, 20}, {"relative", 3, 4}, {"keyDown", 65, 0, 7}};
  }
  if (kind == 4) {
    f.batch(abs1 + abs2 + "CINN" + integer(1, 2) + integer(2, 2) + integer(7, 4) + integer(0, 2));
    want = {{"enter", 1, 2, 7, 0, "false"}};
  }
  QVERIFY(f.client.calls == want);
  QCOMPARE(f.stream.output, std::string("CNOP"));
}
void ServerProxyTests::P04_infoAcknowledgment()
{
  DispatchFixture f;
  f.proxy->onInfoChanged();
  const auto info = std::string("DINF") + integer(-7, 2) + integer(9, 2) + integer(800, 2) + integer(600, 2) +
                    integer(0, 2) + integer(31, 2) + integer(43, 2);
  QCOMPARE(f.stream.output, info);
  f.batch("DMMV" + integer(12, 2) + integer(34, 2));
  QVERIFY(f.client.calls.empty());
  QCOMPARE(f.message("CIAK"), DispatchProxy::ConnectionResult::Okay);
  f.batch("DMMV" + integer(12, 2) + integer(34, 2));
  QCOMPARE(f.client.calls.size(), size_t(1));
  QVERIFY((f.client.calls.front() == Call{"move", 12, 34}));
  DispatchFixture query;
  QCOMPARE(query.message("QINF"), DispatchProxy::ConnectionResult::Okay);
  QCOMPARE(query.stream.output, info);
}
void ServerProxyTests::P05_optionsAndTranslation()
{
  DispatchFixture f;
  QCOMPARE(f.events.state.timers.size(), size_t(1));
  QVERIFY(f.events.state.timerFor(9.0) != nullptr);
  const auto before = f.events.state.timerDeletes;
  const auto body = options({kOptionHeartbeat, 1000, kOptionModifierMapForControl, kKeyModifierIDAlt});
  QCOMPARE(f.message("DSOP" + body), DispatchProxy::ConnectionResult::Okay);
  QVERIFY(
      f.client.lastOptions == OptionsList({kOptionHeartbeat, 1000, kOptionModifierMapForControl, kKeyModifierIDAlt})
  );
  QCOMPARE(f.events.state.timerDeletes, before + 1);
  QVERIFY(f.events.state.timerFor(3.0) != nullptr);
  f.client.calls.clear();
  f.message("DKDN" + integer(kKeyControl_L, 2) + integer(2, 2) + integer(7, 2));
  QVERIFY((f.client.calls.back() == Call{"keyDown", kKeyAlt_L, 4, 7}));
  f.client.calls.clear();
  f.message("DSOP" + options({kOptionHeartbeat}));
  QVERIFY(f.client.calls.empty());
  QVERIFY(f.events.state.timerFor(3.0) != nullptr);
  f.message("CROP");
  QCOMPARE(f.client.resets, 1);
  QVERIFY(f.events.state.timerFor(9.0) != nullptr);
  f.message("DKDN" + integer(kKeyControl_L, 2) + integer(2, 2) + integer(8, 2));
  QVERIFY((f.client.calls.back() == Call{"keyDown", kKeyControl_L, 2, 8}));
  Fixture owner;
  owner.ready();
  owner.send("DSOP" + options({kOptionClipboardSharing, 0}));
  IScreen::ClipboardInfo grab{0, 7};
  QVERIFY(owner.events.dispatchEvent(
      Event(EventTypes::ClipboardGrabbed, owner.screenTarget, &grab, Event::EventFlags::DontFreeData)
  ));
  QVERIFY(owner.events.state.chunks.empty());
  owner.send("DSOP" + options({kOptionHeartbeat}));
  QVERIFY(owner.client->isConnected());
  owner.send("DMMV" + integer(12, 2) + integer(34, 2));
  QCOMPARE(owner.inputCalls(), size_t(1));
}
void ServerProxyTests::P06_controlBodies()
{
  DispatchFixture f;
  const auto deleted = f.events.state.timerDeletes;
  f.batch("CALVCNOPSECN" + text("xy") + "DMMV" + integer(12, 2) + integer(34, 2));
  QCOMPARE(f.stream.output, std::string("CALVCNOP"));
  QCOMPARE(f.events.state.timerDeletes, deleted + 1);
  QVERIFY(f.events.state.timerFor(9.0) != nullptr);
  QVERIFY((f.client.calls.back() == Call{"move", 12, 34}));
  Fixture owner;
  owner.awaitingHandshake();
  const auto before = owner.socket()->output;
  owner.send("LSYN" + text("en") + "CALVCNOP");
  QCOMPARE(owner.socket()->output, before + packet("CALV"));
  QCOMPARE(owner.screenState->enabled, 0);
  owner.send("DSOP" + options({}));
  QCOMPARE(owner.screenState->enabled, 1);
}
void ServerProxyTests::P07_clipboardAssembly_data()
{
  QTest::addColumn<int>("kind");
  QTest::newRow("valid-complete") << 0;
  QTest::newRow("invalid-CCLP-ignored") << 1;
  QTest::newRow("invalid-DCLP-id") << 2;
  QTest::newRow("invalid-DCLP-marker") << 3;
  QTest::newRow("DCLP-announced-over-bound") << 4;
}
void ServerProxyTests::P07_clipboardAssembly()
{
  QFETCH(int, kind);
  Fixture owner;
  owner.ready();
  if (kind == 0) {
    DispatchFixture direct;
    direct.message("CCLP" + integer(0, 1) + integer(7, 4));
    QVERIFY((direct.client.calls.back() == Call{"grabClipboard", 0}));
    const auto clears = owner.screenState->clipboardClears;
    owner.send("CCLP" + integer(0, 1) + integer(7, 4));
    QCOMPARE(owner.screenState->clipboardClears, clears + 1);
    owner.send("DCLP" + clipboardBody(0, 7, 1, "15"));
    QCOMPARE(owner.screenState->clipboardSets, 0);
    owner.send("DCLP" + clipboardBody(0, 7, 2, syntheticClipboard()));
    QCOMPARE(owner.screenState->clipboardSets, 0);
    owner.send("DCLP" + clipboardBody(0, 7, 3, ""));
    QCOMPARE(owner.screenState->clipboardSets, 1);
    QCOMPARE(owner.screenState->clipboardText, std::string("abc"));
    owner.send("DMMV" + integer(12, 2) + integer(34, 2));
    QVERIFY(owner.client->isConnected());
  } else if (kind == 1) {
    const auto clears = owner.screenState->clipboardClears;
    owner.send("CCLP" + integer(255, 1) + integer(7, 4));
    QCOMPARE(owner.screenState->clipboardClears, clears);
    QVERIFY(owner.client->isConnected());
    QVERIFY(owner.events.state.failures.empty());
  } else {
    const auto before = owner.socket()->output;
    owner.send("DCLP" + clipboardBody(kind == 2 ? 255 : 0, 7, kind == 3 ? 255 : 1, kind == 4 ? "129" : "15"));
    owner.assertReleased(true);
    QVERIFY(!QTest::currentTestFailed());
    QCOMPARE(owner.screenState->clipboardSets, 0);
    QCOMPARE(owner.socket()->output, before);
    QCOMPARE(owner.events.state.failures.front().what, QStringLiteral("invalid clipboard data from server"));
  }
}
void ServerProxyTests::P08_disabledTransferConsumes()
{
  // A valid, owned but absent drop directory makes "disabled" observable: a
  // receiver that accidentally enables writes must not create this directory.
  QTemporaryDir owned;
  QVERIFY(owned.isValid());
  const auto dropDirectory = owned.path() + "/must-not-exist";
  QVERIFY(!QFileInfo::exists(dropDirectory));
  ServerProxyConfig config;
  config.fileTransfer.enabled = false;
  config.fileTransfer.dropDirectory = dropDirectory.toStdString();
  Fixture owner(false, config);
  owner.ready();
  owner.send("DDRG" + integer(1, 2) + text("synthetic.txt"));
  owner.send("DFTR" + integer(1, 1) + text("3"));
  owner.send("DFTR" + integer(2, 1) + text("abc"));
  owner.send("DFTR" + integer(3, 1) + text(""));
  owner.send("DMMV" + integer(12, 2) + integer(34, 2));
  QVERIFY(owner.client->isConnected());
  QVERIFY(owner.events.state.failures.empty());
  QCOMPARE(owner.inputCalls(), size_t(1));
  QCOMPARE(owner.screenState->drags, 0);
  DispatchFixture direct(config);
  direct.batch("DDRG" + integer(1, 2) + text("synthetic.txt") + "DFTR" + integer(1, 1) + text("3") + "CNOP");
  QCOMPARE(direct.client.offers, 0);
  QCOMPARE(direct.client.disconnectCalls, 0);
  QCOMPARE(direct.stream.getSize(), 0U);
  QCOMPARE(direct.stream.output, std::string("CNOP"));
  QVERIFY(!QFileInfo::exists(dropDirectory));
  QVERIFY(QDir(owned.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).empty());
}
void ServerProxyTests::N04_screensaverForwarding_data()
{
  QTest::addColumn<QByteArray>("body");
  QTest::addColumn<bool>("complete");
  QTest::addColumn<bool>("on");
  QTest::newRow("CSEC-empty-body-no-forward") << QByteArray() << false << false;
  QTest::newRow("CSEC-complete-off") << QByteArray(1, '\0') << true << false;
  QTest::newRow("CSEC-complete-on") << QByteArray(1, '\1') << true << true;
}
void ServerProxyTests::N04_screensaverForwarding()
{
  QFETCH(QByteArray, body);
  QFETCH(bool, complete);
  QFETCH(bool, on);
  DispatchFixture f;
  // Real Screen::screensaver intentionally does nothing, so retain the owning
  // CSEC prefix row AND observe the decoder's actual IServerProxyClient callback
  // here. This is CSEC screensaver(bool), not SECN secure-input notification.
  const auto result = f.message("CSEC" + body.toStdString());
  if (complete) {
    QCOMPARE(result, DispatchProxy::ConnectionResult::Okay);
    QCOMPARE(f.client.disconnectCalls, 0);
    QCOMPARE(f.client.calls.size(), size_t(1));
    QVERIFY((f.client.calls.front() == Call{"screensaver", on}));
  } else {
    QCOMPARE(result, DispatchProxy::ConnectionResult::Disconnect);
    QCOMPARE(f.client.disconnectCalls, 1);
    QVERIFY(f.client.calls.empty());
  }
  QVERIFY(f.stream.output.empty());
}
QTEST_MAIN(ServerProxyTests)
