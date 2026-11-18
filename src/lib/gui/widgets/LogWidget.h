/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <array>
#include <deque>

#include <QWidget>

class QTableWidget;

/**
 * @brief 日志日期、时间和级别按内容取宽，消息占剩余宽度。
 */
class LogWidget : public QWidget
{
  Q_OBJECT
public:
  explicit LogWidget(QWidget *parent = nullptr);
  void appendLine(const QString &msg);
  void findNext(const QString &text);
  void findPrevious(const QString &text);
  void scrollToBottom() const;

  /// 展开时最多显示六条记录；空态不预留高度。
  void setVisibleLines(int lines);
  /// Canonical keys: "" / "info" / "warn" / "error"; keeps retained history.
  void setLevelFilter(const QString &level);
  void clear();
  [[nodiscard]] QString allText() const;
  [[nodiscard]] QString latestDisplayText() const;
  /// {info, warn, error} counts over the whole history (not just the filter).
  [[nodiscard]] std::array<int, 3> levelCounts() const;

Q_SIGNALS:
  void historyChanged(const QString &latestLine);

protected:
  void changeEvent(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  struct Record
  {
    QString time;
    QString level;
    QString message;
  };

  static QString levelOf(const QString &line);
  static int levelIndex(const QString &line);
  static Record parse(const QString &line);
  void rebuild(bool resetScroll = true);
  void updateVisibleHeight(bool refreshColumnWidth = false);
  void requestTailScroll();
  int rowHeight() const;

  QTableWidget *m_table = nullptr;
  std::deque<QString> m_history;
  std::array<int, 3> m_counts{0, 0, 0};
  QString m_filter;
  QString m_latestLine;
  int m_visibleLines = 6;
  int m_messageColumnWidth = 1;
  mutable bool m_followTail = true;
  bool m_adjustingContents = false;
  bool m_tailScrollPending = false;
};
