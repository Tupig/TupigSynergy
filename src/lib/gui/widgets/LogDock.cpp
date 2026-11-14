/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogDock.h"
#include "LogWidget.h"
#include "SearchWidget.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kVisibleLines = 6;

QPushButton *toolButton(QWidget *parent, const QString &icon, const QString &tooltip)
{
  auto *button = new QPushButton(parent);
  button->setFixedSize(24, 24);
  button->setFlat(true);
  button->setIcon(QIcon::fromTheme(icon));
  button->setIconSize(QSize(14, 14));
  button->setToolTip(tooltip);
  button->setProperty("iconButton", true);
  return button;
}
} // namespace

LogDock::LogDock(QWidget *parent)
    : QDockWidget(tr("Log"), parent),
      m_textLog{new LogWidget(this)},
      m_btnClose{new QPushButton(this)},
      m_btnFloat{new QPushButton(this)},
      m_btnCollapse{toolButton(this, QStringLiteral("go-down"), tr("Collapse Log"))},
      m_btnCopy{toolButton(this, QStringLiteral("edit-copy"), tr("Copy Log"))},
      m_btnClear{toolButton(this, QStringLiteral("edit-clear"), tr("Clear Log"))},
      m_lblTitle{new QLabel(tr("Log"), this)},
      m_lblTail{new QLabel(this)},
      m_lblBadge{new QLabel(this)},
      m_searchWidget{new SearchWidget(this)},
      m_levelGroup{new QButtonGroup(this)}
{
  connect(m_searchWidget, &SearchWidget::findNext, m_textLog, &LogWidget::findNext);
  connect(m_searchWidget, &SearchWidget::findPrevious, m_textLog, &LogWidget::findPrevious);

  const auto iconSize = QSize(fontMetrics().height() - 2, fontMetrics().height() - 2);
  const auto maxBtnSize = QSize(fontMetrics().height() + 2, fontMetrics().height() + 2);

  m_btnFloat->setFixedSize(maxBtnSize);
  m_btnFloat->setCheckable(true);
  m_btnFloat->setFlat(true);
  m_btnFloat->setIcon(QIcon::fromTheme(QStringLiteral("window-minimize-pip")));
  m_btnFloat->setIconSize(iconSize);
  m_btnFloat->setProperty("iconButton", true);
  m_btnFloat->setToolTip(tr("Detach from window"));
  connect(m_btnFloat, &QPushButton::toggled, this, &LogDock::setFloating);

  m_btnClose->setFixedSize(maxBtnSize);
  m_btnClose->setFlat(true);
  m_btnClose->setIcon(QIcon::fromTheme(QStringLiteral("view-close")));
  m_btnClose->setIconSize(iconSize);
  m_btnClose->setProperty("iconButton", true);
  m_btnClose->setToolTip(tr("Close Log"));
  connect(m_btnClose, &QPushButton::clicked, this, &QDockWidget::hide);

  connect(m_btnCopy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_textLog->allText()); });
  connect(m_btnClear, &QPushButton::clicked, this, [this] {
    m_textLog->clear();
    m_lblTail->clear();
  });
  connect(m_btnCollapse, &QPushButton::clicked, this, [this] { setCollapsed(!m_collapsed); });

  m_lblTitle->setObjectName(QStringLiteral("logTitle"));
  m_lblBadge->setObjectName(QStringLiteral("logBadge"));
  m_lblTail->setObjectName(QStringLiteral("logTail"));
  m_lblTail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  // 级别筛选：始终可见，不折叠成下拉框。
  auto *titleWidget = new QWidget(this);
  titleWidget->installEventFilter(this);
  auto *titleLayout = new QHBoxLayout(titleWidget);
  titleLayout->setContentsMargins(10, 4, 10, 4);
  titleLayout->setSpacing(6);
  titleLayout->addWidget(m_lblTitle);
  titleLayout->addWidget(m_lblBadge);
  const auto levels = QList<QPair<QString, QString>>{
      {QString(), tr("All")},
      {QStringLiteral("info"), tr("Info")},
      {QStringLiteral("warn"), tr("Warn")},
      {QStringLiteral("error"), tr("Error")},
  };
  for (const auto &[key, label] : levels) {
    auto *chip = new QPushButton(label, titleWidget);
    chip->setCheckable(true);
    chip->setFlat(true);
    chip->setProperty("levelChip", true);
    chip->setProperty("level", key);
    m_levelGroup->addButton(chip);
    titleLayout->addWidget(chip);
    connect(chip, &QPushButton::clicked, this, [this, chip] { m_textLog->setLevelFilter(chip->property("level").toString()); });
  }
  if (auto *first = m_levelGroup->buttons().value(0))
    first->setChecked(true);
  titleLayout->addStretch(1);
  titleLayout->addWidget(m_searchWidget);
  titleLayout->addWidget(m_btnCopy);
  titleLayout->addWidget(m_btnClear);
  titleLayout->addWidget(m_btnCollapse);
  titleLayout->addWidget(m_btnFloat);
  titleLayout->addWidget(m_btnClose);
  setTitleBarWidget(titleWidget);

  // 展开：日志正文（固定 6 行高度）
  m_bodyWidget = new QWidget(this);
  auto *bodyLayout = new QVBoxLayout(m_bodyWidget);
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->addWidget(m_textLog);

  // 收起：只保留最新一行
  m_tailWidget = new QWidget(this);
  auto *tailLayout = new QHBoxLayout(m_tailWidget);
  tailLayout->setContentsMargins(10, 6, 8, 6);
  tailLayout->setSpacing(8);
  tailLayout->addWidget(m_lblTail, 1);
  auto *expand = toolButton(m_tailWidget, QStringLiteral("go-up"), tr("Expand Log"));
  connect(expand, &QPushButton::clicked, this, [this] { setCollapsed(false); });
  tailLayout->addWidget(expand, 0, Qt::AlignRight);
  m_tailWidget->setVisible(false);

  auto *container = new QWidget(this);
  auto *containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->setSpacing(0);
  containerLayout->addWidget(m_bodyWidget);
  containerLayout->addWidget(m_tailWidget);
  setWidget(container);

  m_textLog->setVisibleLines(kVisibleLines);
  setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
  setAllowedAreas(Qt::BottomDockWidgetArea);
  updateText();
}

