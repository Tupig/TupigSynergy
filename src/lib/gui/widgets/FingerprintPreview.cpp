/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FingerprintPreview.h"
#include "gui/StyleUtils.h"

#include <QClipboard>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <net/SecureUtils.h>

FingerprintPreview::FingerprintPreview(
    QWidget *parent, const Fingerprint &fingerprint, const QString &titleText, bool hashMode
)
    : QFrame(parent)
{
  setFrameShape(QFrame::StyledPanel);
  setObjectName(QStringLiteral("card"));
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

  setLayout(
      fingerprint.type == QCryptographicHash::Sha256 ? sha256Layout(fingerprint, titleText, hashMode) : emptyLayout()
  );
}

void FingerprintPreview::toggleMode(bool hashMode)
{
  if (m_lblHash)
    m_lblHash->setVisible(hashMode);
  if (m_lblArt)
    m_lblArt->setVisible(!hashMode);
}

QLayout *FingerprintPreview::emptyLayout()
{
  auto *label = new QLabel(tr("Invalid hash format"));
  auto *layout = new QHBoxLayout();
  layout->addWidget(label);
  return layout;
}

QLayout *FingerprintPreview::sha256Layout(const Fingerprint &fingerprint, const QString &titleText, bool hashMode)
{
  auto labelTitle = new QLabel(titleText, this);
  auto f = font();
  f.setBold(true);
  f.setItalic(false);
  labelTitle->setFont(f);
  labelTitle->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  labelTitle->setProperty("sectionHeading", true);
  labelTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  labelTitle->setVisible(!titleText.isEmpty());

  QString hash;
  for (int offset = 0; offset < fingerprint.data.size(); offset += 16) {
    if (!hash.isEmpty())
      hash += QLatin1Char('\n');
    hash += QString::fromLatin1(fingerprint.data.mid(offset, 16).toHex(':').toUpper());
  }
  m_lblHash = new QLabel(hash, this);
  m_lblHash->setObjectName(QStringLiteral("fingerprintHash"));
  m_lblHash->setTextFormat(Qt::PlainText);
  m_lblHash->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_lblHash->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_lblHash->setVisible(hashMode);

  m_lblArt = new QLabel(synergy::generateFingerprintArt(fingerprint.data), this);
  m_lblArt->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
  m_lblArt->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_lblArt->setVisible(!hashMode);

  const auto itemFont = font();
  auto codeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  if (itemFont.pixelSize() > 0)
    codeFont.setPixelSize(itemFont.pixelSize());
  else if (itemFont.pointSizeF() > 0)
    codeFont.setPointSizeF(itemFont.pointSizeF());
  m_lblArt->setFont(codeFont);
  m_lblHash->setFont(codeFont);

  auto innersha256Layout = new QHBoxLayout();
  innersha256Layout->setContentsMargins(0, 0, 0, 0);
  innersha256Layout->addWidget(m_lblHash);
  innersha256Layout->addWidget(m_lblArt);

  auto sha256Layout = new QVBoxLayout();
  sha256Layout->setContentsMargins(0, 0, 0, 0);
  sha256Layout->addWidget(labelTitle);
  auto *algorithm = new QLabel(QStringLiteral("SHA-256"), this);
  algorithm->setProperty("muted", true);
  sha256Layout->addWidget(algorithm);
  sha256Layout->addLayout(innersha256Layout);
  auto *copy = new QPushButton(tr("Copy fingerprint"), this);
  copy->setAccessibleName(tr("Copy fingerprint"));
  copy->setIcon(synergy::gui::appIcon(QStringLiteral("edit-copy")));
  sha256Layout->addWidget(copy, 0, Qt::AlignLeft);
  connect(copy, &QPushButton::clicked, this, [data = fingerprint.data] {
    QGuiApplication::clipboard()->setText(QString::fromLatin1(data.toHex(':').toUpper()));
  });

  sha256Layout->setContentsMargins(12, 12, 12, 12);
  sha256Layout->setSpacing(8);
  auto *layout = sha256Layout;

  return layout;
}
