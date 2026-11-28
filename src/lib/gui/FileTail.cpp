/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FileTail.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>

namespace synergy::gui {

namespace {
const QRegularExpression kSegmentPattern(QStringLiteral("^(\\d{14})(?:-(\\d{2}))?\\.log$"));
QString segmentKey(const QString &name)
{
  const auto match = kSegmentPattern.match(name);
  return match.hasMatch() ? match.captured(1) + QStringLiteral("-") +
                                (match.captured(2).isEmpty() ? QStringLiteral("00") : match.captured(2))
                          : QString();
}
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
  if (QFileInfo(filePath).isDir())
    setWatchedDirectory(filePath);
  else
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
  m_directoryMode = false;
  m_currentSegment.clear();
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

void FileTail::setWatchedDirectory(const QString &directoryPath)
{
  const QFileInfo info(directoryPath);
  if (!info.isDir())
    return;
  const auto absolute = info.absoluteFilePath();
  if (m_directoryMode && m_directoryPath == absolute)
    return;
  m_watcher->removePaths(m_watcher->files());
  m_watcher->removePaths(m_watcher->directories());
  m_directoryMode = true;
  m_directoryPath = absolute;
  m_filePath.clear();
  m_currentSegment.clear();
  m_lastPos = 0;
  m_tailAnchor.clear();
  m_watcher->addPath(m_directoryPath);
  QStringList segments;
  for (const auto &entry : QDir(m_directoryPath).entryList(QDir::Files, QDir::Name)) {
    if (kSegmentPattern.match(entry).hasMatch() && !QFileInfo(m_directoryPath, entry).isSymLink())
      segments.append(entry);
  }
  std::sort(segments.begin(), segments.end(), [](const auto &left, const auto &right) {
    return segmentKey(left) < segmentKey(right);
  });
  if (!segments.isEmpty()) {
    m_currentSegment = segments.back();
    m_filePath = QDir(m_directoryPath).filePath(m_currentSegment);
    QFile file(m_filePath);
    if (file.open(QIODevice::ReadOnly)) {
      m_lastPos = file.size();
      m_tailAnchor = tailAnchor(file, m_lastPos);
    }
    m_watcher->addPath(m_filePath);
  }
}

void FileTail::handlePathChanged(const QString &path)
{
  if (m_directoryMode) {
    if (path != m_directoryPath && path != m_filePath)
      return;
    QStringList segments;
    for (const auto &entry : QDir(m_directoryPath).entryList(QDir::Files, QDir::Name)) {
      if (kSegmentPattern.match(entry).hasMatch() && !QFileInfo(m_directoryPath, entry).isSymLink())
        segments.append(entry);
    }
    std::sort(segments.begin(), segments.end(), [](const auto &left, const auto &right) {
      return segmentKey(left) < segmentKey(right);
    });
    for (const auto &segment : segments) {
      if (!m_currentSegment.isEmpty() && segmentKey(segment) < segmentKey(m_currentSegment))
        continue;
      if (segment != m_currentSegment) {
        if (!m_filePath.isEmpty())
          m_watcher->removePath(m_filePath);
        m_currentSegment = segment;
        m_filePath = QDir(m_directoryPath).filePath(segment);
        m_lastPos = 0;
        m_tailAnchor.clear();
        m_watcher->addPath(m_filePath);
      }
      QFile file(QDir(m_directoryPath).filePath(segment));
      if (!file.open(QIODevice::ReadOnly) || !file.seek(m_lastPos))
        continue;
      QTextStream stream(&file);
      while (!stream.atEnd())
        Q_EMIT newLine(stream.readLine());
      m_lastPos = file.pos();
      m_tailAnchor = tailAnchor(file, m_lastPos);
    }
    return;
  }
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
