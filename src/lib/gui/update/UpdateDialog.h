/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "UpdateChecker.h"

#include <QDialog>
#include <QElapsedTimer>

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
  explicit UpdateDialog(const UpdateInfo &info, QWidget *parent = nullptr);
  ~UpdateDialog() override;

private:
  void startDownload();
  void installDownloaded(const QString &filePath);
  void openReleasePage();
  void setBusy(bool busy);

  UpdateInfo m_info;
  UpdateDownloader *m_downloader = nullptr;
  QLabel *m_summary = nullptr;
  QPlainTextEdit *m_notes = nullptr;
  QProgressBar *m_progress = nullptr;
  QLabel *m_progressText = nullptr;
  QElapsedTimer m_downloadTimer;
  QPushButton *m_install = nullptr;
  QPushButton *m_release = nullptr;
  QPushButton *m_later = nullptr;
};

} // namespace synergy::gui::update
