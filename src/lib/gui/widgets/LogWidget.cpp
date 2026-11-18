/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogWidget.h"
#include "common/LogLevel.h"
#include "gui/StyleUtils.h"

#include <gui/Logger.h>

#include <QEvent>
#include <QFontDatabase>
#include <QHeaderView>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextLayout>
#include <QTextOption>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <utility>

namespace {
constexpr int kHistoryLimit = 10000;
// 筛选后仍保留完整的有界历史，避免搜索或复制静默丢失早期记录。
constexpr int kRenderLimit = kHistoryLimit;

QColor levelColor(const QString &level)
{
  if (level == QStringLiteral("warn"))
    return QColor(QStringLiteral("#e0a83c"));
  if (level == QStringLiteral("error"))
    return QColor(QStringLiteral("#e0605f"));
  return QColor(QStringLiteral("#7aa2f7"));
}
} // namespace

LogWidget::LogWidget(QWidget *parent) : QWidget{parent}, m_table{new QTableWidget(this)}
{
  m_table->setObjectName(QStringLiteral("logRecords"));
  m_table->setColumnCount(3);
  m_table->setShowGrid(false);
  m_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setFocusPolicy(Qt::NoFocus);
  m_table->setWordWrap(true);
  m_table->setTextElideMode(Qt::ElideNone);
  m_table->verticalHeader()->setMinimumSectionSize(0);
  m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_table->horizontalHeader()->hide();
  m_table->verticalHeader()->hide();
  // 时间与级别按内容取宽，避免等宽字体下的省略号；消息列占满剩余宽度。
  m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  m_table->viewport()->installEventFilter(this);
  auto *scrollBar = m_table->verticalScrollBar();
  connect(scrollBar, &QScrollBar::valueChanged, this, [this, scrollBar](int value) {
    // User scrolling and search may pause following; content/layout adjustments may not.
    if (!m_adjustingContents)
      m_followTail = value == scrollBar->maximum();
  });
  connect(scrollBar, &QScrollBar::rangeChanged, this, [this] { requestTailScroll(); });
  connect(m_table->horizontalHeader(), &QHeaderView::sectionResized, this, [this](int column, int, int newWidth) {
    if (column == 2) {
      m_messageColumnWidth = qMax(1, newWidth);
      QTimer::singleShot(0, this, [this] { updateVisibleHeight(); });
    }
  });

  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  font.setPixelSize(12);
  m_table->setFont(font);

  auto *layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_table);
  setLayout(layout);
  m_messageColumnWidth = qMax(1, m_table->columnWidth(2));
  m_table->installEventFilter(this);

  connect(synergy::gui::Logger::instance(), &synergy::gui::Logger::newLine, this, &LogWidget::appendLine);
}

int LogWidget::rowHeight() const
{
  // 设计稿行高 1.55：字号 12 时约 19px。
  return qRound(m_table->fontMetrics().height() * 1.55);
}

LogWidget::Record LogWidget::parse(const QString &line)
{
  // "[2026-11-15T00:52:25.518] INFO: message"
  Record record;
  record.message = line;
  const auto close = line.indexOf(QLatin1String("] "));
  if (close > 0 && line.startsWith(QLatin1Char('['))) {
    record.time = line.mid(1, close - 1);
    if (record.time.size() >= 19 && record.time.at(10) == QLatin1Char('T')) {
      record.time.truncate(19);
      record.time[10] = QLatin1Char(' ');
    }
    const auto colon = line.indexOf(QLatin1Char(':'), close + 2);
    if (colon > 0) {
      record.level = line.mid(close + 2, colon - close - 2).trimmed();
      const auto level = LogLevel::indexOfOption(record.level);
      if (level >= 0)
        record.level = LogLevel::toString(static_cast<int>(level));
      else if (record.level == QStringLiteral("CRITICAL"))
        record.level = tr("Critical");
      record.message = line.mid(colon + 1).trimmed();
    }
  }
  return record;
}

