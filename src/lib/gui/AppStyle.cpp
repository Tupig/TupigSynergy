/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AppStyle.h"

#include "StyleUtils.h"

namespace synergy::gui {

QString appStyleSheet()
{
  // 两套配色共用同一套几何值：高度、圆角、内边距只写一份。
  const auto dark = isDarkMode();
  const auto window = dark ? QStringLiteral("#1c1c1e") : QStringLiteral("#f5f5f7");
  const auto card = dark ? QStringLiteral("#242427") : QStringLiteral("#ffffff");
  const auto surface = dark ? QStringLiteral("#1f2024") : QStringLiteral("#f0f0f3");
  const auto bar = dark ? QStringLiteral("#232326") : QStringLiteral("#e8e8ec");
  const auto text = dark ? QStringLiteral("#e9e9ea") : QStringLiteral("#1c1c1e");
  const auto textMuted = dark ? QStringLiteral("#9a9aa0") : QStringLiteral("#6f6f78");
  const auto border = dark ? QStringLiteral("#3f4046") : QStringLiteral("#d0d0d5");
  const auto control = dark ? QStringLiteral("#2a2a2e") : QStringLiteral("#ffffff");
  const auto controlHover = dark ? QStringLiteral("#33343a") : QStringLiteral("#ececf0");
  const auto accent = QStringLiteral("#4cc38a");
  const auto accentInk = QStringLiteral("#08120d");
  const auto accentSoft = dark ? QStringLiteral("rgba(76,195,138,0.16)") : QStringLiteral("rgba(76,195,138,0.18)");

  return QStringLiteral(R"(
    QWidget { color: %1; font-size: 13px; }
    QMainWindow, QDialog, QDockWidget { background: %2; }

    /* 卡片：分组框与主窗口配置区 */
    QGroupBox {
      background: %3; border: 0; border-radius: 11px; padding: 12px; margin-top: 0;
    }
    QGroupBox::title { color: %4; padding: 0 4px; }

    /* 模式选择做成可选卡片 */
    QRadioButton { color: %1; spacing: 8px; padding: 6px 10px; border-radius: 9px;
                  min-height: 52px; max-height: 52px; }
    QRadioButton:checked { background: %5; }

    /* 按钮 32px / 圆角 8；主操作使用强调色 */
    QPushButton {
      min-height: 30px; padding: 0 12px; border-radius: 8px;
      background: %6; border: 1px solid %7; color: %1;
    }
    QPushButton:hover { background: %8; }
    QPushButton:disabled { color: %4; }
    /* 日志级别胶囊：透明底，选中用浅色块 */
    QPushButton[levelChip="true"] {
      min-height: 22px; padding: 0 9px; border: 0; border-radius: 6px;
      background: transparent; color: %4; font-size: 12px;
    }
    QPushButton[levelChip="true"]:checked { background: #35353c; color: #f2f2f4; }
    /* 纯图标按钮 */
    QPushButton[iconButton="true"] {
      min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px;
      padding: 0; border: 0; background: transparent;
    }
    QPushButton[iconButton="true"]:hover { background: #2e2e34; border-radius: 6px; }
    QPushButton:default, QPushButton[primary="true"] {
      background: %9; border-color: %9; color: %10; font-weight: 600;
    }

    /* 输入控件 28px / 圆角 7 */
    QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QAbstractSpinBox {
      min-height: 28px; padding: 0 8px; border-radius: 7px;
      border: 1px solid %7; background: %11; color: %1;
    }
    QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: %9; }

    /* 日志区 */
    QPlainTextEdit { background: %12; border: 0; border-radius: 9px; font-size: 11px; }
    QLabel#logStats, QLabel#logHint { color: %4; font-size: 10px; }
    QLabel#logTail { color: %4; font-size: 10px; }
    QLabel#logTitle { font-size: 12px; font-weight: 600; }

    /* 状态栏 / 菜单 */
    QStatusBar { background: %13; color: %1; }
    QStatusBar::item { border: 0; }

    /* 标签页 */
    QTabWidget::pane { border: 0; }
    QTabBar::tab {
      background: transparent; color: %4; padding: 6px 10px; border-radius: 7px; margin-right: 2px;
    }
    QTabBar::tab:selected { background: %5; color: %9; }

    QCheckBox { color: %1; spacing: 8px; }
    QLabel { color: %1; }
    QLabel:disabled { color: %4; }
  )")
      .arg(text)
      .arg(window)
      .arg(card)
      .arg(textMuted)
      .arg(accentSoft)
      .arg(control)
      .arg(border)
      .arg(controlHover)
      .arg(accent)
      .arg(accentInk)
      .arg(surface)
      .arg(surface)
      .arg(bar);
}

} // namespace synergy::gui
