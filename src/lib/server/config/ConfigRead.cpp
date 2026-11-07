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
void Config::read(ConfigReadContext &context)
{
  Config tmp(m_events);
  while (context.getStream()) {
    tmp.readSection(context);
  }
  *this = tmp;
}

void Config::readSection(ConfigReadContext &s)
{
  static const char s_section[] = "section:";
  static const char s_options[] = "options";
  static const char s_screens[] = "screens";
  static const char s_links[] = "links";
  static const char s_aliases[] = "aliases";

  std::string line;
  if (!s.readLine(line)) {
    // no more sections
    return;
  }

  // should be a section header
  if (line.find(s_section) != 0) {
    throw ServerConfigReadException(s, "found data outside section");
  }

  // get section name
  std::string::size_type i = line.find_first_not_of(" \t", sizeof(s_section) - 1);
  if (i == std::string::npos) {
    throw ServerConfigReadException(s, "section name is missing");
  }
  std::string name = line.substr(i);
  i = name.find_first_of(" \t");
  if (i != std::string::npos) {
    throw ServerConfigReadException(s, "unexpected data after section name");
  }

  // read section
  if (name == s_options) {
    readSectionOptions(s);
  } else if (name == s_screens) {
    readSectionScreens(s);
  } else if (name == s_links) {
    readSectionLinks(s);
  } else if (name == s_aliases) {
    readSectionAliases(s);
  } else {
    throw ServerConfigReadException(s, "unknown section name \"%{1}\"", name);
  }
}

void Config::readSectionOptions(ConfigReadContext &s)
{
  std::string line;
  while (s.readLine(line)) {
    // check for end of section
    if (line == "end") {
      return;
    }

    // parse argument:  `nameAndArgs = [values][;[values]]'
    //   nameAndArgs  := <name>[(arg[,...])]
    //   values       := valueAndArgs[,valueAndArgs]...
    //   valueAndArgs := <value>[(arg[,...])]
    std::string::size_type i = 0;
    std::string name;
    std::string value;
    ConfigReadContext::ArgList nameArgs;
    ConfigReadContext::ArgList valueArgs;
    s.parseNameWithArgs("name", line, "=", i, name, nameArgs);
    ++i;
    s.parseNameWithArgs("value", line, ",;\n", i, value, valueArgs);

    // Skip old protocol name
    if (name == "protocol")
      continue;

    bool handled = true;

    if (name == "address") {
      try {
        m_synergyAddress = NetworkAddress(value, kDefaultPort);
        m_synergyAddress.resolve();
      } catch (SocketAddressException &e) {
        throw ServerConfigReadException(s, std::string("invalid address argument ") + e.what());
      }
    } else if (name == "heartbeat") {
      addOption("", kOptionHeartbeat, s.parseInt(value));
    } else if (name == "switchCorners") {
      addOption("", kOptionScreenSwitchCorners, s.parseCorners(value));
    } else if (name == "switchCornerSize") {
      addOption("", kOptionScreenSwitchCornerSize, s.parseInt(value));
    } else if (name == "switchDelay") {
      addOption("", kOptionScreenSwitchDelay, s.parseInt(value));
    } else if (name == "switchDoubleTap") {
      addOption("", kOptionScreenSwitchTwoTap, s.parseInt(value));
    } else if (name == "switchNeedsShift") {
      addOption("", kOptionScreenSwitchNeedsShift, s.parseBoolean(value));
    } else if (name == "switchNeedsControl") {
      addOption("", kOptionScreenSwitchNeedsControl, s.parseBoolean(value));
    } else if (name == "switchNeedsAlt") {
      addOption("", kOptionScreenSwitchNeedsAlt, s.parseBoolean(value));
    } else if (name == "relativeMouseMoves") {
      addOption("", kOptionRelativeMouseMoves, s.parseBoolean(value));
    } else if (name == "win32KeepForeground") {
      addOption("", kOptionWin32KeepForeground, s.parseBoolean(value));
    } else if (name == "defaultLockToScreenState") {
      addOption("", kOptionDefaultLockToScreenState, s.parseBoolean(value));
    } else if (name == "disableLockToScreen") {
      addOption("", kOptionDisableLockToScreen, s.parseBoolean(value));
    } else if (name == "clipboardSharing") {
      addOption("", kOptionClipboardSharing, s.parseBoolean(value));
    } else if (name == "clipboardSharingSize") {
      addOption("", kOptionClipboardSharingSize, s.parseInt(value));
    } else {
      handled = false;
    }

    if (handled) {
      // make sure handled options aren't followed by more values
      if (i < line.size() && (line[i] == ',' || line[i] == ';')) {
        throw ServerConfigReadException(s, std::string("too many arguments for: ").append(name));
      }
    } else {
      // make filter rule
      InputFilter::Rule rule(parseCondition(s, name, nameArgs));

      // save first action (if any)
      if (!value.empty() || line[i] != ';') {
        parseAction(s, value, valueArgs, rule, true);
      }

      // get remaining activate actions
      while (i < line.length() && line[i] != ';') {
        ++i;
        s.parseNameWithArgs("value", line, ",;\n", i, value, valueArgs);
        parseAction(s, value, valueArgs, rule, true);
      }

      // get deactivate actions
      if (i < line.length() && line[i] == ';') {
        // allow trailing ';'
        i = line.find_first_not_of(" \t", i + 1);
        if (i == std::string::npos) {
          i = line.length();
        } else {
          --i;
        }

        // get actions
        while (i < line.length()) {
          ++i;
          s.parseNameWithArgs("value", line, ",\n", i, value, valueArgs);
          parseAction(s, value, valueArgs, rule, false);
        }
      }

      // add rule
      m_inputFilter.addFilterRule(rule);
    }
  }
  throw ServerConfigReadException(s, "unexpected end of options section");
}

