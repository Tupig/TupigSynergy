/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FingerprintDialog.h"
#include "gui/StyleUtils.h"

#include "widgets/FingerprintPreview.h"

#include <QEvent>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

FingerprintDialog::FingerprintDialog(
    QWidget *parent, const Fingerprint &localFingerprint, FingerprintDialogMode mode,
    const Fingerprint &remoteFingerprint, const QString &savedCertificatePath
)
    : QDialog(parent),
      m_lblHeader{new QLabel(this)},
      m_lblFooter{new QLabel(this)},
      m_buttonBox{new QDialogButtonBox(QDialogButtonBox::Help, this)}
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  m_lblHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  const bool localMode = mode == FingerprintDialogMode::Local;
  const bool isServer = mode == FingerprintDialogMode::Server;

  auto layout = new QVBoxLayout();
  layout->addLayout(
      localMode ? makeLocalLayout(localFingerprint) : makeCompareLayout(localFingerprint, isServer, remoteFingerprint)
  );
  auto *identityLayout = static_cast<QBoxLayout *>((localMode ? m_localPreview : m_remotePreview)->layout());
  identityLayout->insertWidget(localMode ? 0 : 1, m_lblHeader);
  m_lblHeader->setProperty(localMode ? "sectionHeading" : "muted", true);
  if (localMode && !savedCertificatePath.isEmpty()) {
    auto *source = new QLabel(tr("Saved certificate: %1").arg(savedCertificatePath), m_localPreview);
    source->setObjectName(QStringLiteral("savedCertificateSource"));
    source->setProperty("muted", true);
    source->setTextFormat(Qt::PlainText);
    source->setWordWrap(true);
    source->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identityLayout->insertWidget(1, source);
  }
  if (!localMode)
    identityLayout->addWidget(m_lblFooter);
  layout->addWidget(m_buttonBox);
  setLayout(layout);

  if (localMode) {
    setWindowTitle(tr("Local Fingerprints"));
    setWindowIcon(synergy::gui::appIcon("fingerprint"));

    m_lblHeader->setText(tr("Local computer's fingerprint"));

    m_buttonBox->addButton(QDialogButtonBox::Ok);
    m_buttonBox->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    m_lblFooter->hide();
    connect(m_buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &QDialog::accept);

  } else {
    setWindowIcon(synergy::gui::appIcon("question"));
    m_lblHeader->setAlignment(Qt::AlignLeft);
    m_lblFooter->setAlignment(Qt::AlignLeft);

    auto body =
        tr("Compare the fingerprints in this dialog to those on the %1.\n"
           "Only connect if they match!");

    if (isServer) {
      setWindowTitle(tr("New client connecting"));
      m_lblHeader->setText(body.arg(tr("client")));
      m_lblFooter->setText(tr("Trust client and allow connection?"));
    } else {
      setWindowTitle(tr("Connecting to a new server"));
      m_lblHeader->setText(body.arg(tr("server")));
      m_lblFooter->setText(tr("Do you want to connect to the server?"));
    }

    m_buttonBox->addButton(QDialogButtonBox::Yes);
    m_buttonBox->addButton(QDialogButtonBox::No);
    m_buttonBox->button(QDialogButtonBox::Yes)->setText(tr("Trust after verification"));
    m_buttonBox->button(QDialogButtonBox::Yes)->setProperty("primary", true);
    m_buttonBox->button(QDialogButtonBox::No)->setText(tr("Reject"));
    m_buttonBox->button(QDialogButtonBox::No)->setProperty("danger", true);
    m_buttonBox->button(QDialogButtonBox::Yes)->setAutoDefault(false);
    m_buttonBox->button(QDialogButtonBox::No)->setDefault(true);
    m_buttonBox->button(QDialogButtonBox::No)->setFocus();
    connect(m_buttonBox->button(QDialogButtonBox::No), &QPushButton::clicked, this, &QDialog::reject);
    connect(m_buttonBox->button(QDialogButtonBox::Yes), &QPushButton::clicked, this, &QDialog::accept);
  }

  updateModeButton(true);
  m_buttonBox->button(QDialogButtonBox::Help)->setCheckable(true);
  m_buttonBox->button(QDialogButtonBox::Help)->setIcon(QIcon());
  m_buttonBox->button(QDialogButtonBox::Help)->setChecked(true);
  connect(
      m_buttonBox->button(QDialogButtonBox::Help), &QPushButton::toggled, this, &FingerprintDialog::togglePreviewMode
  );
  m_buttonBox->installEventFilter(this);

  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(8);
  m_lblHeader->setWordWrap(true);
  m_lblFooter->setWordWrap(true);
  adjustSize();
}

QLayout *FingerprintDialog::makeLocalLayout(const Fingerprint &localFingerprint)
{
  m_localPreview = new FingerprintPreview(this, localFingerprint);

  auto layout = new QVBoxLayout();
  layout->addWidget(m_localPreview, 0, Qt::AlignTop);
  return layout;
}

QLayout *FingerprintDialog::makeCompareLayout(
    const Fingerprint &localFingerprint, bool isServer, const Fingerprint &remoteFingerprint
)
{
  const auto serverText = tr("Server Fingerprint");
  const auto clientText = tr("Client Fingerprint");

  m_localPreview = new FingerprintPreview(this, localFingerprint, isServer ? serverText : clientText, true);
  m_remotePreview = new FingerprintPreview(this, remoteFingerprint, isServer ? clientText : serverText, true);

  auto fpLayout = new QVBoxLayout();
  fpLayout->setAlignment(Qt::AlignTop);
  if (isServer) {
    fpLayout->addWidget(m_localPreview);
    fpLayout->addWidget(m_remotePreview);
  } else {
    fpLayout->addWidget(m_remotePreview);
    fpLayout->addWidget(m_localPreview);
  }
  return fpLayout;
}

void FingerprintDialog::togglePreviewMode(bool hashMode)
{
  m_localPreview->toggleMode(hashMode);
  if (m_remotePreview)
    m_remotePreview->toggleMode(hashMode);
  updateModeButton(hashMode);
}

bool FingerprintDialog::eventFilter(QObject *object, QEvent *event)
{
  if (object == m_buttonBox && event->type() == QEvent::LanguageChange) {
    // QDialogButtonBox translates its standard buttons after this filter.
    // Restore custom captions after that event, bound to this dialog's life.
    QTimer::singleShot(0, this, &FingerprintDialog::restoreButtonCaptions);
  }
  return QDialog::eventFilter(object, event);
}

void FingerprintDialog::restoreButtonCaptions()
{
  if (auto *help = m_buttonBox->button(QDialogButtonBox::Help))
    updateModeButton(help->isChecked());
  if (auto *trust = m_buttonBox->button(QDialogButtonBox::Yes)) {
    trust->setText(tr("Trust after verification"));
    trust->setAutoDefault(false);
    if (auto *reject = m_buttonBox->button(QDialogButtonBox::No)) {
      reject->setText(tr("Reject"));
      reject->setDefault(true);
    }
  }
}

void FingerprintDialog::updateModeButton(bool hashMode) const
{
  const auto text = hashMode ? tr("Show image") : tr("Show hash");
  const auto toolTip = hashMode ? tr("Display the fingerprint as an image") : tr("Display the fingerprint as a hash");
  m_buttonBox->button(QDialogButtonBox::Help)->setText(text);
  m_buttonBox->button(QDialogButtonBox::Help)->setToolTip(toolTip);
}
