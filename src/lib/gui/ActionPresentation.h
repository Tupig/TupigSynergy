/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "Action.h"
#include "Hotkey.h"
#include "StyleUtils.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QListWidget>

namespace synergy::gui {

// Captions consume the persisted sequence. modifiers() is recording state and
// is intentionally reset by loadSettings; it is not the saved combination.
inline QString keySequenceCaption(const KeySequence &sequence)
{
  if (sequence.sequence().isEmpty())
    return QCoreApplication::translate("GuiPresentation", "Record a shortcut");
  QStringList parts;
  for (int part : sequence.sequence()) {
    const int modifiers = part & Qt::KeyboardModifierMask;
    if (modifiers) {
      auto caption = QKeySequence(modifiers).toString(QKeySequence::NativeText);
      while (caption.endsWith(QLatin1Char('+')))
        caption.chop(1);
      if (!caption.isEmpty())
        parts.append(caption);
    }
    const int key = part & ~Qt::KeyboardModifierMask;
    if (!key)
      continue;
    QString caption;
    if (key < Qt::Key_Space) {
      switch (key) {
      case Qt::LeftButton:
        caption = QCoreApplication::translate("GuiPresentation", "Left mouse button");
        break;
      case Qt::RightButton:
        caption = QCoreApplication::translate("GuiPresentation", "Right mouse button");
        break;
      case Qt::MiddleButton:
        caption = QCoreApplication::translate("GuiPresentation", "Middle mouse button");
        break;
      case Qt::XButton1:
        caption = QCoreApplication::translate("GuiPresentation", "Mouse X1");
        break;
      case Qt::XButton2:
        caption = QCoreApplication::translate("GuiPresentation", "Mouse X2");
        break;
      default:
        caption = QCoreApplication::translate("GuiPresentation", "Mouse button %1").arg(key);
        break;
      }
    } else {
      // Format one stored key at a time. Qt's multi-chord constructor would
      // introduce comma-separated shortcuts rather than this simultaneous set.
      caption = QKeySequence(key).toString(QKeySequence::NativeText);
    }
    if (!caption.isEmpty())
      parts.append(caption);
  }
  return parts.isEmpty() ? QCoreApplication::translate("GuiPresentation", "Record a shortcut")
                         : parts.join(QStringLiteral(" + "));
}

inline QString actionCaption(const Action &action)
{
  if (action.type() < 0 || action.type() > 7)
    return QCoreApplication::translate("GuiPresentation", "Unsupported action (%1) — read-only").arg(action.type());
  QString description;
  switch (static_cast<Action::Type>(action.type())) {
  case Action::Type::keyDown:
    description =
        QCoreApplication::translate("GuiPresentation", "Press keys: %1").arg(keySequenceCaption(action.keySequence()));
    break;
  case Action::Type::keyUp:
    description = QCoreApplication::translate("GuiPresentation", "Release keys: %1")
                      .arg(keySequenceCaption(action.keySequence()));
    break;
  case Action::Type::keystroke:
    description = QCoreApplication::translate("GuiPresentation", "Press and release keys: %1")
                      .arg(keySequenceCaption(action.keySequence()));
    break;
  case Action::Type::switchToScreen:
    description = QCoreApplication::translate("GuiPresentation", "Switch to computer: %1")
                      .arg(
                          action.switchScreenName().isEmpty()
                              ? QCoreApplication::translate("GuiPresentation", "No computer selected")
                              : action.switchScreenName()
                      );
    break;
  case Action::Type::switchInDirection: {
    const QStringList directions{
        QCoreApplication::translate("GuiPresentation", "Switch left"),
        QCoreApplication::translate("GuiPresentation", "Switch right"),
        QCoreApplication::translate("GuiPresentation", "Switch up"),
        QCoreApplication::translate("GuiPresentation", "Switch down")
    };
    const int direction = action.switchDirection();
    description = direction >= 0 && direction < directions.size()
                      ? directions.at(direction)
                      : QCoreApplication::translate("GuiPresentation", "Switch direction: choose a direction");
    break;
  }
  case Action::Type::switchToNextScreen:
    description = QCoreApplication::translate("GuiPresentation", "Switch to next computer");
    break;
  case Action::Type::lockCursorToScreen: {
    const QStringList modes{
        QCoreApplication::translate("GuiPresentation", "Toggle cursor lock"),
        QCoreApplication::translate("GuiPresentation", "Lock cursor"),
        QCoreApplication::translate("GuiPresentation", "Unlock cursor")
    };
    const int mode = action.lockCursorMode();
    description = mode >= 0 && mode < modes.size()
                      ? modes.at(mode)
                      : QCoreApplication::translate("GuiPresentation", "Cursor lock: choose a mode");
    break;
  }
  case Action::Type::restartAllConnections:
    description = QCoreApplication::translate("GuiPresentation", "Restart server");
    break;
  default:
    break;
  }
  if (action.type() <= 2 && !action.keySequence().isMouseButton()) {
    const auto recipients = action.haveScreens() && !action.typeScreenNames().isEmpty()
                                ? QCoreApplication::translate("GuiPresentation", "Specified computers: %1")
                                      .arg(action.typeScreenNames().join(QStringLiteral(", ")))
                                : QCoreApplication::translate("GuiPresentation", "All computers");
    description += QStringLiteral(" · ") + recipients;
  }
  return (action.activeOnRelease() ? QCoreApplication::translate("GuiPresentation", "On release")
                                   : QCoreApplication::translate("GuiPresentation", "On press")) +
         QStringLiteral(" · ") + description;
}

inline QIcon actionIcon(const Action &action)
{
  if (action.type() >= 0 && action.type() <= 2)
    return appIcon(QStringLiteral("key"));
  if (action.type() == 6)
    return appIcon(QStringLiteral("lock"));
  if (action.type() == 7)
    return appIcon(QStringLiteral("view-refresh"));
  return appIcon(QStringLiteral("video-display"));
}

inline void presentAction(QListWidgetItem *item, const Action &action)
{
  item->setText(actionCaption(action));
  item->setToolTip(item->text());
  item->setIcon(actionIcon(action));
}

inline void presentHotkey(QListWidgetItem *item, const Hotkey &hotkey)
{
  item->setText(keySequenceCaption(hotkey.keySequence()));
  item->setToolTip(item->text());
  item->setIcon(appIcon(QStringLiteral("key")));
}

inline void appendAction(QListWidget *list, const Action &action)
{
  presentAction(new QListWidgetItem(list), action);
}

inline void appendHotkey(QListWidget *list, const Hotkey &hotkey)
{
  presentHotkey(new QListWidgetItem(list), hotkey);
}

} // namespace synergy::gui
