/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "PageHost.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

PageHost::PageHost(QWidget *home, QWidget *parent) : QWidget(parent)
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  auto *navigation = new QHBoxLayout;
  navigation->setContentsMargins(12, 4, 12, 4);
  m_back = new QPushButton(this);
  m_back->setIcon(QIcon::fromTheme(QStringLiteral("go-previous"), style()->standardIcon(QStyle::SP_ArrowBack)));
  m_back->setAccessibleName(tr("Back"));
  m_back->setToolTip(tr("Back"));
  m_back->setProperty("iconButton", true);
  m_title = new QLabel(this);
  m_title->setObjectName(QStringLiteral("pageTitle"));
  navigation->addWidget(m_back);
  navigation->addWidget(m_title, 1);
  layout->addLayout(navigation);
  // QStackedWidget 默认采用所有页的最大 sizeHint；隐藏的大表单不应撑大首页。
  class ContentStack : public QStackedWidget
  {
  public:
    using QStackedWidget::QStackedWidget;
    QSize sizeHint() const override
    {
      return currentWidget() ? currentWidget()->sizeHint() : QSize();
    }
    QSize minimumSizeHint() const override
    {
      return currentWidget() ? currentWidget()->minimumSizeHint() : QSize();
    }
  };
  m_stack = new ContentStack(this);
  m_stack->addWidget(home);
  layout->addWidget(m_stack, 1);
  connect(m_back, &QPushButton::clicked, this, [this] {
    if (auto *area = qobject_cast<QScrollArea *>(m_stack->currentWidget())) {
      if (auto *editor = qobject_cast<QDialog *>(area->widget()))
        editor->reject();
    }
  });
  updateNavigation();
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
  return m_stack->count() > 1;
}

bool PageHost::activateEditor(const QMetaObject &type)
{
  for (int index = 1; index < m_stack->count(); ++index) {
    auto *area = qobject_cast<QScrollArea *>(m_stack->widget(index));
    if (area && area->widget()->metaObject()->inherits(&type)) {
      // 保持后代草稿所在的当前页，不让父编辑器在子页仍存活时被更改。
      m_stack->currentWidget()->setFocus();
      return true;
    }
  }
  return false;
}

void PageHost::push(QDialog *editor)
{
  auto *area = new QScrollArea(m_stack);
  area->setFrameShape(QFrame::NoFrame);
  area->setWidgetResizable(true);
  editor->setWindowFlags(Qt::Widget);
  editor->setWindowModality(Qt::NonModal);
  editor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  area->setWidget(editor);
  m_stack->addWidget(area);
  m_stack->setCurrentWidget(area);
  connect(editor, &QDialog::windowTitleChanged, this, &PageHost::updateNavigation);
  connect(editor, &QDialog::finished, this, [this, area] {
    const int index = m_stack->indexOf(area);
    const bool current = m_stack->currentWidget() == area;
    // 父页被外部动作关闭时先拒绝后代，避免编辑器继续持有父页草稿引用。
    while (index >= 0 && m_stack->count() > index + 1) {
      auto *childArea = qobject_cast<QScrollArea *>(m_stack->widget(index + 1));
      if (!childArea)
        break;
      auto *child = qobject_cast<QDialog *>(childArea->widget());
      if (!child)
        break;
      child->reject();
    }
    m_stack->removeWidget(area);
    if (current || m_stack->currentIndex() < 0)
      m_stack->setCurrentIndex(qMax(0, index - 1));
    // finished 的业务回调仍可访问编辑器；下一轮事件循环再销毁。
    area->deleteLater();
    updateNavigation();
  });
  editor->show();
  updateNavigation();
}

void PageHost::updateNavigation()
{
  if (m_editorVisible != hasEditor()) {
    m_editorVisible = hasEditor();
    Q_EMIT editorVisibilityChanged(m_editorVisible);
  }
  m_back->setVisible(hasEditor());
  m_title->setVisible(hasEditor());
  if (auto *area = qobject_cast<QScrollArea *>(m_stack->currentWidget()))
    m_title->setText(area->widget()->windowTitle());
}
