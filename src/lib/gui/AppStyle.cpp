/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AppStyle.h"

#include "StyleUtils.h"

namespace synergy::gui {

namespace {

// 设计稿的设计令牌：间距只用 4/8/12，圆角 11/8，控件高度 32/30/26。
struct Tokens
{
  QString app;
  QString panel;
  QString panelAlt;
  QString control;
  QString controlHover;
  QString line;
  QString text;
  QString textSecondary;
  QString muted;
  QString accent;
  QString accentInk;
  QString accentSoft;
  QString danger;
  QString dangerText;
};

Tokens tokens(bool dark)
{
  if (dark) {
    return {
        .app = QStringLiteral("#1c1c1e"),
        .panel = QStringLiteral("#242427"),
        .panelAlt = QStringLiteral("#2a2a2e"),
        .control = QStringLiteral("#1b1c20"),
        .controlHover = QStringLiteral("#33343a"),
        .line = QStringLiteral("#34353b"),
        .text = QStringLiteral("#e9e9ea"),
        .textSecondary = QStringLiteral("#c9c9cf"),
        .muted = QStringLiteral("#9a9aa0"),
        .accent = QStringLiteral("#4cc38a"),
        .accentInk = QStringLiteral("#08120d"),
        .accentSoft = QStringLiteral("rgba(76,195,138,0.16)"),
        .danger = QStringLiteral("#3a2424"),
        .dangerText = QStringLiteral("#ffb4b3"),
    };
  }
  return {
      .app = QStringLiteral("#f5f5f7"),
      .panel = QStringLiteral("#ffffff"),
      .panelAlt = QStringLiteral("#ececf0"),
      .control = QStringLiteral("#ffffff"),
      .controlHover = QStringLiteral("#e4e4e9"),
      .line = QStringLiteral("#d0d0d5"),
      .text = QStringLiteral("#1c1c1e"),
      .textSecondary = QStringLiteral("#3a3a40"),
      .muted = QStringLiteral("#6f6f78"),
      .accent = QStringLiteral("#2f9e6b"),
      .accentInk = QStringLiteral("#ffffff"),
      .accentSoft = QStringLiteral("rgba(47,158,107,0.14)"),
      .danger = QStringLiteral("#fdeaea"),
      .dangerText = QStringLiteral("#a12624"),
  };
}

} // namespace

QString appStyleSheet()
{
  const auto t = tokens(isDarkMode());
  return QStringLiteral(R"(
    QWidget { color: %1; font-size: 13px; }
    QMainWindow, QDialog { background: %2; }
    QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: 0; }

    QLabel#pageTitle { font-size: 13px; font-weight: 600; color: %1; }
    QLabel#lblComputerName { font-size: 13px; font-weight: 600; color: %1; }
    QLabel#lblIpAddresses { font-size: 11.5px; color: %4; }
    QLabel#fieldLabel, QLabel[fieldLabel="true"] { color: %3; font-size: 12px; }
    QLabel[muted="true"] { color: %4; font-size: 11px; }

    /* 卡片：无边框、圆角 11，内边距由布局给出 10/12 */
    QFrame#card { background: %5; border: 0; border-radius: 11px; }
    QGroupBox {
      background: %5; border: 0; border-radius: 11px;
      margin-top: 0; padding: 10px 12px;
    }
    QGroupBox::title { color: %6; font-size: 13px; subcontrol-origin: margin; left: 12px; padding: 0 4px; }

    /* 按钮：常规 32 / 小号 26 / 图标 24 */
    QPushButton {
      min-height: 32px; padding: 0 13px; border-radius: 8px;
      border: 1px solid %7; background: %8; color: %1; font-size: 13px;
    }
    QPushButton:hover { background: %9; }
    QPushButton:disabled { color: %4; }
    QPushButton[small="true"] {
      min-height: 26px; max-height: 26px; padding: 0 9px; font-size: 12px; border-radius: 7px;
    }
    QPushButton[iconButton="true"] {
      min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px;
      padding: 0; border: 0; background: transparent;
    }
    QPushButton[iconButton="true"]:hover { background: %10; border-radius: 6px; }
    QPushButton[primary="true"] {
      background: %11; border-color: %11; color: %12; font-weight: 600;
    }
    QPushButton[danger="true"] {
      background: %13; border-color: %14; color: %14;
    }

    /* 日志级别胶囊 */
    QFrame#logFilter { background: %15; border: 0; border-radius: 7px; }
    QPushButton[levelChip="true"] {
      min-height: 20px; max-height: 20px; padding: 0 7px; border: 0; border-radius: 5px;
      background: transparent; color: %4; font-size: 10.5px;
    }
    QPushButton[levelChip="true"]:checked { background: %7; color: #f0f0f3; }

    /* 输入控件：30px 高、圆角 7 */
    QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
      min-height: 30px; max-height: 30px; padding: 0 9px; border-radius: 7px;
      border: 1px solid %7; background: %16; color: %1; font-size: 12.5px;
      selection-background-color: %11; selection-color: %12;
    }
    QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: %11; }
    QComboBox::drop-down { border: 0; width: 18px; }
    QComboBox QAbstractItemView {
      background: %5; border: 1px solid %7; border-radius: 8px; padding: 4px;
      selection-background-color: %10;
    }

    /* 模式卡片由 ModeCard 自行绘制，这里只给可访问性颜色 */
    ModeCard { background: transparent; }

    /* 日志面板 */
    QFrame#logPanel { background: %17; border: 1px solid %7; border-radius: 9px; }
    QLabel#logTitle { font-size: 12px; font-weight: 600; }
    QLabel#logBadge {
      font-size: 10.5px; font-weight: 600; padding: 1px 8px; border-radius: 999px;
      background: %18; color: %11; border: 1px solid %11;
    }
    QLabel#logTail { color: %4; font-size: 11px; }
    QLabel#logStats, QLabel#logHint { color: %4; font-size: 10.5px; }
    QPlainTextEdit#logRecords {
      background: transparent; border: 0; font-size: 11.5px;
    }

    /* 页签 */
    QTabWidget::pane { border: 0; }
    QTabBar::tab {
      background: transparent; color: %4; padding: 5px 9px; border-radius: 7px;
      margin-right: 2px; font-size: 12px;
    }
    QTabBar::tab:selected { background: %18; color: %11; }

    QCheckBox, QRadioButton { color: %1; font-size: 12.5px; spacing: 8px; }
    QCheckBox:disabled, QRadioButton:disabled, QLabel:disabled { color: %4; }

    QWidget#statusBar { background: %19; }
    QLabel#statusText { font-size: 12px; }
    QLabel#statusRight { font-size: 11px; color: %4; }
  )")
      .arg(t.text)          // 1
      .arg(t.app)           // 2
      .arg(t.textSecondary) // 3
      .arg(t.muted)         // 4
      .arg(t.panel)         // 5
      .arg(t.textSecondary) // 6
      .arg(t.line)          // 7
      .arg(t.panelAlt)      // 8
      .arg(t.controlHover)  // 9
      .arg(t.panelAlt)      // 10
      .arg(t.accent)        // 11
      .arg(t.accentInk)     // 12
      .arg(t.danger)        // 13
      .arg(t.dangerText)    // 14
      .arg(t.panelAlt)      // 15
      .arg(t.control)       // 16
      .arg(t.panelAlt)      // 17
      .arg(t.accentSoft)    // 18
      .arg(t.panelAlt);     // 19
}

} // namespace synergy::gui
