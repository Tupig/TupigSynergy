/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "PageHost.h"

#include <QDialog>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVariant>

PageHost::PageHost(QWidget *parent) : QWidget(parent)
{
}

QStackedWidget *PageHost::stack()
{
  // uic 先构造 PageHost 再添加子控件，因此不能在建構函数里查找。
  if (m_stack == nullptr)
    m_stack = findChild<QStackedWidget *>(QStringLiteral("pageStack"));
  return m_stack;
}

void PageHost::present(QWidget *origin, QDialog *editor)
{
  for (auto *widget = origin; widget; widget = widget->parentWidget()) {
    if (auto *host = qobject_cast<PageHost *>(widget)) {
      host->push(editor);
      return;
    }
  }
  if (origin) {
    if (auto *host = origin->findChild<PageHost *>()) {
      host->push(editor);
      return;
    }
  }
  // 独立编辑器的调用方仍可异步使用，不引入嵌套事件循环。
  editor->setAttribute(Qt::WA_DeleteOnClose);
  editor->open();
}

bool PageHost::hasEditor() const
{
  auto *pages = const_cast<PageHost *>(this)->stack();
  return pages != nullptr && pages->count() > 1;
}

bool PageHost::activateEditor(const QMetaObject &type)
{
  auto *pages = stack();
  if (pages == nullptr)
    return false;
  for (int index = 1; index < pages->count(); ++index) {
    auto *area = qobject_cast<QScrollArea *>(pages->widget(index));
    auto *editor = area ? area->property("pageEditor").value<QDialog *>() : nullptr;
    if (editor != nullptr && editor->metaObject()->inherits(&type)) {
      // 保持后代草稿所在的当前页，不让父编辑器在子页仍存活时被更改。
      pages->currentWidget()->setFocus();
      return true;
    }
  }
  return false;
}

void PageHost::push(QDialog *editor)
{
  auto *pages = stack();
  if (pages == nullptr) {
    editor->deleteLater();
    return;
  }
  auto *area = new QScrollArea(pages);
  area->setFrameShape(QFrame::NoFrame);
  area->setWidgetResizable(true);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // 设计稿的内容区内边距 10：编辑器与主页卡片左边缘对齐。
  // 重新设置窗口标志会清空窗口标题，先记住设计稿要求的页面标题。
  const auto pageTitle = editor->windowTitle();
  auto *container = new QWidget;
  auto *containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(10, 10, 10, 10);
  containerLayout->addWidget(editor);
  // 有页签的编辑器使用整个内容区，使网格/列表获得真正的可用高度。
  // 简短消息页仍按内容贴顶，不拉伸卡片。
  if (editor->findChild<QTabWidget *>() == nullptr)
    containerLayout->addStretch(1);
  editor->setWindowFlags(Qt::Widget);
  editor->setWindowTitle(pageTitle);
  editor->setWindowModality(Qt::NonModal);
  editor->setMinimumWidth(0);
  editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  // 设计稿的页签不省略、不用滚动按钮，标签始终完整可读。
  for (auto *tabs : editor->findChildren<QTabWidget *>()) {
    tabs->setDocumentMode(true);
    tabs->tabBar()->setElideMode(Qt::ElideNone);
    tabs->setUsesScrollButtons(false);
  }
  area->setProperty("pageEditor", QVariant::fromValue(editor));
  area->setWidget(container);
  pages->addWidget(area);
  pages->setCurrentWidget(area);
  connect(editor, &QDialog::windowTitleChanged, this, &PageHost::updateNavigation);
  connect(editor, &QDialog::finished, this, [this, area, pages] {
    const int index = pages->indexOf(area);
    const bool current = pages->currentWidget() == area;
    // 父页被外部动作关闭时先拒绝后代，避免编辑器继续持有父页草稿引用。
    while (index >= 0 && pages->count() > index + 1) {
      auto *childArea = qobject_cast<QScrollArea *>(pages->widget(index + 1));
      if (!childArea)
        break;
      auto *child = childArea->property("pageEditor").value<QDialog *>();
      if (!child)
        break;
      child->reject();
    }
    pages->removeWidget(area);
    if (current || pages->currentIndex() < 0)
      pages->setCurrentIndex(qMax(0, index - 1));
    // finished 的业务回调仍可访问编辑器；下一轮事件循环再销毁。
    area->deleteLater();
    updateNavigation();
  });
  editor->show();
  updateNavigation();
  Q_EMIT titleChanged(pageTitle);
}

void PageHost::updateNavigation()
{
  if (m_editorVisible != hasEditor()) {
    m_editorVisible = hasEditor();
    Q_EMIT editorVisibilityChanged(m_editorVisible);
  }
  auto *pages = stack();
  if (auto *area = pages ? qobject_cast<QScrollArea *>(pages->currentWidget()) : nullptr) {
    auto *editor = area->property("pageEditor").value<QDialog *>();
    Q_EMIT titleChanged(editor ? editor->windowTitle() : QString());
  } else
    Q_EMIT titleChanged(QString());
}
