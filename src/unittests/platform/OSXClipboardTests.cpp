/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2011 Nick Bolton
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXClipboardTests.h"

#include "OSXClipboard.h"
#include "OSXClipboardHTMLConverter.h"
#include "OSXClipboardTextConverter.h"
#include "OSXClipboardUTF8Converter.h"

void OSXClipboardTests::open()
{
  OSXClipboard clipboard;
  QVERIFY(clipboard.open(0));
  QVERIFY(clipboard.empty());
  clipboard.close();
}

void OSXClipboardTests::singleFormat()
{
  using enum IClipboard::Format;

  OSXClipboard clipboard;
  QVERIFY(clipboard.empty());
  clipboard.add(Text, m_testString);
  QVERIFY(clipboard.has(Text));
  QCOMPARE(clipboard.get(Text), m_testString);
}

void OSXClipboardTests::formatConvert_UTF8()
{
  OSXClipboardUTF8Converter converter;
  QCOMPARE(IClipboard::Format::Text, converter.getFormat());
  QCOMPARE(converter.getOSXFormat(), CFSTR("public.utf8-plain-text"));
  QCOMPARE(converter.fromIClipboard("test data\n"), "test data\r");
  QCOMPARE(converter.toIClipboard("test data\r"), "test data\n");
}

namespace {
class EncodingConverter : public OSXClipboardTextConverter
{
public:
  using OSXClipboardTextConverter::convertString;
};
} // namespace

void OSXClipboardTests::textEncodingPreservesBytes_data()
{
  QTest::addColumn<QByteArray>("input");
  QTest::addColumn<quint32>("encoding");
  QTest::addColumn<QByteArray>("expected");
  QTest::newRow("ascii") << QByteArray("plain") << quint32(kCFStringEncodingUTF8) << QByteArray("plain");
  QTest::newRow("unicode") << QByteArray::fromHex("c3a9e4b8ad") << quint32(kCFStringEncodingUTF16LE)
                           << QByteArray::fromHex("e9002d4e");
  QTest::newRow("empty") << QByteArray() << quint32(kCFStringEncodingUTF8) << QByteArray();
  QTest::newRow("invalid-utf8") << QByteArray::fromHex("ff") << quint32(kCFStringEncodingUTF8) << QByteArray();
}

void OSXClipboardTests::textEncodingPreservesBytes()
{
  QFETCH(QByteArray, input);
  QFETCH(quint32, encoding);
  QFETCH(QByteArray, expected);
  const auto actual = EncodingConverter::convertString(input.toStdString(), kCFStringEncodingUTF8, encoding);
  QCOMPARE(QByteArray::fromStdString(actual), expected);
}

void OSXClipboardTests::htmlPreservesUnicodeAndNormalizesLinefeeds()
{
  OSXClipboardHTMLConverter converter;
  const std::string unicode = QByteArray::fromHex("c3a9e4b8ad").toStdString();
  const auto html = std::string("<p>") + unicode + "</p>";
  QCOMPARE(converter.fromIClipboard(html + "\n"), html + "\r");
  QCOMPARE(converter.toIClipboard(html + "\r\n"), html + "\n");
}

QTEST_MAIN(OSXClipboardTests)
