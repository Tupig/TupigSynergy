/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QWidget>

class QDialog;
class QStackedWidget;

/**
 * @brief 设计稿的内容区：主页与子页面共用同一窗口区域。
 *
 * 页面栈由 MainWindow.ui 声明（objectName 为 pageStack）；这里只负责把编辑器
 * 包装成可滚动的子页，并在页面关闭后回到上一页。
 */
class PageHost : public QWidget
{
  Q_OBJECT
public:
  explicit PageHost(QWidget *parent = nullptr);

  void push(QDialog *editor);
  static void present(QWidget *origin, QDialog *editor);
  [[nodiscard]] bool hasEditor() const;
  bool activateEditor(const QMetaObject &type);

Q_SIGNALS:
  void editorVisibilityChanged(bool visible);
  void titleChanged(const QString &title);

private:
  QStackedWidget *m_stack = nullptr;
  QStackedWidget *stack();
  bool m_editorVisible = false;
  void updateNavigation();
};
