/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QPointer>
#include <QPushButton>

#include "KeySequence.h"

class KeySequenceWidget : public QPushButton
{
  Q_OBJECT

public:
  explicit KeySequenceWidget(QWidget *parent, const KeySequence &seq = KeySequence());

Q_SIGNALS:
  void keySequenceChanged();
  void recordingChanged(bool recording);

public:
  const QString &mousePrefix() const
  {
    return m_MousePrefix;
  }
  const QString &mousePostfix() const
  {
    return m_MousePostfix;
  }
  const QString &keyPrefix() const
  {
    return m_KeyPrefix;
  }
  const QString &keyPostfix() const
  {
    return m_KeyPostfix;
  }

  void setMousePrefix(const QString &s)
  {
    m_MousePrefix = s;
  }
  void setMousePostfix(const QString &s)
  {
    m_MousePostfix = s;
  }
  void setKeyPrefix(const QString &s)
  {
    m_KeyPrefix = s;
  }
  void setKeyPostfix(const QString &s)
  {
    m_KeyPostfix = s;
  }

  const KeySequence &keySequence() const
  {
    return m_KeySequence;
  }
  const KeySequence &backupSequence() const
  {
    return m_BackupSequence;
  }
  void setKeySequence(const KeySequence &seq);
  QPushButton *createRecordButton(QWidget *parent);

  bool valid() const
  {
    return keySequence().valid();
  }
  bool isRecording() const
  {
    return m_Status == Recording;
  }

protected:
  void mousePressEvent(QMouseEvent *) override;
  void keyPressEvent(QKeyEvent *) override;
  bool event(QEvent *event) override;
  void appendToSequence(int key);
  void updateOutput();
  void startRecording();
  void stopRecording();

private:
  enum Status
  {
    Stopped,
    Recording
  };
  void setStatus(Status s)
  {
    if (m_Status == s)
      return;
    m_Status = s;
    Q_EMIT recordingChanged(s == Recording);
  }
  Status status() const
  {
    return m_Status;
  }

private:
  KeySequence m_KeySequence;
  KeySequence m_BackupSequence;
  Status m_Status = Status::Stopped;
  QPointer<QPushButton> m_recordButton;
  QString m_MousePrefix = QStringLiteral("mousebutton(");
  QString m_MousePostfix = QStringLiteral(")");
  QString m_KeyPrefix = QStringLiteral("keystroke(");
  QString m_KeyPostfix = QStringLiteral(")");
};
