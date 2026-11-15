/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QFrame>

class LogWidget;
class QButtonGroup;
class QLabel;
class QPushButton;
class SearchWidget;

/**
 * @brief 设计稿中的日志卡片：工具条 + 记录区 + 统计脚注，收起时只留最新一条。
 */
class LogPanel : public QFrame
{
  Q_OBJECT
public:
  explicit LogPanel(QWidget *parent = nullptr);

  void appendLine(const QString &msg);
  void setCollapsed(bool collapsed);
  [[nodiscard]] bool isCollapsed() const;

Q_SIGNALS:
  void collapsedChanged(bool collapsed);

protected:
  void changeEvent(QEvent *e) override;

private:
  void updateText();
  void updateStats();

  LogWidget *m_records = nullptr;
  QWidget *m_expanded = nullptr;
  QWidget *m_collapsedRow = nullptr;
  QLabel *m_title = nullptr;
  QLabel *m_stats = nullptr;
  QLabel *m_hint = nullptr;
  QLabel *m_badge = nullptr;
  QLabel *m_tail = nullptr;
  QPushButton *m_btnCopy = nullptr;
  QPushButton *m_btnClear = nullptr;
  QPushButton *m_btnCollapse = nullptr;
  QPushButton *m_btnExpand = nullptr;
  SearchWidget *m_search = nullptr;
  QButtonGroup *m_levels = nullptr;
  bool m_collapsed = false;
};
