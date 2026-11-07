/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ProtocolUtilTests.h"

#include "OptionTypes.h"
#include "ProtocolTypes.h"
#include "ProtocolUtil.h"
#include "TestStreams.h"
#include "io/IStream.h"
#include "synergy/core/SynergyException.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <set>

namespace {

std::string binaryPayload(size_t size)
{
  std::string result(size, '\0');
  for (size_t i = 0; i < size; ++i) {
    result[i] = static_cast<char>((i * 131 + 17) & 0xff);
  }
  if (size > 0) {
    result[0] = '\0';
  }
  if (size > 1) {
    result[1] = static_cast<char>(0xff);
  }
  return result;
}

std::string encodedString(const std::string &payload)
{
  const auto size = static_cast<uint32_t>(payload.size());
  std::string result;
  result.push_back(static_cast<char>((size >> 24) & 0xff));
  result.push_back(static_cast<char>((size >> 16) & 0xff));
  result.push_back(static_cast<char>((size >> 8) & 0xff));
  result.push_back(static_cast<char>(size & 0xff));
  result += payload;
  return result;
}

class ObservedReadStream : public MemoryStream
{
public:
  uint32_t read(void *buffer, uint32_t count) override
  {
    const auto result = MemoryStream::read(buffer, count);
    bytesRead += result;
    if (result == 0)
      ++emptyReads;
    return result;
  }
  size_t bytesRead = 0;
  unsigned emptyReads = 0;
};

} // namespace

void ProtocolUtilTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Debug);
}

void ProtocolUtilTests::readBytesAcceptsVariableStringWithinLimit()
{
  MemoryStream stream(MemoryStream::WriteMode::Loopback);
  // encode a variable-length string the way a legitimate peer would
  std::string payload("hello synergy");
  ProtocolUtil::writef(&stream, "%s", &payload);

  std::string result;
  ProtocolUtil::readf(&stream, "%s", &result);

  QCOMPARE(result, payload);
}

void ProtocolUtilTests::readBytesRejectsVariableStringBeyondLimit()
{
  MemoryStream stream(MemoryStream::WriteMode::Loopback);
  // 4 raw bytes, big-endian on the wire: declares a 4GB variable string
  // with no payload following. must be rejected before any allocation.
  stream.push(std::string("\xff\xff\xff\xff", 4));

  std::string result;
  try {
    ProtocolUtil::readf(&stream, "%s", &result);
    QFAIL("expected BadClientException for oversized variable string");
  } catch (const BadClientException &) {
    // expected: peer gets kicked, process stays alive
  }
}

void ProtocolUtilTests::roundTripKeyDownFormat()
{
  // real message body: DKDN%2i%2i%2i (key id, modifier mask, button).
  // writef() takes integer arguments by value (as production callers do);
  // readf() takes pointers, which is where the read side returns the values.
  MemoryStream stream(MemoryStream::WriteMode::Loopback);
  const uint16_t id = 0x1234;
  const uint16_t mask = 0xabcd;
  const uint16_t button = 0x0042;
  ProtocolUtil::writef(&stream, kMsgDKeyDown + 4, id, mask, button);

  uint16_t readId = 0;
  uint16_t readMask = 0;
  uint16_t readButton = 0;
  QVERIFY(ProtocolUtil::readf(&stream, kMsgDKeyDown + 4, &readId, &readMask, &readButton));
  QCOMPARE(readId, id);
  QCOMPARE(readMask, mask);
  QCOMPARE(readButton, button);
}

void ProtocolUtilTests::truncatedKeyDownFormatReturnsFalse()
{
  // three of the six expected bytes: the stream ends mid-message
  MemoryStream stream(MemoryStream::WriteMode::Loopback);
  stream.push(std::string("\x12\x34\xab", 3));

  uint16_t id = 0;
  uint16_t mask = 0;
  uint16_t button = 0;
  QVERIFY(!ProtocolUtil::readf(&stream, kMsgDKeyDown + 4, &id, &mask, &button));
}

void ProtocolUtilTests::roundTripOptionsListFormat()
{
  // real message body: DSOP%4I (count-prefixed list of 4-byte integers)
  MemoryStream stream(MemoryStream::WriteMode::Loopback);
  const OptionsList written = {1, 2, 0xffffffff};
  ProtocolUtil::writef(&stream, kMsgDSetOptions + 4, &written);

  OptionsList read;
  QVERIFY(ProtocolUtil::readf(&stream, kMsgDSetOptions + 4, &read));
  QCOMPARE(read, written);
}

void ProtocolUtilTests::truncatedOptionsListReturnsFalse()
{
  // declares a two-entry list but only carries one entry
  MemoryStream stream(MemoryStream::WriteMode::Loopback);
  stream.push(std::string("\x00\x00\x00\x02\x00\x00\x00\x01", 8));

  OptionsList read;
  QVERIFY(!ProtocolUtil::readf(&stream, kMsgDSetOptions + 4, &read));
}

