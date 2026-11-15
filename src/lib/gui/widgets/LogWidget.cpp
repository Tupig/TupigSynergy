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
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <utility>

LogWidget::LogWidget(QWidget *parent) : QWidget{parent}, m_textLog{new QPlainTextEdit(this)}
{
  m_textLog->setReadOnly(true);
  m_textLog->setUndoRedoEnabled(false);
  m_textLog->viewport()->installEventFilter(this);
  // Wrap long lines instead of showing a horizontal scrollbar: log messages are
  // sentences, and reading them normally beats scrolling sideways.
  m_textLog->setLineWrapMode(QPlainTextEdit::WidgetWidth);

  // setup the log font
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

void LogWidget::appendLine(const QString &msg)
{
  constexpr size_t kHistoryLimit = 10000;
  if (m_history.size() == kHistoryLimit) {
    const auto &oldest = m_history.front();
    --m_counts[levelIndex(oldest)];
    if (m_filter.isEmpty() || levelOf(oldest) == m_filter) {
      const int blocks = m_visibleBlockCounts.front();
      m_visibleBlockCounts.pop_front();
      if (m_visibleBlockCounts.empty()) {
        m_textLog->clear();
        m_latestLine.clear();
      } else {
        QTextCursor cursor(m_textLog->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, blocks);
        cursor.removeSelectedText();
      }
    }
    m_history.pop_front();
  }
  m_history.push_back(msg);
  ++m_counts[levelIndex(msg)];
  if (m_filter.isEmpty() || levelOf(msg) == m_filter) {
    m_textLog->appendPlainText(msg);
    m_visibleBlockCounts.push_back(msg.count(QLatin1Char('\n')) + 1);
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
  for (auto it = m_visibleBlockCounts.rbegin(); it != m_visibleBlockCounts.rend() && records < m_visibleLines;
       ++it, ++records) {
    for (int i = 0; i < *it && block.isValid(); ++i, block = block.previous()) {
      height += m_textLog->document()->documentLayout()->blockBoundingRect(block).height();
    }
  }
  height += (m_visibleLines - records) * m_textLog->fontMetrics().lineSpacing();
  height += 2 * (m_textLog->document()->documentMargin() + m_textLog->frameWidth());
  // 六条是记录数量而非无限高度；超长记录仍在文本视口内滚动。
  setFixedHeight(qMin(qCeil(height), 236));
}

void LogWidget::setLevelFilter(const QString &level)
{
  // Canonical, language-independent keys: "" (all) / "info" / "warn" / "error".
  // Using the translated label here would break filtering in every non-Chinese UI.
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
  // "[2026-11-15T00:52:25.518] INFO: message"
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
  m_visibleBlockCounts.clear();
  m_latestLine.clear();
  for (const auto &line : std::as_const(m_history)) {
    if (m_filter.isEmpty() || levelOf(line) == m_filter) {
      m_textLog->appendPlainText(line);
      m_visibleBlockCounts.push_back(line.count(QLatin1Char('\n')) + 1);
      m_latestLine = line;
    }
  }
  updateVisibleHeight();
  scrollToBottom();
  Q_EMIT historyChanged(m_latestLine);
}

void LogWidget::clear()
{
  m_history.clear();
  m_visibleBlockCounts.clear();
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
  auto sb = m_textLog->verticalScrollBar();
  sb->setValue(sb->maximum());
}
