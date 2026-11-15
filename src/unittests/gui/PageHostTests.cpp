/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/widgets/PageHost.h"
#include <QDialog>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QtTest>

class PageHostTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void embeddedAccept();
  void backRejectsOnlyChild();
  void closingParentRejectsChildren();
};

void PageHostTests::embeddedAccept()
{
  PageHost host(new QWidget);
  auto *editor = new QDialog;
  QPointer<QDialog> tracked(editor);
  QSignalSpy accepted(editor, &QDialog::accepted);
  host.push(editor);
  QVERIFY(!editor->isWindow());
  QVERIFY(host.hasEditor());
  editor->accept();
  QCOMPARE(accepted.count(), 1);
  QVERIFY(!host.hasEditor());
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(tracked.isNull());
}

void PageHostTests::backRejectsOnlyChild()
{
  PageHost host(new QWidget);
  auto *parent = new QDialog;
  auto *child = new QDialog;
  QSignalSpy parentRejected(parent, &QDialog::rejected);
  QSignalSpy childRejected(child, &QDialog::rejected);
  host.push(parent);
  host.push(child);
  host.findChild<QPushButton *>()->click();
  QCOMPARE(childRejected.count(), 1);
  QCOMPARE(parentRejected.count(), 0);
  QVERIFY(host.hasEditor());
  auto *stack = host.findChild<QStackedWidget *>();
  QCOMPARE(stack->count(), 2);
  parent->reject();
  QVERIFY(!host.hasEditor());
}

void PageHostTests::closingParentRejectsChildren()
{
  PageHost host(new QWidget);
  auto *parent = new QDialog;
  auto *child = new QDialog;
  QSignalSpy childRejected(child, &QDialog::rejected);
  host.push(parent);
  host.push(child);
  parent->reject();
  QCOMPARE(childRejected.count(), 1);
  QVERIFY(!host.hasEditor());
  QCOMPARE(host.findChild<QStackedWidget *>()->count(), 1);
}

QTEST_MAIN(PageHostTests)
#include "PageHostTests.moc"
