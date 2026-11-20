/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateDialog.h"

#include "UpdateDownloader.h"
#include "common/VersionInfo.h"
#include "gui/StyleUtils.h"
#include "gui/widgets/PageHost.h"
#include "platform/UpdateInstaller.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextOption>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <QtMath>

namespace synergy::gui::update {

namespace {
QString visibleNotes(QString notes)
{
  static const QRegularExpression comments(
      QStringLiteral("<!--.*?(?:-->|\\z)"), QRegularExpression::DotMatchesEverythingOption
  );
  static const QRegularExpression provenance(
      QStringLiteral("^Built-From: [0-9a-fA-F]{40}\\s*$"), QRegularExpression::MultilineOption
  );
  notes.remove(comments);
  notes.remove(provenance);
  return notes.trimmed();
}

class ReleaseNotes final : public QTextEdit
{
public:
  ReleaseNotes(const QString &notes, QWidget *parent) : QTextEdit(parent)
  {
    setReadOnly(true);
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(
        document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
        [this] { fitDocument(); }, Qt::QueuedConnection
    );
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    auto features = QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub);
    features.setFlag(QTextDocument::MarkdownNoHTML);
    document()->setMarkdown(notes, features);
#else
    setPlainText(notes);
#endif
    fitDocument();
  }

private:
  QVariant loadResource(int, const QUrl &) override
  {
    QImage placeholder(1, 1, QImage::Format_ARGB32_Premultiplied);
    placeholder.fill(Qt::transparent);
    return placeholder;
  }

  void resizeEvent(QResizeEvent *event) override
  {
    QTextEdit::resizeEvent(event);
    fitDocument();
  }
  void fitDocument()
  {
    constexpr int maximumHeight = 180;
    const int chrome = qBound(0, height() - viewport()->height(), 32);
    const int contentHeight = qCeil(document()->size().height()) + chrome;
    const auto scrollPolicy = contentHeight > maximumHeight ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    if (verticalScrollBarPolicy() != scrollPolicy)
      setVerticalScrollBarPolicy(scrollPolicy);
    const int target = qBound(fontMetrics().height() + chrome, contentHeight, maximumHeight);
    if (minimumHeight() != target || this->maximumHeight() != target)
      setFixedHeight(target);
  }
};
} // namespace

