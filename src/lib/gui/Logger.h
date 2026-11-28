/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <atomic>
#include <memory>

class FileLogOutputter;

namespace synergy::gui {

Q_DECLARE_LOGGING_CATEGORY(updateLog)

class Logger : public QObject
{
  Q_OBJECT

public:
  static Logger *instance()
  {
    static Logger m;
    return &m;
  }

  void handleMessage(const QtMsgType type, const QString &fileLine, const QString &message, bool update = false);

Q_SIGNALS:
  void newLine(const QString &line);

private:
  explicit Logger();
  ~Logger() override;
  void settingChanged(const QString &key);
  std::unique_ptr<FileLogOutputter> m_fileLog;
  std::unique_ptr<FileLogOutputter> m_updateFileLog;
  std::atomic_bool m_fileErrorReported{false};
  std::atomic_bool m_guiDebug{false};
};

} // namespace synergy::gui
