/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QWidget>
#include <QtTest>

// COMPLETE-R1 mechanical capture correction: static evidence depicts the settled
// endpoint; R3MotionContractTests independently retains intermediate-frame checks.
inline bool settleStaticRender(QWidget &root)
{
  QElapsedTimer timer;
  timer.start();
  do {
    QCoreApplication::processEvents();
    bool active = false;
    for (auto *widget : root.findChildren<QWidget *>()) {
      if (!widget->isVisibleTo(&root))
        continue;
      const auto name = widget->objectName();
      if (name == QStringLiteral("contentTransitionOverlay") || name == QStringLiteral("buttonFeedbackOverlay")) {
        active = true;
        break;
      }
    }
    if (!active)
      return true;
    QTest::qWait(10);
  } while (timer.elapsed() < 1000);
  return false;
}
