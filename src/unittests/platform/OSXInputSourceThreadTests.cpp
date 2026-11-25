/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXAutoTypes.h"
#include "OSXCoreQuitFilter.h"
#include "OSXInputSourceThread.h"

#include <QEventLoop>
#include <QTest>
#include <QThread>

class OSXInputSourceThreadTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void inputSourceReadsUseTheNativeMainThread()
  {
    const auto readInputSource = [] {
      if (!pthread_main_np())
        return false;
      const AutoTISInputSourceRef source(TISCopyCurrentKeyboardLayoutInputSource(), CFRelease);
      return true;
    };

    bool mainCall = false;
    synergy::platform::onInputSourceThread([&] { mainCall = readInputSource(); });
    QVERIFY(mainCall);

    bool workerIsMain = true;
    int mainCallsFromWorker = 0;
    QEventLoop loop;
    const std::unique_ptr<QThread> worker(QThread::create([&] {
      workerIsMain = pthread_main_np();
      for (int i = 0; i < 8; ++i)
        synergy::platform::onInputSourceThread([&] { mainCallsFromWorker += readInputSource(); });
    }));
    connect(worker.get(), &QThread::finished, &loop, &QEventLoop::quit);
    worker->start();
    loop.exec();
    worker->wait();

    QVERIFY(!workerIsMain);
    QCOMPARE(mainCallsFromWorker, 8);
  }

  void quitKeepsTheMainThreadAvailableUntilTheWorkerStops()
  {
    bool queryCompleted = false;
    QEventLoop loop;
    const std::unique_ptr<QThread> worker(QThread::create([&] {
      synergy::platform::onInputSourceThread([&] { queryCompleted = pthread_main_np(); });
    }));
    int stopRequests = 0;
    synergy::platform::OSXCoreQuitFilter filter(*worker, [&] { ++stopRequests; });
    const auto app = QCoreApplication::instance();
    app->installEventFilter(&filter);
    connect(worker.get(), &QThread::finished, &loop, &QEventLoop::quit);
    worker->start();

    QEvent quit(QEvent::Quit);
    QCoreApplication::sendEvent(app, &quit);
    const bool quitDeferred = !quit.isAccepted();
    QEvent repeatedQuit(QEvent::Quit);
    QCoreApplication::sendEvent(app, &repeatedQuit);

    loop.exec();
    worker->wait();
    app->removeEventFilter(&filter);

    QVERIFY(quitDeferred);
    QVERIFY(queryCompleted);
    QCOMPARE(stopRequests, 1);
  }
};

QTEST_GUILESS_MAIN(OSXInputSourceThreadTests)
#include "OSXInputSourceThreadTests.moc"
