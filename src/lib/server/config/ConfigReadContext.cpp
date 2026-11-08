/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/Config.h"

#include "KeyMap.h"
#include "KeyTypes.h"
#include "OptionTypes.h"
#include "ProtocolTypes.h"
#include "SynergyException.h"
#include "base/IEventQueue.h"
#include "net/SocketException.h"
#include "server/Server.h"

#include <assert.h>
#include <cstdlib>
#include <istream>
#include <ostream>

using namespace synergy::string;

namespace synergy::server {
//
// Config
//

//
// ConfigReadContext
//

ConfigReadContext::ConfigReadContext(std::istream &s, int32_t firstLine) : m_stream(s), m_line(firstLine - 1)
{
  // do nothing
}

bool ConfigReadContext::readLine(std::string &line)
{
  ++m_line;
  while (std::getline(m_stream, line)) {
    // strip leading whitespace
    std::string::size_type i = line.find_first_not_of(" \t");
    if (i != std::string::npos) {
      line.erase(0, i);
    }

    // strip comments and then trailing whitespace
    i = line.find('#');
    if (i != std::string::npos) {
      line.erase(i);
    }
    i = line.find_last_not_of(" \r\t");
    if (i != std::string::npos) {
      line.erase(i + 1);
    }

    // return non empty line
    if (!line.empty()) {
      // make sure there are no invalid characters
      for (i = 0; i < line.length(); ++i) {
        if (!isgraph(line[i]) && line[i] != ' ' && line[i] != '\t') {
          throw ServerConfigReadException(*this, "invalid character %{1}", synergy::string::sprintf("%#2x", line[i]));
        }
      }

      return true;
    }

    // next line
    ++m_line;
  }
  return false;
}

uint32_t ConfigReadContext::getLineNumber() const
{
  return m_line;
}

bool ConfigReadContext::operator!() const
{
  return !m_stream;
}

OptionValue ConfigReadContext::parseBoolean(const std::string &arg) const
{
  if (CaselessCmp::equal(arg, "true")) {
    return static_cast<OptionValue>(true);
  }
  if (CaselessCmp::equal(arg, "false")) {
    return static_cast<OptionValue>(false);
  }
  throw ServerConfigReadException(*this, "invalid boolean argument \"%{1}\"", arg);
}

OptionValue ConfigReadContext::parseInt(const std::string &arg) const
{
  const char *s = arg.c_str();
  char *end;
  long tmp = strtol(s, &end, 10);
  if (*end != '\0') {
    // invalid characters
    throw ServerConfigReadException(*this, "invalid integer argument \"%{1}\"", arg);
  }
  auto value = static_cast<OptionValue>(tmp);
  if (value != tmp) {
    // out of range
    throw ServerConfigReadException(*this, "integer argument \"%{1}\" out of range", arg);
  }
  return value;
}

OptionValue ConfigReadContext::parseModifierKey(const std::string &arg) const
{
  if (CaselessCmp::equal(arg, "shift")) {
    return static_cast<OptionValue>(kKeyModifierIDShift);
  }
  if (CaselessCmp::equal(arg, "ctrl")) {
    return static_cast<OptionValue>(kKeyModifierIDControl);
  }
  if (CaselessCmp::equal(arg, "alt")) {
    return static_cast<OptionValue>(kKeyModifierIDAlt);
  }
  if (CaselessCmp::equal(arg, "altgr")) {
    return static_cast<OptionValue>(kKeyModifierIDAltGr);
  }
  if (CaselessCmp::equal(arg, "meta")) {
    return static_cast<OptionValue>(kKeyModifierIDMeta);
  }
  if (CaselessCmp::equal(arg, "super")) {
    return static_cast<OptionValue>(kKeyModifierIDSuper);
  }
  if (CaselessCmp::equal(arg, "none")) {
    return static_cast<OptionValue>(kKeyModifierIDNull);
  }
  throw ServerConfigReadException(*this, "invalid argument \"%{1}\"", arg);
}

OptionValue ConfigReadContext::parseCorner(const std::string &arg) const
{
  if (CaselessCmp::equal(arg, "left")) {
    return s_topLeftCornerMask | s_bottomLeftCornerMask;
  } else if (CaselessCmp::equal(arg, "right")) {
    return s_topRightCornerMask | s_bottomRightCornerMask;
  } else if (CaselessCmp::equal(arg, "top")) {
    return s_topLeftCornerMask | s_topRightCornerMask;
  } else if (CaselessCmp::equal(arg, "bottom")) {
    return s_bottomLeftCornerMask | s_bottomRightCornerMask;
  } else if (CaselessCmp::equal(arg, "top-left")) {
    return s_topLeftCornerMask;
  } else if (CaselessCmp::equal(arg, "top-right")) {
    return s_topRightCornerMask;
  } else if (CaselessCmp::equal(arg, "bottom-left")) {
    return s_bottomLeftCornerMask;
  } else if (CaselessCmp::equal(arg, "bottom-right")) {
    return s_bottomRightCornerMask;
  } else if (CaselessCmp::equal(arg, "none")) {
    return s_noCornerMask;
  } else if (CaselessCmp::equal(arg, "all")) {
    return s_allCornersMask;
  }
  throw ServerConfigReadException(*this, "invalid argument \"%{1}\"", arg);
}

OptionValue ConfigReadContext::parseCorners(const std::string &args) const
{
  // find first token
  std::string::size_type i = args.find_first_not_of(" \t", 0);
  if (i == std::string::npos) {
    throw ServerConfigReadException(*this, "missing corner argument");
  }
  std::string::size_type j = args.find_first_of(" \t", i);

  // parse first corner token
  OptionValue corners = parseCorner(args.substr(i, j - i));

  // get +/-
  i = args.find_first_not_of(" \t", j);
  while (i != std::string::npos) {
    // parse +/-
    bool add;
    if (args[i] == '-') {
      add = false;
    } else if (args[i] == '+') {
      add = true;
    } else {
      throw ServerConfigReadException(*this, "invalid corner operator \"%{1}\"", std::string(args.c_str() + i, 1));
    }

    // get next corner token
    i = args.find_first_not_of(" \t", i + 1);
    j = args.find_first_of(" \t", i);
    if (i == std::string::npos) {
      throw ServerConfigReadException(*this, "missing corner argument");
    }

    // parse next corner token
    if (add) {
      corners |= parseCorner(args.substr(i, j - i));
    } else {
      corners &= ~parseCorner(args.substr(i, j - i));
    }
    i = args.find_first_not_of(" \t", j);
  }

  return corners;
}

Config::Interval ConfigReadContext::parseInterval(const ArgList &args) const
{
  if (args.size() == 0) {
    return Config::Interval(0.0f, 1.0f);
  }
  if (args.size() != 2 || args[0].empty() || args[1].empty()) {
    throw ServerConfigReadException(*this, "invalid interval \"%{1}\"", concatArgs(args));
  }

  char *end;
  double startValue = strtod(args[0].c_str(), &end);
  if (end[0] != '\0') {
    throw ServerConfigReadException(*this, "invalid interval \"%{1}\"", concatArgs(args));
  }
  double endValue = strtod(args[1].c_str(), &end);
  if (end[0] != '\0') {
    throw ServerConfigReadException(*this, "invalid interval \"%{1}\"", concatArgs(args));
  }

  if (startValue < 0 || startValue > 100 || endValue < 0 || endValue > 100 || startValue >= endValue) {
    throw ServerConfigReadException(*this, "invalid interval \"%{1}\"", concatArgs(args));
  }

  auto startInterval = static_cast<float>(startValue / 100.0f);
  auto endInterval = static_cast<float>(endValue / 100.0f);
  return Config::Interval(startInterval, endInterval);
}

void ConfigReadContext::parseNameWithArgs(
    const std::string &type, const std::string &line, const std::string &delim, std::string::size_type &index,
    std::string &name, ArgList &args
) const
{
  // skip leading whitespace
  std::string::size_type i = line.find_first_not_of(" \t", index);
  if (i == std::string::npos) {
    throw ServerConfigReadException(*this, std::string("missing ") + type);
  }

  // find end of name
  std::string::size_type j = line.find_first_of(" \t(" + delim, i);
  if (j == std::string::npos) {
    j = line.length();
  }

  // save name
  name = line.substr(i, j - i);
  args.clear();

  // is it okay to not find a delimiter?
  bool needDelim = (!delim.empty() && delim.find('\n') == std::string::npos);

  // skip whitespace
  i = line.find_first_not_of(" \t", j);
  if (i == std::string::npos && needDelim) {
    // expected delimiter but didn't find it
    throw ServerConfigReadException(*this, std::string("missing ") + delim[0]);
  }
  if (i == std::string::npos) {
    // no arguments
    index = line.length();
    return;
  }
  if (line[i] != '(') {
    // no arguments
    index = i;
    return;
  }

  // eat '('
  ++i;

  // parse arguments
  j = line.find_first_of(",)", i);
  while (j != std::string::npos) {
    // extract arg
    std::string arg(line.substr(i, j - i));
    i = j;

    // trim whitespace
    j = arg.find_first_not_of(" \t");
    if (j != std::string::npos) {
      arg.erase(0, j);
    }
    j = arg.find_last_not_of(" \t");
    if (j != std::string::npos) {
      arg.erase(j + 1);
    }

    // save arg
    args.push_back(arg);

    // exit loop at end of arguments
    if (line[i] == ')') {
      break;
    }

    // eat ','
    ++i;

    // next
    j = line.find_first_of(",)", i);
  }

  // verify ')'
  if (j == std::string::npos) {
    // expected )
    throw ServerConfigReadException(*this, "missing )");
  }

  // eat ')'
  ++i;

  // skip whitespace
  j = line.find_first_not_of(" \t", i);
  if (j == std::string::npos && needDelim) {
    // expected delimiter but didn't find it
    throw ServerConfigReadException(*this, std::string("missing ") + delim[0]);
  }

  // verify delimiter
  if (needDelim && delim.find(line[j]) == std::string::npos) {
    throw ServerConfigReadException(*this, std::string("expected ") + delim[0]);
  }

  if (j == std::string::npos) {
    j = line.length();
  }

  index = j;
  return;
}

IPlatformScreen::KeyInfo *ConfigReadContext::parseKeystroke(const std::string &keystroke) const
{
  return parseKeystroke(keystroke, std::set<std::string>());
}

IPlatformScreen::KeyInfo *
ConfigReadContext::parseKeystroke(const std::string &keystroke, const std::set<std::string> &screens) const
{
  std::string s = keystroke;

  KeyModifierMask mask;
  if (!synergy::KeyMap::parseModifiers(s, mask)) {
    throw ServerConfigReadException(*this, "unable to parse key modifiers");
  }

  KeyID key;
  if (!synergy::KeyMap::parseKey(s, key)) {
    throw ServerConfigReadException(*this, "unable to parse key");
  }

  if (key == kKeyNone && mask == 0) {
    throw ServerConfigReadException(*this, "missing key and/or modifiers in keystroke");
  }

  return IPlatformScreen::KeyInfo::alloc(key, mask, 0, 0, screens);
}

IPlatformScreen::ButtonInfo ConfigReadContext::parseMouse(const std::string &mouse) const
{
  std::string s = mouse;

  KeyModifierMask mask;
  if (!synergy::KeyMap::parseModifiers(s, mask)) {
    throw ServerConfigReadException(*this, "unable to parse button modifiers");
  }

  char *end;
  auto button = static_cast<ButtonID>(strtol(s.c_str(), &end, 10));
  if (*end != '\0') {
    throw ServerConfigReadException(*this, "unable to parse button");
  }
  if (s.empty() || button <= 0) {
    throw ServerConfigReadException(*this, "invalid button");
  }

  return IPlatformScreen::ButtonInfo{button, mask};
}

KeyModifierMask ConfigReadContext::parseModifier(const std::string &modifiers) const
{
  std::string s = modifiers;

  KeyModifierMask mask;
  if (!synergy::KeyMap::parseModifiers(s, mask)) {
    throw ServerConfigReadException(*this, "unable to parse modifiers");
  }

  if (mask == 0) {
    throw ServerConfigReadException(*this, "no modifiers specified");
  }

  return mask;
}

std::string ConfigReadContext::concatArgs(const ArgList &args)
{
  std::string s("(");
  for (size_t i = 0; i < args.size(); ++i) {
    if (i != 0) {
      s += ",";
    }
    s += args[i];
  }
  s += ")";
  return s;
}

//
// Config I/O exceptions
//

ServerConfigReadException::ServerConfigReadException(const ConfigReadContext &context, const std::string &error)
    : m_error(synergy::string::sprintf("line %d: %s", context.getLineNumber(), error.c_str()))
{
  // do nothing
}

ServerConfigReadException::ServerConfigReadException(
    const ConfigReadContext &context, const char *errorFmt, const std::string &arg
)
    : m_error(
          synergy::string::sprintf("line %d: ", context.getLineNumber()) +
          synergy::string::format(errorFmt, arg.c_str())
      )
{
  // do nothing
}

QString ServerConfigReadException::getWhat() const throw()
{
  return format("ServerConfigReadException", "read error: %{1}", m_error.c_str());
}

} // namespace synergy::server
