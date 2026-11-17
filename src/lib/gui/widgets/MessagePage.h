/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

/**
 * @brief 设计稿的提示页：图标 + 说明 + 右对齐操作按钮，可附带“不再提示”。
 *
 * 用于嵌入主窗口内容区的提示；系统级致命错误仍使用原生对话框。
 */
class MessagePage : public QDialog
{
  Q_OBJECT
public:
  enum class Kind
  {
    Information,
    Warning,
    Question
  };

  explicit MessagePage(QWidget *parent, Kind kind, const QString &title, const QString &message, bool richText = false);

  void addContentWidget(QWidget *widget);
  QCheckBox *addCheckBox(const QString &text);
  QPushButton *addButton(const QString &text, bool primary = false, bool danger = false);

private:
  QVBoxLayout *m_contentLayout = nullptr;
  QVBoxLayout *m_layout = nullptr;
  QWidget *m_actions = nullptr;
};
