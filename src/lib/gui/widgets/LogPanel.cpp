/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/StyleUtils.h"

#include "LogPanel.h"

#include "LogWidget.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

namespace {

// Keep the full source text/tooltip and accessibility value while painting one line.
class ElidedLogLabel final : public QLabel
{
public:
  using QLabel::QLabel;
  QSize minimumSizeHint() const override
  {
    return {0, fontMetrics().height()};
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    if (fontMetrics().horizontalAdvance(text()) <= contentsRect().width()) {
      QLabel::paintEvent(event);
      return;
    }
    QPainter painter(this);
    painter.setFont(font());
    painter.setPen(palette().color(foregroundRole()));
    painter.drawText(
        contentsRect(), Qt::AlignLeft | Qt::AlignVCenter,
        fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width())
    );
  }
};

QPushButton *iconButton(QWidget *parent, const QString &icon, const QString &tip)
{
  auto *button = new QPushButton(parent);
  button->setProperty("iconButton", true);
  button->setIcon(synergy::gui::appIcon(icon));
  button->setIconSize(QSize(14, 14));
  button->setToolTip(tip);
  button->setAccessibleName(tip);
  return button;
}

} // namespace

LogPanel::LogPanel(QWidget *parent) : QFrame(parent)
{
  setObjectName(QStringLiteral("logPanel"));
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // 展开：工具条 + 记录 + 统计
  m_expanded = new QWidget(this);
  auto *expandedLayout = new QVBoxLayout(m_expanded);
  expandedLayout->setContentsMargins(0, 0, 0, 0);
  expandedLayout->setSpacing(0);

  auto *toolbar = new QWidget(m_expanded);
  auto *toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(12, 5, 12, 5);
  toolbarLayout->setSpacing(6);

  m_title = new QLabel(tr("Log"), toolbar);
  m_title->setObjectName(QStringLiteral("logTitle"));
  toolbarLayout->addWidget(m_title);

  auto *filter = new QFrame(toolbar);
  filter->setObjectName(QStringLiteral("logFilter"));
  auto *filterLayout = new QHBoxLayout(filter);
  filterLayout->setContentsMargins(2, 2, 2, 2);
  filterLayout->setSpacing(2);
  m_levels = new QButtonGroup(this);
  const auto levels = QList<QPair<QString, QString>>{
      {QString(), tr("All")},
      {QStringLiteral("info"), tr("Info")},
      {QStringLiteral("warn"), tr("Warn")},
      {QStringLiteral("error"), tr("Error")},
  };
  for (const auto &[key, label] : levels) {
    auto *chip = new QPushButton(label, filter);
    chip->setCheckable(true);
    chip->setProperty("levelChip", true);
    chip->setProperty("level", key);
    chip->setToolTip(label);
    m_levels->addButton(chip);
    filterLayout->addWidget(chip);
  }
  m_levels->buttons().value(0)->setChecked(true);
  toolbarLayout->addWidget(filter);
  toolbarLayout->addStretch(1);

  // 设计稿：工具条右侧是固定宽度的搜索输入框，而不是折叠图标。
  m_search = new QLineEdit(toolbar);
  m_search->setObjectName(QStringLiteral("logSearch"));
  m_search->setPlaceholderText(tr("Search log"));
  m_search->setMaximumWidth(104);
  m_search->setAccessibleName(tr("Search log"));
  toolbarLayout->addWidget(m_search);

  m_btnCopy = iconButton(toolbar, QStringLiteral("edit-copy"), tr("Copy Log"));
  m_btnClear = iconButton(toolbar, QStringLiteral("edit-clear"), tr("Clear Log"));
  m_btnCollapse = iconButton(toolbar, QStringLiteral("go-down"), tr("Collapse Log"));
  toolbarLayout->addWidget(m_btnCopy);
  toolbarLayout->addWidget(m_btnClear);
  toolbarLayout->addWidget(m_btnCollapse);
  expandedLayout->addWidget(toolbar);

  m_records = new LogWidget(m_expanded);
  expandedLayout->addWidget(m_records);

  auto *foot = new QWidget(m_expanded);
  auto *footLayout = new QHBoxLayout(foot);
  footLayout->setContentsMargins(12, 4, 12, 6);
  footLayout->setSpacing(12);
  m_stats = new QLabel(foot);
  m_stats->setObjectName(QStringLiteral("logStats"));
  m_stats->setTextFormat(Qt::RichText);
  m_hint = new QLabel(tr("Latest 6 · wrapped"), foot);
  m_hint->setObjectName(QStringLiteral("logHint"));
  footLayout->addWidget(m_stats);
  footLayout->addStretch(1);
  footLayout->addWidget(m_hint);
  expandedLayout->addWidget(foot);

  layout->addWidget(m_expanded);

  // 收起：运行徽标 + 最新一条 + 展开
  m_collapsedRow = new QWidget(this);
  auto *collapsedLayout = new QHBoxLayout(m_collapsedRow);
  collapsedLayout->setContentsMargins(10, 12, 10, 12);
  collapsedLayout->setSpacing(8);
  m_badge = new QLabel(tr("Log"), m_collapsedRow);
  m_badge->setObjectName(QStringLiteral("logBadge"));
  m_tail = new ElidedLogLabel(m_collapsedRow);
  m_tail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  m_tail->setMinimumWidth(0);
  m_tail->setWordWrap(false);
  m_tail->setObjectName(QStringLiteral("logTail"));
  m_tail->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_btnExpand = iconButton(m_collapsedRow, QStringLiteral("go-up"), tr("Expand Log"));
  collapsedLayout->addWidget(m_badge);
  collapsedLayout->addWidget(m_tail, 1);
  collapsedLayout->addWidget(m_btnExpand);
  m_collapsedRow->setVisible(false);
  layout->addWidget(m_collapsedRow);

  for (auto *chip : m_levels->buttons()) {
    connect(chip, &QPushButton::clicked, this, [this, chip] {
      m_records->setLevelFilter(chip->property("level").toString());
    });
  }
  connect(m_search, &QLineEdit::returnPressed, this, [this] { m_records->findNext(m_search->text()); });
  connect(m_btnCopy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_records->allText()); });
  connect(m_btnClear, &QPushButton::clicked, this, [this] {
    m_records->clear();
    updateStats();
  });
  connect(m_btnCollapse, &QPushButton::clicked, this, [this] { setCollapsed(true); });
  connect(m_btnExpand, &QPushButton::clicked, this, [this] { setCollapsed(false); });
  connect(m_records, &LogWidget::historyChanged, this, [this] {
    const auto latest = m_records->latestDisplayText();
    m_tail->setText(latest);
    m_tail->setToolTip(latest);
    updateStats();
  });

  setCollapsed(false);
  updateStats();
}

