/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QFileSystemWatcher;

namespace synergy::gui {

class FileTail : public QObject
{
  Q_OBJECT

public:
  explicit FileTail(const QString &filePath, QObject *parent = nullptr);
  void setWatchedFile(const QString &filePath);
  void setWatchedDirectory(const QString &directoryPath);

Q_SIGNALS:
  void newLine(const QString &line);

private Q_SLOTS:
  void handlePathChanged(const QString &path);

private:
  QFileSystemWatcher *m_watcher = nullptr;
  QString m_filePath;
  QString m_directoryPath;
  qint64 m_lastPos = 0;
  QByteArray m_tailAnchor;
  bool m_directoryMode = false;
  QString m_currentSegment;
};

} // namespace synergy::gui
