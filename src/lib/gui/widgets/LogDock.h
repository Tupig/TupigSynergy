/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDockWidget>

class LogWidget;
class QButtonGroup;
class QLabel;
class QPushButton;
class SearchWidget;

class LogDock : public QDockWidget
{
  Q_OBJECT
public:
  explicit LogDock(QWidget *parent = nullptr);
  void appendLine(const QString &msg);
  void setFloating(bool floating);

  /// Expanded fits the latest six records; collapsed keeps the latest matching record.
  void setCollapsed(bool collapsed);
  [[nodiscard]] bool isCollapsed() const;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void changeEvent(QEvent *e) override;

private:
  void updateText();

  LogWidget *m_textLog = nullptr;
  QPushButton *m_btnClose = nullptr;
  QPushButton *m_btnFloat = nullptr;
  QPushButton *m_btnCollapse = nullptr;
  QPushButton *m_btnCopy = nullptr;
  QPushButton *m_btnClear = nullptr;
  QLabel *m_lblTitle = nullptr;
  QLabel *m_lblTail = nullptr;
  QLabel *m_lblStats = nullptr;
  SearchWidget *m_searchWidget = nullptr;
  QButtonGroup *m_levelGroup = nullptr;
  QWidget *m_bodyWidget = nullptr;
  QWidget *m_tailWidget = nullptr;
  QWidget *m_statsWidget = nullptr;
  void updateStats();
  bool m_collapsed = false;
};