void LogWidget::appendLine(const QString &msg)
{
  const QScopedValueRollback<bool> adjusting(m_adjustingContents, true);
  if (static_cast<int>(m_history.size()) == kHistoryLimit) {
    const auto &oldest = m_history.front();
    --m_counts[levelIndex(oldest)];
    if (m_filter.isEmpty() || levelOf(oldest) == m_filter)
      m_table->removeRow(0);
    m_history.pop_front();
  }
  m_history.push_back(msg);
  ++m_counts[levelIndex(msg)];
  if (m_filter.isEmpty() || levelOf(msg) == m_filter) {
    const auto record = parse(msg);
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setRowHeight(row, rowHeight());

    auto *time = new QTableWidgetItem(record.time);
    time->setForeground(QColor(QStringLiteral("#6f6f78")));
    auto *level = new QTableWidgetItem(record.level);
    level->setForeground(levelColor(levelOf(msg)));
    auto *message = new QTableWidgetItem(record.message);
    message->setForeground(QColor(synergy::gui::isDarkMode() ? QStringLiteral("#cfcfd6") : QStringLiteral("#3a3a40")));
    m_table->setItem(row, 0, time);
    m_table->setItem(row, 1, level);
    m_table->setItem(row, 2, message);
    m_latestLine = msg;
  }
  updateVisibleHeight();
  Q_EMIT historyChanged(m_latestLine);
}

void LogWidget::setVisibleLines(int lines)
{
  m_visibleLines = qMax(1, lines);
  updateVisibleHeight();
}

bool LogWidget::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_table->viewport() && event->type() == QEvent::Resize) {
    const auto *resize = static_cast<QResizeEvent *>(event);
    updateVisibleHeight(resize->size().width() != resize->oldSize().width());
  } else if (watched == m_table &&
             (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange)) {
    updateVisibleHeight(true);
  }
  return QWidget::eventFilter(watched, event);
}

void LogWidget::updateVisibleHeight(bool refreshColumnWidth)
{
  const QScopedValueRollback<bool> adjusting(m_adjustingContents, true);
  // 根据实际列宽重算换行高度，不能只打开 wordWrap 后仍固定单行高度。
  const int count = m_table->rowCount();
  // Querying sectionSize flushes native ResizeToContents work. Keep that out of
  // bulk append, while small logs and explicit geometry changes stay immediate.
  if (refreshColumnWidth || (count > 0 && count <= m_visibleLines))
    m_messageColumnWidth = qMax(1, m_table->columnWidth(2));
  const int width = qMax(1, m_messageColumnWidth - 16);
  int height = 0;
  for (int row = qMax(0, count - m_visibleLines); row < count; ++row) {
    const auto *item = m_table->item(row, 2);
    if (!item)
      continue;
    // Qt 的表格 sizeHint 在不同平台会忽略长单词换行，显式按消息列宽排版。
    qreal textHeight = 0;
    const auto paragraphs = item->text().split(QLatin1Char('\n'));
    for (const auto &paragraph : paragraphs) {
      QTextLayout layout(paragraph, m_table->font());
      QTextOption option;
      option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
      layout.setTextOption(option);
      layout.beginLayout();
      while (true) {
        auto line = layout.createLine();
        if (!line.isValid())
          break;
        line.setLineWidth(width);
        textHeight += line.height();
      }
      layout.endLayout();
      if (paragraph.isEmpty())
        textHeight += m_table->fontMetrics().height();
    }
    const int measured = qMax(rowHeight(), qCeil(textHeight) + 8);
    m_table->setRowHeight(row, qMin(measured, 65535));
    height += m_table->rowHeight(row);
  }
  const int rows = qMin(count, m_visibleLines);
  height = rows == 0 ? 0 : qMin(height + 2, 236);
  // 空态完全隐藏记录区，避免留下一条边框线。
  m_table->setVisible(rows > 0);
  m_table->setFixedHeight(height);
  setFixedHeight(height);
  requestTailScroll();
}

void LogWidget::setLevelFilter(const QString &level)
{
  const auto next =
      (level == QStringLiteral("info") || level == QStringLiteral("warn") || level == QStringLiteral("error"))
          ? level
          : QString();
  if (next == m_filter)
    return;
  m_filter = next;
  rebuild();
}

QString LogWidget::levelOf(const QString &line)
{
  const auto close = line.indexOf(QLatin1String("] "));
  if (close < 0)
    return {};
  const auto start = close + 2;
  const auto colon = line.indexOf(QLatin1Char(':'), start);
  if (colon < 0)
    return {};
  const auto level = line.mid(start, colon - start).trimmed();
  if (level.startsWith(QLatin1String("WARN")))
    return QStringLiteral("warn");
  if (level.startsWith(QLatin1String("ERR")) || level.startsWith(QLatin1String("CRIT")) ||
      level.startsWith(QLatin1String("FATAL")))
    return QStringLiteral("error");
  return QStringLiteral("info");
}

