/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "../client/ProtocolTestSupport.h"
#include "Screen.h"
#include "server/ClientProxy1_0.h"
#include "server/Config.h"
#include "server/PrimaryClient.h"
#include "server/Server.h"

namespace protocol_test {

class ServerFixture
{
public:
  PrivateSettings settings;
  EventQueue events;
  PlatformScreen *platform = new PlatformScreen(&events);
  synergy::Screen screen{platform, &events};
  PrimaryClient primary{"primary", &screen};
  synergy::server::Config config{&events};
  std::unique_ptr<Server> server;

  explicit ServerFixture(bool connectSecondary = true, Direction direction = Direction::Right)
  {
    config.addScreen("primary");
    config.addScreen("secondary");
    config.connect("primary", direction, 0.0f, 1.0f, "secondary", 0.0f, 1.0f);
    const auto opposite = direction == Direction::Left    ? Direction::Right
                          : direction == Direction::Right ? Direction::Left
                          : direction == Direction::Top   ? Direction::Bottom
                                                          : Direction::Top;
    config.connect("secondary", opposite, 0.0f, 1.0f, "primary", 0.0f, 1.0f);
    server =
        std::make_unique<Server>(config, &primary, &screen, &events, NetworkProtocol::Synergy, FileTransferSettings{});
    if (connectSecondary) {
      secondaryStream = new Stream;
      auto *proxy = new ClientProxy1_0("secondary", secondaryStream, &events);
      secondaryStream->push("DINF" + integers16({0, 0, 800, 600, 0, 400, 300}));
      events.dispatchEvent(Event(EventTypes::StreamInputReady, secondaryStream));
      events.drain();
      server->adoptClient(proxy);
      events.drain();
      secondaryStream->written.clear();
    }
  }

  void movePrimary(int32_t x, int32_t y)
  {
    IPrimaryScreen::MotionInfo info{x, y};
    events.dispatchEvent(Event(
        EventTypes::PrimaryScreenMotionOnPrimary, primary.getEventTarget(), &info, Event::EventFlags::DontFreeData
    ));
  }
  void switchTo(const std::string &name)
  {
    Server::SwitchToScreenInfo info(name);
    events.dispatchEvent(Event(
        EventTypes::ServerSwitchToScreen, config.getInputFilter(), static_cast<void *>(&info),
        Event::EventFlags::DontFreeData
    ));
  }
  Stream *secondaryStream = nullptr; // owned by the Server's adopted proxy
};

} // namespace protocol_test
