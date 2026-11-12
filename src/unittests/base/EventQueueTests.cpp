/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "EventQueueTests.h"

#include "arch/Arch.h"
#include "base/EventQueue.h"

#include <QElapsedTimer>

void EventQueueTests::initTestCase()
{
  // The event queue reaches for the ARCH singleton (signal handlers), which
  // only the application normally installs.
  static Arch arch;
#if defined(Q_OS_WIN)
  arch.init();
#endif
}

void EventQueueTests::oneShotTimerFiresOnce()
{
  EventQueue events;
  int fired = 0;

  EventQueueTimer *timer = events.newOneShotTimer(0.05, nullptr);
  events.addHandler(EventTypes::Timer, timer, [&fired](const auto &) { ++fired; });

  Event event;
  QVERIFY(events.getEvent(event, 2.0));
  QCOMPARE(event.getType(), EventTypes::Timer);
  QVERIFY(events.dispatchEvent(event));
  Event::deleteData(event);
  QCOMPARE(fired, 1);

  // a one-shot timer must not fire a second time
  Event waitForSecond;
  QVERIFY(!events.getEvent(waitForSecond, 0.2));
  QCOMPARE(fired, 1);
}

void EventQueueTests::postedEventIsDispatched()
{
  EventQueue events;
  bool handled = false;
  int target = 0;

  events.addHandler(EventTypes::ScreenSuspend, &target, [&handled](const auto &) { handled = true; });

  // dispatch directly: addEvent() only reaches the buffer once loop() runs
  // (the queue drains its pre-loop pending list there), so a dispatch event
  // is the contract under test here.
  const Event event(EventTypes::ScreenSuspend, &target);
  QCOMPARE(event.getType(), EventTypes::ScreenSuspend);
  QVERIFY(events.dispatchEvent(event));
  QVERIFY(handled);
}

void EventQueueTests::removedHandlerStopsDispatch()
{
  EventQueue events;
  bool handled = false;
  int target = 0;

  events.addHandler(EventTypes::ScreenSuspend, &target, [&handled](const auto &) { handled = true; });
  events.removeHandler(EventTypes::ScreenSuspend, &target);

  const Event event(EventTypes::ScreenSuspend, &target);
  QVERIFY(!events.dispatchEvent(event));
  QVERIFY(!handled);
}

void EventQueueTests::deletedTimerDoesNotFire()
{
  EventQueue events;
  int fired = 0;

  EventQueueTimer *timer = events.newOneShotTimer(0.05, nullptr);
  events.addHandler(EventTypes::Timer, timer, [&fired](const auto &) { ++fired; });
  events.deleteTimer(timer);

  Event event;
  QVERIFY(!events.getEvent(event, 0.3));
  QCOMPARE(fired, 0);
}

void EventQueueTests::waitForReadyAfterLoopStarted()
{
  EventQueue events;

  // Queue Quit before the loop starts: loop() signals readiness, drains the
  // pending event and returns, so the wakeup predates waitForReady(). The old
  // implementation treated CondVar::wait()'s return value as the predicate and
  // lost this wakeup, blocking until the 10 second deadline.
  events.addEvent(Event(EventTypes::Quit));
  events.loop();

  QElapsedTimer timer;
  timer.start();
  events.waitForReady();
  QVERIFY2(timer.elapsed() < 2000, "waitForReady lost the ready signal");
}

QTEST_MAIN(EventQueueTests)