UpdateDialog::UpdateDialog(const UpdateInfo &info, QWidget *parent, const DownloadPolicy &policy)
    : QDialog(parent),
      m_info(info),
      m_downloader(new UpdateDownloader(this, policy))
{
  setWindowTitle(tr("Update Available"));
  setMinimumWidth(0);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(8);

  auto *summaryCard = new QGroupBox(this);
  summaryCard->setObjectName(QStringLiteral("updateSummaryCard"));
  summaryCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *summaryLayout = new QVBoxLayout(summaryCard);
  summaryLayout->setContentsMargins(12, 12, 12, 12);
  summaryLayout->setSpacing(8);
  auto *summaryHeading = new QLabel(tr("New version"), summaryCard);
  summaryHeading->setObjectName(QStringLiteral("updateHeading"));
  summaryHeading->setProperty("sectionHeading", true);
  summaryLayout->addWidget(summaryHeading);

  m_summary = new QLabel(
      tr("Current version: %1\nLatest version: %2\nPackage: %3")
          .arg(QString::fromLatin1(kVersion), info.version, info.asset.name),
      summaryCard
  );
  m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_summary->setWordWrap(true);
  m_summary->setTextFormat(Qt::PlainText);
  m_summary->setProperty("muted", true);
  summaryLayout->addWidget(m_summary);
  layout->addWidget(summaryCard);

  const auto notes = visibleNotes(info.notes);
  if (!notes.isEmpty()) {
    auto *notesCard = new QWidget(summaryCard);
    notesCard->setObjectName(QStringLiteral("updateNotesCard"));
    notesCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *notesLayout = new QVBoxLayout(notesCard);
    notesLayout->setContentsMargins(0, 0, 0, 0);
    notesLayout->setSpacing(8);
    auto *releaseNotes = new ReleaseNotes(notes, notesCard);
    releaseNotes->setObjectName(QStringLiteral("releaseNotes"));
    auto *notesHeader = new QHBoxLayout;
    auto *notesHeading = new QLabel(tr("Release Notes"), notesCard);
    notesHeader->addWidget(notesHeading);
    notesHeader->addStretch(1);
    auto *toggleNotes = new QPushButton(tr("Hide"), notesCard);
    toggleNotes->setObjectName(QStringLiteral("toggleReleaseNotes"));
    toggleNotes->setCheckable(true);
    toggleNotes->setChecked(true);
    connect(toggleNotes, &QPushButton::toggled, releaseNotes, &QWidget::setVisible);
    connect(toggleNotes, &QPushButton::toggled, this, [toggleNotes](bool expanded) {
      toggleNotes->setText(expanded ? tr("Hide") : tr("Show"));
    });
    notesHeader->addWidget(toggleNotes);
    notesLayout->addLayout(notesHeader);
    notesLayout->addWidget(releaseNotes);
    summaryLayout->addWidget(notesCard);
  }

  auto *progressCard = new QWidget(summaryCard);
  progressCard->setObjectName(QStringLiteral("updateProgressCard"));
  progressCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  progressCard->hide();
  auto *progressLayout = new QVBoxLayout(progressCard);
  progressLayout->setContentsMargins(0, 0, 0, 0);
  progressLayout->setSpacing(8);
  m_progress = new QProgressBar(progressCard);
  m_progress->setObjectName(QStringLiteral("updateProgress"));
  m_progress->setFixedHeight(8);
  m_progress->setTextVisible(false);
  m_progress->setRange(0, 0);
  m_progress->setVisible(false);
  progressLayout->addWidget(m_progress);

  m_progressText = new QLabel(progressCard);
  m_progressText->setWordWrap(true);
  m_progressText->setTextFormat(Qt::PlainText);
  m_progressText->setVisible(false);
  progressLayout->addWidget(m_progressText);
  m_meter = new QWidget(progressCard);
  auto *meterLayout = new QGridLayout(m_meter);
  meterLayout->setContentsMargins(0, 0, 0, 0);
  meterLayout->setHorizontalSpacing(12);
  meterLayout->setVerticalSpacing(8);
  meterLayout->setColumnStretch(1, 1);
  const auto meterRow = [this, meterLayout](int row, const QString &caption, const QString &name) {
    auto *label = new QLabel(caption, m_meter);
    label->setObjectName(name.left(name.size() - 5) + QStringLiteral("Label"));
    label->setProperty("muted", true);
    meterLayout->addWidget(label, row, 0);
    auto *value = new QLabel(m_meter);
    value->setObjectName(name);
    value->setProperty("meterValue", true);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setTextFormat(Qt::PlainText);
    meterLayout->addWidget(value, row, 1);
    return value;
  };
  m_amount = meterRow(0, tr("Download progress"), QStringLiteral("downloadAmountValue"));
  m_rate = meterRow(1, tr("Transfer speed"), QStringLiteral("downloadRateValue"));
  m_eta = meterRow(2, tr("Estimated remaining"), QStringLiteral("downloadEtaValue"));
  progressLayout->addWidget(m_meter);
  summaryLayout->insertWidget(2, progressCard);

  auto *buttons = new QWidget(summaryCard);
  auto *buttonLayout = new QHBoxLayout(buttons);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->addStretch(1);

  m_release = new QPushButton(tr("Open Release Page"), buttons);
  m_release->setIcon(synergy::gui::appIcon(QStringLiteral("document-open")));
  connect(m_release, &QPushButton::clicked, this, &UpdateDialog::openReleasePage);
  buttonLayout->addWidget(m_release);
  buttonLayout->setSpacing(8);

  m_later = new QPushButton(tr("Later"), buttons);
  connect(m_later, &QPushButton::clicked, this, &UpdateDialog::cancelAndClose);
  buttonLayout->addWidget(m_later);

  m_install = new QPushButton(tr("Download and Install"), buttons);
  m_install->setObjectName(QStringLiteral("downloadInstall"));
  m_install->setIcon(synergy::gui::appIcon(QStringLiteral("download")));
  m_install->setProperty("primary", true);
  m_install->setDefault(true);
  connect(m_install, &QPushButton::clicked, this, [this] {
    if (m_downloadedPath.isEmpty())
      startDownload();
    else
      installDownloaded(m_downloadedPath);
  });
  buttonLayout->addWidget(m_install);

  summaryLayout->addWidget(buttons);

  m_rateTimer.setInterval(500);
  connect(&m_rateTimer, &QTimer::timeout, this, &UpdateDialog::updateProgressText);
  connect(m_downloader, &UpdateDownloader::progress, this, [this](qint64 received, qint64 total) {
    const auto now = m_downloadTimer.elapsed();
    const auto interval = now - m_sampleTime;
    const auto added = received - m_received;
    // Sample recent byte movement, never a whole-download average that masks a stall.
    m_bytesPerSecond = interval > 0 && added > 0 ? static_cast<double>(added) * 1000.0 / interval : 0.0;
    m_received = received;
    m_total = total;
    m_sampleTime = now;
    if (total > 0) {
      m_progress->setRange(0, 100);
      m_progress->setValue(static_cast<int>(qBound(0.0, 100.0 * received / total, 100.0)));
    }
    updateProgressText();
  });
  // 信号只在构造时连接一次，失败重试不会重复交付进度或错误。
  connect(m_downloader, &UpdateDownloader::finished, this, [this](const QString &path) {
    m_downloadedPath = path;
    setBusy(false);
    m_progress->parentWidget()->show();
    m_progress->setVisible(true);
    m_progress->setRange(0, 100);
    m_progress->setValue(100);
    m_progressText->setVisible(true);
    m_progressText->setText(tr("Download verified. Ready to install."));
    m_install->setText(tr("Install Now"));
  });
  connect(m_downloader, &UpdateDownloader::failed, this, [this](const QString &reason) {
    setBusy(false);
    m_progress->parentWidget()->show();
    m_progressText->setText(reason);
    m_progressText->show();
    if (isVisible()) {
      auto *message = new QMessageBox(QMessageBox::Critical, tr("Update Failed"), reason, QMessageBox::Ok, this);
      message->setTextFormat(Qt::PlainText);
      PageHost::present(this, message);
    }
  });
}

