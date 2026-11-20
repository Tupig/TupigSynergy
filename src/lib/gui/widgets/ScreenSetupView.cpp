/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSetupView.h"

#include "PageHost.h"
#include "ScreenSetupModel.h"
#include "common/Settings.h"
#include "dialogs/ScreenSettingsDialog.h"
#include "gui/StyleUtils.h"
#include <functional>
#include <memory>
#include <utility>

#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QFontMetricsF>
#include <QHeaderView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTextOption>

namespace {

struct NameLayout
{
  int lines = 0;
  qreal height = 0;
  bool complete = true;
};

NameLayout layoutName(QTextLayout &text, const QRectF &rect)
{
  NameLayout result;
  text.beginLayout();
  for (auto line = text.createLine(); line.isValid(); line = text.createLine()) {
    line.setLineWidth(rect.width());
    if (result.height + line.height() > rect.height()) {
      result.complete = false;
      break;
    }
    line.setPosition(QPointF(0, result.height));
    ++result.lines;
    result.height += line.height();
  }
  text.endLayout();
  return result;
}

void drawName(QPainter &painter, const QRectF &rect, const QString &name, const QFont &font)
{
  if (rect.isEmpty())
    return;

  QTextLayout text(name, font, painter.device());
  QTextOption option;
  option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  option.setAlignment(Qt::AlignHCenter);
  text.setTextOption(option);

  auto fitted = layoutName(text, rect);
  if (!fitted.complete) {
    QFont compactFont = font;
    for (int pixelSize : {10, 9}) {
      compactFont.setPixelSize(pixelSize);
      if (pixelSize == 9)
        compactFont.setStretch(93);
      text.setFont(compactFont);
      fitted = layoutName(text, rect);
      if (fitted.complete)
        break;
    }
    if (!fitted.complete) {
      // Keep the regular reading size for names too long to display in full.
      text.setFont(font);
      fitted = layoutName(text, rect);
    }
  }

  if (fitted.lines == 0)
    return;
  const QPointF origin(rect.left(), rect.top() + (rect.height() - fitted.height) / 2);
  painter.setClipRect(rect, Qt::IntersectClip);
  if (fitted.complete) {
    text.draw(&painter, origin);
    return;
  }

  for (int index = 0; index < fitted.lines - 1; ++index)
    text.lineAt(index).draw(&painter, origin);

  const auto last = text.lineAt(fitted.lines - 1);
  const QFontMetricsF metrics(font, painter.device());
  const auto shortened = metrics.elidedText(name.mid(last.textStart()), Qt::ElideRight, rect.width());
  painter.setFont(font);
  painter.drawText(
      QRectF(origin + last.position(), QSizeF(rect.width(), last.height())), Qt::AlignHCenter | Qt::AlignVCenter,
      shortened
  );
}

// Presentation only: the table retains the model's indexes, flags and drag/drop hit areas.
class ScreenTileDelegate final : public QStyledItemDelegate
{
public:
  ScreenTileDelegate(QObject *parent, std::function<bool(const QModelIndex &)> isLocal)
      : QStyledItemDelegate(parent),
        m_isLocal(std::move(isLocal))
  {
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
  {
    const bool dark = synergy::gui::isDarkMode();
    const auto name = index.data(Qt::DisplayRole).toString();
    const bool filled = !name.isEmpty();
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool local = filled && m_isLocal(index);
    const QColor accent = local ? QColor(dark ? "#7aa2f7" : "#3c65b5") : QColor(dark ? "#4cc38a" : "#2f9e6b");
    const QRectF tile = QRectF(option.rect).adjusted(3, 3, -3, -3);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipRect(option.rect);
    const qreal radius = dark ? 9 : 8;
    QColor background(dark ? "#232428" : "#ececf0");
    QColor border = accent;
    if (filled) {
      background = accent;
      if (dark) {
        background.setAlphaF(local ? 0.14 : 0.16);
        border.setAlphaF(0.5);
      } else {
        background.setAlpha(24);
      }
    }
    painter->setBrush(background);
    painter->setPen(filled && !dark ? QPen(border, 1) : QPen(Qt::NoPen));
    painter->drawRoundedRect(tile, radius, radius);
    if (filled && dark) {
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(border, 1));
      painter->drawRoundedRect(tile.adjusted(0.5, 0.5, -0.5, -0.5), radius - 0.5, radius - 0.5);
    }
    if (filled) {
      QFont textFont = option.font;
      textFont.setPixelSize(11);
      textFont.setWeight(selected || local ? QFont::DemiBold : QFont::Normal);
      painter->setPen(dark ? QColor(local ? "#cfdfff" : "#d9f3e6") : accent);
      QFont roleFont = option.font;
      roleFont.setPixelSize(10);
      const QRectF roleRect(
          tile.left() + 2, tile.top() + 2, tile.width() - 4, QFontMetricsF(roleFont, painter->device()).height()
      );
      painter->setFont(roleFont);
      painter->drawText(
          roleRect, Qt::AlignTop | Qt::AlignHCenter, local ? QObject::tr("Local") : QObject::tr("Client")
      );
      const QRectF textRect(
          roleRect.left(), roleRect.bottom() + 1, roleRect.width(), tile.bottom() - roleRect.bottom() - 3
      );
      drawName(*painter, textRect, name, textFont);
    } else {
      painter->setPen(QPen(QColor(dark ? "#9a9aa0" : "#777782"), 1.8, Qt::SolidLine, Qt::RoundCap));
      const auto center = tile.center();
      painter->drawLine(center + QPointF(-4, 0), center + QPointF(4, 0));
      painter->drawLine(center + QPointF(0, -4), center + QPointF(0, 4));
    }
    if (option.state.testFlag(QStyle::State_HasFocus)) {
      painter->setClipping(false);
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(accent, 1, Qt::DashLine));
      painter->drawRoundedRect(tile.adjusted(2, 2, -2, -2), radius - 2, radius - 2);
    }
    painter->restore();
  }

private:
  std::function<bool(const QModelIndex &)> m_isLocal;
};

} // namespace