void LogPanel::appendLine(const QString &msg)
{
  m_records->appendLine(msg);
}

void LogPanel::setCollapsed(bool collapsed)
{
  if (m_collapsed == collapsed && m_expanded->isVisibleTo(this) == !collapsed)
    return;
  m_collapsed = collapsed;
  m_expanded->setVisible(!collapsed);
  m_collapsedRow->setVisible(collapsed);
  Q_EMIT collapsedChanged(collapsed);
}

bool LogPanel::isCollapsed() const
{
  return m_collapsed;
}

void LogPanel::updateStats()
{
  const auto counts = m_records->levelCounts();
  m_stats->setText(QStringLiteral(
                       "<span style='color:#7aa2f7'>●</span> %1&nbsp;&nbsp;"
                       "<span style='color:#e0a83c'>●</span> %2&nbsp;&nbsp;"
                       "<span style='color:#e0605f'>●</span> %3"
  )
                       .arg(counts[0])
                       .arg(counts[1])
                       .arg(counts[2]));
}

void LogPanel::changeEvent(QEvent *e)
{
  QFrame::changeEvent(e);
  if (e->type() == QEvent::LanguageChange)
    updateText();
}

void LogPanel::updateText()
{
  m_title->setText(tr("Log"));
  m_hint->setText(tr("Latest 6 · wrapped"));
  m_badge->setText(tr("Log"));
  m_btnCopy->setToolTip(tr("Copy Log"));
  m_btnClear->setToolTip(tr("Clear Log"));
  m_btnCollapse->setToolTip(tr("Collapse Log"));
  m_btnExpand->setToolTip(tr("Expand Log"));
  m_search->setPlaceholderText(tr("Search log"));
  m_search->setAccessibleName(tr("Search log"));
  for (auto *button : {m_btnCopy, m_btnClear, m_btnCollapse, m_btnExpand})
    button->setAccessibleName(button->toolTip());
  const auto labels = QStringList{tr("All"), tr("Info"), tr("Warn"), tr("Error")};
  const auto buttons = m_levels->buttons();
  for (int i = 0; i < buttons.size() && i < labels.size(); ++i) {
    buttons.at(i)->setText(labels.at(i));
    buttons.at(i)->setToolTip(labels.at(i));
  }
}