void LogDock::appendLine(const QString &msg)
{
  m_textLog->appendLine(msg);
  m_lblTail->setText(msg);
  m_lblTail->setToolTip(msg);
  if (auto p = static_cast<QWidget *>(parent()); p->isVisible() && !p->isMinimized() && !m_searchWidget->isExpanded()) {
    m_textLog->scrollToBottom();
  }
}

void LogDock::setCollapsed(bool collapsed)
{
  m_collapsed = collapsed;

  m_bodyWidget->setVisible(!collapsed);
  m_tailWidget->setVisible(collapsed);
  m_searchWidget->setVisible(!collapsed);
  m_btnCollapse->setIcon(QIcon::fromTheme(collapsed ? QStringLiteral("go-up") : QStringLiteral("go-down")));
  m_btnCollapse->setToolTip(collapsed ? tr("Expand Log") : tr("Collapse Log"));
}

bool LogDock::isCollapsed() const
{
  return m_collapsed;
}

void LogDock::setFloating(bool floating)
{
  if (floating) {
    m_btnFloat->setToolTip(tr("Attach to window"));
    m_btnFloat->setIcon(QIcon::fromTheme(QStringLiteral("window-restore-pip")));
    setWindowFlags(Qt::Dialog);
  } else {
    m_btnFloat->setProperty("iconButton", true);
  m_btnFloat->setToolTip(tr("Detach from window"));
    m_btnFloat->setIcon(QIcon::fromTheme(QStringLiteral("window-minimize-pip")));
    setWindowFlags(Qt::Widget);
  }
  m_lblTitle->setVisible(!floating);
  m_btnClose->setVisible(!floating);
  show();
}

bool LogDock::eventFilter(QObject *watched, QEvent *event)
{
  // Filter out doubleclick on the titlebar, we only want the dock to float if the user uses the button
  if (watched == titleBarWidget() && event->type() == QEvent::MouseButtonDblClick)
    return true;
  return false;
}

void LogDock::changeEvent(QEvent *e)
{
  QDockWidget::changeEvent(e);
  if (e->type() != QEvent::LanguageChange)
    return;
  updateText();
}

void LogDock::updateText()
{
  setWindowTitle(tr("Log"));
  m_lblTitle->setText(tr("Log"));
  m_lblBadge->setText(tr("Running"));
  m_btnCopy->setToolTip(tr("Copy Log"));
  m_btnClear->setToolTip(tr("Clear Log"));
  m_btnCollapse->setToolTip(m_collapsed ? tr("Expand Log") : tr("Collapse Log"));
  m_searchWidget->setToolTip(tr("Search"));
  const auto labels = QStringList{tr("All"), tr("Info"), tr("Warn"), tr("Error")};
  const auto buttons = m_levelGroup->buttons();
  for (int i = 0; i < buttons.size() && i < labels.size(); ++i) {
    buttons.at(i)->setText(labels.at(i));
  }
}
