/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "KeySequenceWidget.h"
#include "gui/ActionPresentation.h"

#include <QMouseEvent>

KeySequenceWidget::KeySequenceWidget(QWidget *parent, const KeySequence &seq)
    : QPushButton(parent),
      m_KeySequence(seq),
      m_BackupSequence(seq)
{
  setFocusPolicy(Qt::StrongFocus);
  setProperty("keyCapture", true);
  connect(this, &QPushButton::clicked, this, [this] {
    if (isEnabled() && status() == Stopped)
      startRecording();
  });
  updateOutput();
}

void KeySequenceWidget::setKeySequence(const KeySequence &seq)
{
  if (isRecording())
    releaseKeyboard();
  m_KeySequence = seq;
  m_BackupSequence = seq;

  setStatus(Stopped);
  updateOutput();
}

QPushButton *KeySequenceWidget::createRecordButton(QWidget *parent)
{
  m_recordButton = new QPushButton(tr("Record keys"), parent);
  m_recordButton->setObjectName(QStringLiteral("recordShortcutButton"));
  m_recordButton->setIcon(synergy::gui::appIcon(QStringLiteral("key")));
  m_recordButton->setAutoDefault(false);
  connect(m_recordButton, &QPushButton::clicked, this, [this] {
    if (!isRecording())
      click();
  });
  connect(this, &KeySequenceWidget::recordingChanged, m_recordButton, [button = m_recordButton](bool recording) {
    if (button)
      button->setEnabled(!recording);
  });
  return m_recordButton;
}

void KeySequenceWidget::mousePressEvent(QMouseEvent *event)
{
  event->accept();

  if (status() == Stopped) {
    startRecording();
    return;
  }

  if (m_KeySequence.appendMouseButton(event->button()))
    stopRecording();

  updateOutput();
}

void KeySequenceWidget::startRecording()
{
  m_BackupSequence = m_KeySequence;
  m_KeySequence = KeySequence();
  setDown(true);
  setFocus();
  grabKeyboard();
  setStatus(Recording);
  updateOutput();
}

void KeySequenceWidget::stopRecording()
{
  if (!keySequence().valid()) {
    m_KeySequence = backupSequence();
    updateOutput();
  }

  setStatus(Stopped);
  setDown(false);
  releaseKeyboard();
  updateOutput();
  focusNextChild();
  Q_EMIT keySequenceChanged();
}

bool KeySequenceWidget::event(QEvent *event)
{
  if (event->type() == QEvent::LanguageChange || event->type() == QEvent::EnabledChange)
    updateOutput();
  if (status() == Recording) {
    switch (event->type()) {
    case QEvent::KeyPress:
      keyPressEvent(static_cast<QKeyEvent *>(event));
      return true;

    case QEvent::MouseButtonRelease:
      event->accept();
      return true;

    case QEvent::ShortcutOverride:
      event->accept();
      return true;

    case QEvent::FocusOut:
      stopRecording();
      if (!valid()) {
        m_KeySequence = backupSequence();
        updateOutput();
      }
      break;

    default:
      break;
    }
  }

  return QPushButton::event(event);
}

void KeySequenceWidget::keyPressEvent(QKeyEvent *event)
{
  if (status() == Stopped) {
    if (isEnabled() &&
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
      event->accept();
      startRecording();
      return;
    }
    QPushButton::keyPressEvent(event);
    return;
  }

  event->accept();

  if (event->key() == Qt::Key_Escape) {
    m_KeySequence = backupSequence();
    stopRecording();
    updateOutput();
    return;
  }

  if (m_KeySequence.appendKey(event->key(), event->modifiers()))
    stopRecording();

  updateOutput();
}

void KeySequenceWidget::updateOutput()
{
  if (m_recordButton) {
    m_recordButton->setText(tr("Record keys"));
    m_recordButton->setEnabled(isEnabled() && !isRecording());
  }
  if (isRecording()) {
    setText(tr("Recording… Press Esc to cancel"));
    return;
  }
  setText(synergy::gui::keySequenceCaption(m_KeySequence));
}
