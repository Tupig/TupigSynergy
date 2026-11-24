/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "NetworkInterfaces.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace synergy::network {

namespace {

// QHostAddress::isPrivateUse() arrived in Qt 6.6; on older Qt replicate its ranges
// (RFC 1918 private IPv4 + ULA fc00::/7) via isInSubnet().
bool isPrivateUse(const QHostAddress &address)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  return address.isPrivateUse();
#else
  return address.isInSubnet(QHostAddress(QStringLiteral("10.0.0.0")), 8) ||
         address.isInSubnet(QHostAddress(QStringLiteral("172.16.0.0")), 12) ||
         address.isInSubnet(QHostAddress(QStringLiteral("192.168.0.0")), 16) ||
         address.isInSubnet(QHostAddress(QStringLiteral("fc00::")), 7);
#endif
}

} // namespace

bool isVirtualInterface(const QString &interfaceName)
{
  // Common virtual network interface patterns
  static const auto virtualRegEx = QRegularExpression(
      QStringLiteral("^vboxnet|vmnet|docker|virbr|veth|br\\-|tun|utun|awdl|p2p|llw|anpi|tap|vEth"),
      QRegularExpression::CaseInsensitiveOption
  );
  return virtualRegEx.match(interfaceName).hasMatch();
}

QStringList availableAddresses()
{
  QList<QHostAddress> physicalIP4;
  QList<QHostAddress> physicalIP6;
  QList<QHostAddress> virtualIP4;
  QList<QHostAddress> virtualIP6;
  QSet<QHostAddress> uniqueAddresses;

  const auto allInterfaces = QNetworkInterface::allInterfaces();
  for (const auto &interface : allInterfaces) {
    if (!(interface.flags() & QNetworkInterface::IsUp) || !(interface.flags() & QNetworkInterface::IsRunning) ||
        (interface.flags() & QNetworkInterface::IsLoopBack)) {
      continue;
    }

    const bool isP2P = (interface.flags() & QNetworkInterface::IsPointToPoint);
    const bool isVirtualType = interface.type() == QNetworkInterface::Virtual;
    const bool isVirtual = isVirtualInterface(interface.humanReadableName()) || isP2P || isVirtualType;
    const auto addressEntries = interface.addressEntries();

    for (const auto &entry : addressEntries) {
      const QHostAddress address = entry.ip();

      if (address.isNull() || address.isLinkLocal() || address.isLoopback() || address.isMulticast() ||
          address == QHostAddress::Any || address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6 ||
          uniqueAddresses.contains(address)) {
        continue;
      }

      uniqueAddresses.insert(address);

      if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        if (isVirtual)
          virtualIP6.append(address);
        else
          physicalIP6.append(address);
      } else {
        if (isVirtual)
          virtualIP4.append(address);
        else
          physicalIP4.append(address);
      }
    }
  }

  std::sort(physicalIP4.begin(), physicalIP4.end(), [](const QHostAddress &a, const QHostAddress &b) {
    if (isPrivateUse(a) != isPrivateUse(b))
      return isPrivateUse(a);
    return a.toIPv4Address() < b.toIPv4Address();
  });

  std::sort(virtualIP4.begin(), virtualIP4.end(), [](const QHostAddress &a, const QHostAddress &b) {
    return a.toIPv4Address() < b.toIPv4Address();
  });

  std::sort(physicalIP6.begin(), physicalIP6.end(), [](const QHostAddress &a, const QHostAddress &b) {
    if (isPrivateUse(a) != isPrivateUse(b))
      return isPrivateUse(a);
    return a.toString() < b.toString();
  });

  std::sort(virtualIP6.begin(), virtualIP6.end(), [](const QHostAddress &a, const QHostAddress &b) {
    return a.toString() < b.toString();
  });

  auto result = physicalIP4;
  result.append(virtualIP4);
  result.append(physicalIP6);
  result.append(virtualIP6);

  QStringList ipList;
  for (const auto &host : result) {
    ipList.append(host.toString());
  }
  return ipList;
}

QString selectServerBindAddress(const QString &configured, const QStringList &available)
{
  if (!configured.isEmpty())
    return configured;
  return available.isEmpty() ? QHostAddress(QHostAddress::LocalHost).toString() : available.first();
}

} // namespace synergy::network
