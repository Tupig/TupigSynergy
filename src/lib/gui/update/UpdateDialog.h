/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "UpdateChecker.h"
#include "UpdateDownloader.h"

#include <QDialog>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTimer>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace synergy::gui::update {

class UpdateDownloader;

/**
 * @brief Shows an available update, downloads it and hands it to the installer.
 *
 * Nothing is downloaded until the user asks, and no installer runs until the
 * SHA-256 matches the release metadata.
 */
class UpdateDialog : public QDialog
{
  Q_OBJECT
public:
  explicit UpdateDialog(const UpdateInfo &info, QWidget *parent = nullptr, const DownloadPolicy &policy = {});
  ~UpdateDialog() override;
  void reject() override;

private:
  void startDownload();
  void installDownloaded(const QString &filePath);
  void openReleasePage();
  void setBusy(bool busy);
  void updateProgressText();

  QTemporaryDir m_downloadDirectory;
  QString m_downloadedPath;
  UpdateInfo m_info;
  UpdateDownloader *m_downloader = nullptr;
  QLabel *m_summary = nullptr;
  QPlainTextEdit *m_notes = nullptr;
  QProgressBar *m_progress = nullptr;
  QLabel *m_progressText = nullptr;
  QWidget *m_meter = nullptr;
  QLabel *m_amount = nullptr;
  QLabel *m_rate = nullptr;
  QLabel *m_eta = nullptr;
  QElapsedTimer m_downloadTimer;
  QTimer m_rateTimer;
  qint64 m_received = 0;
  qint64 m_total = -1;
  qint64 m_sampleTime = 0;
  double m_bytesPerSecond = 0;
  QPushButton *m_install = nullptr;
  QPushButton *m_release = nullptr;
  QPushButton *m_later = nullptr;
};

} // namespace synergy::gui::update
