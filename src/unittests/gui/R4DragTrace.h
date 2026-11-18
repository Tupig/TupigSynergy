/* SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception */
#pragma once

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QDrag>
#include <QDragEnterEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QSet>
#include <QTimer>

namespace synergy::testing {
// Passive and test-only. The application filter receives app-local events; it
// never synthesizes/changes/consumes them or reads the MIME payload.
class R4DragTrace final : public QObject
{
public:
  R4DragTrace(QWidget *window, const QString &directory)
      : QObject(window),
        m_window(window),
        m_path(directory + "/drag-events.jsonl")
  {
    m_time.start();
    m_ready = record(
        {{"event", "trace_start"},
         {"pid", qApp->applicationPid()},
         {"executable", QCoreApplication::applicationFilePath()},
         {"scope", "owned MainWindow tools/grid only; passive pre-dispatch events; no global input"}}
    );
    if (!m_ready)
      return;
    qApp->installEventFilter(this);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { record({{"event", "trace_quit"}}); });
  }
  bool isReady() const
  {
    return m_ready;
  }

protected:
  bool eventFilter(QObject *receiver, QEvent *event) override
  {
    auto *widget = qobject_cast<QWidget *>(receiver);
    if (!m_window || !widget || !m_window->isAncestorOf(widget) || !relevant(widget))
      return false;
    const auto type = event->type();
    if (type != QEvent::MouseButtonPress && type != QEvent::MouseMove && type != QEvent::MouseButtonRelease &&
        type != QEvent::DragEnter && type != QEvent::DragMove && type != QEvent::DragLeave && type != QEvent::Drop)
      return false;
    QJsonObject row{{"event", "widget_event"},        {"type", int(type)},
                    {"phase", "pre-dispatch"},        {"receiver", describe(receiver)},
                    {"enabled", widget->isEnabled()}, {"accepted_before", event->isAccepted()}};
    if (type == QEvent::MouseButtonPress || type == QEvent::MouseMove || type == QEvent::MouseButtonRelease) {
      auto *mouse = static_cast<QMouseEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      const QPointF point = mouse->position();
#else
      const QPointF point = mouse->localPos();
#endif
      row["position"] = QJsonArray{point.x(), point.y()};
      row["button"] = int(mouse->button());
      row["buttons"] = int(mouse->buttons());
      row["modifiers"] = int(mouse->modifiers());
    } else if (type != QEvent::DragLeave) {
      auto *drop = static_cast<QDropEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      const QPointF point = drop->position();
#else
      const QPointF point = drop->posF();
#endif
      row["position"] = QJsonArray{point.x(), point.y()};
      row["drop_action"] = int(drop->dropAction());
      row["proposed_action"] = int(drop->proposedAction());
      row["mime_types"] = QJsonArray::fromStringList(drop->mimeData()->formats());
    }
    record(row);
    discover();
    // The event pointer is intentionally not captured; the native dispatch may
    // outlive this stack frame or run a nested QDrag event loop.
    QTimer::singleShot(0, this, [this] { discover(); });
    return false;
  }

private:
  QPointer<QWidget> m_window;
  QString m_path;
  QElapsedTimer m_time;
  QSet<QObject *> m_models, m_drags;
  bool m_ready = false;
  static QJsonObject describe(QObject *object)
  {
    return object
               ? QJsonObject{{"name", object->objectName()}, {"class", object->metaObject()->className()}, {"identity", QString::number(quintptr(object), 16)}}
               : QJsonObject{};
  }
  bool record(QJsonObject row)
  {
    row["utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    row["elapsed_ms"] = m_time.elapsed();
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
      return false;
    const QByteArray line = QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
    return file.write(line) == line.size();
  }
  bool relevant(QWidget *widget) const
  {
    for (auto *at = widget; at && at != m_window; at = at->parentWidget())
      if (at->objectName() == "addComputerButton" || at->objectName() == "deleteSelectedComputerButton" ||
          at->objectName() == "screenSetupView" || at->objectName() == "lblNewScreen" ||
          at->objectName() == "lblRemoveScreen")
        return true;
    return false;
  }
  void discover()
  {
    if (!m_window)
      return;
    for (auto *view : m_window->findChildren<QAbstractItemView *>("screenSetupView")) {
      auto *model = view->model();
      if (!model || m_models.contains(model))
        continue;
      m_models.insert(model);
      record(
          {{"event", "model_observed"},
           {"model", describe(model)},
           {"rows", model->rowCount()},
           {"columns", model->columnCount()}}
      );
      connect(model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &first, const QModelIndex &last) {
        record(
            {{"event", "model_data_changed"},
             {"first", QJsonArray{first.row(), first.column()}},
             {"last", QJsonArray{last.row(), last.column()}}}
        );
      });
      connect(
          model, &QAbstractItemModel::rowsMoved, this,
          [this](const QModelIndex &, int first, int last, const QModelIndex &, int destination) {
            record({{"event", "model_rows_moved"}, {"first", first}, {"last", last}, {"destination", destination}});
          }
      );
      connect(model, &QAbstractItemModel::modelReset, this, [this] { record({{"event", "model_reset"}}); });
      connect(model, &QAbstractItemModel::layoutChanged, this, [this] { record({{"event", "model_layout_changed"}}); });
      connect(model, &QObject::destroyed, this, [this, model] { m_models.remove(model); });
    }
    for (auto *drag : m_window->findChildren<QDrag *>()) {
      if (m_drags.contains(drag))
        continue;
      m_drags.insert(drag);
      record(
          {{"event", "qdrag_object_observed"},
           {"drag", describe(drag)},
           {"source", describe(drag->source())},
           {"mime_types", drag->mimeData() ? QJsonArray::fromStringList(drag->mimeData()->formats()) : QJsonArray{}}}
      );
      connect(drag, &QDrag::actionChanged, this, [this](Qt::DropAction action) {
        record({{"event", "qdrag_action_changed"}, {"action", int(action)}});
      });
      connect(drag, &QDrag::targetChanged, this, [this](QObject *target) {
        record({{"event", "qdrag_target_changed"}, {"target", describe(target)}});
      });
      connect(drag, &QObject::destroyed, this, [this, drag] {
        record({{"event", "qdrag_object_destroyed"}});
        m_drags.remove(drag);
      });
    }
  }
};
} // namespace synergy::testing
