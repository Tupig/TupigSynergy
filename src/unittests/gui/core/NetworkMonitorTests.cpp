/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "NetworkMonitorTests.h"

#include "common/NetworkInterfaces.h"
#include "gui/core/NetworkMonitor.h"

#include <QHostAddress>
#include <QSignalSpy>

using namespace synergy::gui;

void NetworkMonitorTests::testVirtualInterface()
{
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("vboxnet0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("vboXnet0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("vmnet0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("docker-bridge")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("virbr0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("veth0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("br-0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("tun0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("utun0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("awdl0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("p2p0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("llw0")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("anpi5")));
  QVERIFY(NetworkMonitor::isVirtualInterface(QStringLiteral("tap1")));

  QVERIFY(!NetworkMonitor::isVirtualInterface(QStringLiteral("eth0")));
  QVERIFY(!NetworkMonitor::isVirtualInterface(QStringLiteral("enp0s0f")));
  QVERIFY(!NetworkMonitor::isVirtualInterface(QStringLiteral("wifi")));
  QVERIFY(!NetworkMonitor::isVirtualInterface(QStringLiteral("wlan0")));
}

void NetworkMonitorTests::emitsOnlyWhenAddressesChange()
{
  NetworkMonitor monitor;
  QSignalSpy spy(&monitor, &NetworkMonitor::ipAddressesChanged);
  QVERIFY(spy.isValid());

  monitor.setIpAddresses({QStringLiteral("10.0.0.1")});
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.last().at(0).toStringList(), QStringList{QStringLiteral("10.0.0.1")});

  // an identical list must not be re-emitted
  monitor.setIpAddresses({QStringLiteral("10.0.0.1")});
  QCOMPARE(spy.count(), 1);

  monitor.setIpAddresses({QStringLiteral("10.0.0.2")});
  QCOMPARE(spy.count(), 2);
  QCOMPARE(spy.last().at(0).toStringList(), QStringList{QStringLiteral("10.0.0.2")});
}

void NetworkMonitorTests::validAddressesExcludeLoopbackAndLinkLocal()
{
  const auto addresses = NetworkMonitor::validAddresses();
  for (const auto &entry : addresses) {
    QHostAddress address(entry);
    QVERIFY2(!address.isNull(), qPrintable(entry));
    QVERIFY(!address.isLoopback());
    QVERIFY(!address.isLinkLocal());
  }

  QVERIFY(!addresses.contains(QStringLiteral("127.0.0.1")));
  QVERIFY(!addresses.contains(QStringLiteral("::1")));
}

void NetworkMonitorTests::automaticServerBindingUsesTheCurrentCandidate()
{
  using synergy::network::selectServerBindAddress;
  const auto oldAddress = QStringLiteral("192.0.2.10");
  const auto newAddress = QStringLiteral("192.0.2.11");
  const auto ipv6Address = QStringLiteral("2001:db8::10");

  QCOMPARE(selectServerBindAddress({}, {oldAddress, ipv6Address}), oldAddress);
  QCOMPARE(selectServerBindAddress({}, {newAddress, ipv6Address}), newAddress);
  QCOMPARE(selectServerBindAddress({}, {ipv6Address}), ipv6Address);
  QCOMPARE(selectServerBindAddress({}, {}), QStringLiteral("127.0.0.1"));
}

void NetworkMonitorTests::explicitServerBindingsArePreserved()
{
  using synergy::network::selectServerBindAddress;
  const QStringList available{QStringLiteral("192.0.2.11")};
  for (const auto &address :
       {QStringLiteral("192.0.2.10"), QStringLiteral("2001:db8::10"), QStringLiteral("0.0.0.0"),
        QStringLiteral("::")}) {
    QCOMPARE(selectServerBindAddress(address, available), address);
    QCOMPARE(selectServerBindAddress(address, {}), address);
  }
}

QTEST_MAIN(NetworkMonitorTests)
