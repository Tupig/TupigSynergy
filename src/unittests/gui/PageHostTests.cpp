/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/widgets/PageHost.h"
#include <QDialog>
#include <QPointer>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtTest>

namespace {

/// 复现 MainWindow.ui 的声明式结构：PageHost 内有一个名为 pageStack 的页面栈。
QStackedWidget *makeHost(PageHost &host, QWidget *home)
{
  auto *stack = new QStackedWidget(&host);
  stack->setObjectName(QStringLiteral("pageStack"));
  stack->addWidget(home);
  auto *layout = new QVBoxLayout(&host);
  layout->addWidget(stack);
  return stack;
}

} // namespace

class PageHostTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void embeddedAccept();
  void backRejectsOnlyChild();
  void reportsTitle();
};

void PageHostTests::embeddedAccept()
{
  PageHost host;
  auto *stack = makeHost(host, new QWidget);
  auto *editor = new QDialog;
  editor->setWindowTitle(QStringLiteral("Editor"));
  QPointer<QDialog> tracked(editor);
  QSignalSpy accepted(editor, &QDialog::accepted);
  host.push(editor);
  QVERIFY(!editor->isWindow());
  QVERIFY(host.hasEditor());
  QCOMPARE(stack->count(), 2);
  editor->accept();
  QCOMPARE(accepted.count(), 1);
  QVERIFY(!host.hasEditor());
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(tracked.isNull());
}

void PageHostTests::backRejectsOnlyChild()
{
  PageHost host;
  QWidget home;
  makeHost(host, &home);
  auto *parent = new QDialog;
  auto *child = new QDialog;
  parent->setWindowTitle(QStringLiteral("Server configuration"));
  child->setWindowTitle(QStringLiteral("Computer settings"));
  QSignalSpy titles(&host, &PageHost::titleChanged);
  QSignalSpy parentRejected(parent, &QDialog::rejected);
  QSignalSpy childRejected(child, &QDialog::rejected);
  host.push(parent);
  host.push(child);
  // 返回语义由 MainWindow 的返回按钮触发：拒绝当前编辑器。
  child->reject();
  QCOMPARE(childRejected.count(), 1);
  QCOMPARE(parentRejected.count(), 0);
  QVERIFY(host.hasEditor());
  QCOMPARE(titles.last().first().toString(), QStringLiteral("Server configuration"));
  parent->reject();
  QVERIFY(!host.hasEditor());
}

void PageHostTests::reportsTitle()
{
  PageHost host;
  QWidget home;
  makeHost(host, &home);
  QSignalSpy titleSpy(&host, &PageHost::titleChanged);
  QSignalSpy visibleSpy(&host, &PageHost::editorVisibilityChanged);

  auto *editor = new QDialog;
  editor->setWindowTitle(QStringLiteral("Preferences"));
  host.push(editor);

  QVERIFY(!titleSpy.isEmpty());
  QCOMPARE(titleSpy.last().first().toString(), QStringLiteral("Preferences"));
  QCOMPARE(visibleSpy.last().first().toBool(), true);
}

QTEST_MAIN(PageHostTests)
#include "PageHostTests.moc"
