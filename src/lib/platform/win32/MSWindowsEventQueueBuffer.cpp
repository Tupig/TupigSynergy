/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsEventQueueBuffer.h"

#include "arch/win32/ArchDaemonWindows.h"
#include "base/IEventQueue.h"

//
// MSWindowsEventQueueBuffer
//

MSWindowsEventQueueBuffer::MSWindowsEventQueueBuffer(IEventQueue *events) : m_events(events)
{
  // remember thread.  we'll be posting messages to it.
  m_thread = GetCurrentThreadId();

  // create a message type for custom events
  m_userEvent = RegisterWindowMessage(L"SYNERGY_USER_EVENT");

  // get message type for daemon quit
  m_daemonQuit = ArchDaemonWindows::getDaemonQuitMessage();

  // make sure this thread has a message queue
  MSG dummy;
  PeekMessage(&dummy, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

  m_dispatcher = QAbstractEventDispatcher::instance();
  if (m_dispatcher) {
    m_dispatcher->installNativeEventFilter(this);
  }
}

MSWindowsEventQueueBuffer::~MSWindowsEventQueueBuffer()
{
  if (m_dispatcher) {
    m_dispatcher->removeNativeEventFilter(this);
  }
}

bool MSWindowsEventQueueBuffer::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
  if (eventType != "windows_generic_MSG")
    return false;
  const auto &msg = *static_cast<const MSG *>(message);
  if (msg.hwnd || (msg.message != m_userEvent && (m_daemonQuit == 0 || msg.message != m_daemonQuit)))
    return false;

  // Qt pumps the same Win32 queue for socket notifications. It removes
  // private thread messages too, but DispatchMessage cannot deliver them
  // without a window. Preserve them for the core's normal event dispatch.
  m_deferredMessages.push(msg);
  return true;
}

void MSWindowsEventQueueBuffer::waitForEvent(double timeout)
{
  // check if messages are available first.  if we don't do this then
  // MsgWaitForMultipleObjects() will block even if the queue isn't
  // empty if the messages in the queue were there before the last
  // call to GetMessage()/PeekMessage().
  if (!isEmpty()) {
    return;
  }

  // convert timeout
  DWORD t;
  if (timeout < 0.0) {
    t = INFINITE;
  } else {
    t = (DWORD)(1000.0 * timeout);
  }

  // wait for a message.  we cannot be interrupted by thread
  // cancellation but that's okay because we're run in the main
  // thread and we never cancel that thread.
  HANDLE dummy[1];
  MsgWaitForMultipleObjects(0, dummy, FALSE, t, m_supportedMessages);
}

IEventQueueBuffer::Type MSWindowsEventQueueBuffer::getEvent(Event &event, uint32_t &dataID)
{
  using enum IEventQueueBuffer::Type;
  // NOTE: QS_ALLINPUT was replaced with m_supportedMessages.
  //
  // peek at messages first.  waiting for QS_ALLINPUT will return
  // if a message has been sent to our window but GetMessage will
  // dispatch that message behind our backs and block.  PeekMessage
  // will also dispatch behind our backs but won't block.
  if (!m_deferredMessages.empty()) {
    m_event = m_deferredMessages.front();
    m_deferredMessages.pop();
  } else if (!PeekMessage(&m_event, nullptr, 0, 0, PM_REMOVE)) {
    return Unknown;
  }
  if (m_event.message == WM_QUIT || (m_daemonQuit != 0 && m_event.message == m_daemonQuit)) {
    event = Event(EventTypes::Quit);
    return System;
  } else if (m_event.message == m_userEvent) {
    dataID = static_cast<uint32_t>(m_event.wParam);
    return User;
  } else {
    event = Event(EventTypes::System, m_events->getSystemTarget(), &m_event);
    return System;
  }
}

bool MSWindowsEventQueueBuffer::addEvent(uint32_t dataID)
{
  return (PostThreadMessage(m_thread, m_userEvent, static_cast<WPARAM>(dataID), 0) != 0);
}

bool MSWindowsEventQueueBuffer::isEmpty() const
{
  return m_deferredMessages.empty() && HIWORD(GetQueueStatus(m_supportedMessages)) == 0;
}
