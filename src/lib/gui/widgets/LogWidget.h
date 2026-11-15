/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <array>
#include <deque>

#include <QObject>
#include <QStringList>
#include <QWidget>

class QTextEdit;

/**
 * @brief 设计稿的记录区：时间 / 级别 / 消息 三列，级别按颜色区分。
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

  /// Size the viewport for the latest records, including wrapped lines.
  void setVisibleLines(int lines);
  /// Canonical keys: "" / "info" / "warn" / "error"; keeps retained history.
  void setLevelFilter(const QString &level);
  void clear();
  [[nodiscard]] QString allText() const;
  /// {info, warn, error} counts over the whole history (not just the filter).
  [[nodiscard]] std::array<int, 3> levelCounts() const;

Q_SIGNALS:
  void historyChanged(const QString &latestLine);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  static QString levelOf(const QString &line);
  static int levelIndex(const QString &line);
  static QString recordHtml(const QString &line);
  void rebuild();
  void updateVisibleHeight();

  QTextEdit *m_textLog = nullptr;
  std::deque<QString> m_history;
  std::array<int, 3> m_counts{0, 0, 0};
  QString m_filter;
  QString m_latestLine;
  int m_visibleLines = 6;
};
