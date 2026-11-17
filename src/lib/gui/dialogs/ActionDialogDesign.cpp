/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "ActionDialogDesign.h"
#include "CompactLayout.h"
#include "gui/StyleUtils.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {

class ScreenCheckDelegate final : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
  {
    QStyleOptionViewItem row(option);
    initStyleOption(&row, index);
    row.features &= ~QStyleOptionViewItem::HasCheckIndicator;
    auto *style = row.widget ? row.widget->style() : QApplication::style();
    auto background = row;
    background.text.clear();
    background.icon = QIcon();
    background.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
    style->drawControl(QStyle::CE_ItemViewItem, &background, painter, row.widget);
    row.rect = QStyle::visualRect(row.direction, option.rect, option.rect.adjusted(26, 0, 0, 0));
    row.state &= ~QStyle::State_HasFocus;
    style->drawControl(QStyle::CE_ItemViewItem, &row, painter, row.widget);

    const bool enabled = option.state.testFlag(QStyle::State_Enabled) && index.flags().testFlag(Qt::ItemIsEnabled);
    const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    const bool dark = synergy::gui::isDarkMode();
    const QColor outline =
        dark ? QColor(enabled ? "#3f4046" : "#9a9aa0") : option.palette.color(enabled ? QPalette::Text : QPalette::Mid);
    const QColor fill = checked ? (dark ? QColor(enabled ? "#4cc38a" : "#9a9aa0")
                                        : option.palette.color(enabled ? QPalette::Highlight : QPalette::Mid))
                                : (dark ? QColor("#1b1c20") : option.palette.color(QPalette::Base));
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(checked ? fill : outline, 1));
    painter->setBrush(fill);
    painter->drawRoundedRect(QRectF(indicatorRect(option)).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    if (checked) {
      const auto rect = indicatorRect(option);
      painter->setPen(QPen(
          dark ? QColor("#08120d") : option.palette.color(QPalette::HighlightedText), 2, Qt::SolidLine, Qt::RoundCap,
          Qt::RoundJoin
      ));
      painter->drawPolyline(
          QPolygonF{
              QPointF(rect.left() + 4, rect.top() + 8), QPointF(rect.left() + 7, rect.top() + 11),
              QPointF(rect.left() + 12, rect.top() + 5)
          }
      );
    }
    painter->restore();
  }

  bool editorEvent(
      QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index
  ) override
  {
    if (!option.state.testFlag(QStyle::State_Enabled) || !index.flags().testFlag(Qt::ItemIsEnabled) ||
        !index.flags().testFlag(Qt::ItemIsUserCheckable) || !index.data(Qt::CheckStateRole).isValid())
      return false;
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick) {
      const auto *mouse = static_cast<QMouseEvent *>(event);
      if (mouse->button() != Qt::LeftButton || !indicatorRect(option).contains(mouse->pos()))
        return false;
      if (event->type() != QEvent::MouseButtonRelease)
        return true;
    } else if (event->type() == QEvent::KeyPress) {
      const auto *key = static_cast<QKeyEvent *>(event);
      if (key->key() != Qt::Key_Space && key->key() != Qt::Key_Select)
        return false;
      if (key->isAutoRepeat())
        return true;
    } else {
      return false;
    }
    return model->setData(
        index, index.data(Qt::CheckStateRole).toInt() == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole
    );
  }

private:
  static QRect indicatorRect(const QStyleOptionViewItem &option)
  {
    const QRect logical(option.rect.left() + 10, option.rect.top() + (option.rect.height() - 16) / 2, 16, 16);
    return QStyle::visualRect(option.direction, option.rect, logical);
  }
};

} // namespace

