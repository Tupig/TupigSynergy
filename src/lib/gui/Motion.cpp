/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "Motion.h"

#include "platform/ReducedMotion.h"
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPointer>
#include <QTabWidget>
#include <QVariantAnimation>
#include <QWidget>
#include <utility>

namespace synergy::gui {
namespace {
MotionMode s_mode = MotionMode::System;

class MotionLayer;
QList<QPointer<MotionLayer>> &layers()
{
  static QList<QPointer<MotionLayer>> value;
  return value;
}

class MotionLayer final : public QWidget
{
public:
  MotionLayer(QWidget *target, bool content) : QWidget(target), m_animation(this), m_content(content)
  {
    setObjectName(content ? QStringLiteral("contentTransitionOverlay") : QStringLiteral("buttonFeedbackOverlay"));
    m_animation.setObjectName(
        content ? QStringLiteral("contentTransitionAnimation") : QStringLiteral("buttonFeedbackAnimation")
    );
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    setGeometry(target->rect());
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
      if (reducedMotion()) {
        finish();
        return;
      }
      m_opacity = value.toReal();
      setProperty("motionOpacity", m_opacity);
      update();
    });
    connect(&m_animation, &QVariantAnimation::finished, this, [this] { finish(); });
    auto &items = layers();
    for (int index = items.size() - 1; index >= 0; --index) {
      if (items.at(index).isNull())
        items.removeAt(index);
    }
    items.append(this);
  }

  void start()
  {
    m_opacity = m_content ? 0.045 : 0.22;
    setProperty("motionOpacity", m_opacity);
    m_animation.setStartValue(m_opacity);
    m_animation.setEndValue(0.0);
    m_animation.setDuration(m_content ? 160 : 110);
    m_animation.setEasingCurve(QEasingCurve::Linear);
    show();
    raise();
    m_animation.start();
  }

  void finish()
  {
    m_animation.stop();
    m_opacity = 0;
    setProperty("motionOpacity", 0.0);
    // Detach the discoverable presentation immediately, then destroy safely after dispatch.
    setObjectName(QString());
    hide();
    deleteLater();
  }

  bool isStarting() const
  {
    return m_animation.state() == QAbstractAnimation::Running && m_animation.currentTime() == 0;
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, qRound(255 * m_opacity)));
    if (m_content)
      painter.drawRect(rect());
    else
      painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 7, 7);
  }

private:
  QVariantAnimation m_animation;
  bool m_content;
  qreal m_opacity = 0;
};

void pulse(QWidget *target, bool content)
{
  if (!target || target->property("motionLayerCreating").toBool())
    return;
  const auto name = content ? QStringLiteral("contentTransitionOverlay") : QStringLiteral("buttonFeedbackOverlay");
  const bool reduced = reducedMotion();
  if (auto *old = dynamic_cast<MotionLayer *>(target->findChild<QWidget *>(name, Qt::FindDirectChildrenOnly))) {
    // A Qt input event and its clicked signal can request the same frame.
    if (!content && !reduced && old->isStarting())
      return;
    old->finish();
  }
  if (reduced)
    return;
  target->setProperty("motionLayerCreating", true);
  auto *layer = new MotionLayer(target, content);
  layer->start();
  target->setProperty("motionLayerCreating", false);
}

QWidget *optionSurface(QWidget *widget)
{
  for (auto *parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
    if (qobject_cast<QDialog *>(parent))
      return parent;
  }
  return widget;
}

class InteractionMotion final : public QObject
{
public:
  explicit InteractionMotion(QObject *parent) : QObject(parent)
  {
  }

  void watch(QWidget *widget)
  {
    if (widget->property("interactionMotionWatching").toBool())
      return;
    widget->setProperty("interactionMotionWatching", true);
    if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
      // Mouse and keyboard events already give immediate feedback. Native
      // accessible activation may defer clicked after pressed.
      connect(button, &QAbstractButton::clicked, button, [button] {
        if (button->isEnabled())
          pulse(button, false);
      });
    } else if (auto *tabs = qobject_cast<QTabWidget *>(widget)) {
      connect(tabs, &QTabWidget::currentChanged, tabs, [tabs] {
        if (auto *page = tabs->currentWidget())
          animateContentTransition(page);
      });
    } else if (auto *combo = qobject_cast<QComboBox *>(widget)) {
      connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), combo, [combo] {
        animateContentTransition(optionSurface(combo));
      });
    }
  }

protected:
  bool eventFilter(QObject *object, QEvent *event) override
  {
    auto *widget = qobject_cast<QWidget *>(object);
    if (!widget)
      return false;
    if (event->type() == QEvent::Polish || event->type() == QEvent::Show)
      watch(widget);
    if (event->type() == QEvent::Resize) {
      for (const auto &name : {QStringLiteral("buttonFeedbackOverlay"), QStringLiteral("contentTransitionOverlay")}) {
        if (auto *layer = widget->findChild<QWidget *>(name, Qt::FindDirectChildrenOnly))
          layer->setGeometry(widget->rect());
      }
    }
    if (auto *button = qobject_cast<QAbstractButton *>(widget); button && button->isEnabled()) {
      const auto type = event->type();
      if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease) {
        watch(button);
        pulse(button, false);
      } else if (type == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter) {
          watch(button);
          pulse(button, false);
        }
      }
    }
    // Observational only: never consume input, call click(), or change focus/model state.
    return false;
  }
};
} // namespace

bool reducedMotion()
{
  if (s_mode != MotionMode::System)
    return s_mode == MotionMode::Reduced;
  const auto native = synergy::platform::readMotionPreference();
  if (native != synergy::platform::ReduceMotionPreference::Unknown)
    return native == synergy::platform::ReduceMotionPreference::Reduced;
  // Qt's flag is an application effect hint, not a portable native accessibility preference.
  return qApp && !QApplication::isEffectEnabled(Qt::UI_General);
}

void setMotionModeForTesting(MotionMode mode)
{
  s_mode = mode;
  for (const auto &layer : std::as_const(layers())) {
    if (layer)
      layer->finish();
  }
  auto &items = layers();
  for (int index = items.size() - 1; index >= 0; --index) {
    if (items.at(index).isNull())
      items.removeAt(index);
  }
}

void animateContentTransition(QWidget *target)
{
  pulse(target, true);
}

void installInteractionMotion()
{
  if (!qApp || qApp->findChild<QObject *>(QStringLiteral("buttonFeedbackController")))
    return;
  auto *controller = new InteractionMotion(qApp);
  controller->setObjectName(QStringLiteral("buttonFeedbackController"));
  qApp->installEventFilter(controller);
  for (auto *widget : QApplication::allWidgets())
    controller->watch(widget);
}
} // namespace synergy::gui
