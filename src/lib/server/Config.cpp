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

Config::Config(IEventQueue *events) : m_inputFilter(events), m_events(events)
{
  // do nothing
}

bool Config::addScreen(const std::string &name)
{
  // alias name must not exist
  if (m_nameToCanonicalName.contains(name)) {
    return false;
  }

  // add cell
  m_map.insert(std::make_pair(name, Cell()));

  // add name
  m_nameToCanonicalName.insert(std::make_pair(name, name));

  return true;
}

bool Config::renameScreen(const std::string &oldName, const std::string &newName)
{
  // get canonical name and find cell
  std::string oldCanonical = getCanonicalName(oldName);
  CellMap::iterator index = m_map.find(oldCanonical);
  if (index == m_map.end()) {
    return false;
  }

  // accept if names are equal but replace with new name to maintain
  // case.  otherwise, the new name must not exist.
  if (!CaselessCmp::equal(oldName, newName) && m_nameToCanonicalName.contains(newName)) {
    return false;
  }

  // update cell
  Cell tmpCell = index->second;
  m_map.erase(index);
  m_map.insert(std::make_pair(newName, tmpCell));

  // update name
  m_nameToCanonicalName.erase(oldCanonical);
  m_nameToCanonicalName.insert(std::make_pair(newName, newName));

  // update connections
  Name oldNameObj(this, oldName);
  for (index = m_map.begin(); index != m_map.end(); ++index) {
    index->second.rename(oldNameObj, newName);
  }

  // update alias targets
  if (CaselessCmp::equal(oldName, oldCanonical)) {
    for (auto iter = m_nameToCanonicalName.begin(); iter != m_nameToCanonicalName.end(); ++iter) {
      if (CaselessCmp::equal(iter->second, oldCanonical)) {
        iter->second = newName;
      }
    }
  }

  return true;
}

void Config::removeScreen(const std::string &name)
{
  // get canonical name and find cell
  std::string canonical = getCanonicalName(name);
  CellMap::iterator index = m_map.find(canonical);
  if (index == m_map.end()) {
    return;
  }

  // remove from map
  m_map.erase(index);

  // disconnect
  Name nameObj(this, name);
  for (index = m_map.begin(); index != m_map.end(); ++index) {
    index->second.remove(nameObj);
  }

  // remove aliases (and canonical name)
  for (auto iter = m_nameToCanonicalName.begin(); iter != m_nameToCanonicalName.end();) {
    if (iter->second == canonical) {
      m_nameToCanonicalName.erase(iter++);
    } else {
      ++iter;
    }
  }
}

void Config::removeAllScreens()
{
  m_map.clear();
  m_nameToCanonicalName.clear();
}

bool Config::addAlias(const std::string &canonical, const std::string &alias)
{
  // alias name must not exist
  if (m_nameToCanonicalName.contains(alias)) {
    return false;
  }

  // canonical name must be known
  if (!m_map.contains(canonical)) {
    return false;
  }

  // insert alias
  m_nameToCanonicalName.insert(std::make_pair(alias, canonical));

  return true;
}

bool Config::removeAlias(const std::string &alias)
{
  // must not be a canonical name
  if (m_map.contains(alias)) {
    return false;
  }

  // find alias
  NameMap::iterator index = m_nameToCanonicalName.find(alias);
  if (index == m_nameToCanonicalName.end()) {
    return false;
  }

  // remove alias
  m_nameToCanonicalName.erase(index);

  return true;
}

bool Config::removeAliases(const std::string &canonical)
{
  // must be a canonical name
  if (!m_map.contains(canonical)) {
    return false;
  }

  // find and removing matching aliases
  for (auto index = m_nameToCanonicalName.begin(); index != m_nameToCanonicalName.end();) {
    if (index->second == canonical && index->first != canonical) {
      m_nameToCanonicalName.erase(index++);
    } else {
      ++index;
    }
  }

  return true;
}

void Config::removeAllAliases()
{
  // remove all names
  m_nameToCanonicalName.clear();

  // put the canonical names back in
  for (auto index = m_map.begin(); index != m_map.end(); ++index) {
    m_nameToCanonicalName.insert(std::make_pair(index->first, index->first));
  }
}

bool Config::connect(
    const std::string &srcName, Direction srcSide, float srcStart, float srcEnd, const std::string &dstName,
    float dstStart, float dstEnd
)
{
  assert(srcSide >= Direction::FirstDirection && srcSide <= Direction::LastDirection);

  // find source cell
  CellMap::iterator index = m_map.find(getCanonicalName(srcName));
  if (index == m_map.end()) {
    return false;
  }

  // add link
  CellEdge srcEdge(srcSide, Interval(srcStart, srcEnd));
  CellEdge dstEdge(dstName, srcSide, Interval(dstStart, dstEnd));
  return index->second.add(srcEdge, dstEdge);
}

