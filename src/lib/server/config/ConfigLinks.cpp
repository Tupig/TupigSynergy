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
// Config::Name
//

Config::Name::Name(Config *config, const std::string &name) : m_config(config), m_name(config->getCanonicalName(name))
{
  // do nothing
}

bool Config::Name::operator==(const std::string &name) const
{
  std::string canonical = m_config->getCanonicalName(name);
  return CaselessCmp::equal(canonical, m_name);
}

//
// Config::CellEdge
//

Config::CellEdge::CellEdge(Direction side, float position)
{
  init("", side, Interval(position, position));
}

Config::CellEdge::CellEdge(Direction side, const Interval &interval)
{
  assert(interval.first >= 0.0f);
  assert(interval.second <= 1.0f);
  assert(interval.first < interval.second);

  init("", side, interval);
}

Config::CellEdge::CellEdge(const std::string &name, Direction side, const Interval &interval)
{
  assert(interval.first >= 0.0f);
  assert(interval.second <= 1.0f);
  assert(interval.first < interval.second);

  init(name, side, interval);
}

void Config::CellEdge::init(const std::string_view &name, Direction side, const Interval &interval)
{
  assert(side != Direction::NoDirection);

  m_name = name;
  m_side = side;
  m_interval = interval;
}

Config::Interval Config::CellEdge::getInterval() const
{
  return m_interval;
}

void Config::CellEdge::setName(const std::string_view &newName)
{
  m_name = newName;
}

std::string Config::CellEdge::getName() const
{
  return m_name;
}

Direction Config::CellEdge::getSide() const
{
  return m_side;
}

bool Config::CellEdge::overlaps(const CellEdge &edge) const
{
  const Interval &x = m_interval;
  const Interval &y = edge.m_interval;
  if (m_side != edge.m_side) {
    return false;
  }
  return (x.first >= y.first && x.first < y.second) || (x.second > y.first && x.second <= y.second) ||
         (y.first >= x.first && y.first < x.second) || (y.second > x.first && y.second <= x.second);
}

bool Config::CellEdge::isInside(float x) const
{
  return (x >= m_interval.first && x < m_interval.second);
}

float Config::CellEdge::transform(float x) const
{
  return (x - m_interval.first) / (m_interval.second - m_interval.first);
}

float Config::CellEdge::inverseTransform(float x) const
{
  return x * (m_interval.second - m_interval.first) + m_interval.first;
}

bool Config::CellEdge::operator<(const CellEdge &o) const
{
  if (static_cast<int>(m_side) < static_cast<int>(o.m_side)) {
    return true;
  } else if (static_cast<int>(m_side) > static_cast<int>(o.m_side)) {
    return false;
  }

  return (m_interval.first < o.m_interval.first);
}

bool Config::CellEdge::operator==(const CellEdge &x) const
{
  return (m_side == x.m_side && m_interval == x.m_interval);
}

//
// Config::Cell
//

bool Config::Cell::add(const CellEdge &src, const CellEdge &dst)
{
  // cannot add an edge that overlaps other existing edges but we
  // can exactly replace an edge.
  if (!hasEdge(src) && overlaps(src)) {
    return false;
  }

  m_neighbors.erase(src);
  m_neighbors.insert(std::make_pair(src, dst));
  return true;
}

void Config::Cell::remove(Direction side)
{
  for (auto j = m_neighbors.begin(); j != m_neighbors.end();) {
    if (j->first.getSide() == side) {
      m_neighbors.erase(j++);
    } else {
      ++j;
    }
  }
}

void Config::Cell::remove(Direction side, float position)
{
  for (auto j = m_neighbors.begin(); j != m_neighbors.end(); ++j) {
    if (j->first.getSide() == side && j->first.isInside(position)) {
      m_neighbors.erase(j);
      break;
    }
  }
}
void Config::Cell::remove(const Name &name)
{
  for (auto j = m_neighbors.begin(); j != m_neighbors.end();) {
    if (name == j->second.getName()) {
      m_neighbors.erase(j++);
    } else {
      ++j;
    }
  }
}

void Config::Cell::rename(const Name &oldName, const std::string &newName)
{
  for (auto j = m_neighbors.begin(); j != m_neighbors.end(); ++j) {
    if (oldName == j->second.getName()) {
      j->second.setName(newName);
    }
  }
}

bool Config::Cell::hasEdge(const CellEdge &edge) const
{
  EdgeLinks::const_iterator i = m_neighbors.find(edge);
  return (i != m_neighbors.end() && i->first == edge);
}

bool Config::Cell::overlaps(const CellEdge &edge) const
{
  EdgeLinks::const_iterator i = m_neighbors.upper_bound(edge);
  if (i != m_neighbors.end() && i->first.overlaps(edge)) {
    return true;
  }
  if (i != m_neighbors.begin() && (--i)->first.overlaps(edge)) {
    return true;
  }
  return false;
}

bool Config::Cell::getLink(Direction side, float position, const CellEdge *&src, const CellEdge *&dst) const
{
  CellEdge edge(side, position);
  EdgeLinks::const_iterator i = m_neighbors.upper_bound(edge);
  if (i == m_neighbors.begin()) {
    return false;
  }
  --i;
  if (i->first.getSide() == side && i->first.isInside(position)) {
    src = &i->first;
    dst = &i->second;
    return true;
  }
  return false;
}

bool Config::Cell::operator==(const Cell &x) const
{
  // compare options
  if (m_options != x.m_options) {
    return false;
  }

  // compare links
  if (m_neighbors.size() != x.m_neighbors.size()) {
    return false;
  }

  auto index2neighbors = x.m_neighbors.cbegin();
  for (auto const &index1 : m_neighbors) {
    if (index1.first != index2neighbors->first) {
      return false;
    }
    if (index1.second != index2neighbors->second) {
      return false;
    }

    // operator== doesn't compare names.  only compare destination
    // names.
    if (!CaselessCmp::equal(index1.second.getName(), index2neighbors->second.getName())) {
      return false;
    }
    ++index2neighbors;
  }

  return true;
}

Config::Cell::const_iterator Config::Cell::begin() const
{
  return m_neighbors.begin();
}

Config::Cell::const_iterator Config::Cell::end() const
{
  return m_neighbors.end();
}

} // namespace synergy::server