int LogWidget::levelIndex(const QString &line)
{
  const auto level = levelOf(line);
  return level == QStringLiteral("warn") ? 1 : level == QStringLiteral("error") ? 2 : 0;
}

void LogWidget::rebuild()
{
  const QScopedValueRollback<bool> adjusting(m_adjustingContents, true);
  m_table->setRowCount(0);
  m_latestLine.clear();
  QList<QString> visible;
  for (const auto &line : m_history) {
    if (m_filter.isEmpty() || levelOf(line) == m_filter)
      visible.append(line);
  }
  if (!visible.isEmpty())
    m_latestLine = visible.last();
  const int start = qMax(0, visible.size() - kRenderLimit);
  m_table->setRowCount(visible.size() - start);
  for (int index = start; index < visible.size(); ++index) {
    const auto record = parse(visible.at(index));
    const int row = index - start;
    m_table->setRowHeight(row, rowHeight());
    auto *time = new QTableWidgetItem(record.time);
    time->setForeground(QColor(QStringLiteral("#6f6f78")));
    auto *level = new QTableWidgetItem(record.level);
    level->setForeground(levelColor(levelOf(visible.at(index))));
    auto *message = new QTableWidgetItem(record.message);
    message->setForeground(QColor(synergy::gui::isDarkMode() ? QStringLiteral("#cfcfd6") : QStringLiteral("#3a3a40")));
    m_table->setItem(row, 0, time);
    m_table->setItem(row, 1, level);
    m_table->setItem(row, 2, message);
  }
  updateVisibleHeight(true);
  scrollToBottom();
  Q_EMIT historyChanged(m_latestLine);
}

void LogWidget::clear()
{
  const QScopedValueRollback<bool> adjusting(m_adjustingContents, true);
  m_followTail = true;
  m_history.clear();
  m_latestLine.clear();
  m_counts = {0, 0, 0};
  m_table->setRowCount(0);
  updateVisibleHeight();
  Q_EMIT historyChanged(m_latestLine);
}

QString LogWidget::allText() const
{
  QStringList lines;
  lines.reserve(m_table->rowCount());
  for (const auto &line : m_history) {
    if (m_filter.isEmpty() || levelOf(line) == m_filter)
      lines.append(line);
  }
  return lines.join(QLatin1Char('\n'));
}

std::array<int, 3> LogWidget::levelCounts() const
{
  return m_counts;
}

void LogWidget::findNext(const QString &text)
{
  if (text.isEmpty() || m_table->rowCount() == 0)
    return;
  const int current = m_table->currentRow();
  for (int offset = 1; offset <= m_table->rowCount(); ++offset) {
    const int row = (qMax(0, current) + offset) % m_table->rowCount();
    if (m_table->item(row, 2) && m_table->item(row, 2)->text().contains(text)) {
      m_table->setCurrentCell(row, 2);
      m_table->scrollToItem(m_table->item(row, 2));
      return;
    }
  }
}

void LogWidget::findPrevious(const QString &text)
{
  if (text.isEmpty() || m_table->rowCount() == 0)
    return;
  const int current = qMax(0, m_table->currentRow());
  for (int offset = 1; offset <= m_table->rowCount(); ++offset) {
    const int row = (current - offset + m_table->rowCount() * 2) % m_table->rowCount();
    if (m_table->item(row, 2) && m_table->item(row, 2)->text().contains(text)) {
      m_table->setCurrentCell(row, 2);
      m_table->scrollToItem(m_table->item(row, 2));
      return;
    }
  }
}

void LogWidget::requestTailScroll()
{
  if (!m_followTail || m_tailScrollPending)
    return;
  m_tailScrollPending = true;
  // Wait for Qt's pending row/viewport layout, including appends while hidden.
  QTimer::singleShot(0, this, [this] {
    if (m_followTail)
      scrollToBottom();
    m_tailScrollPending = false;
  });
}

void LogWidget::scrollToBottom() const
{
  m_followTail = true;
  auto *bar = m_table->verticalScrollBar();
  bar->setValue(bar->maximum());
}
