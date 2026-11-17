/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "ModeCard.h"

#include "gui/StyleUtils.h"

#include <QPaintEvent>
#include <QPainter>

ModeCard::ModeCard(QWidget *parent) : QRadioButton(parent)
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setFixedHeight(56);
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_Hover);
}

QSize ModeCard::sizeHint() const
{
  return {230, 56};
}

QSize ModeCard::minimumSizeHint() const
{
  return {180, 56};
}

bool ModeCard::hitButton(const QPoint &position) const
{
  return rect().contains(position);
}

void ModeCard::paintEvent(QPaintEvent *)
{
  const bool dark = synergy::gui::isDarkMode();
  const QColor accent(dark ? QStringLiteral("#4cc38a") : QStringLiteral("#2f9e6b"));
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(isChecked() ? QPen(QColor(76, 195, 138, 140), 1) : QPen(Qt::NoPen));
  painter.setBrush(
      isChecked() ? QColor(76, 195, 138, 38) : QColor(dark ? QStringLiteral("#28292e") : QStringLiteral("#f0f0f3"))
  );
  painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 9, 9);
  painter.setPen(isChecked() ? QPen(accent) : QPen(QColor(QStringLiteral("#777980"))));
  painter.setBrush(isChecked() ? accent : Qt::transparent);
  painter.drawEllipse(QPointF(17, 16), 7, 7);
  if (isChecked()) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(17, 16), 2.5, 2.5);
  }
  const bool server = objectName() == QStringLiteral("rbModeServer");
  QFont titleFont = font();
  titleFont.setPixelSize(13);
  titleFont.setWeight(QFont::DemiBold);
  painter.setFont(titleFont);
  painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
  const QString title = text();
  painter.drawText(QRect(32, 4, width() - 42, 20), Qt::AlignLeft | Qt::AlignVCenter, title);
  QFont subtitle = font();
  subtitle.setPixelSize(11);
  painter.setFont(subtitle);
  painter.setPen(QColor(dark ? QStringLiteral("#9a9aa0") : QStringLiteral("#62626a")));
  painter.drawText(
      QRect(32, 25, width() - 42, 28), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
      server ? tr("Share this computer's keyboard and mouse") : tr("Use another computer's keyboard and mouse")
  );
  if (hasFocus()) {
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(accent, 1, Qt::DashLine));
    painter.drawRoundedRect(QRectF(rect()).adjusted(3, 3, -3, -3), 7, 7);
  }
}