void Config::readSectionScreens(ConfigReadContext &s)
{
  std::string line;
  std::string screen;
  while (s.readLine(line)) {
    // check for end of section
    if (line == "end") {
      return;
    }

    // see if it's the next screen
    if (line[line.size() - 1] == ':') {
      // strip :
      screen = line.substr(0, line.size() - 1);

      // verify validity of screen name
      if (!isValidScreenName(screen)) {
        throw ServerConfigReadException(s, "invalid screen name \"%{1}\"", screen);
      }

      // add the screen to the configuration
      if (!addScreen(screen)) {
        throw ServerConfigReadException(s, "duplicate screen name \"%{1}\"", screen);
      }
    } else if (screen.empty()) {
      throw ServerConfigReadException(s, "argument before first screen");
    } else {
      // parse argument:  `<name>=<value>'
      std::string::size_type i = line.find_first_of(" \t=");
      if (i == 0) {
        throw ServerConfigReadException(s, "missing argument name");
      }
      if (i == std::string::npos) {
        throw ServerConfigReadException(s, "missing =");
      }
      std::string name = line.substr(0, i);
      i = line.find_first_not_of(" \t", i);
      if (i == std::string::npos || line[i] != '=') {
        throw ServerConfigReadException(s, "missing =");
      }
      i = line.find_first_not_of(" \t", i + 1);
      std::string value;
      if (i != std::string::npos) {
        value = line.substr(i);
      }

      // handle argument
      if (name == "halfDuplexCapsLock") {
        addOption(screen, kOptionHalfDuplexCapsLock, s.parseBoolean(value));
      } else if (name == "halfDuplexNumLock") {
        addOption(screen, kOptionHalfDuplexNumLock, s.parseBoolean(value));
      } else if (name == "halfDuplexScrollLock") {
        addOption(screen, kOptionHalfDuplexScrollLock, s.parseBoolean(value));
      } else if (name == "shift") {
        addOption(screen, kOptionModifierMapForShift, s.parseModifierKey(value));
      } else if (name == "ctrl") {
        addOption(screen, kOptionModifierMapForControl, s.parseModifierKey(value));
      } else if (name == "alt") {
        addOption(screen, kOptionModifierMapForAlt, s.parseModifierKey(value));
      } else if (name == "altgr") {
        addOption(screen, kOptionModifierMapForAltGr, s.parseModifierKey(value));
      } else if (name == "meta") {
        addOption(screen, kOptionModifierMapForMeta, s.parseModifierKey(value));
      } else if (name == "super") {
        addOption(screen, kOptionModifierMapForSuper, s.parseModifierKey(value));
      } else if (name == "xtestIsXineramaUnaware") {
        addOption(screen, kOptionXTestXineramaUnaware, s.parseBoolean(value));
      } else if (name == "switchCorners") {
        addOption(screen, kOptionScreenSwitchCorners, s.parseCorners(value));
      } else if (name == "switchCornerSize") {
        addOption(screen, kOptionScreenSwitchCornerSize, s.parseInt(value));
      } else if (name == "preserveFocus") {
        addOption(screen, kOptionScreenPreserveFocus, s.parseBoolean(value));
      } else {
        // unknown argument
        throw ServerConfigReadException(s, "unknown argument \"%{1}\"", name);
      }
    }
  }
  throw ServerConfigReadException(s, "unexpected end of screens section");
}

