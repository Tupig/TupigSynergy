/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-FileCopyrightText: (C) 2026 Lucide Icons and Contributors
 * SPDX-FileCopyrightText: (C) 2013-present Cole Bemis
 * SPDX-License-Identifier: ISC AND MIT
 *
 * Offline painter adaptations of the supplied Lucide/Feather artwork.
 * Full notices: LICENSES/Lucide-Feather.txt (also embedded in the GUI).
 */

#include "StyleUtils.h"

#include <initializer_list>

#include <QApplication>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace synergy::gui {
namespace {

enum class Symbol
{
  Sliders,
  Pencil,
  Refresh,
  Copy,
  Trash,
  Down,
  Up,
  Left,
  Square,
  Stop,
  Play,
  Alert,
  Check,
  Close,
  Circle,
  Search,
  Terminal,
  Monitor,
  Power,
  Download,
  Help,
  Lock,
  Unlock,
  Key,
  Fingerprint,
  File,
  Grid,
  Info,
  Plus,
  Minus,
  Save
};

const QHash<QString, Symbol> &symbols()
{
  static const QHash<QString, Symbol> result = {
      {QStringLiteral("sliders"), Symbol::Sliders},
      {QStringLiteral("configure"), Symbol::Sliders},
      {QStringLiteral("pencil"), Symbol::Pencil},
      {QStringLiteral("document-edit"), Symbol::Pencil},
      {QStringLiteral("refresh"), Symbol::Refresh},
      {QStringLiteral("view-refresh"), Symbol::Refresh},
      {QStringLiteral("copy"), Symbol::Copy},
      {QStringLiteral("edit-copy"), Symbol::Copy},
      {QStringLiteral("trash"), Symbol::Trash},
      {QStringLiteral("edit-clear"), Symbol::Trash},
      {QStringLiteral("edit-clear-all"), Symbol::Trash},
      {QStringLiteral("user-trash"), Symbol::Trash},
      {QStringLiteral("down"), Symbol::Down},
      {QStringLiteral("go-down"), Symbol::Down},
      {QStringLiteral("up"), Symbol::Up},
      {QStringLiteral("go-up"), Symbol::Up},
      {QStringLiteral("left"), Symbol::Left},
      {QStringLiteral("go-previous"), Symbol::Left},
      {QStringLiteral("square"), Symbol::Square},
      {QStringLiteral("window-restore-pip"), Symbol::Square},
      {QStringLiteral("stop"), Symbol::Stop},
      {QStringLiteral("process-stop"), Symbol::Stop},
      {QStringLiteral("network-disconnect"), Symbol::Stop},
      {QStringLiteral("play"), Symbol::Play},
      {QStringLiteral("system-run"), Symbol::Play},
      {QStringLiteral("network-connect"), Symbol::Play},
      {QStringLiteral("alert"), Symbol::Alert},
      {QStringLiteral("dialog-warning"), Symbol::Alert},
      {QStringLiteral("dialog-error"), Symbol::Alert},
      {QStringLiteral("tools-report-bug"), Symbol::Alert},
      {QStringLiteral("check"), Symbol::Check},
      {QStringLiteral("dialog-ok"), Symbol::Check},
      {QStringLiteral("dialog-ok-apply"), Symbol::Check},
      {QStringLiteral("dialog-positive"), Symbol::Check},
      {QStringLiteral("x"), Symbol::Close},
      {QStringLiteral("window-close"), Symbol::Close},
      {QStringLiteral("view-close"), Symbol::Close},
      {QStringLiteral("dialog-cancel"), Symbol::Close},
      {QStringLiteral("circle"), Symbol::Circle},
      {QStringLiteral("search"), Symbol::Search},
      {QStringLiteral("system-search"), Symbol::Search},
      {QStringLiteral("terminal"), Symbol::Terminal},
      {QStringLiteral("monitor"), Symbol::Monitor},
      {QStringLiteral("video-display"), Symbol::Monitor},
      {QStringLiteral("power"), Symbol::Power},
      {QStringLiteral("application-exit"), Symbol::Power},
      {QStringLiteral("download"), Symbol::Download},
      {QStringLiteral("system-software-update"), Symbol::Download},
      {QStringLiteral("help"), Symbol::Help},
      {QStringLiteral("help-about"), Symbol::Help},
      {QStringLiteral("dialog-question"), Symbol::Help},
      {QStringLiteral("lock"), Symbol::Lock},
      {QStringLiteral("security-high"), Symbol::Lock},
      {QStringLiteral("security-medium"), Symbol::Lock},
      {QStringLiteral("security-low"), Symbol::Unlock},
      {QStringLiteral("key"), Symbol::Key},
      {QStringLiteral("fingerprint"), Symbol::Fingerprint},
      {QStringLiteral("file"), Symbol::File},
      {QStringLiteral("document-open"), Symbol::File},
      {QStringLiteral("grid"), Symbol::Grid},
      {QStringLiteral("info"), Symbol::Info},
      {QStringLiteral("dialog-information"), Symbol::Info},
      {QStringLiteral("plus"), Symbol::Plus},
      {QStringLiteral("list-add"), Symbol::Plus},
      {QStringLiteral("minus"), Symbol::Minus},
      {QStringLiteral("list-remove"), Symbol::Minus},
      {QStringLiteral("window-minimize-pip"), Symbol::Minus},
      {QStringLiteral("save"), Symbol::Save},
      {QStringLiteral("document-save"), Symbol::Save},
      {QStringLiteral("document-save-as"), Symbol::Save},
  };
  return result;
}

class LineIconEngine final : public QIconEngine
{
public:
  explicit LineIconEngine(Symbol symbol) : m_symbol(symbol)
  {
  }
  QIconEngine *clone() const override
  {
    return new LineIconEngine(m_symbol);
  }
  QString key() const override
  {
    return QStringLiteral("synergy-offline-line");
  }

  QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
  {
    QPixmap result(size);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    paint(&painter, result.rect(), mode, state);
    return result;
  }

  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
  {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const qreal side = qMin(rect.width(), rect.height());
    painter->translate(rect.center().x() + 0.5 - side / 2, rect.center().y() + 0.5 - side / 2);
    painter->scale(side / 24, side / 24);
    const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
    const QColor color = QApplication::palette().color(group, QPalette::WindowText);
    painter->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    const auto line = [painter](qreal x1, qreal y1, qreal x2, qreal y2) {
      painter->drawLine(QPointF(x1, y1), QPointF(x2, y2));
    };
    const auto poly = [painter](std::initializer_list<QPointF> points) {
      painter->drawPolyline(QPolygonF(QList<QPointF>(points)));
    };
    QPainterPath path;
    switch (m_symbol) {
    case Symbol::Sliders:
      line(3, 4, 10, 4);
      line(14, 4, 21, 4);
      line(14, 2, 14, 6);
      line(3, 12, 8, 12);
      line(12, 12, 21, 12);
      line(8, 10, 8, 14);
      line(3, 20, 12, 20);
      line(16, 20, 21, 20);
      line(16, 18, 16, 22);
      break;
    case Symbol::Pencil:
      poly({{15, 5}, {19, 9}, {7, 21}, {3, 22}, {4, 18}, {18, 4}, {21, 7}, {19, 9}});
      break;
    case Symbol::Refresh:
      path.moveTo(3, 12);
      path.cubicTo(3, 3, 14, 0, 21, 8);
      path.moveTo(21, 12);
      path.cubicTo(21, 21, 10, 24, 3, 16);
      painter->drawPath(path);
      poly({{21, 3}, {21, 8}, {16, 8}});
      poly({{8, 16}, {3, 16}, {3, 21}});
      break;
    case Symbol::Copy:
      painter->drawRoundedRect(QRectF(8, 8, 14, 14), 2, 2);
      path.moveTo(4, 16);
      path.quadTo(2, 16, 2, 14);
      path.lineTo(2, 4);
      path.quadTo(2, 2, 4, 2);
      path.lineTo(14, 2);
      path.quadTo(16, 2, 16, 4);
      painter->drawPath(path);
      break;
    case Symbol::Trash:
      line(3, 6, 21, 6);
      poly({{5, 6}, {5, 20}, {7, 22}, {17, 22}, {19, 20}, {19, 6}});
      poly({{8, 6}, {8, 3}, {16, 3}, {16, 6}});
      line(10, 11, 10, 17);
      line(14, 11, 14, 17);
      break;
    case Symbol::Down:
      poly({{6, 9}, {12, 15}, {18, 9}});
      break;
    case Symbol::Up:
      poly({{6, 15}, {12, 9}, {18, 15}});
      break;
    case Symbol::Left:
      poly({{15, 6}, {9, 12}, {15, 18}});
      break;
    case Symbol::Square:
      painter->drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
      break;
    case Symbol::Stop:
      painter->setPen(Qt::NoPen);
      painter->setBrush(color);
      painter->drawRoundedRect(QRectF(6, 6, 12, 12), 2.5, 2.5);
      break;
    case Symbol::Play:
      poly({{6, 3}, {20, 12}, {6, 21}, {6, 3}});
      break;
    case Symbol::Alert:
      poly({{12, 3}, {22, 21}, {2, 21}, {12, 3}});
      line(12, 9, 12, 13);
      line(12, 17, 12.01, 17);
      break;
    case Symbol::Check:
      poly({{4, 12}, {9, 17}, {20, 6}});
      break;
    case Symbol::Close:
      line(6, 6, 18, 18);
      line(6, 18, 18, 6);
      break;
    case Symbol::Circle:
      painter->drawEllipse(QRectF(3, 3, 18, 18));
      break;
    case Symbol::Search:
      painter->drawEllipse(QRectF(4, 4, 14, 14));
      line(16, 16, 20, 20);
      break;
    case Symbol::Terminal:
      poly({{4, 5}, {10, 11}, {4, 17}});
      line(12, 19, 20, 19);
      break;
    case Symbol::Monitor:
      painter->drawRoundedRect(QRectF(2, 3, 20, 14), 2, 2);
      line(8, 21, 16, 21);
      line(12, 17, 12, 21);
      break;
    case Symbol::Power:
      line(12, 2, 12, 12);
      painter->drawArc(QRectF(3, 3, 18, 18), 135 * 16, 270 * 16);
      break;
    case Symbol::Download:
      poly({{3, 15}, {3, 21}, {21, 21}, {21, 15}});
      poly({{7, 10}, {12, 15}, {17, 10}});
      line(12, 3, 12, 15);
      break;
    case Symbol::Help:
      painter->drawEllipse(QRectF(3, 3, 18, 18));
      path.moveTo(9.1, 9);
      path.cubicTo(10, 5.5, 16, 6, 15, 10);
      path.cubicTo(14.6, 12, 12, 11.8, 12, 14);
      painter->drawPath(path);
      line(12, 17, 12.01, 17);
      break;
    case Symbol::Lock:
    case Symbol::Unlock:
      painter->drawRoundedRect(QRectF(4, 10, 16, 11), 2, 2);
      path.moveTo(8, 10);
      path.lineTo(8, 7);
      path.cubicTo(8, 1.7, 16, 1.7, 16, 7);
      if (m_symbol == Symbol::Lock)
        path.lineTo(16, 10);
      painter->drawPath(path);
      break;
    case Symbol::Key:
      painter->drawEllipse(QRectF(3, 11, 9, 9));
      line(11, 12, 20, 3);
      line(17, 6, 20, 9);
      break;
    case Symbol::Fingerprint:
      path.moveTo(12, 10);
      path.cubicTo(9, 9, 10, 14, 9, 17);
      path.moveTo(14, 10);
      path.cubicTo(17, 10, 16, 15, 15, 18);
      path.moveTo(10, 6);
      path.cubicTo(17, 4, 18, 13, 16, 19);
      path.moveTo(8, 8);
      path.cubicTo(5, 10, 7, 14, 7, 16);
      path.moveTo(6, 5);
      path.cubicTo(2, 8, 3, 14, 5, 17);
      painter->drawPath(path);
      break;
    case Symbol::File:
      poly({{14, 3}, {19, 8}, {19, 21}, {5, 21}, {5, 3}, {14, 3}, {14, 8}, {19, 8}});
      break;
    case Symbol::Grid:
      for (int x : {3, 14})
        for (int y : {3, 14})
          painter->drawRoundedRect(QRectF(x, y, 7, 7), 1, 1);
      break;
    case Symbol::Info:
      painter->drawEllipse(QRectF(3, 3, 18, 18));
      line(12, 11, 12, 16);
      line(12, 8, 12.01, 8);
      break;
    case Symbol::Plus:
      line(12, 5, 12, 19);
      line(5, 12, 19, 12);
      break;
    case Symbol::Minus:
      line(5, 12, 19, 12);
      break;
    case Symbol::Save:
      poly({{4, 3}, {17, 3}, {21, 7}, {21, 21}, {3, 21}, {3, 3}, {4, 3}});
      poly({{7, 3}, {7, 9}, {16, 9}, {16, 3}});
      poly({{7, 21}, {7, 14}, {17, 14}, {17, 21}});
      break;
    }
    painter->restore();
  }

private:
  Symbol m_symbol;
};
} // namespace

QIcon appIcon(const QString &name)
{
  const auto found = symbols().constFind(name);
  return found == symbols().cend() ? QIcon::fromTheme(name) : QIcon(new LineIconEngine(found.value()));
}
} // namespace synergy::gui
