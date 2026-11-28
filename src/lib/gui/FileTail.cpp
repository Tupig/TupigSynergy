/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FileTail.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTextStream>

namespace synergy::gui {

namespace {
QByteArray tailAnchor(QFile &file, qint64 position)
{
  constexpr qint64 kAnchorBytes = 128;
  const auto count = qMin(position, kAnchorBytes);
  return file.seek(position - count) ? file.read(count) : QByteArray{};
}
} // namespace

FileTail::FileTail(const QString &filePath, QObject *parent) : QObject(parent), m_watcher(new QFileSystemWatcher(this))
{
  connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &FileTail::handlePathChanged);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &FileTail::handlePathChanged);
  setWatchedFile(filePath);
}

void FileTail::setWatchedFile(const QString &filePath)
{
  if (filePath.isEmpty()) {
    return;
  }
  const QFileInfo pathInfo(filePath);
  const auto absolutePath = pathInfo.absoluteFilePath();
  if (m_filePath == absolutePath) {
    return;
  }

  if (m_watcher->files().contains(m_filePath)) {
    m_watcher->removePath(m_filePath);
  }
  if (m_watcher->directories().contains(m_directoryPath)) {
    m_watcher->removePath(m_directoryPath);
  }
  m_filePath = absolutePath;
  m_directoryPath = pathInfo.absolutePath();
  m_lastPos = 0;
  m_tailAnchor.clear();

  // A service can create a new log after the old file's watch has been removed.
  m_watcher->addPath(m_directoryPath);
  if (pathInfo.isFile()) {
    QFile file(m_filePath);
    if (file.open(QIODevice::ReadOnly)) {
      m_lastPos = file.size();
      m_tailAnchor = tailAnchor(file, m_lastPos);
    } else {
      qWarning().noquote() << QCoreApplication::translate("GuiLogMessages", "failed to open file for tail:")
                           << m_filePath;
    }
    m_watcher->addPath(m_filePath);
  }
  qDebug() << "starting file tail:" << m_filePath;
}

void FileTail::handlePathChanged(const QString &path)
{
  const bool watched = m_watcher->files().contains(m_filePath);
  if (path != m_filePath && (path != m_directoryPath || watched)) {
    return;
  }
  if (!QFileInfo(m_filePath).isFile()) {
    m_lastPos = 0;
    m_tailAnchor.clear();
    return;
  }
  if (!watched) {
    m_lastPos = 0;
    m_tailAnchor.clear();
    m_watcher->addPath(m_filePath);
  }

  // Close the read handle after every notification so the service can rotate it.
  QFile file(m_filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning().noquote() << QCoreApplication::translate("GuiLogMessages", "failed to reopen tailed file:")
                         << m_filePath;
    return;
  }
  // Some watcher backends report a same-path replacement as a modification.
  // Check the bytes preceding the old offset before resuming in the new file.
  if (file.size() < m_lastPos || (!m_tailAnchor.isEmpty() && tailAnchor(file, m_lastPos) != m_tailAnchor)) {
    m_lastPos = 0;
  }
  if (!file.seek(m_lastPos)) {
    qWarning().noquote() << QCoreApplication::translate("GuiLogMessages", "failed to seek in tailed file:")
                         << m_filePath;
    return;
  }
  QTextStream stream(&file);
  while (!stream.atEnd()) {
    Q_EMIT newLine(stream.readLine());
  }
  m_lastPos = file.pos();
  m_tailAnchor = tailAnchor(file, m_lastPos);
}

} // namespace synergy::gui