void ProtocolUtilTests::invalidIntegerLengthThrows()
{
  // %8i is not a valid protocol integer length; the reader must refuse the
  // format instead of reading an arbitrary amount of data
  MemoryStream stream(MemoryStream::WriteMode::Loopback);
  stream.push(std::string("\x00\x00\x00\x00\x00\x00\x00\x01", 8));

  uint32_t value = 0;
  bool threw = false;
  try {
    ProtocolUtil::readf(&stream, "%8i", &value);
  } catch (const BadClientException &) {
    threw = true;
  }
  QVERIFY(threw);
}

void ProtocolUtilTests::writeStringBinaryExact_data()
{
  QTest::addColumn<int>("payloadBytes");
  for (const int size : {0, 32, 4096, 262144}) {
    QTest::newRow(std::to_string(size).c_str()) << size;
  }
}

void ProtocolUtilTests::writeStringBinaryExact()
{
  QFETCH(int, payloadBytes);
  const std::string payload = binaryPayload(payloadBytes);
  BufferWriteStream stream;
  ProtocolUtil::writef(&stream, "%s", &payload);
  QCOMPARE(stream.str(), encodedString(payload));
}

void ProtocolUtilTests::writeRawBinaryExact_data()
{
  writeStringBinaryExact_data();
}

void ProtocolUtilTests::writeRawBinaryExact()
{
  QFETCH(int, payloadBytes);
  const std::string payload = binaryPayload(payloadBytes);
  const uint8_t sentinel = 0;
  const auto *data = payload.empty() ? &sentinel : reinterpret_cast<const uint8_t *>(payload.data());
  BufferWriteStream stream;
  ProtocolUtil::writef(&stream, "%S", static_cast<uint32_t>(payload.size()), data);
  QCOMPARE(stream.str(), encodedString(payload));
}

void ProtocolUtilTests::mixedWriterFormatExact()
{
  const auto text = binaryPayload(32);
  const auto raw = binaryPayload(4096);
  BufferWriteStream stream;
  ProtocolUtil::writef(
      &stream, "MIXD%2i%s%S", uint32_t{0x1234}, &text, static_cast<uint32_t>(raw.size()),
      reinterpret_cast<const uint8_t *>(raw.data())
  );
  QCOMPARE(stream.str(), std::string("MIXD\x12\x34", 6) + encodedString(text) + encodedString(raw));
}

void ProtocolUtilTests::readBinaryStrings_data()
{
  QTest::addColumn<int>("payloadBytes");
  for (const int size : {0, 1, 127, 128, 129, 4096, 65536}) {
    QTest::newRow(std::to_string(size).c_str()) << size;
  }
}

void ProtocolUtilTests::readBinaryStrings()
{
  QFETCH(int, payloadBytes);
  const auto payload = binaryPayload(payloadBytes);
  ObservedReadStream stream;
  stream.push(encodedString(payload));
  std::string destination = "unchanged sentinel";
  QVERIFY(ProtocolUtil::readf(&stream, "%s", &destination));
  QCOMPARE(destination, payload);
  QCOMPARE(stream.bytesRead, payload.size() + 4);
  QCOMPARE(stream.emptyReads, 0u);
  QCOMPARE(stream.getSize(), 0u);
}

void ProtocolUtilTests::truncatedStringPreservesDestination_data()
{
  QTest::addColumn<int>("payloadBytes");
  QTest::addColumn<int>("wireBytes");
  for (int prefix = 0; prefix < 4; ++prefix) {
    QTest::newRow(("prefix-" + std::to_string(prefix)).c_str()) << 129 << prefix;
  }
  for (const int size : {128, 129, 4096}) {
    for (const int cut : std::set<int>{0, 1, 127, 128, 129, size - 1}) {
      if (cut < size) {
        QTest::newRow((std::to_string(size) + "-payload-" + std::to_string(cut)).c_str()) << size << (4 + cut);
      }
    }
  }
}

void ProtocolUtilTests::truncatedStringPreservesDestination()
{
  QFETCH(int, payloadBytes);
  QFETCH(int, wireBytes);
  ObservedReadStream stream;
  stream.push(encodedString(binaryPayload(payloadBytes)).substr(0, wireBytes));
  std::string destination = "unchanged sentinel";
  QVERIFY(!ProtocolUtil::readf(&stream, "%s", &destination));
  QCOMPARE(destination, std::string("unchanged sentinel"));
  QCOMPARE(stream.bytesRead, static_cast<size_t>(wireBytes));
  QCOMPARE(stream.emptyReads, 1u);
}

QTEST_MAIN(ProtocolUtilTests)
