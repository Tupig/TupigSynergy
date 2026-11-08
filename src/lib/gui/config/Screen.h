/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "ScreenConfig.h"

#include "common/QSettingsProxy.h"

#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

class QSettings;
class QTextStream;
class ScreenSettingsDialog;
class ScreenSetupModel;
class ScreenSetupView;

namespace synergy::gui {

class Screen : public ScreenConfig
{
  friend class ::ScreenSettingsDialog;
  friend class ::ScreenSetupModel;
  friend class ::ScreenSetupView;

  friend QDataStream &operator<<(QDataStream &outStream, const Screen &screen)
  {
    return outStream << screen.name() << screen.switchCornerSize() << screen.aliases() << screen.modifiers()
                     << screen.switchCorners() << screen.fixes() << screen.isPrimary();
  }

  friend QDataStream &operator>>(QDataStream &inStream, Screen &screen)
  {
    return inStream >> screen.m_name >> screen.m_switchCornerSize >> screen.m_aliases >> screen.m_modifiers >>
           screen.m_switchCorners >> screen.m_fixes >> screen.m_isPrimary;
  }

public:
  explicit Screen(const QString &name = QString());

  [[nodiscard]] const QPixmap &pixmap() const
  {
    return m_pixmap;
  }
  [[nodiscard]] const QString &name() const
  {
    return m_name;
  }
  [[nodiscard]] const QStringList &aliases() const
  {
    return m_aliases;
  }

  [[nodiscard]] bool isNull() const
  {
    return m_name.isEmpty();
  }
  [[nodiscard]] int modifier(int m) const
  {
    return m_modifiers[m] == static_cast<int>(ScreenConfig::Modifier::DefaultMod) ? m : m_modifiers[m];
  }
  [[nodiscard]] const QList<int> &modifiers() const
  {
    return m_modifiers;
  }
  [[nodiscard]] bool switchCorner(int c) const
  {
    return m_switchCorners[c];
  }
  [[nodiscard]] const QList<bool> &switchCorners() const
  {
    return m_switchCorners;
  }
  [[nodiscard]] int switchCornerSize() const
  {
    return m_switchCornerSize;
  }
  [[nodiscard]] bool fix(const Fix f) const
  {
    return m_fixes[static_cast<int8_t>(f)];
  }
  [[nodiscard]] const QList<bool> &fixes() const
  {
    return m_fixes;
  }

  void loadSettings(QSettingsProxy &settings);
  void saveSettings(QSettingsProxy &settings) const;
  [[nodiscard]] QString screensSection() const;
  [[nodiscard]] QString aliasesSection() const;

  [[nodiscard]] bool swapped() const
  {
    return m_swapped;
  }

  void setName(const QString &name)
  {
    m_name = name;
  }
  [[nodiscard]] bool isPrimary() const
  {
    return m_isPrimary;
  }
  void markAsPrimary()
  {
    m_isPrimary = true;
  }

  bool operator==(const Screen &screen) const;

protected:
  QStringList &aliases()
  {
    return m_aliases;
  }
  void setModifier(const Modifier m, const int n)
  {
    m_modifiers[static_cast<int8_t>(m)] = n;
  }
  QList<int> &modifiers()
  {
    return m_modifiers;
  }
  void addAlias(const QString &alias)
  {
    m_aliases.append(alias);
  }
  void setSwitchCorner(const SwitchCorner c, const bool on)
  {
    m_switchCorners[static_cast<int8_t>(c)] = on;
  }
  QList<bool> &switchCorners()
  {
    return m_switchCorners;
  }
  void setSwitchCornerSize(const int val)
  {
    m_switchCornerSize = val;
  }
  void setFix(const Fix f, const bool on)
  {
    m_fixes[static_cast<int8_t>(f)] = on;
  }
  QList<bool> &fixes()
  {
    return m_fixes;
  }
  void setSwapped(const bool on)
  {
    m_swapped = on;
  }

private:
  QPixmap m_pixmap = QIcon::fromTheme("video-display").pixmap(QSize(96, 96));
  QString m_name = {};
  QStringList m_aliases = {};
  QList<int> m_modifiers = {0, 1, 2, 3, 4, 5, 6};
  QList<bool> m_switchCorners = {false, false, false, false};
  int m_switchCornerSize = 0;
  QList<bool> m_fixes{false, false, false, false};
  bool m_swapped = false;
  bool m_isPrimary = false;
};

} // namespace synergy::gui
