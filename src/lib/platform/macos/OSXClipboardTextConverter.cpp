/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXClipboardTextConverter.h"

//
// OSXClipboardTextConverter
//

CFStringRef OSXClipboardTextConverter::getOSXFormat() const
{
  return CFSTR("public.plain-text");
}

std::string OSXClipboardTextConverter::doFromIClipboard(const std::string &data) const
{
  return convertString(data, kCFStringEncodingUTF8, CFStringGetSystemEncoding());
}

std::string OSXClipboardTextConverter::doToIClipboard(const std::string &data) const
{
  return convertString(data, CFStringGetSystemEncoding(), kCFStringEncodingUTF8);
}