void Config::readSectionLinks(ConfigReadContext &s)
{
  std::string line;
  std::string screen;
  while (s.readLine(line)) {
    // check for end of section
    if (line == "end") {
      return;
    }

    // see if it's the next screen
    if (line[line.size() - 1] == ':') {
      // strip :
      screen = line.substr(0, line.size() - 1);

      // verify we know about the screen
      if (!isScreen(screen)) {
        throw ServerConfigReadException(s, "unknown screen name \"%{1}\"", screen);
      }
      if (!isCanonicalName(screen)) {
        throw ServerConfigReadException(s, "cannot use screen name alias here");
      }
    } else if (screen.empty()) {
      throw ServerConfigReadException(s, "argument before first screen");
    } else {
      // parse argument:  `<name>[(<s0>,<e0>)]=<value>[(<s1>,<e1>)]'
      // the stuff in brackets is optional.  interval values must be
      // in the range [0,100] and start < end.  if not given the
      // interval is taken to be (0,100).
      std::string::size_type i = 0;
      std::string side;
      std::string dstScreen;
      std::string srcArgString;
      std::string dstArgString;
      ConfigReadContext::ArgList srcArgs;
      ConfigReadContext::ArgList dstArgs;
      s.parseNameWithArgs("link", line, "=", i, side, srcArgs);
      ++i;
      s.parseNameWithArgs("screen", line, "", i, dstScreen, dstArgs);
      Interval srcInterval(s.parseInterval(srcArgs));
      Interval dstInterval(s.parseInterval(dstArgs));

      // handle argument
      using enum Direction;
      Direction dir;
      if (side == "left") {
        dir = Left;
      } else if (side == "right") {
        dir = Right;
      } else if (side == "up") {
        dir = Top;
      } else if (side == "down") {
        dir = Bottom;
      } else {
        // unknown argument
        throw ServerConfigReadException(s, "unknown side \"%{1}\" in link", side);
      }
      if (!isScreen(dstScreen)) {
        throw ServerConfigReadException(s, "unknown screen name \"%{1}\"", dstScreen);
      }
      if (!connect(
              screen, dir, srcInterval.first, srcInterval.second, dstScreen, dstInterval.first, dstInterval.second
          )) {
        throw ServerConfigReadException(s, "overlapping range");
      }
    }
  }
  throw ServerConfigReadException(s, "unexpected end of links section");
}

void Config::readSectionAliases(ConfigReadContext &s)
{
  std::string line;
  std::string screen;
  while (s.readLine(line)) {
    // check for end of section
    if (line == "end") {
      return;
    }

    // see if it's the next screen
    if (line[line.size() - 1] == ':') {
      // strip :
      screen = line.substr(0, line.size() - 1);

      // verify we know about the screen
      if (!isScreen(screen)) {
        throw ServerConfigReadException(s, "unknown screen name \"%{1}\"", screen);
      }
      if (!isCanonicalName(screen)) {
        throw ServerConfigReadException(s, "cannot use screen name alias here");
      }
    } else if (screen.empty()) {
      throw ServerConfigReadException(s, "argument before first screen");
    } else {
      // verify validity of screen name
      if (!isValidScreenName(line)) {
        throw ServerConfigReadException(s, "invalid screen alias \"%{1}\"", line);
      }

      // add alias
      if (!addAlias(screen, line)) {
        throw ServerConfigReadException(s, "alias \"%{1}\" is already used", line);
      }
    }
  }
  throw ServerConfigReadException(s, "unexpected end of aliases section");
}

} // namespace synergy::server
