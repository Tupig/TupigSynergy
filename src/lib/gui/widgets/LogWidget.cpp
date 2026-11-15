/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogWidget.h"
#include "common/PlatformInfo.h"

#include <gui/Logger.h>

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFontDatabase>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <utility>

namespace {
constexpr int kHistoryLimit = 10000;
// 历史可能很长，但只渲染最近的记录；更早的仍参与筛选与统计。
constexpr int kRenderLimit = 500;

QString levelColor(const QString &level)
{
  if (level == QStringLiteral("warn"))
    return QStringLiteral("#e0a83c");
  if (level == QStringLiteral("error"))
    return QStringLiteral("#e0605f");
  return QStringLiteral("#7aa2f7");
}
} // namespace

LogWidget::LogWidget(QWidget *parent) : QWidget{parent}, m_textLog{new QTextEdit(this)}
{
  m_textLog->setObjectName(QStringLiteral("logRecords"));
  m_textLog->setReadOnly(true);
  m_textLog->setUndoRedoEnabled(false);
  m_textLog->viewport()->installEventFilter(this);
  // 记录按 时间 / 级别 / 消息 三列显示，长消息自动换行。
  m_textLog->setLineWrapMode(QTextEdit::WidgetWidth);

  if (synergy::platform::isWindows()) {
    QFont f = font();
    f.setFamilies({"Hack", "Liberation Mono", "Monospace", "Andale Mono"});
    f.setStyleHint(QFont::Monospace);
    m_textLog->setFont(f);
  } else {
    m_textLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    if (synergy::platform::isMac()) {
      auto f = m_textLog->font();
      f.setPixelSize(12);
      m_textLog->setFont(f);
    }
  }

  auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_textLog);
  setLayout(layout);

  connect(synergy::gui::Logger::instance(), &synergy::gui::Logger::newLine, this, &LogWidget::appendLine);
}

QString LogWidget::recordHtml(const QString &line)
{
  // "[2026-11-15T00:52:25.518] INFO: message"
  QString time;
  QString level = QStringLiteral("INFO");
  QString message = line;
  const auto close = line.indexOf(QLatin1String("] "));
  if (close > 0 && line.startsWith(QLatin1Char('['))) {
    time = line.mid(1, close - 1);
    if (time.size() >= 19)
      time = time.mid(11, 8);
    const auto colon = line.indexOf(QLatin1Char(':'), close + 2);
    if (colon > 0) {
      level = line.mid(close + 2, colon - close - 2).trimmed();
      message = line.mid(colon + 1).trimmed();
    }
  }
  const auto color = levelColor(levelOf(line));
  return QStringLiteral(
             "<span style='color:#6f6f78'>%1</span>&nbsp;&nbsp;"
             "<span style='color:%2'><b>%3</b></span>&nbsp;&nbsp;"
             "<span style='color:#cfcfd6'>%4</span>"
  )
      .arg(
          time.isEmpty() ? QStringLiteral("&nbsp;") : time.toHtmlEscaped(), color, level.toHtmlEscaped(),
          message.toHtmlEscaped()
      );
}

void LogWidget::appendLine(const QString &msg)
{
  if (static_cast<int>(m_history.size()) == kHistoryLimit) {
    const auto &oldest = m_history.front();
    --m_counts[levelIndex(oldest)];
    if (m_filter.isEmpty() || levelOf(oldest) == m_filter)
      m_textLog->clear();
    m_history.pop_front();
  }
  m_history.push_back(msg);
  ++m_counts[levelIndex(msg)];
  if (m_filter.isEmpty() || levelOf(msg) == m_filter) {
    m_textLog->append(recordHtml(msg));
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
  if (watched == m_textLog->viewport() && event->type() == QEvent::Resize) {
    QTimer::singleShot(0, this, &LogWidget::updateVisibleHeight);
  }
  return QWidget::eventFilter(watched, event);
}

void LogWidget::updateVisibleHeight()
{
  auto block = m_textLog->document()->lastBlock();
  qreal height = 0;
  int records = 0;
  while (block.isValid() && records < m_visibleLines) {
    height += m_textLog->document()->documentLayout()->blockBoundingRect(block).height();
    ++records;
    block = block.previous();
  }
  // 空态不预留六行空白；有记录时最多显示六条，超长记录仍在视口内滚动。
  if (records == 0)
    height = m_textLog->fontMetrics().lineSpacing();
  height += 2 * (m_textLog->document()->documentMargin() + m_textLog->frameWidth());
  setFixedHeight(qMin(qCeil(height), 236));
}

void LogWidget::setLevelFilter(const QString &level)
{
  // Canonical, language-independent keys: "" (all) / "info" / "warn" / "error".
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
  m_textLog->clear();
  m_latestLine.clear();
  QString html;
  int rendered = 0;
  for (auto it = m_history.rbegin(); it != m_history.rend() && rendered < kRenderLimit; ++it) {
    if (m_filter.isEmpty() || levelOf(*it) == m_filter) {
      html.prepend(recordHtml(*it) + QStringLiteral("<br/>"));
      m_latestLine = *it;
      ++rendered;
    }
  }
  m_textLog->setHtml(html);
  updateVisibleHeight();
  scrollToBottom();
  Q_EMIT historyChanged(m_latestLine);
}

void LogWidget::clear()
{
  m_history.clear();
  m_latestLine.clear();
  m_counts = {0, 0, 0};
  m_textLog->clear();
  updateVisibleHeight();
  Q_EMIT historyChanged(m_latestLine);
}

QString LogWidget::allText() const
{
  return m_textLog->toPlainText();
}

std::array<int, 3> LogWidget::levelCounts() const
{
  return m_counts;
}

void LogWidget::findNext(const QString &text)
{
  if (text.isEmpty())
    return;
  if (!m_textLog->find(text)) {
    m_textLog->moveCursor(QTextCursor::Start);
    m_textLog->find(text);
  }
}

void LogWidget::findPrevious(const QString &text)
{
  if (text.isEmpty())
    return;
  if (!m_textLog->find(text, QTextDocument::FindBackward)) {
    m_textLog->moveCursor(QTextCursor::End);
    m_textLog->find(text, QTextDocument::FindBackward);
  }
}

void LogWidget::scrollToBottom() const
{
  auto *sb = m_textLog->verticalScrollBar();
  sb->setValue(sb->maximum());
}
