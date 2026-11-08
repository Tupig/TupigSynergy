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

InputFilter::Condition *
Config::parseCondition(const ConfigReadContext &s, const std::string &name, const std::vector<std::string> &args)
{
  if (name == "keystroke") {
    if (args.size() != 1) {
      throw ServerConfigReadException(s, "syntax for condition: keystroke(modifiers+key)");
    }

    IPlatformScreen::KeyInfo *keyInfo = s.parseKeystroke(args[0]);

    return new InputFilter::KeystrokeCondition(m_events, keyInfo);
  }

  if (name == "mousebutton") {
    if (args.size() != 1) {
      throw ServerConfigReadException(s, "syntax for condition: mousebutton(modifiers+button)");
    }

    auto mouseInfo = s.parseMouse(args[0]);

    return new InputFilter::MouseButtonCondition(m_events, mouseInfo);
  }

  if (name == "connect") {
    if (args.size() != 1) {
      throw ServerConfigReadException(s, "syntax for condition: connect([screen])");
    }

    std::string screen = args[0];
    if (isScreen(screen)) {
      screen = getCanonicalName(screen);
    } else if (!screen.empty()) {
      throw ServerConfigReadException(s, "unknown screen name \"%{1}\" in connect", screen);
    }

    return new InputFilter::ScreenConnectedCondition(m_events, screen);
  }

  throw ServerConfigReadException(s, "unknown argument \"%{1}\"", name);
}