ScreenSetupView::ScreenSetupView(QWidget *parent) : QTableView(parent)
{
  setDropIndicatorShown(true);
  setDragDropMode(DragDrop);
  setSelectionMode(SingleSelection);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  setItemDelegate(new ScreenTileDelegate(this, [this](const QModelIndex &index) {
    return model() && index.isValid() && model()->screen(index).isPrimary();
  }));
  setShowGrid(false);
  setFrameShape(QFrame::NoFrame);
  setStyleSheet(QStringLiteral(
      "QTableView { background: transparent; border: 0; selection-background-color: transparent; }"
      "QTableView::item, QTableView::item:selected, QTableView::item:hover { background: transparent; border: 0; }"
  ));
  setIconSize(QSize(16, 16));
  horizontalHeader()->hide();
  verticalHeader()->hide();
}

void ScreenSetupView::setModel(QAbstractItemModel *model)
{
  QTableView::setModel(model);
  setTableSize();
}

ScreenSetupModel *ScreenSetupView::model() const
{
  return qobject_cast<ScreenSetupModel *>(QTableView::model());
}

void ScreenSetupView::showScreenConfig(int col, int row)
{
  auto screen = std::make_shared<synergy::gui::Screen>(model()->screen(col, row));
  auto *dialog = new ScreenSettingsDialog(this, screen.get(), &model()->m_Screens);
  connect(dialog, &QDialog::accepted, this, [this, screen, col, row] {
    model()->screen(col, row) = *screen;
    Q_EMIT model()->screensChanged();
  });
  PageHost::present(this, dialog);
}

