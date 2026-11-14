/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <array>

#include <QObject>
#include <QStringList>
#include <QWidget>

class QPlainTextEdit;
class QPushButton;
class QTemporaryFile;

class LogWidget : public QWidget
{
  Q_OBJECT
public:
  explicit LogWidget(QWidget *parent = nullptr);
  void appendLine(const QString &msg);
  void findNext(const QString &text);
  void findPrevious(const QString &text);
  void scrollToBottom() const;

  /// Number of visible text lines the dock should show (6 expanded, 1 collapsed).
  void setVisibleLines(int lines);
  /// "全部" / "信息" / "警告" / "错误" - filters what is rendered, keeps history.
  void setLevelFilter(const QString &level);
  void clear();
  [[nodiscard]] QString allText() const;
  /// {info, warn, error} counts over the whole history (not just the filter).
  [[nodiscard]] std::array<int, 3> levelCounts() const;

private:
  static QString levelOf(const QString &line);
  void rebuild();

  QPlainTextEdit *m_textLog = nullptr;
  QStringList m_history;
  QString m_filter;
};
