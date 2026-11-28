/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/LogOutputters.h"
#include "arch/Arch.h"

#include <algorithm>
#include <iostream>

#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QString>

constexpr qint64 kSegmentSizeLimit = 10 * 1024 * 1024;
constexpr int kSegmentCount = 10;
constexpr int kSegmentRetentionDays = 14;

constexpr auto s_logFileSizeLimit = 1024 * 1024; //!< Max Log size before rotating (1Mb)

//! How many bytes to write before checking the log size again.  Checking on
//! every line is a stat-like syscall per line; the exact rotation point may
//! overshoot by up to this amount, which is irrelevant for a log.
constexpr auto s_logFileSizeCheckBytes = 64 * 1024;

//
// StopLogOutputter
//

void StopLogOutputter::open(const QString &)
{
  // do nothing
}

void StopLogOutputter::close()
{
  // do nothing
}

bool StopLogOutputter::write(LogLevel::Level, const QString &)
{
  return false;
}

//
// ConsoleLogOutputter
//

void ConsoleLogOutputter::open(const QString &)
{
  // do nothing
}

void ConsoleLogOutputter::close()
{
  // do nothing
}

bool ConsoleLogOutputter::write(LogLevel::Level level, const QString &msg)
{
  using enum LogLevel::Level;
  // std::endl already flushes the target stream
  const auto utf8 = msg.toUtf8();
  if ((level >= Fatal) && (level <= Warning))
    std::cerr << utf8.constData() << std::endl;
  else
    std::cout << utf8.constData() << std::endl;
  return true;
}

void ConsoleLogOutputter::flush() const
{
  // do nothing
}

//
// SystemLogOutputter
//

void SystemLogOutputter::open(const QString &title)
{
  ARCH->openLog(title);
}

void SystemLogOutputter::close()
{
  ARCH->closeLog();
}

bool SystemLogOutputter::write(LogLevel::Level level, const QString &msg)
{
  ARCH->writeLog(level, msg);
  return true;
}

//
// SystemLogger
//

SystemLogger::SystemLogger(const QString &title, bool blockConsole)
{
  // redirect log messages
  if (blockConsole) {
    m_stop = new StopLogOutputter; // NOSONAR - Adopted by `Log`
    CLOG->insert(m_stop);
  }
  m_syslog = new SystemLogOutputter; // NOSONAR - Adopted by `Log`
  m_syslog->open(title);
  CLOG->insert(m_syslog);
}

SystemLogger::~SystemLogger()
{
  CLOG->remove(m_syslog);
  delete m_syslog;
  if (m_stop != nullptr) {
    CLOG->remove(m_stop);
    delete m_stop;
  }
}

//
// FileLogOutputter
//

FileLogOutputter::FileLogOutputter(const QString &logFile)
{
  setLogFilename(logFile);
}

FileLogOutputter::FileLogOutputter(const QString &path, const Mode mode, Clock clock)
    : m_mode(mode),
      m_directory(path),
      m_clock(std::move(clock))
{
  if (!m_clock)
    m_clock = [] { return QDateTime::currentDateTime(); };
  if (m_mode == Mode::SingleFile)
    setLogFilename(path);
}

void FileLogOutputter::setLogFilename(const QString &logFile)
{
  const QMutexLocker lock(&m_fileMutex);
  if (m_file.fileName() == logFile) {
    return;
  }
  m_file.close();
  m_file.setFileName(logFile);
  m_bytesSinceCheck = 0;
}

bool FileLogOutputter::ensureOpen()
{
  if (m_file.isOpen()) {
    return true;
  }
  return !m_file.fileName().isEmpty() && m_file.open(QFile::WriteOnly | QFile::Append);
}

