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
  int keyDownCalls = 0, keyUpCalls = 0, keyRepeatCalls = 0;
  int mouseDownCalls = 0, mouseUpCalls = 0, relativeCalls = 0, wheelCalls = 0;
  int resetCalls = 0, optionsCalls = 0, refusalCalls = 0;
  KeyID lastKey = 0;
  KeyModifierMask lastMask = 0;
  KeyButton lastButton = 0;
  int32_t lastRepeat = 0;
  ButtonID lastMouseButton = 0;
  OptionsList lastOptions;
  synergy::core::ConnectionRefusal lastRefusal{};
  int enterCalls = 0;
  int leaveCalls = 0;
  int mouseMoveCalls = 0;
  int32_t lastX = 0;
  int32_t lastY = 0;

  void disconnect(const char *) override
  {
    ++disconnectCalls;
  }

  void refuseConnection(synergy::core::ConnectionRefusal reason, const char *) override
  {
    ++refusalCalls;
    lastRefusal = reason;
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

  void keyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &) override
  {
    ++keyDownCalls;
    lastKey = id;
    lastMask = mask;
    lastButton = button;
  }

  void keyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &) override
  {
    ++keyRepeatCalls;
    lastKey = id;
    lastMask = mask;
    lastButton = button;
    lastRepeat = count;
  }

  void keyUp(KeyID id, KeyModifierMask mask, KeyButton button) override
  {
    ++keyUpCalls;
    lastKey = id;
    lastMask = mask;
    lastButton = button;
  }

  void mouseDown(ButtonID button) override
  {
    ++mouseDownCalls;
    lastMouseButton = button;
  }

  void mouseUp(ButtonID button) override
  {
    ++mouseUpCalls;
    lastMouseButton = button;
  }

  void mouseMove(int32_t xAbs, int32_t yAbs) override
  {
    ++mouseMoveCalls;
    lastX = xAbs;
    lastY = yAbs;
  }

  void mouseRelativeMove(int32_t x, int32_t y) override
  {
    ++relativeCalls;
    lastX = x;
    lastY = y;
  }

  void mouseWheel(int32_t x, int32_t y) override
  {
    ++wheelCalls;
    lastX = x;
    lastY = y;
  }

  void screensaver(bool) override
  {
  }

  void resetOptions() override
  {
    ++resetCalls;
  }

  void setOptions(const OptionsList &options) override
  {
    ++optionsCalls;
    lastOptions = options;
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
  using ServerProxy::parseHandshakeMessage;
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

void ServerProxyTests::keyMessagesPreserveArguments()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});
  uint8_t code[4];
  stream.push(protocol_test::integers16({65, KeyModifierShift, 42}));
  copyCode(kMsgDKeyDown, code);
  QCOMPARE(proxy.parseMessage(code), TestableServerProxy::ConnectionResult::Okay);
  QCOMPARE(client.keyDownCalls, 1);
  QCOMPARE(client.lastKey, KeyID(65));
  QCOMPARE(client.lastMask, KeyModifierMask(KeyModifierShift));
  QCOMPARE(client.lastButton, KeyButton(42));
  stream.push(protocol_test::integers16({65, KeyModifierShift, 3, 42}) + protocol_test::integer32(0));
  copyCode(kMsgDKeyRepeat, code);
  proxy.parseMessage(code);
  QCOMPARE(client.keyRepeatCalls, 1);
  QCOMPARE(client.lastRepeat, 3);
  stream.push(protocol_test::integers16({65, 0, 42}));
  copyCode(kMsgDKeyUp, code);
  proxy.parseMessage(code);
  QCOMPARE(client.keyUpCalls, 1);
  QCOMPARE(client.lastMask, KeyModifierMask(0));
  QCOMPARE(client.lastButton, KeyButton(42));
}