void Ui::ActionDialog::setupUi(QDialog *dialog)
{
  dialog->setObjectName(QStringLiteral("ActionDialog"));
  auto *mainLayout = new QVBoxLayout(dialog);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(8);
  scrollArea = new QScrollArea(dialog);
  scrollArea->setObjectName(QStringLiteral("scrollArea"));
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollAreaWidgetContents = new QWidget;
  scrollAreaWidgetContents->setObjectName(QStringLiteral("scrollAreaWidgetContents"));
  auto *page = new QVBoxLayout(scrollAreaWidgetContents);
  page->setContentsMargins(0, 0, 0, 0);
  page->setSpacing(8);
  actionCard = new QGroupBox(scrollAreaWidgetContents);
  actionCard->setObjectName(QStringLiteral("actionCard"));
  actionCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *actionLayout = new QVBoxLayout(actionCard);
  actionLayout->setContentsMargins(12, 10, 12, 12);
  actionLayout->setSpacing(12);
  auto *form = new QFormLayout;
  form->setContentsMargins(0, 0, 0, 0);
  form->setHorizontalSpacing(10);
  form->setVerticalSpacing(8);
  form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  const auto combo = [card = actionCard](const char *name, int count) {
    auto *field = new QComboBox(card);
    field->setView(new QListView(field));
    field->setObjectName(QString::fromLatin1(name));
    field->setFixedWidth(180);
    for (int i = 0; i < count; ++i)
      field->addItem(QString());
    return field;
  };
  const auto label = [card = actionCard](const char *name) {
    auto *result = new QLabel(card);
    result->setObjectName(QString::fromLatin1(name));
    result->setFixedWidth(120);
    result->setWordWrap(true);
    return result;
  };
  comboTriggerOn = combo("comboTriggerOn", 2);
  comboTriggerOn->setItemData(0, false);
  comboTriggerOn->setItemData(1, true);
  labelTrigger = label("labelTrigger");
  form->addRow(labelTrigger, comboTriggerOn);
  comboActionType = combo("comboActionType", 8);
  const int actionIds[]{3, 4, 2, 0, 1, 5, 6, 7};
  for (int row = 0; row < 8; ++row)
    comboActionType->setItemData(row, actionIds[row]);
  labelActionType = label("labelActionType");
  form->addRow(labelActionType, comboActionType);
  keySequenceWidget = new KeySequenceWidget(actionCard);
  keySequenceWidget->setObjectName(QStringLiteral("keySequenceWidget"));
  keySequenceWidget->setFixedWidth(180);
  keySequenceField = new QWidget(actionCard);
  keySequenceField->setFixedWidth(180);
  auto *keyLayout = new QVBoxLayout(keySequenceField);
  keyLayout->setContentsMargins(0, 0, 0, 0);
  keyLayout->setSpacing(8);
  keyLayout->addWidget(keySequenceWidget);
  keyLayout->addWidget(keySequenceWidget->createRecordButton(keySequenceField), 0, Qt::AlignLeft);
  labelKeys = label("labelKeys");
  form->addRow(labelKeys, keySequenceField);
  comboSwitchToScreen = combo("comboSwitchToScreen", 0);
  labelTarget = label("labelTarget");
  form->addRow(labelTarget, comboSwitchToScreen);
  comboSwitchInDirection = combo("comboSwitchInDirection", 4);
  const int directionIds[]{1, 0, 2, 3};
  for (int row = 0; row < 4; ++row)
    comboSwitchInDirection->setItemData(row, directionIds[row]);
  labelDirection = label("labelDirection");
  form->addRow(labelDirection, comboSwitchInDirection);
  comboLockCursorToScreen = combo("comboLockCursorToScreen", 3);
  for (int row = 0; row < 3; ++row)
    comboLockCursorToScreen->setItemData(row, row);
  labelCursor = label("labelCursor");
  form->addRow(labelCursor, comboLockCursorToScreen);
  for (int row = 0; row < form->rowCount(); ++row) {
    auto *field = form->itemAt(row, QFormLayout::FieldRole)->widget();
    static_cast<QLabel *>(form->itemAt(row, QFormLayout::LabelRole)->widget())->setBuddy(field);
  }
  actionLayout->addLayout(form);
  page->addWidget(actionCard);
  groupScreens = new QGroupBox(actionCard);
  groupScreens->setObjectName(QStringLiteral("groupScreens"));
  groupScreens->setProperty("cardSection", true);
  groupScreens->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *recipients = new QFormLayout(groupScreens);
  recipients->setContentsMargins(0, 0, 0, 0);
  recipients->setHorizontalSpacing(10);
  recipients->setVerticalSpacing(8);
  recipients->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  recipients->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  recipients->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  labelRecipients = new QLabel(groupScreens);
  labelRecipients->setObjectName(QStringLiteral("labelRecipients"));
  labelRecipients->setFixedWidth(120);
  labelRecipients->setWordWrap(true);
  comboRecipients = new QComboBox(groupScreens);
  comboRecipients->setView(new QListView(comboRecipients));
  comboRecipients->setObjectName(QStringLiteral("comboRecipients"));
  comboRecipients->setFixedWidth(180);
  comboRecipients->addItems({QString(), QString()});
  comboRecipients->setItemData(0, 0);
  comboRecipients->setItemData(1, 1);
  labelRecipients->setBuddy(comboRecipients);
  recipients->addRow(labelRecipients, comboRecipients);
  listScreens = new QListWidget(groupScreens);
  listScreens->setObjectName(QStringLiteral("listScreens"));
  listScreens->setItemDelegate(new ScreenCheckDelegate(listScreens));
  recipients->addRow(listScreens);
  synergy::gui::compact::fitRows(listScreens, QCoreApplication::translate("ActionDialog", "No computers available"));
  actionLayout->addWidget(groupScreens);
  scrollArea->setWidget(scrollAreaWidgetContents);
  mainLayout->addWidget(scrollArea);
  buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, dialog);
  buttonBox->setObjectName(QStringLiteral("buttonBox"));
  synergy::gui::compact::placeActions(actionCard, buttonBox);
  retranslateUi(dialog);
}

