/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QTimer>

namespace synergy::gui::compact {

// Native group-box titles sit on the border. Keep the business widget, but
// move its heading into the card's actual content flow.
inline void normalizeCards(QWidget *root)
{
  for (auto *card : root->findChildren<QGroupBox *>()) {
    if (card->isCheckable() || !card->layout() || card->title().isEmpty())
      continue;
    auto *heading = card->findChild<QLabel *>(QStringLiteral("cardHeading"), Qt::FindDirectChildrenOnly);
    if (!heading) {
      auto *body = new QWidget(card);
      body->setObjectName(QStringLiteral("cardBody"));
      body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
      body->setLayout(card->layout());
      body->layout()->setContentsMargins(0, 0, 0, 0);
      auto *flow = new QVBoxLayout(card);
      flow->setContentsMargins(12, 10, 12, 12);
      flow->setSpacing(8);
      heading = new QLabel(card);
      heading->setObjectName(QStringLiteral("cardHeading"));
      heading->setProperty("sectionHeading", true);
      heading->setWordWrap(true);
      flow->addWidget(heading);
      flow->addWidget(body);
    }
    heading->setText(card->title().remove(QLatin1Char('&')));
    card->setTitle(QString());
    if (card->property("cardSection").toBool())
      card->layout()->setContentsMargins(0, 0, 0, 0);
    else
      card->layout()->setContentsMargins(12, 10, 12, 12);
  }
}

inline void combineCards(QWidget *page, const QList<QGroupBox *> &sections, const QString &name)
{
  for (auto *section : sections)
    page->layout()->removeWidget(section);
  delete page->layout();
  auto *pageLayout = new QVBoxLayout(page);
  pageLayout->setContentsMargins(0, 0, 0, 0);
  pageLayout->setSpacing(0);
  auto *card = new QGroupBox(page);
  card->setObjectName(name);
  card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *flow = new QVBoxLayout(card);
  flow->setContentsMargins(12, 10, 12, 12);
  flow->setSpacing(12);
  for (auto *section : sections) {
    section->setProperty("cardSection", true);
    flow->addWidget(section);
  }
  pageLayout->addWidget(card);
  pageLayout->setAlignment(Qt::AlignTop);
}

inline void styleButtons(QDialogButtonBox *box)
{
  for (auto role : {QDialogButtonBox::Save, QDialogButtonBox::Ok})
    if (auto *button = box->button(role))
      button->setProperty("primary", true);
}

// Reference pages opt in individually: Client and trust decisions deliberately
// keep their actions outside their content cards.
inline void placeActions(QGroupBox *card, QDialogButtonBox *box)
{
  if (!card || !card->layout())
    return;
  if (auto *parent = box->parentWidget(); parent && parent->layout())
    parent->layout()->removeWidget(box);
  card->layout()->addWidget(box);
  box->show();
}

inline void setSubtleText(QLabel *label, const QString &text)
{
  label->setProperty("muted", true);
  label->setTextFormat(Qt::RichText);
  label->setWordWrap(true);
  label->setText(QStringLiteral("<p style=\"margin:0; line-height:165%;\">%1</p>")
                     .arg(text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
}

// Lists occupy their real rows, rather than lending an empty viewport its height.
// The label is a presentation overlay; no placeholder item enters the data model.
class ListRows final : public QObject
{
public:
  ListRows(QListWidget *list, const QString &emptyText) : QObject(list->viewport()), m_list(list)
  {
    m_empty = new QLabel(emptyText, list->viewport());
    m_empty->setObjectName(list->objectName() + QStringLiteral("Empty"));
    m_empty->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_empty->setContentsMargins(10, 7, 10, 7);
    m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_empty->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    list->viewport()->installEventFilter(this);
    list->installEventFilter(this);
    list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setIconSize(QSize(14, 14));
    list->setWordWrap(true);
    list->setTextElideMode(Qt::ElideNone);
    // QListWidget's model emits reset signals during destruction. Never inspect
    // rows synchronously from those signals: the viewport may already be gone.
    // The viewport owns this observer; its destruction cancels queued refreshes.
    const auto refresh = [this] { scheduleRows(); };
    connect(list->model(), &QAbstractItemModel::rowsInserted, this, refresh);
    connect(list->model(), &QAbstractItemModel::rowsRemoved, this, refresh);
    connect(list->model(), &QAbstractItemModel::modelReset, this, refresh);
    connect(list->model(), &QAbstractItemModel::dataChanged, this, refresh);
    list->ensurePolished();
    updateRows();
  }

private:
  bool eventFilter(QObject *object, QEvent *event) override
  {
    if (m_empty && object == parent() && event->type() == QEvent::Resize) {
      m_empty->setGeometry(static_cast<QWidget *>(object)->rect());
      scheduleRows();
    }
    if (object == m_list && (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange ||
                             event->type() == QEvent::Polish))
      scheduleRows();
    return QObject::eventFilter(object, event);
  }

  void scheduleRows()
  {
    if (m_pending)
      return;
    m_pending = true;
    QTimer::singleShot(0, this, [this] {
      m_pending = false;
      updateRows();
    });
  }

  void updateRows()
  {
    if (!m_empty)
      return;
    m_list->doItemsLayout();
    const int rows = qMin(5, m_list->count());
    int height = 0;
    for (int row = 0; row < rows; ++row)
      height += qMax(1, m_list->sizeHintForRow(row));
    if (!rows)
      height = m_list->fontMetrics().height() + 14;
    m_list->setFixedHeight(height + 2 * m_list->frameWidth());
    m_empty->setVisible(!rows);
    m_empty->setGeometry(m_list->viewport()->rect());
    m_list->updateGeometry();
  }

  QListWidget *m_list;
  QPointer<QLabel> m_empty;
  bool m_pending = false;
};

inline void fitRows(QListWidget *list, const QString &emptyText)
{
  new ListRows(list, emptyText);
}

} // namespace synergy::gui::compact