void Config::parseAction(
    ConfigReadContext &s, const std::string &name, const std::vector<std::string> &args, InputFilter::Rule &rule,
    bool activate
)
{
  InputFilter::Action *action;

  if (name == "keystroke" || name == "keyDown" || name == "keyUp") {
    if (args.size() < 1 || args.size() > 2) {
      throw ServerConfigReadException(s, "syntax for action: keystroke(modifiers+key[,screens])");
    }

    IPlatformScreen::KeyInfo *keyInfo;
    if (args.size() == 1) {
      keyInfo = s.parseKeystroke(args[0]);
    } else {
      std::set<std::string> screens;
      parseScreens(s, args[1], screens);
      keyInfo = s.parseKeystroke(args[0], screens);
    }

    if (name == "keystroke") {
      IPlatformScreen::KeyInfo *keyInfo2 = IKeyState::KeyInfo::alloc(*keyInfo);
      action = new InputFilter::KeystrokeAction(m_events, keyInfo2, true);
      rule.adoptAction(action, true);
      action = new InputFilter::KeystrokeAction(m_events, keyInfo, false);
      activate = false;
    } else if (name == "keyDown") {
      action = new InputFilter::KeystrokeAction(m_events, keyInfo, true);
    } else {
      action = new InputFilter::KeystrokeAction(m_events, keyInfo, false);
    }
  }

  else if (name == "mousebutton" || name == "mouseDown" || name == "mouseUp") {
    if (args.size() != 1) {
      throw ServerConfigReadException(s, "syntax for action: mousebutton(modifiers+button)");
    }

    auto mouseInfo = s.parseMouse(args[0]);

    if (name == "mousebutton") {
      action = new InputFilter::MouseButtonAction(m_events, mouseInfo, true);
      rule.adoptAction(action, true);
      action = new InputFilter::MouseButtonAction(m_events, mouseInfo, false);
      activate = false;
    } else if (name == "mouseDown") {
      action = new InputFilter::MouseButtonAction(m_events, mouseInfo, true);
    } else {
      action = new InputFilter::MouseButtonAction(m_events, mouseInfo, false);
    }
  }

  /* XXX -- not supported
          else if (name == "modifier") {
                  if (args.size() != 1) {
                          throw ServerConfigReadException(s, "syntax for action:
     modifier(modifiers)");
                  }

                  KeyModifierMask mask = s.parseModifier(args[0]);

                  action = new InputFilter::ModifierAction(mask, ~mask);
          }
  */

  else if (name == "switchToScreen") {
    if (args.size() != 1) {
      throw ServerConfigReadException(s, "syntax for action: switchToScreen(name)");
    }

    std::string screen = args[0];
    if (isScreen(screen)) {
      screen = getCanonicalName(screen);
    } else if (!screen.empty()) {
      throw ServerConfigReadException(s, "unknown screen name in switchToScreen");
    }

    action = new InputFilter::SwitchToScreenAction(m_events, screen);
  }

  else if (name == "switchInDirection") {
    if (args.size() != 1) {
      throw ServerConfigReadException(s, "syntax for action: switchInDirection(<left|right|up|down>)");
    }

    Direction direction;
    using enum Direction;
    if (args[0] == "left") {
      direction = Left;
    } else if (args[0] == "right") {
      direction = Right;
    } else if (args[0] == "up") {
      direction = Top;
    } else if (args[0] == "down") {
      direction = Bottom;
    } else {
      throw ServerConfigReadException(s, "unknown direction \"%{1}\" in switchToScreen", args[0]);
    }

    action = new InputFilter::SwitchInDirectionAction(m_events, direction);
  }

  else if (name == "switchToNextScreen") {
    if (!args.empty()) {
      throw ServerConfigReadException(s, "syntax for action: switchToNextScreen");
    }

    action = new InputFilter::SwitchToNextScreenAction(m_events);
  }

  else if (name == "lockCursorToScreen") {
    if (args.size() > 1) {
      throw ServerConfigReadException(s, "syntax for action: lockCursorToScreen([{off|on|toggle}])");
    }

    InputFilter::LockCursorToScreenAction::Mode mode = InputFilter::LockCursorToScreenAction::kToggle;
    if (args.size() == 1) {
      if (args[0] == "off") {
        mode = InputFilter::LockCursorToScreenAction::kOff;
      } else if (args[0] == "on") {
        mode = InputFilter::LockCursorToScreenAction::kOn;
      } else if (args[0] == "toggle") {
        mode = InputFilter::LockCursorToScreenAction::kToggle;
      } else {
        throw ServerConfigReadException(s, "syntax for action: lockCursorToScreen([{off|on|toggle}])");
      }
    }

    if (mode != InputFilter::LockCursorToScreenAction::kOff) {
      m_hasLockToScreenAction = true;
    }

    action = new InputFilter::LockCursorToScreenAction(m_events, mode);
  }

  else if (name == "restartServer") {
    if (args.size() > 1) {
      throw ServerConfigReadException(s, "syntax for action: restartServer([{{restart}}])");
    }

    InputFilter::RestartServer::Mode mode = InputFilter::RestartServer::restart;

    if (args.size() == 1) {
      if (args[0] == "restart") {
        mode = InputFilter::RestartServer::restart;
      } else {
        throw ServerConfigReadException(s, "syntax for action: restartServer([{restart}])");
      }
    }

    action = new InputFilter::RestartServer(mode);
  }

  else if (name == "keyboardBroadcast") {
    if (args.size() > 2) {
      throw ServerConfigReadException(s, "syntax for action: keyboardBroadcast([{off|on|toggle}[,screens]])");
    }

    InputFilter::KeyboardBroadcastAction::Mode mode = InputFilter::KeyboardBroadcastAction::kToggle;
    if (args.size() >= 1) {
      if (args[0] == "off") {
        mode = InputFilter::KeyboardBroadcastAction::kOff;
      } else if (args[0] == "on") {
        mode = InputFilter::KeyboardBroadcastAction::kOn;
      } else if (args[0] == "toggle") {
        mode = InputFilter::KeyboardBroadcastAction::kToggle;
      } else {
        throw ServerConfigReadException(
            s, "syntax for action: "
               "keyboardBroadcast([{off|on|toggle}[,screens]])"
        );
      }
    }

    std::set<std::string> screens;
    if (args.size() >= 2) {
      parseScreens(s, args[1], screens);
    }

    action = new InputFilter::KeyboardBroadcastAction(m_events, mode, screens);
  }

  else {
    throw ServerConfigReadException(s, "unknown action argument \"%{1}\"", name);
  }

  rule.adoptAction(action, activate);
}

void Config::parseScreens(const ConfigReadContext &c, const std::string_view &s, std::set<std::string> &screens) const
{
  screens.clear();

  std::string::size_type i = 0;
  while (i < s.size()) {
    // find end of next screen name
    std::string::size_type j = s.find(':', i);
    if (j == std::string::npos) {
      j = s.size();
    }

    // extract name
    std::string rawName;
    i = s.find_first_not_of(" \t", i);
    if (i < j) {
      rawName = s.substr(i, s.find_last_not_of(" \t", j - 1) - i + 1);
    }

    // add name
    if (rawName == "*") {
      screens.insert("*");
    } else if (!rawName.empty()) {
      std::string name = getCanonicalName(rawName);
      if (name.empty()) {
        throw ServerConfigReadException(c, "unknown screen name \"%{1}\"", rawName);
      }
      screens.insert(name);
    }

    // next
    i = j + 1;
  }
}

} // namespace synergy::server
