/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DialogStyle.h"

#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>

namespace synergy::gui {

namespace {
constexpr int kDialogMargin = 12;
constexpr int kRowSpacing = 8;
constexpr int kGroupSpacing = 10;
constexpr int kControlHeight = 28;
} // namespace

void applyCompactDialogStyle(QDialog *dialog)
{
  if (dialog == nullptr) {
    return;
  }

  // 统一控件高度与分组内边距；QSS 负责外观，代码负责布局节奏。
  dialog->setStyleSheet(QStringLiteral(R"(
    QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QAbstractSpinBox {
      min-height: %1px; padding: 0 8px;
    }
    QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { border-radius: 7px; }
    /* 下拉框按设计稿限制宽度，避免横跨整行；路径类输入保持整行。 */
    QComboBox { max-width: 260px; }
    QPushButton { min-height: %2px; padding: 0 12px; border-radius: 8px; }
    QTabWidget::pane { border: 0; margin-top: %3px; }
    QTabBar::tab { padding: 6px 10px; margin-right: 2px; border-radius: 7px; }
    QGroupBox { margin-top: %4px; padding-top: %5px; }
    QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
  )")
                                .arg(kControlHeight)
                                .arg(kControlHeight + 4)
                                .arg(kGroupSpacing)
                                .arg(kGroupSpacing)
                                .arg(kGroupSpacing));

  if (auto *layout = dialog->layout(); layout != nullptr) {
    layout->setContentsMargins(kDialogMargin, kDialogMargin, kDialogMargin, kDialogMargin);
    layout->setSpacing(kGroupSpacing);
  }

  // 表单行距统一 8，横向标签与控件间距 12（与设计稿的 120/180 栅格一致）。
  for (auto *form : dialog->findChildren<QFormLayout *>()) {
    form->setVerticalSpacing(kRowSpacing);
    form->setHorizontalSpacing(kDialogMargin);
    form->setContentsMargins(0, 0, 0, 0);
    // 标签左对齐、表单向顶部收拢（设计稿的对齐方式）。
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  }

  for (auto *group : dialog->findChildren<QGroupBox *>()) {
    if (auto *layout = group->layout(); layout != nullptr) {
      layout->setContentsMargins(kDialogMargin, kGroupSpacing + 4, kDialogMargin, kDialogMargin);
      layout->setSpacing(kRowSpacing);
    }
  }

  for (auto *tabs : dialog->findChildren<QTabWidget *>()) {
    tabs->setDocumentMode(true);
    // 标签文字不能被省略号截断（窄窗口下最容易发生）。
    tabs->tabBar()->setElideMode(Qt::ElideNone);
    tabs->setUsesScrollButtons(false);
  }
}

} // namespace synergy::gui
