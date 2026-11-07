/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerTests.h"

#include "ServerTestFixture.h"
#include "ipc/CoreIpcServer.h"
#include "server/Server.h"

void ServerTests::SwitchToScreenInfo_alloc_screen()
{
  auto actual = new Server::SwitchToScreenInfo("test");
  QCOMPARE(actual->m_screen, "test");
  delete actual;
}

void ServerTests::KeyboardBroadcastInfo_alloc_stateAndSceens()
{
  auto info = new Server::KeyboardBroadcastInfo(Server::KeyboardBroadcastInfo::State::kOn, "test");
  QCOMPARE(info->m_state, Server::KeyboardBroadcastInfo::State::kOn);
  QCOMPARE(info->m_screens, "test");
  delete info;
}

void ServerTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Error);
#ifdef Q_OS_WIN
  m_arch.init();
#endif
  static synergy::core::ipc::CoreIpcServer ipc(nullptr);
}

void ServerTests::motionInsideScreenDoesNotSwitch()
{
  protocol_test::ServerFixture fixture;
  QCOMPARE(fixture.server->getNumClients(), 2u);
  fixture.movePrimary(798, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.secondaryStream->written.empty());
}

void ServerTests::crossingConnectedEdgeEntersSecondary_data()
{
  QTest::addColumn<int>("direction");
  QTest::addColumn<int>("x");
  QTest::addColumn<int>("y");
  QTest::addColumn<int>("expectedX");
  QTest::addColumn<int>("expectedY");
  QTest::newRow("right") << static_cast<int>(Direction::Right) << 799 << 300 << 0 << 300;
  QTest::newRow("left") << static_cast<int>(Direction::Left) << 0 << 300 << 799 << 300;
  QTest::newRow("top") << static_cast<int>(Direction::Top) << 400 << 0 << 400 << 599;
  QTest::newRow("bottom") << static_cast<int>(Direction::Bottom) << 400 << 599 << 400 << 0;
}

void ServerTests::crossingConnectedEdgeEntersSecondary()
{
  QFETCH(int, direction);
  QFETCH(int, x);
  QFETCH(int, y);
  QFETCH(int, expectedX);
  QFETCH(int, expectedY);
  protocol_test::ServerFixture fixture(true, static_cast<Direction>(direction));
  fixture.movePrimary(x, y);
  QCOMPARE(fixture.platform->leaves, 1);
  QVERIFY(!fixture.screen.isOnScreen());
  const auto &output = fixture.secondaryStream->written;
  QVERIFY(output.starts_with("CINN"));
  QCOMPARE(output.substr(4, 4), protocol_test::integers16({expectedX, expectedY}));
}

void ServerTests::crossingMissingNeighborDoesNotSwitch()
{
  protocol_test::ServerFixture fixture(false);
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.screen.isOnScreen());
}

void ServerTests::deniedLeaveDoesNotEnterSecondary()
{
  protocol_test::ServerFixture fixture;
  fixture.platform->allowLeave = false;
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.screen.isOnScreen());
  QVERIFY(fixture.secondaryStream->written.empty());
}

void ServerTests::pressedMousePreventsEdgeSwitch()
{
  protocol_test::ServerFixture fixture;
  fixture.platform->mousePressed = true;
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.secondaryStream->written.empty());
}

void ServerTests::explicitSwitchReturnsToPrimary()
{
  protocol_test::ServerFixture fixture;
  fixture.switchTo("secondary");
  QCOMPARE(fixture.platform->leaves, 1);
  QVERIFY(fixture.secondaryStream->written.starts_with("CINN"));
  fixture.secondaryStream->written.clear();
  fixture.switchTo("primary");
  QCOMPARE(fixture.platform->enters, 1);
  QVERIFY(fixture.screen.isOnScreen());
  QVERIFY(fixture.secondaryStream->written.starts_with("COUT"));
}

void ServerTests::unknownTargetDoesNotSwitch()
{
  protocol_test::ServerFixture fixture;
  fixture.switchTo("missing");
  QCOMPARE(fixture.platform->leaves, 0);
  QVERIFY(fixture.secondaryStream->written.empty());
}

void ServerTests::configurationWithoutPrimaryIsRejected()
{
  protocol_test::ServerFixture fixture;
  synergy::server::Config invalid(&fixture.events);
  invalid.addScreen("secondary");
  QVERIFY(!fixture.server->setConfig(invalid));
  QCOMPARE(fixture.server->getNumClients(), 2u);
  fixture.movePrimary(799, 300);
  QCOMPARE(fixture.platform->leaves, 1);
}

QTEST_MAIN(ServerTests)
