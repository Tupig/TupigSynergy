/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MessagePage.h"
#include "gui/StyleUtils.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

MessagePage::MessagePage(QWidget *parent, Kind kind, const QString &title, const QString &message, bool richText)
    : QDialog(parent)
{
  setWindowTitle(title);
  setMinimumWidth(420);

  auto *card = new QFrame(this);
  card->setObjectName(QStringLiteral("card"));
  card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(12, 12, 12, 12);
  cardLayout->setSpacing(8);

  auto *header = new QWidget(card);
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(12);

  auto *icon = new QLabel(header);
  icon->setObjectName(QStringLiteral("messageIcon"));
  icon->setFixedSize(32, 32);
  const auto theme = kind == Kind::Question  ? QStringLiteral("dialog-question")
                     : kind == Kind::Warning ? QStringLiteral("dialog-warning")
                                             : QStringLiteral("dialog-information");
  icon->setPixmap(synergy::gui::appIcon(theme).pixmap(QSize(32, 32)));
  headerLayout->addWidget(icon, 0, Qt::AlignTop);

  auto *body = new QWidget(header);
  auto *bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(6);
  auto *heading = new QLabel(title, body);
  heading->setObjectName(QStringLiteral("messageTitle"));
  heading->setWordWrap(true);
  heading->setTextFormat(Qt::PlainText);
  auto headingFont = heading->font();
  headingFont.setBold(true);
  heading->setFont(headingFont);
  bodyLayout->addWidget(heading);
  auto *text = new QLabel(message, body);
  text->setObjectName(QStringLiteral("messageText"));
  text->setWordWrap(true);
  text->setTextFormat(richText ? Qt::RichText : Qt::PlainText);
  text->setTextInteractionFlags(Qt::TextSelectableByMouse);
  bodyLayout->addWidget(text);
  headerLayout->addWidget(body, 1);
  cardLayout->addWidget(header);

  m_actions = new QWidget(card);
  auto *actionsLayout = new QHBoxLayout(m_actions);
  actionsLayout->setContentsMargins(0, 0, 0, 0);
  actionsLayout->setSpacing(8);
  actionsLayout->addStretch(1);
  cardLayout->addWidget(m_actions);

  m_contentLayout = cardLayout;
  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(10, 10, 10, 10);
  m_layout->setSpacing(8);
  m_layout->addWidget(card);
}

void MessagePage::addContentWidget(QWidget *widget)
{
  m_contentLayout->insertWidget(m_contentLayout->count() - 1, widget);
}

QCheckBox *MessagePage::addCheckBox(const QString &text)
{
  auto *checkBox = new QCheckBox(text, this);
  addContentWidget(checkBox);
  return checkBox;
}

QPushButton *MessagePage::addButton(const QString &text, bool primary, bool danger)
{
  auto *button = new QPushButton(text, m_actions);
  if (primary)
    button->setProperty("primary", true);
  if (danger)
    button->setProperty("danger", true);
  static_cast<QHBoxLayout *>(m_actions->layout())->addWidget(button);
  return button;
}