UpdateDialog::~UpdateDialog()
{
  delete m_downloader;
  m_downloader = nullptr;
}

void UpdateDialog::reject()
{
  if (property("embeddedPage").toBool() && (m_busy || !m_downloadedPath.isEmpty())) {
    setProperty("retainWhenFinished", true);
    QDialog::reject();
    return;
  }
  cancelAndClose();
}

void UpdateDialog::cancelAndClose()
{
  m_rateTimer.stop();
  setProperty("retainWhenFinished", false);
  delete m_downloader;
  m_downloader = nullptr;
  QDialog::reject();
}

void UpdateDialog::setBusy(bool busy)
{
  m_busy = busy;
  if (auto *heading = findChild<QLabel *>(QStringLiteral("updateHeading")))
    heading->setText(busy ? tr("Downloading update %1").arg(m_info.version) : tr("New version"));
  if (auto *toggle = findChild<QPushButton *>(QStringLiteral("toggleReleaseNotes")))
    toggle->setChecked(!busy);
  if (busy)
    m_rateTimer.start();
  else
    m_rateTimer.stop();
  m_later->setText(busy ? tr("Cancel download") : tr("Later"));
  // Text-only controls fit the full English button labels during a download.
  m_install->setIcon(busy ? QIcon() : synergy::gui::appIcon(QStringLiteral("download")));
  m_release->setIcon(busy ? QIcon() : synergy::gui::appIcon(QStringLiteral("document-open")));
  m_progress->parentWidget()->setVisible(busy);
  m_install->setEnabled(!busy);
  m_release->setEnabled(!busy);
  m_progress->setVisible(busy);
  m_progressText->hide();
  m_meter->setVisible(busy);
}

void UpdateDialog::updateProgressText()
{
  if (m_total > 0 && m_received >= m_total) {
    m_meter->hide();
    m_progressText->setText(tr("Verifying download…"));
    m_progressText->show();
    return;
  }
  if (m_busy) {
    m_meter->show();
    m_progressText->hide();
  }

  // No new bytes for 1.5 seconds is a stalled interval, even if an earlier sample was fast.
  const auto speed =
      m_downloadTimer.isValid() && m_downloadTimer.elapsed() - m_sampleTime < 1500 ? m_bytesPerSecond : 0.0;
  m_amount->setText(
      m_total > 0 ? QStringLiteral("%1 / %2 MB · %3%")
                        .arg(m_received / 1048576.0, 0, 'f', 1)
                        .arg(m_total / 1048576.0, 0, 'f', 1)
                        .arg(m_progress->value())
                  : QStringLiteral("%1 MB").arg(m_received / 1048576.0, 0, 'f', 1)
  );
  m_rate->setText(
      speed > 0 ? QStringLiteral("%1 MB/s").arg(QString::number(speed / 1048576.0, 'g', 3)) : tr("Waiting for network")
  );
  if (speed > 0 && m_total > 0) {
    const auto remaining = qMax(0.0, (m_total - m_received) / speed);
    m_eta->setText(tr("%1 s remaining").arg(remaining, 0, 'f', 0));
  } else {
    m_eta->setText(QStringLiteral("—"));
  }
}

void UpdateDialog::startDownload()
{
  m_downloadTimer.start();
  m_received = 0;
  m_total = -1;
  m_sampleTime = 0;
  m_bytesPerSecond = 0;
  setBusy(true);
  m_progress->setRange(0, 0);
  updateProgressText();
  if (!m_downloadDirectory.isValid()) {
    setBusy(false);
    m_progressText->parentWidget()->show();
    m_progressText->setText(tr("Could not create a temporary download directory."));
    m_progressText->show();
    return;
  }
  m_downloader->start(m_info.asset, m_downloadDirectory.path());
}

void UpdateDialog::installDownloaded(const QString &filePath)
{
  QString error;
  const auto result = synergy::platform::launchUpdateInstaller(filePath, m_info.asset.sha256, &error);
  if (result == synergy::platform::UpdateInstallResult::Launched) {
    // 安装器异步读取包；交接后保留独立临时目录，避免退出时删除安装源。
    m_downloadDirectory.setAutoRemove(false);
    QCoreApplication::quit();
    return;
  }

  setBusy(false);
  auto *message = new QMessageBox(
      QMessageBox::Critical, tr("Update Failed"),
      error.isEmpty() ? tr("The system installer could not be started.") : error, QMessageBox::Ok, this
  );
  message->setTextFormat(Qt::PlainText);
  PageHost::present(this, message);
}

void UpdateDialog::openReleasePage()
{
  QDesktopServices::openUrl(m_info.releaseUrl);
}

} // namespace synergy::gui::update