void Ui::ActionDialog::retranslateUi(QDialog *dialog)
{
  dialog->setWindowTitle(QCoreApplication::translate("ActionDialog", "Configure Action"));
  actionCard->setTitle(QString());
  labelTrigger->setText(QCoreApplication::translate("ActionDialog", "Trigger"));
  labelActionType->setText(QCoreApplication::translate("ActionDialog", "Action type"));
  labelKeys->setText(QCoreApplication::translate("ActionDialog", "Key combination"));
  labelTarget->setText(QCoreApplication::translate("ActionDialog", "Target computer"));
  labelDirection->setText(QCoreApplication::translate("ActionDialog", "Direction"));
  labelCursor->setText(QCoreApplication::translate("ActionDialog", "Cursor lock"));
  comboRecipients->setItemText(0, QCoreApplication::translate("ActionDialog", "All computers"));
  comboRecipients->setItemText(1, QCoreApplication::translate("ActionDialog", "Specified computers"));

  comboTriggerOn->setItemText(0, QCoreApplication::translate("ActionDialog", "On press"));
  comboTriggerOn->setItemText(1, QCoreApplication::translate("ActionDialog", "On release"));

  comboActionType->setItemText(0, QCoreApplication::translate("ActionDialog", "Switch to a computer"));
  comboActionType->setItemText(1, QCoreApplication::translate("ActionDialog", "Switch in a direction"));
  comboActionType->setItemText(2, QCoreApplication::translate("ActionDialog", "Press and release keys"));
  comboActionType->setItemText(3, QCoreApplication::translate("ActionDialog", "Press key(s)"));
  comboActionType->setItemText(4, QCoreApplication::translate("ActionDialog", "Release key(s)"));
  comboActionType->setItemText(5, QCoreApplication::translate("ActionDialog", "Next computer"));
  comboActionType->setItemText(6, QCoreApplication::translate("ActionDialog", "Change cursor lock"));
  comboActionType->setItemText(7, QCoreApplication::translate("ActionDialog", "Restart the server"));

  comboSwitchInDirection->setItemText(0, QCoreApplication::translate("ActionDialog", "Right"));
  comboSwitchInDirection->setItemText(1, QCoreApplication::translate("ActionDialog", "Left"));
  comboSwitchInDirection->setItemText(2, QCoreApplication::translate("ActionDialog", "Up"));
  comboSwitchInDirection->setItemText(3, QCoreApplication::translate("ActionDialog", "Down"));

  comboLockCursorToScreen->setItemText(0, QCoreApplication::translate("ActionDialog", "Toggle lock"));
  comboLockCursorToScreen->setItemText(1, QCoreApplication::translate("ActionDialog", "Enable lock"));
  comboLockCursorToScreen->setItemText(2, QCoreApplication::translate("ActionDialog", "Disable lock"));

  groupScreens->setTitle(QString());
  labelRecipients->setText(QCoreApplication::translate("ActionDialog", "Computers to receive this event"));
  synergy::gui::compact::styleButtons(buttonBox);
}