bool FileLogOutputter::ensureSegment(const qint64 incomingBytes)
{
  const auto now = m_clock();
  if (m_directory.isEmpty() || !now.isValid()) {
    return false;
  }
  if (m_file.isOpen() && m_segmentDay == now.date() && incomingBytes <= kSegmentSizeLimit - m_segmentBytes) {
    return true;
  }
  m_file.close();
  const QDir dir(m_directory);
  if (!dir.exists() && !QDir().mkpath(m_directory)) {
    return false;
  }
  const auto stamp = now.toString(QStringLiteral("yyyyMMddHHmmss"));
  int firstSequence = 0;
  const auto usedNames =
      dir.entryList({stamp + QStringLiteral(".log"), stamp + QStringLiteral("-??.log")}, QDir::AllEntries);
  for (const auto &name : usedNames) {
    if (name.size() == stamp.size() + 4) {
      firstSequence = qMax(firstSequence, 1);
    } else {
      bool numeric = false;
      const int sequence = name.mid(stamp.size() + 1, 2).toInt(&numeric);
      if (numeric) {
        firstSequence = qMax(firstSequence, sequence + 1);
      }
    }
  }
  for (int sequence = firstSequence; sequence <= 99; ++sequence) {
    const auto name = sequence == 0 ? stamp + QStringLiteral(".log")
                                    : QStringLiteral("%1-%2.log").arg(stamp).arg(sequence, 2, 10, QLatin1Char('0'));
    const auto path = dir.filePath(name);
    m_file.setFileName(path);
    if (m_file.open(QFile::WriteOnly | QFile::NewOnly)) {
      m_segmentDay = now.date();
      m_segmentBytes = 0;
      m_lastFlush.invalidate();
      m_bytesSinceCheck = 0;
      pruneSegments(now.date());
      return true;
    }
    const QFileInfo existing(path);
    if (!existing.exists() && !existing.isSymLink()) {
      return false;
    }
  }
  return false;
}

void FileLogOutputter::pruneSegments(const QDate &today) const
{
  static const QRegularExpression pattern(QStringLiteral("^[0-9]{14}(?:-(?:0[1-9]|[1-9][0-9]))?$"));
  auto files =
      QDir(m_directory).entryInfoList({QStringLiteral("*.log")}, QDir::Files | QDir::NoSymLinks, QDir::Unsorted);
  // Compare names without their extensions so an unsuffixed segment sorts
  // before the numbered segments created later in the same second.
  std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
    return a.baseName() > b.baseName();
  });
  int kept = 1; // Always preserve the segment currently in use.
  for (const auto &info : files) {
    if (info.filePath() == m_file.fileName()) {
      continue;
    }
    const auto base = info.baseName();
    if (!pattern.match(base).hasMatch()) {
      continue;
    }
    const auto stamp = QDateTime::fromString(base.left(14), QStringLiteral("yyyyMMddHHmmss"));
    if (!stamp.isValid()) {
      continue;
    }
    if (stamp.date().daysTo(today) >= kSegmentRetentionDays || kept >= kSegmentCount) {
      QFile::remove(info.filePath());
    } else {
      ++kept;
    }
  }
}

void FileLogOutputter::rotateIfNeeded()
{
  m_bytesSinceCheck = 0;
  if (m_file.size() <= s_logFileSizeLimit) {
    return;
  }

  const auto logFile = m_file.fileName();
  const auto oldFile = logFile + QStringLiteral(".1");
  // rotate: drop any previous backup, then rename the current log into
  // its place; the next write recreates the log file from scratch
  m_file.close();
  QFile::remove(oldFile);
  QFile::rename(logFile, oldFile);
}

bool FileLogOutputter::write(LogLevel::Level level, const QString &message)
{
  const QMutexLocker lock(&m_fileMutex);
  // Avoid allocating a UTF-8 copy of an already oversized input.
  if (m_mode == Mode::SegmentedDirectory && message.size() >= kSegmentSizeLimit) {
    return false;
  }
  const auto line = message.toUtf8() + '\n';
  if (m_mode == Mode::SegmentedDirectory) {
    if (line.size() > kSegmentSizeLimit || !ensureSegment(line.size())) {
      return false;
    }
    if (m_file.write(line) != line.size()) {
      m_file.close();
      return false;
    }
    m_segmentBytes += line.size();
    m_bytesSinceCheck += line.size();
    // Debug bursts can stay in Qt's bounded file buffer. Flush the first
    // record, the next important record, and each subsequent small batch.
    if (level != LogLevel::Level::Debug || !m_lastFlush.isValid() || m_lastFlush.hasExpired(250) ||
        m_bytesSinceCheck >= s_logFileSizeCheckBytes) {
      if (!m_file.flush()) {
        m_file.close();
        return false;
      }
      m_lastFlush.start();
      m_bytesSinceCheck = 0;
    }
    return true;
  }
  if (!ensureOpen()) {
    return false;
  }

  // Flush each complete line so the GUI can tail the service log immediately.
  if (m_file.write(line) != line.size() || !m_file.flush()) {
    return false;
  }
  m_bytesSinceCheck += line.size();
  if (m_bytesSinceCheck >= s_logFileSizeCheckBytes) {
    rotateIfNeeded();
  }

  return true;
}

void FileLogOutputter::open(const QString &)
{
  // do nothing
}

void FileLogOutputter::close()
{
  const QMutexLocker lock(&m_fileMutex);
  m_file.close();
}
