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
const char *Config::getOptionName(OptionID id)
{
  if (id == kOptionHalfDuplexCapsLock) {
    return "halfDuplexCapsLock";
  }
  if (id == kOptionHalfDuplexNumLock) {
    return "halfDuplexNumLock";
  }
  if (id == kOptionHalfDuplexScrollLock) {
    return "halfDuplexScrollLock";
  }
  if (id == kOptionModifierMapForShift) {
    return "shift";
  }
  if (id == kOptionModifierMapForControl) {
    return "ctrl";
  }
  if (id == kOptionModifierMapForAlt) {
    return "alt";
  }
  if (id == kOptionModifierMapForAltGr) {
    return "altgr";
  }
  if (id == kOptionModifierMapForMeta) {
    return "meta";
  }
  if (id == kOptionModifierMapForSuper) {
    return "super";
  }
  if (id == kOptionHeartbeat) {
    return "heartbeat";
  }
  if (id == kOptionScreenSwitchCorners) {
    return "switchCorners";
  }
  if (id == kOptionScreenSwitchCornerSize) {
    return "switchCornerSize";
  }
  if (id == kOptionScreenSwitchDelay) {
    return "switchDelay";
  }
  if (id == kOptionScreenSwitchTwoTap) {
    return "switchDoubleTap";
  }
  if (id == kOptionScreenSwitchNeedsShift) {
    return "switchNeedsShift";
  }
  if (id == kOptionScreenSwitchNeedsControl) {
    return "switchNeedsControl";
  }
  if (id == kOptionScreenSwitchNeedsAlt) {
    return "switchNeedsAlt";
  }
  if (id == kOptionXTestXineramaUnaware) {
    return "xtestIsXineramaUnaware";
  }
  if (id == kOptionRelativeMouseMoves) {
    return "relativeMouseMoves";
  }
  if (id == kOptionWin32KeepForeground) {
    return "win32KeepForeground";
  }
  if (id == kOptionScreenPreserveFocus) {
    return "preserveFocus";
  }
  if (id == kOptionDefaultLockToScreenState) {
    return "defaultLockToScreenState";
  }
  if (id == kOptionDisableLockToScreen) {
    return "disableLockToScreen";
  }
  if (id == kOptionClipboardSharing) {
    return "clipboardSharing";
  }
  if (id == kOptionClipboardSharingSize) {
    return "clipboardSharingSize";
  }
  return nullptr;
}

std::string Config::getOptionValue(OptionID id, OptionValue value)
{
  if (id == kOptionHalfDuplexCapsLock || id == kOptionHalfDuplexNumLock || id == kOptionHalfDuplexScrollLock ||
      id == kOptionScreenSwitchNeedsShift || id == kOptionScreenSwitchNeedsControl ||
      id == kOptionScreenSwitchNeedsAlt || id == kOptionXTestXineramaUnaware || id == kOptionRelativeMouseMoves ||
      id == kOptionWin32KeepForeground || id == kOptionScreenPreserveFocus || id == kOptionClipboardSharing ||
      id == kOptionClipboardSharingSize) {
    return (value != 0) ? "true" : "false";
  }
  if (id == kOptionModifierMapForShift || id == kOptionModifierMapForControl || id == kOptionModifierMapForAlt ||
      id == kOptionModifierMapForAltGr || id == kOptionModifierMapForMeta || id == kOptionModifierMapForSuper) {
    switch (value) {
    case kKeyModifierIDShift:
      return "shift";

    case kKeyModifierIDControl:
      return "ctrl";

    case kKeyModifierIDAlt:
      return "alt";

    case kKeyModifierIDAltGr:
      return "altgr";

    case kKeyModifierIDMeta:
      return "meta";

    case kKeyModifierIDSuper:
      return "super";

    default:
      return "none";
    }
  }
  if (id == kOptionHeartbeat || id == kOptionScreenSwitchCornerSize || id == kOptionScreenSwitchDelay ||
      id == kOptionScreenSwitchTwoTap) {
    return synergy::string::sprintf("%d", value);
  }
  if (id == kOptionScreenSwitchCorners) {
    std::string result("none");
    if ((value & s_topLeftCornerMask) != 0) {
      result += " +top-left";
    }
    if ((value & s_topRightCornerMask) != 0) {
      result += " +top-right";
    }
    if ((value & s_bottomLeftCornerMask) != 0) {
      result += " +bottom-left";
    }
    if ((value & s_bottomRightCornerMask) != 0) {
      result += " +bottom-right";
    }
    return result;
  }
  return "";
}

//
// Config I/O
//

std::istream &operator>>(std::istream &s, Config &config)
{
  ConfigReadContext context(s);
  config.read(context);
  return s;
}

std::ostream &operator<<(std::ostream &s, const Config &config)
{
  // screens section
  s << "section: screens" << std::endl;
  for (const auto &screen : config) {
    s << "\t" << screen.c_str() << ":" << std::endl;
    const auto options = config.getOptions(screen);
    if (options != nullptr && options->size() > 0) {
      for (auto [optionId, optionValue] : *options) {
        const char *name = Config::getOptionName(optionId);
        std::string value = Config::getOptionValue(optionId, optionValue);
        if (name != nullptr && !value.empty()) {
          s << "\t\t" << name << " = " << value << std::endl;
        }
      }
    }
  }
  s << "end" << std::endl;

  // links section
  std::string neighbor;
  s << "section: links" << std::endl;
  for (const auto &screen : config) {
    s << "\t" << screen.c_str() << ":" << std::endl;

    for (Config::link_const_iterator link = config.beginNeighbor(screen), nend = config.endNeighbor(screen);
         link != nend; ++link) {
      s << "\t\t" << Config::dirName(link->first.getSide()) << Config::formatInterval(link->first.getInterval())
        << " = " << link->second.getName().c_str() << Config::formatInterval(link->second.getInterval()) << std::endl;
    }
  }
  s << "end" << std::endl;

  // aliases section (if there are any)
  if (config.m_map.size() != config.m_nameToCanonicalName.size()) {
    // map canonical to alias
    using CMNameMap = std::multimap<std::string, std::string, CaselessCmp>;
    CMNameMap aliases;
    for (auto index = config.m_nameToCanonicalName.begin(); index != config.m_nameToCanonicalName.end(); ++index) {
      if (index->first != index->second) {
        aliases.insert(std::make_pair(index->second, index->first));
      }
    }

    // dump it
    std::string screen;
    s << "section: aliases" << std::endl;
    for (CMNameMap::const_iterator index = aliases.begin(); index != aliases.end(); ++index) {
      if (index->first != screen) {
        screen = index->first;
        s << "\t" << screen.c_str() << ":" << std::endl;
      }
      s << "\t\t" << index->second.c_str() << std::endl;
    }
    s << "end" << std::endl;
  }

  // options section
  s << "section: options" << std::endl;
  if (const Config::ScreenOptions *options = config.getOptions(""); options && options->size() > 0) {
    for (auto [optionId, optionValue] : *options) {
      const char *name = Config::getOptionName(optionId);
      std::string value = Config::getOptionValue(optionId, optionValue);
      if (name != nullptr && !value.empty()) {
        s << "\t" << name << " = " << value << std::endl;
      }
    }
  }
  if (config.m_synergyAddress.isValid()) {
    s << "\taddress = " << config.m_synergyAddress.getHostname().c_str() << std::endl;
  }
  s << config.m_inputFilter.format("\t");
  s << "end" << std::endl;

  return s;
}

} // namespace synergy::server