bool Config::disconnect(const std::string &srcName, Direction srcSide)
{
  assert(srcSide >= Direction::FirstDirection && srcSide <= Direction::LastDirection);

  // find source cell
  CellMap::iterator index = m_map.find(srcName);
  if (index == m_map.end()) {
    return false;
  }

  // disconnect side
  index->second.remove(srcSide);

  return true;
}

bool Config::disconnect(const std::string &srcName, Direction srcSide, float position)
{
  assert(srcSide >= Direction::FirstDirection && srcSide <= Direction::LastDirection);

  // find source cell
  CellMap::iterator index = m_map.find(srcName);
  if (index == m_map.end()) {
    return false;
  }

  // disconnect side
  index->second.remove(srcSide, position);

  return true;
}

void Config::setSynergyAddress(const NetworkAddress &addr)
{
  m_synergyAddress = addr;
}

bool Config::addOption(const std::string &name, OptionID option, OptionValue value)
{
  // find options
  ScreenOptions *options = nullptr;
  if (name.empty()) {
    options = &m_globalOptions;
  } else {
    CellMap::iterator index = m_map.find(name);
    if (index != m_map.end()) {
      options = &index->second.m_options;
    }
  }
  if (options == nullptr) {
    return false;
  }

  // add option
  options->insert(std::make_pair(option, value));
  return true;
}

bool Config::removeOption(const std::string &name, OptionID option)
{
  // find options
  ScreenOptions *options = nullptr;
  if (name.empty()) {
    options = &m_globalOptions;
  } else {
    CellMap::iterator index = m_map.find(name);
    if (index != m_map.end()) {
      options = &index->second.m_options;
    }
  }
  if (options == nullptr) {
    return false;
  }

  // remove option
  options->erase(option);
  return true;
}

bool Config::removeOptions(const std::string &name)
{
  // find options
  ScreenOptions *options = nullptr;
  if (name.empty()) {
    options = &m_globalOptions;
  } else {
    CellMap::iterator index = m_map.find(name);
    if (index != m_map.end()) {
      options = &index->second.m_options;
    }
  }
  if (options == nullptr) {
    return false;
  }

  // remove options
  options->clear();
  return true;
}

bool Config::isValidScreenName(const std::string &name) const
{
  // name is valid if matches validname
  //  name      ::= [_A-Za-z0-9] | [_A-Za-z0-9][-_A-Za-z0-9]*[_A-Za-z0-9]
  //  domain    ::= . name
  //  validname ::= name domain*
  // we also accept names ending in . because many OS X users have
  // so misconfigured their systems.

  // empty name is invalid
  if (name.empty()) {
    return false;
  }

  // check each dot separated part
  for (std::string::size_type b = 0; b == name.size();) {
    // find end of part
    std::string::size_type e = name.find('.', b);
    if (e == std::string::npos) {
      e = name.size();
    }

    // part may not be empty
    if (e - b < 1) {
      return false;
    }

    // check first and last characters
    if (!(isalnum(name[b]) || name[b] == '_') || !(isalnum(name[e - 1]) || name[e - 1] == '_')) {
      return false;
    }

    // check interior characters
    for (std::string::size_type i = b; i < e; ++i) {
      if (!isalnum(name[i]) && name[i] != '_' && name[i] != '-') {
        return false;
      }
    }

    // next part
    if (e == name.size()) {
      // no more parts
      break;
    }
    b = e + 1;
  }

  return true;
}

Config::const_iterator Config::begin() const
{
  return const_iterator(m_map.begin());
}

Config::const_iterator Config::end() const
{
  return const_iterator(m_map.end());
}

Config::all_const_iterator Config::beginAll() const
{
  return m_nameToCanonicalName.begin();
}

Config::all_const_iterator Config::endAll() const
{
  return m_nameToCanonicalName.end();
}

bool Config::isScreen(const std::string &name) const
{
  return m_nameToCanonicalName.contains(name);
}

bool Config::isCanonicalName(const std::string &name) const
{
  return (!name.empty() && CaselessCmp::equal(getCanonicalName(name), name));
}

std::string Config::getCanonicalName(const std::string &name) const
{
  NameMap::const_iterator index = m_nameToCanonicalName.find(name);
  if (index == m_nameToCanonicalName.end()) {
    return std::string();
  } else {
    return index->second;
  }
}

