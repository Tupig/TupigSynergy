/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QWidget>

class QDialog;
class QLabel;
class QPushButton;
class QStackedWidget;

// 编辑器保留原有 accept/reject 事务，导航仅负责展示与生命周期。
class PageHost : public QWidget
{
  Q_OBJECT
public:
  explicit PageHost(QWidget *home, QWidget *parent = nullptr);
  void push(QDialog *editor);
  bool activateEditor(const QMetaObject &type);
  static void present(QWidget *origin, QDialog *editor);
  [[nodiscard]] bool hasEditor() const;

Q_SIGNALS:
  void editorVisibilityChanged(bool visible);

private:
  bool m_editorVisible = false;
  QStackedWidget *m_stack;
  QLabel *m_title;
  QPushButton *m_back;
  void updateNavigation();
};
