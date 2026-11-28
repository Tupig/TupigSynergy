/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/IEventQueueBuffer.h"

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QPointer>

#include <queue>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class IEventQueue;

//! Event queue buffer for Win32
class MSWindowsEventQueueBuffer : public IEventQueueBuffer, private QAbstractNativeEventFilter
{
public:
  MSWindowsEventQueueBuffer(IEventQueue *events);
  ~MSWindowsEventQueueBuffer() override;

  // IEventQueueBuffer overrides
  void init() override
  {
  }
  void waitForEvent(double timeout) override;
  Type getEvent(Event &event, uint32_t &dataID) override;
  bool addEvent(uint32_t dataID) override;
  bool isEmpty() const override;

private:
  bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

  QPointer<QAbstractEventDispatcher> m_dispatcher;
  std::queue<MSG> m_deferredMessages;
  DWORD m_thread;
  UINT m_userEvent;
  MSG m_event;
  UINT m_daemonQuit;
  IEventQueue *m_events;
  const UINT m_supportedMessages = (QS_ALLINPUT & ~(QS_TOUCH | QS_POINTER));
};
