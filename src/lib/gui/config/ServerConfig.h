/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/NetworkProtocol.h"
#include "gui/Hotkey.h"
#include "gui/config/ScreenConfig.h"
#include "gui/config/ScreenList.h"

#include <QList>

class QTextStream;
class QSettings;
class QString;
class QFile;
class ServerConfigDialog;

namespace synergy::gui {

inline constexpr int kDefaultColumns = 5;
inline constexpr int kDefaultRows = 3;
inline constexpr auto kDefaultProtocol = NetworkProtocol::Synergy;

class ServerConfig : public ScreenConfig
{
  friend class ::ServerConfigDialog;
  friend QTextStream &operator<<(QTextStream &outStream, const ServerConfig &config);

public:
  explicit ServerConfig(int columns = kDefaultColumns, int rows = kDefaultRows);
  ~ServerConfig() = default;

  bool operator==(const ServerConfig &sc) const;

  const ScreenList &screens() const
  {
    return m_screens;
  }

  //
  // New methods
  //
  int numColumns() const
  {
    return m_columns;
  }
  int numRows() const
  {
    return m_rows;
  }
  bool hasHeartbeat() const
  {
    return m_hasHeartbeat;
  }
  int heartbeat() const
  {
    return m_heartbeat;
  }
  NetworkProtocol protocol() const
  {
    return m_protocol;
  }
  bool relativeMouseMoves() const
  {
    return m_relativeMouseMoves;
  }
  bool win32KeepForeground() const
  {
    return m_win32KeepForeground;
  }
  bool hasSwitchDelay() const
  {
    return m_hasSwitchDelay;
  }
  int switchDelay() const
  {
    return m_switchDelay;
  }
  bool hasSwitchDoubleTap() const
  {
    return m_hasSwitchDoubleTap;
  }
  int switchDoubleTap() const
  {
    return m_switchDoubleTap;
  }
  bool switchCorner(int c) const
  {
    return m_switchCorners[c];
  }
  int switchCornerSize() const
  {
    return m_switchCornerSize;
  }
  const QList<bool> &switchCorners() const
  {
    return m_switchCorners;
  }
  const HotkeyList &hotkeys() const
  {
    return m_hotkeys;
  }
  bool defaultLockToScreenState() const
  {
    return m_defaultLockToScreenState;
  }
  bool disableLockToScreen() const
  {
    return m_disableLockToScreen;
  }
  bool clipboardSharing() const
  {
    return m_clipboardSharing;
  }
  size_t clipboardSharingSize() const
  {
    return m_clipboardSharingSize;
  }
  static size_t defaultClipboardSharingSize();

  bool save(const QString &fileName) const;
  bool screenExists(const QString &screenName) const;
  void save(QFile &file) const;
  bool isFull() const;
  void commit();
  int numScreens() const;
  QString getServerName() const;
  void updateServerName();
  QString configFile() const;
  bool useExternalConfig() const;
  void addClient(const QString &clientName);

private:
  void recall();
  void setupScreens();
  QSettingsProxy &settings();
  ScreenList &screens()
  {
    return m_screens;
  }
  void setScreens(const ScreenList &screens)
  {
    m_screens = screens;
  }
  void addScreen(const Screen &screen)
  {
    m_screens.append(screen);
  }
  void setNumColumns(int n)
  {
    m_columns = n;
  }
  void setNumRows(int n)
  {
    m_rows = n;
  }
  void haveHeartbeat(bool on)
  {
    m_hasHeartbeat = on;
  }
  void setHeartbeat(int val)
  {
    m_heartbeat = val;
  }
  void setProtocol(NetworkProtocol val)
  {
    m_protocol = val;
  }
  void setRelativeMouseMoves(bool on)
  {
    m_relativeMouseMoves = on;
  }
  void setWin32KeepForeground(bool on)
  {
    m_win32KeepForeground = on;
  }
  void haveSwitchDelay(bool on)
  {
    m_hasSwitchDelay = on;
  }
  void setSwitchDelay(int val)
  {
    m_switchDelay = val;
  }
  void haveSwitchDoubleTap(bool on)
  {
    m_hasSwitchDoubleTap = on;
  }
  void setSwitchDoubleTap(int val)
  {
    m_switchDoubleTap = val;
  }
  void setSwitchCorner(int c, bool on)
  {
    m_switchCorners[c] = on;
  }
  void setSwitchCornerSize(int val)
  {
    m_switchCornerSize = val;
  }
  void setDefaultLockToScreenState(bool on)
  {
    m_defaultLockToScreenState = on;
  }
  void setDisableLockToScreen(bool on)
  {
    m_disableLockToScreen = on;
  }
  void setClipboardSharing(bool on)
  {
    m_clipboardSharing = on;
  }
  void setConfigFile(const QString &configFile) const;
  void setUseExternalConfig(bool useExternalConfig) const;
  size_t setClipboardSharingSize(size_t size);
  QList<bool> &switchCorners()
  {
    return m_switchCorners;
  }
  HotkeyList &hotkeys()
  {
    return m_hotkeys;
  }
  int adjacentScreenIndex(int idx, int deltaColumn, int deltaRow) const;
  bool findScreenName(const QString &name, int &index);
  bool fixNoServer(const QString &name, int &index);

private:
  bool m_hasHeartbeat = false;
  int m_heartbeat = 0;
  NetworkProtocol m_protocol = synergy::gui::kDefaultProtocol;
  bool m_relativeMouseMoves = false;
  bool m_win32KeepForeground = false;
  bool m_hasSwitchDelay = false;
  int m_switchDelay = 0;
  bool m_hasSwitchDoubleTap = false;
  int m_switchDoubleTap = 0;
  int m_switchCornerSize = 0;
  bool m_defaultLockToScreenState = false;
  bool m_disableLockToScreen = false;
  bool m_clipboardSharing = true;
  QString m_clientAddress = "";
  QList<bool> m_switchCorners;
  HotkeyList m_hotkeys;

  ScreenList m_screens;
  int m_columns;
  int m_rows;
  size_t m_clipboardSharingSize = defaultClipboardSharingSize();
};

QTextStream &operator<<(QTextStream &outStream, const ServerConfig &config);

} // namespace synergy::gui