std::string Config::getNeighbor(const std::string &srcName, Direction srcSide, float position, float *positionOut) const
{
  assert(srcSide >= Direction::FirstDirection && srcSide <= Direction::LastDirection);

  // find source cell
  CellMap::const_iterator index = m_map.find(getCanonicalName(srcName));
  if (index == m_map.end()) {
    return std::string();
  }

  // find edge
  const CellEdge *srcEdge;
  const CellEdge *dstEdge;
  if (!index->second.getLink(srcSide, position, srcEdge, dstEdge)) {
    // no neighbor
    return "";
  } else {
    // compute position on neighbor
    if (positionOut != nullptr) {
      *positionOut = dstEdge->inverseTransform(srcEdge->transform(position));
    }

    // return neighbor's name
    return getCanonicalName(dstEdge->getName());
  }
}

bool Config::hasNeighbor(const std::string &srcName, Direction srcSide) const
{
  return hasNeighbor(srcName, srcSide, 0.0f, 1.0f);
}

bool Config::hasNeighbor(const std::string &srcName, Direction srcSide, float start, float end) const
{
  assert(srcSide >= Direction::FirstDirection && srcSide <= Direction::LastDirection);

  // find source cell
  CellMap::const_iterator index = m_map.find(getCanonicalName(srcName));
  if (index == m_map.end()) {
    return false;
  }

  return index->second.overlaps(CellEdge(srcSide, Interval(start, end)));
}

Config::link_const_iterator Config::beginNeighbor(const std::string &srcName) const
{
  CellMap::const_iterator index = m_map.find(getCanonicalName(srcName));
  assert(index != m_map.end());
  return index->second.begin();
}

Config::link_const_iterator Config::endNeighbor(const std::string &srcName) const
{
  CellMap::const_iterator index = m_map.find(getCanonicalName(srcName));
  assert(index != m_map.end());
  return index->second.end();
}

const NetworkAddress &Config::getSynergyAddress() const
{
  return m_synergyAddress;
}

const Config::ScreenOptions *Config::getOptions(const std::string &name) const
{
  // find options
  const ScreenOptions *options = nullptr;
  if (name.empty()) {
    options = &m_globalOptions;
  } else {
    CellMap::const_iterator index = m_map.find(name);
    if (index != m_map.end()) {
      options = &index->second.m_options;
    }
  }

  // return options
  return options;
}

bool Config::hasLockToScreenAction() const
{
  return m_hasLockToScreenAction;
}

bool Config::operator==(const Config &x) const
{
  if (m_synergyAddress != x.m_synergyAddress) {
    return false;
  }
  if (m_map.size() != x.m_map.size()) {
    return false;
  }
  if (m_nameToCanonicalName.size() != x.m_nameToCanonicalName.size()) {
    return false;
  }

  // compare global options
  if (m_globalOptions != x.m_globalOptions) {
    return false;
  }

  auto index2map = x.m_map.cbegin();
  for (auto const &index1 : m_map) {
    // compare names
    if (!CaselessCmp::equal(index1.first, index2map->first)) {
      return false;
    }

    // compare cells
    if (index1.second != index2map->second) {
      return false;
    }
    ++index2map;
  }

  auto index2 = x.m_nameToCanonicalName.cbegin();
  for (auto const &index1 : m_nameToCanonicalName) {
    if (index2 == x.m_nameToCanonicalName.cend()) {
      return false; // second source ended
    }
    if (!CaselessCmp::equal(index1.first, index2->first) || !CaselessCmp::equal(index1.second, index2->second)) {
      return false;
    }
    ++index2;
  }

  // compare input filters
  if (m_inputFilter != x.m_inputFilter) {
    return false;
  }

  return true;
}

const char *Config::dirName(Direction dir)
{
  static const char *s_name[] = {"left", "right", "up", "down"};

  const auto index = static_cast<int>(dir) - static_cast<int>(Direction::FirstDirection);
  assert(index >= 0 && index < static_cast<int>(sizeof(s_name) / sizeof(s_name[0])));

  // defensive: a release build has no assert, and a bad index would read past the table
  if (index < 0 || index >= static_cast<int>(sizeof(s_name) / sizeof(s_name[0]))) {
    return "invalid";
  }

  return s_name[index];
}

InputFilter *Config::getInputFilter()
{
  return &m_inputFilter;
}

std::string Config::formatInterval(const Interval &x)
{
  if (x.first == 0.0f && x.second == 1.0f) {
    return "";
  }
  return synergy::string::sprintf("(%d,%d)", (int)(x.first * 100.0f + 0.5f), (int)(x.second * 100.0f + 0.5f));
}

} // namespace synergy::server
