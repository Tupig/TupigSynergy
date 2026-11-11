/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXClipboardAnyTextConverter.h"

#include "base/String.h"

//
// OSXClipboardAnyTextConverter
//

IClipboard::Format OSXClipboardAnyTextConverter::getFormat() const
{
  return IClipboard::Format::Text;
}

std::string OSXClipboardAnyTextConverter::fromIClipboard(const std::string &data) const
{
  // the shared clipboard uses LF line endings; the classic Mac OS
  // pasteboard convention is CR
  return doFromIClipboard(synergy::string::convertLinefeedsFromLf(data, "\r"));
}

std::string OSXClipboardAnyTextConverter::toIClipboard(const std::string &data) const
{
  // convert text then newlines
  return synergy::string::convertLinefeedsToLf(doToIClipboard(data));
}

std::string OSXClipboardAnyTextConverter::convertString(
    const std::string &data, CFStringEncoding fromEncoding, CFStringEncoding toEncoding
)
{
  CFStringRef stringRef = CFStringCreateWithCString(kCFAllocatorDefault, data.c_str(), fromEncoding);

  if (stringRef == nullptr) {
    return std::string();
  }

  CFIndex buffSize;
  CFRange entireString = CFRangeMake(0, CFStringGetLength(stringRef));

  CFStringGetBytes(stringRef, entireString, toEncoding, 0, false, nullptr, 0, &buffSize);

  char *buffer = new char[buffSize];

  if (buffer == nullptr) {
    CFRelease(stringRef);
    return std::string();
  }

  CFStringGetBytes(stringRef, entireString, toEncoding, 0, false, (uint8_t *)buffer, buffSize, nullptr);

  std::string result(buffer, buffSize);

  delete[] buffer;
  CFRelease(stringRef);

  return result;
}
