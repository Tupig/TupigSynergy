/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/ILogOutputter.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QMutex>
#include <QString>
#include <functional>
//! Stop traversing log chain outputter
/*!
This outputter performs no output and returns false from \c write(),
causing the logger to stop traversing the outputter chain.  Insert
this to prevent already inserted outputters from writing.
*/
class StopLogOutputter : public ILogOutputter
{
public:
  StopLogOutputter() = default;
  ~StopLogOutputter() override = default;

  // ILogOutputter overrides
  void open(const QString &title) override;
  void close() override;
  bool write(LogLevel::Level level, const QString &message) override;
};

//! Write log to console
/*!
This outputter writes output to the console.  The level for each
message is ignored.
*/
class ConsoleLogOutputter : public ILogOutputter
{
public:
  ConsoleLogOutputter() = default;
  ~ConsoleLogOutputter() override = default;

  // ILogOutputter overrides
  void open(const QString &title) override;
  void close() override;
  bool write(LogLevel::Level level, const QString &message) override;
  void flush() const;
};

//! Write log to file
/*!
This outputter writes output to the file.  The level for each
message is ignored.
*/

class FileLogOutputter : public ILogOutputter
{
public:
  enum class Mode
  {
    SingleFile,
    SegmentedDirectory
  };
  using Clock = std::function<QDateTime()>;
  explicit FileLogOutputter(const QString &logFile);
  FileLogOutputter(const QString &path, Mode mode, Clock clock = {});
  ~FileLogOutputter() override = default;

  // ILogOutputter overrides
  void open(const QString &title) override;
  void close() override;
  bool write(LogLevel::Level level, const QString &message) override;

  void setLogFilename(const QString &title);

private:
  //! Open the log file for appending when it is not open yet.
  bool ensureOpen();
  //! Rotate the log file when it exceeds the size limit.
  void rotateIfNeeded();
  bool ensureSegment(qint64 incomingBytes);
  void pruneSegments(const QDate &today) const;

  // Serialize file writes and path changes across log producers.
  QMutex m_fileMutex;
  //! Long-lived append handle: avoids an open/write/close cycle per line.
  QFile m_file;
  //! Bytes written since the last rotation check; file.size() is only called
  //! once per interval instead of on every line.
  qint64 m_bytesSinceCheck = 0;
  qint64 m_segmentBytes = 0;
  Mode m_mode = Mode::SingleFile;
  QString m_directory;
  QDate m_segmentDay;
  QElapsedTimer m_lastFlush;
  Clock m_clock;
};

//! Write log to system log
/*!
This outputter writes output to the system log.
*/
class SystemLogOutputter : public ILogOutputter
{
public:
  SystemLogOutputter() = default;
  ~SystemLogOutputter() override = default;

  // ILogOutputter overrides
  void open(const QString &title) override;
  void close() override;
  bool write(LogLevel::Level level, const QString &message) override;
};

//! Write log to system log only
/*!
Creating an object of this type inserts a StopLogOutputter followed
by a SystemLogOutputter into Log.  The destructor removes those
outputters.  Add one of these to any scope that needs to write to
the system log (only) and restore the old outputters when exiting
the scope.
*/
class SystemLogger
{
public:
  SystemLogger(const QString &title, bool blockConsole);
  SystemLogger(SystemLogger const &) = delete;
  SystemLogger(SystemLogger &&) = delete;
  ~SystemLogger();

  SystemLogger &operator=(SystemLogger const &) = delete;
  SystemLogger &operator=(SystemLogger &&) = delete;

private:
  ILogOutputter *m_syslog = nullptr;
  ILogOutputter *m_stop = nullptr;
};
