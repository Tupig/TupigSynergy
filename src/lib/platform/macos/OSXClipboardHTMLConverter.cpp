/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Symless Ltd
 * SPDX-FileCopyrightText: (C) 2014 Ryan Chapman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXClipboardHTMLConverter.h"

#include "base/Unicode.h"

IClipboard::Format OSXClipboardHTMLConverter::getFormat() const
{
  return IClipboard::Format::HTML;
}

CFStringRef OSXClipboardHTMLConverter::getOSXFormat() const
{
  return CFSTR("public.html");
}

std::string OSXClipboardHTMLConverter::doFromIClipboard(const std::string &data) const
{
  return data;
}

std::string OSXClipboardHTMLConverter::doToIClipboard(const std::string &data) const
{
  if (Unicode::isUTF8(data)) {
    return data;
  } else {
    return convertString(data, CFStringGetSystemEncoding(), kCFStringEncodingUTF8);
  }
}
