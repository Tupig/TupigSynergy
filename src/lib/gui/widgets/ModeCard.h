/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QRadioButton>

// 保留 QRadioButton 的键盘、互斥与辅助功能语义，独立绘制设计稿的标题/说明。
class ModeCard : public QRadioButton
{
  Q_OBJECT
public:
  explicit ModeCard(QWidget *parent = nullptr);
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;
  bool hitButton(const QPoint &position) const override;
};