void ScreenSetupView::setTableSize()
{
  if (!model() || model()->columnCount() <= 0 || model()->rowCount() <= 0)
    return;
  const int columns = model()->columnCount();
  const int rows = model()->rowCount();
  for (int i = 0; i < columns; i++)
    setColumnWidth(i, (i + 1) * viewport()->width() / columns - i * viewport()->width() / columns);
  for (int i = 0; i < rows; i++)
    setRowHeight(i, (i + 1) * viewport()->height() / rows - i * viewport()->height() / rows);
}

void ScreenSetupView::resizeEvent(QResizeEvent *event)
{
  QTableView::resizeEvent(event);
  setTableSize();
}

void ScreenSetupView::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (event->buttons() & Qt::LeftButton) {
    int col = columnAt(event->pos().x());
    int row = rowAt(event->pos().y());

    if (!model()->screen(col, row).isNull()) {
      showScreenConfig(col, row);
    }
  } else
    event->ignore();
}

void ScreenSetupView::dragEnterEvent(QDragEnterEvent *event)
{
  // we accept anything that enters us by a drag as long as the
  // mime type is okay. anything else is dealt with in dragMoveEvent()
  if (event->mimeData()->hasFormat(ScreenSetupModel::mimeType()))
    event->accept();
  else
    event->ignore();
}

void ScreenSetupView::dragMoveEvent(QDragMoveEvent *event)
{
  if (event->mimeData()->hasFormat(ScreenSetupModel::mimeType())) {
    // where does the event come from? myself or someone else?
    if (event->source() == this) {
      // myself is ok, but then it must be a move action, never a copy
      event->setDropAction(Qt::MoveAction);
      event->accept();
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      const auto &point = event->position().toPoint();
#else
      const auto &point = event->pos();
#endif
      int col = columnAt(point.x());
      int row = rowAt(point.y());

      // a drop from outside is not allowed if there's a screen already there.
      if (!model()->screen(col, row).isNull())
        event->ignore();
      else {
        event->acceptProposedAction();
      }
    }
  } else
    event->ignore();
}

// this is reimplemented from QAbstractItemView::startDrag()
void ScreenSetupView::startDrag(Qt::DropActions)
{
  QModelIndexList indexes = selectedIndexes();

  if (indexes.count() != 1)
    return;

  QMimeData *pData = model()->mimeData(indexes);
  if (pData == nullptr)
    return;

  const QPixmap pixmap = viewport()->grab(visualRect(indexes[0]));
  auto *pDrag = new QDrag(this);
  pDrag->setPixmap(pixmap);
  pDrag->setMimeData(pData);
  const auto dragSize = QSizeF(pixmap.size()) / pixmap.devicePixelRatio();
  pDrag->setHotSpot(QPoint(qRound(dragSize.width() / 2), qRound(dragSize.height() / 2)));

  if (pDrag->exec(Qt::MoveAction, Qt::MoveAction) == Qt::MoveAction) {
    selectionModel()->clear();

    // make sure to only delete the drag source if screens weren't swapped
    // see ScreenSetupModel::dropMimeData
    if (!model()->screen(indexes[0]).swapped())
      model()->screen(indexes[0]) = synergy::gui::Screen();
    else
      model()->screen(indexes[0]).setSwapped(false);

    Q_EMIT model()->screensChanged();
  }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ScreenSetupView::initViewItemOption(QStyleOptionViewItem *option) const
{
  QTableView::initViewItemOption(option);
  option->decorationSize = QSize(16, 16);
  option->textElideMode = Qt::ElideNone;
}
#else
QStyleOptionViewItem ScreenSetupView::viewOptions() const
{
  QStyleOptionViewItem option = QTableView::viewOptions();
  option.decorationSize = QSize(16, 16);
  option.showDecorationSelected = true;
  option.decorationPosition = QStyleOptionViewItem::Top;
  option.decorationAlignment = Qt::AlignHCenter | Qt::AlignVCenter;
  option.displayAlignment = Qt::AlignTop | Qt::AlignHCenter;
  option.textElideMode = Qt::ElideNone;
  return option;
}
#endif
