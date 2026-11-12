/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateDialog.h"

#include "UpdateDownloader.h"
#include "common/VersionInfo.h"
#include "platform/UpdateInstaller.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

namespace synergy::gui::update {

UpdateDialog::UpdateDialog(const UpdateInfo &info, QWidget *parent)
    : QDialog(parent),
      m_info(info),
      m_downloader(new UpdateDownloader(this))
{
  setWindowTitle(tr("Update Available"));
  setMinimumWidth(520);

  auto *layout = new QVBoxLayout(this);

  m_summary = new QLabel(
      tr("A newer version is available.\n\nCurrent version: %1\nLatest version: %2\nPackage: %3")
          .arg(QString::fromLatin1(kVersion), info.version, info.asset.name),
      this
  );
  m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_summary);

  if (!info.notes.trimmed().isEmpty()) {
    m_notes = new QPlainTextEdit(info.notes, this);
    m_notes->setReadOnly(true);
    m_notes->setMaximumHeight(200);
    layout->addWidget(m_notes);
  }

  m_progress = new QProgressBar(this);
  m_progress->setRange(0, 0);
  m_progress->setVisible(false);
  layout->addWidget(m_progress);

  auto *buttons = new QWidget(this);
  auto *buttonLayout = new QHBoxLayout(buttons);
  buttonLayout->setContentsMargins(0, 0, 0, 0);

  m_release = new QPushButton(tr("Open Release Page"), buttons);
  connect(m_release, &QPushButton::clicked, this, &UpdateDialog::openReleasePage);
  buttonLayout->addWidget(m_release);
  buttonLayout->addStretch();

  m_later = new QPushButton(tr("Later"), buttons);
  connect(m_later, &QPushButton::clicked, this, &QDialog::reject);
  buttonLayout->addWidget(m_later);

  m_install = new QPushButton(tr("Download and Install"), buttons);
  m_install->setDefault(true);
  connect(m_install, &QPushButton::clicked, this, &UpdateDialog::startDownload);
  buttonLayout->addWidget(m_install);

  layout->addWidget(buttons);
}

UpdateDialog::~UpdateDialog() = default;

void UpdateDialog::setBusy(bool busy)
{
  m_install->setEnabled(!busy);
  m_release->setEnabled(!busy);
  m_progress->setVisible(busy);
}

void UpdateDialog::startDownload()
{
  setBusy(true);

  auto directory = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
  directory.mkpath(QStringLiteral("synergy-update"));

  connect(
      m_downloader, &UpdateDownloader::progress, this,
      [this](qint64 received, qint64 total) {
        if (total > 0) {
          m_progress->setRange(0, static_cast<int>(total));
          m_progress->setValue(static_cast<int>(received));
        }
      },
      Qt::UniqueConnection
  );
  connect(
      m_downloader, &UpdateDownloader::finished, this, &UpdateDialog::installDownloaded, Qt::UniqueConnection
  );
  connect(
      m_downloader, &UpdateDownloader::failed, this,
      [this](const QString &reason) {
        setBusy(false);
        QMessageBox::critical(this, tr("Update Failed"), reason);
      },
      Qt::UniqueConnection
  );

  // Start last: start() can fail synchronously.
  m_downloader->start(m_info.asset, directory.filePath(QStringLiteral("synergy-update")));
}

void UpdateDialog::installDownloaded(const QString &filePath)
{
  QString error;
  const auto result = synergy::platform::launchUpdateInstaller(filePath, &error);
  if (result == synergy::platform::UpdateInstallResult::Launched) {
    // The OS installer takes over; the running GUI and core must exit.
    QCoreApplication::quit();
    return;
  }

  setBusy(false);
  QMessageBox::critical(
      this, tr("Update Failed"),
      error.isEmpty() ? tr("The system installer could not be started.") : error
  );
}

void UpdateDialog::openReleasePage()
{
  QDesktopServices::openUrl(m_info.releaseUrl);
}

} // namespace synergy::gui::update
