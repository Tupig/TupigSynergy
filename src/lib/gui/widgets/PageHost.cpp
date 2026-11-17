/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "PageHost.h"
#include "gui/Motion.h"

#include <QDialog>
#include <QEvent>
#include <QLayout>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

namespace {
int layoutHeight(QWidget *widget, int width)
{
  widget->ensurePolished();
  if (auto *layout = widget->layout()) {
    layout->invalidate();
    const int height =
        layout->hasHeightForWidth() ? layout->totalHeightForWidth(width) : layout->totalSizeHint().height();
    return qMax(layout->totalMinimumSize().height(), height);
  }
  return qMax(widget->minimumHeight(), widget->sizeHint().height());
}

// Qt's default tab and scroll hints retain hidden pages and a minimum viewport.
// Normalize only the active subtree; the outer workspace owns overflow scrolling.
void fitActiveContainers(QWidget *root, int width)
{
  root->resize(width, root->height());
  auto children = root->findChildren<QWidget *>();
  for (auto it = children.crbegin(); it != children.crend(); ++it) {
    auto *widget = *it;
    if (!widget->isVisibleTo(root))
      continue;
    if (auto *area = qobject_cast<QScrollArea *>(widget); area && area->widget()) {
      const int frame = 2 * area->frameWidth();
      const int contentWidth = qMax(1, area->width() - frame);
      const int height = layoutHeight(area->widget(), contentWidth);
      area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      area->setFixedHeight(height + frame);
    } else if (auto *tabs = qobject_cast<QTabWidget *>(widget); tabs && tabs->currentWidget()) {
      const int chrome =
          tabs->tabBar()->sizeHint().height() + 2 * tabs->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
      tabs->setFixedHeight(layoutHeight(tabs->currentWidget(), qMax(1, tabs->width() - 4)) + chrome);
    } else if (auto *pages = qobject_cast<QStackedWidget *>(widget); pages && pages->currentWidget()) {
      pages->setFixedHeight(layoutHeight(pages->currentWidget(), pages->width()) + 2 * pages->frameWidth());
    }
  }
}
} // namespace

PageHost::PageHost(QWidget *parent) : QWidget(parent)
{
  // QStackedLayout's all-page minimum must never hold the containing window open.
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
}

QStackedWidget *PageHost::stack()
{
  // uic 先构造 PageHost 再添加子控件，因此不能在建構函数里查找。
  if (m_stack == nullptr) {
    m_stack = findChild<QStackedWidget *>(QStringLiteral("pageStack"));
    if (m_stack) {
      m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
      connect(m_stack, &QStackedWidget::currentChanged, this, [this] { requestContentFit(); });
      watchContent(m_stack);
    }
  }
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
  containerLayout->setAlignment(Qt::AlignTop);
  editor->setWindowFlags(Qt::Widget);
  // PageHost owns the sole workspace inset; standalone dialog margins remain intact.
  if (editor->layout()) {
    editor->layout()->setContentsMargins(0, 0, 0, 0);
    editor->layout()->setSizeConstraint(QLayout::SetNoConstraint);
  }
  editor->setProperty("embeddedPage", true);
  editor->setWindowTitle(pageTitle);
  editor->setWindowModality(Qt::NonModal);
  editor->setMinimumSize(0, 0);
  editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  // 设计稿的页签不省略、不用滚动按钮，标签始终完整可读。
  for (auto *tabs : editor->findChildren<QTabWidget *>()) {
    tabs->setDocumentMode(true);
    tabs->tabBar()->setElideMode(Qt::ElideNone);
    tabs->setUsesScrollButtons(false);
    // Keep the pill strip independent of its cards. The page layout includes
    // this gap in height measurement, so active content remains fully visible.
    for (int index = 0; index < tabs->count(); ++index) {
      if (auto *layout = tabs->widget(index)->layout()) {
        auto margins = layout->contentsMargins();
        margins.setTop(8);
        layout->setContentsMargins(margins);
      }
    }
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
    synergy::gui::animateContentTransition(this);
  });
  editor->show();
  watchContent(area);
  updateNavigation();
  Q_EMIT titleChanged(pageTitle);
  synergy::gui::animateContentTransition(this);
}

void PageHost::updateNavigation()
{
  requestContentFit();
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

void PageHost::watchContent(QWidget *root)
{
  root->installEventFilter(this);
  const auto widgets = root->findChildren<QWidget *>();
  for (auto *widget : widgets) {
    widget->installEventFilter(this);
    if (widget->property("pageHostWatching").toBool())
      continue;
    widget->setProperty("pageHostWatching", true);
    if (auto *tabs = qobject_cast<QTabWidget *>(widget))
      connect(tabs, &QTabWidget::currentChanged, this, [this] { requestContentFit(); });
  }
}

int PageHost::preferredContentHeight(int width)
{
  auto *pages = stack();
  auto *area = pages ? qobject_cast<QScrollArea *>(pages->currentWidget()) : nullptr;
  if (!area || !area->widget())
    return 0;
  const QScopedValueRollback<bool> measuring(m_measuring, true);
  auto *content = area->widget();
  const int contentWidth = qMax(1, width - 2 * area->frameWidth());
  content->resize(contentWidth, content->height());
  if (content->layout())
    content->layout()->activate();
  fitActiveContainers(content, contentWidth);
  const int height = layoutHeight(content, contentWidth);
  content->setMinimumHeight(height);
  return height + 2 * area->frameWidth();
}

void PageHost::requestContentFit()
{
  if (m_fitPending || m_measuring)
    return;
  m_fitPending = true;
  QTimer::singleShot(0, this, [this] {
    m_fitPending = false;
    const int preferred = preferredContentHeight(width());
    if (preferred == m_lastPreferredHeight && width() == m_lastPreferredWidth)
      return;
    m_lastPreferredHeight = preferred;
    m_lastPreferredWidth = width();
    Q_EMIT contentSizeChanged();
  });
}

bool PageHost::eventFilter(QObject *object, QEvent *event)
{
  switch (event->type()) {
  case QEvent::LayoutRequest:
  case QEvent::Show:
  case QEvent::Hide:
  case QEvent::Resize:
  case QEvent::FontChange:
  case QEvent::StyleChange:
  case QEvent::LanguageChange:
    requestContentFit();
    break;
  default:
    break;
  }
  return QWidget::eventFilter(object, event);
}
