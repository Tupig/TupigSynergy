/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogWidget.h"
#include "common/PlatformInfo.h"

#include <gui/Logger.h>

#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QVBoxLayout>

#include <utility>

LogWidget::LogWidget(QWidget *parent) : QWidget{parent}, m_textLog{new QPlainTextEdit(this)}
{
  m_textLog->setReadOnly(true);
  m_textLog->setMaximumBlockCount(10000);
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

  connect(
      synergy::gui::Logger::instance(), &synergy::gui::Logger::newLine, m_textLog, &QPlainTextEdit::appendPlainText
  );
}

void LogWidget::appendLine(const QString &msg)
{
  m_history.append(msg);
  if (m_filter.isEmpty() || levelOf(msg) == m_filter)
    m_textLog->appendPlainText(msg);
}

void LogWidget::setVisibleLines(int lines)
{
  const auto metrics = m_textLog->fontMetrics();
  const auto margins = m_textLog->contentsMargins();
  const auto frame = m_textLog->frameWidth() * 2;
  const auto height = metrics.lineSpacing() * lines + margins.top() + margins.bottom() + frame;
  m_textLog->setFixedHeight(height);
  setFixedHeight(height);
}

void LogWidget::setLevelFilter(const QString &level)
{
  // "全部" (or anything unexpected) means no filtering.
  const auto next = (level == QStringLiteral("信息") || level == QStringLiteral("警告") || level == QStringLiteral("错误"))
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
    return QStringLiteral("警告");
  if (level.startsWith(QLatin1String("ERR")) || level.startsWith(QLatin1String("CRIT")) ||
      level.startsWith(QLatin1String("FATAL")))
    return QStringLiteral("错误");
  return QStringLiteral("信息");
}

void LogWidget::rebuild()
{
  m_textLog->clear();
  for (const auto &line : std::as_const(m_history)) {
    if (m_filter.isEmpty() || levelOf(line) == m_filter)
      m_textLog->appendPlainText(line);
  }
  scrollToBottom();
}

void LogWidget::clear()
{
  m_history.clear();
  m_textLog->clear();
}

QString LogWidget::allText() const
{
  return m_textLog->toPlainText();
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