void ServerProxyTests::mouseButtonsAndSignedDeltas()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});
  uint8_t code[4];
  stream.push(std::string(1, '\x03'));
  copyCode(kMsgDMouseDown, code);
  proxy.parseMessage(code);
  QCOMPARE(client.mouseDownCalls, 1);
  QCOMPARE(client.lastMouseButton, ButtonID(3));
  stream.push(std::string(1, '\x03'));
  copyCode(kMsgDMouseUp, code);
  proxy.parseMessage(code);
  QCOMPARE(client.mouseUpCalls, 1);
  stream.push(protocol_test::integers16({-12, 34}));
  copyCode(kMsgDMouseRelMove, code);
  proxy.parseMessage(code);
  QCOMPARE(client.relativeCalls, 1);
  QCOMPARE(client.lastX, -12);
  QCOMPARE(client.lastY, 34);
  stream.push(protocol_test::integers16({120, -120}));
  copyCode(kMsgDMouseWheel, code);
  proxy.parseMessage(code);
  QCOMPARE(client.wheelCalls, 1);
  QCOMPARE(client.lastX, 120);
  QCOMPARE(client.lastY, -120);
}

void ServerProxyTests::optionsAreForwarded()
{
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});
  uint8_t code[4];
  copyCode(kMsgCResetOptions, code);
  proxy.parseMessage(code);
  QCOMPARE(client.resetCalls, 1);
  stream.push(
      protocol_test::integer32(2) + protocol_test::integer32(kOptionClipboardSharing) + protocol_test::integer32(0)
  );
  copyCode(kMsgDSetOptions, code);
  proxy.parseMessage(code);
  QCOMPARE(client.optionsCalls, 1);
  QCOMPARE(client.lastOptions, OptionsList({kOptionClipboardSharing, 0}));
}

void ServerProxyTests::truncatedInputDoesNotDispatch_data()
{
  QTest::addColumn<QByteArray>("code");
  for (const auto *code : {"DKDN", "DKUP", "DKRP", "DMDN", "DMUP", "DMMV", "DMRM", "DMWM", "CINN"})
    QTest::newRow(code) << QByteArray(code);
}

void ServerProxyTests::truncatedInputDoesNotDispatch()
{
  QFETCH(QByteArray, code);
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});
  proxy.parseMessage(reinterpret_cast<const uint8_t *>(code.constData()));
  QCOMPARE(client.disconnectCalls, 1);
  QCOMPARE(client.keyDownCalls + client.keyUpCalls + client.keyRepeatCalls, 0);
  QCOMPARE(
      client.mouseDownCalls + client.mouseUpCalls + client.mouseMoveCalls + client.relativeCalls + client.wheelCalls, 0
  );
  QCOMPARE(client.enterCalls, 0);
}

void ServerProxyTests::handshakeRefusals_data()
{
  QTest::addColumn<QByteArray>("code");
  QTest::addColumn<int>("reason");
  using enum synergy::core::ConnectionRefusal;
  QTest::newRow("busy") << QByteArray("EBSY") << static_cast<int>(AlreadyConnected);
  QTest::newRow("unknown") << QByteArray("EUNK") << static_cast<int>(UnknownClient);
  QTest::newRow("protocol") << QByteArray("EBAD") << static_cast<int>(ProtocolError);
  QTest::newRow("version") << QByteArray("EICV") << static_cast<int>(IncompatibleVersion);
}

void ServerProxyTests::handshakeRefusals()
{
  QFETCH(QByteArray, code);
  QFETCH(int, reason);
  MockEventQueue events;
  FakeServerProxyClient client;
  MemoryStream stream;
  if (code == "EICV")
    stream.push(protocol_test::integers16({2, 0}));
  TestableServerProxy proxy(&client, &stream, &events, ServerProxyConfig{});
  QCOMPARE(
      proxy.parseHandshakeMessage(reinterpret_cast<const uint8_t *>(code.constData())),
      TestableServerProxy::ConnectionResult::Disconnect
  );
  QCOMPARE(client.refusalCalls, 1);
  QCOMPARE(static_cast<int>(client.lastRefusal), reason);
}

QTEST_MAIN(ServerProxyTests)
