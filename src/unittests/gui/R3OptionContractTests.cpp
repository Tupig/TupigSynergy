/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/Constants.h"
#include "common/I18N.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/Action.h"
#include "gui/AppStyle.h"
#include "gui/Hotkey.h"
#include "gui/config/Screen.h"
#include "gui/config/ScreenList.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ActionDialog.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/widgets/KeySequenceWidget.h"
#include "synergy/gui/SettingsScope.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

namespace {
constexpr int actionIds[]{3, 4, 2, 0, 1, 5, 6, 7};
const char *actionLabels[]{"Switch to a computer", "Switch in a direction", "Press and release keys",
                           "Press key(s)",         "Release key(s)",        "Next computer",
                           "Change cursor lock",   "Restart the server"};
const char *modifierControls[]{"comboShift", "comboCtrl", "comboAlt", "comboMeta", "comboSuper", "comboAltGr"};
class FixtureScreen : public synergy::gui::Screen
{
public:
  using Screen::Screen;
  using Screen::setModifier;
};
Action makeAction(int type, int direction = 0, int lock = 0, const QString &target = QStringLiteral("Studio"))
{
  QTemporaryDir dir;
  QSettings s(dir.filePath("action.ini"), QSettings::IniFormat);
  KeySequence keys;
  keys.appendKey(Qt::Key_Control, Qt::ControlModifier);
  keys.appendKey(Qt::Key_K, Qt::ControlModifier);
  keys.saveSettings(s);
  s.setValue(SettingsKeys::ActionType, type);
  s.setValue(SettingsKeys::SwitchDirection, direction);
  s.setValue(SettingsKeys::LockToScreen, lock);
  s.setValue(SettingsKeys::SwitchToScreen, target);
  s.setValue(SettingsKeys::RestartServer, type == 7);
  s.setValue(SettingsKeys::ActiveOnRelease, true);
  s.setValue(SettingsKeys::HasScreens, true);
  s.beginWriteArray(SettingsKeys::ScreenNames, 1);
  s.setArrayIndex(0);
  s.setValue(SettingsKeys::ScreenName, "Studio");
  s.endArray();
  Action a;
  a.loadSettings(s);
  return a;
}
QPushButton *saveButton(QDialog &dialog)
{
  auto *box = dialog.findChild<QDialogButtonBox *>();
  if (!box)
    return nullptr;
  auto *save = box->button(QDialogButtonBox::Save);
  return save ? save : box->button(QDialogButtonBox::Ok);
}
QComboBox *combo(QObject &owner, const char *name)
{
  return owner.findChild<QComboBox *>(QString::fromLatin1(name));
}
void showForEvents(QWidget &widget)
{
  widget.setAttribute(Qt::WA_DontShowOnScreen);
  widget.show();
  QCoreApplication::processEvents();
}
// Interact with the actual view, never replace it or call setCurrentIndex as a popup oracle.
bool activatePopup(QComboBox *control, int row, bool mouse)
{
  if (!control || row < 0 || row >= control->count())
    return false;
  QSignalSpy activated(control, QOverload<int>::of(&QComboBox::activated));
  qInfo() << "Popup activation diagnostic: before" << control->objectName() << "mouse" << mouse << "targetRow" << row
          << "currentRow" << control->currentIndex() << "popupStyleHint"
          << control->style()->styleHint(QStyle::SH_ComboBox_Popup, nullptr, control);
  control->showPopup();
  QCoreApplication::processEvents();
  auto *view = control->view();
  qInfo() << "Popup activation diagnostic: opened" << "viewVisible" << (view && view->isVisible()) << "enabled"
          << (view && view->isEnabled()) << "class" << (view ? view->metaObject()->className() : "null")
          << "parentClass" << (view && view->parent() ? view->parent()->metaObject()->className() : "null");
  if (!view || !view->isVisible()) {
    qWarning() << "Popup activation diagnostic: popup view unavailable" << control->objectName() << "targetRow" << row
               << "currentRow" << control->currentIndex();
    return false;
  }
  const auto index = control->model()->index(row, control->modelColumn(), control->rootModelIndex());
  view->scrollTo(index);
  QCoreApplication::processEvents();
  if (mouse) {
    const auto pos = view->visualRect(index).center();
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
  } else {
    QTest::keyClick(view, Qt::Key_Home);
    for (int i = 0; i < row; ++i)
      QTest::keyClick(view, Qt::Key_Down);
    QTest::keyClick(view, Qt::Key_Return);
  }
  QCoreApplication::processEvents();
  // COMPLETE-R1 mechanical observation successor: business activation must be
  // immediate; only stock popup hide animation is allowed to settle afterward.
  const bool activatedImmediately = activated.count() == 1 && control->currentIndex() == row;
  auto *container = view->parentWidget();
  qInfo() << "Popup activation diagnostic: immediate" << "activatedCount" << activated.count() << "currentRow"
          << control->currentIndex() << "viewVisible" << view->isVisible() << "parentVisible"
          << (container && container->isVisible());
  QElapsedTimer closing;
  closing.start();
  while (activatedImmediately && (view->isVisible() || (container && container->isVisible())) &&
         closing.elapsed() < 1000)
    QTest::qWait(10);
  qInfo() << "Popup activation diagnostic: settled" << "elapsedMs" << closing.elapsed() << "activatedCount"
          << activated.count() << "currentRow" << control->currentIndex() << "viewVisible" << view->isVisible()
          << "parentVisible" << (container && container->isVisible());
  const bool success = activatedImmediately && activated.count() == 1 && control->currentIndex() == row &&
                       !view->isVisible() && (!container || !container->isVisible());
  if (!success)
    qWarning() << "Popup activation diagnostic" << control->objectName() << "mouse" << mouse << "targetRow" << row
               << "currentRow" << control->currentIndex() << "activatedCount" << activated.count() << "viewVisible"
               << view->isVisible() << "targetRect" << view->visualRect(index) << "viewRect" << view->rect()
               << "viewportRect" << view->viewport()->rect() << "targetCenterContained"
               << view->viewport()->rect().contains(view->visualRect(index).center()) << "enabled" << view->isEnabled()
               << "class" << view->metaObject()->className() << "parentClass"
               << (view->parent() ? view->parent()->metaObject()->className() : "null") << "focusWidget"
               << QApplication::focusWidget();
  return success;
}
} // namespace

class R3OptionContractTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void init();
  void actionStableIdsAndFields_data();
  void actionStableIdsAndFields();
  void legacyAndUnknownAreEntirelyReadOnly_data();
  void legacyAndUnknownAreEntirelyReadOnly();
  void defaultActionAndInvalidRelevantFields();
  void absentStoredTargetNeedsExplicitReplacement();
  void directionLockTriggerAndTarget();
  void popupSaveReopenAndEscape_data();
  void popupSaveReopenAndEscape();
  void explicitRecipientsExcludeFutureComputer();
  void modifierMappings_data();
  void modifierMappings();
  void modifierDefaultAndInvalidPreservation();
  void preferencesTypedValuesSaveCancelAndReopen();
  void languagePreviewCancelAndSystemValue();
  void injectedScopeRetranslatesInSameDialog_data();
  void injectedScopeRetranslatesInSameDialog();
  void controlledSystemLocaleFallback_data();
  void controlledSystemLocaleFallback();
};
void R3OptionContractTests::init()
{
  I18N::setLanguage("en");
  Settings::setValue(Settings::Core::Language, "en");
  Settings::setValue(Settings::Core::ComputerName, "Test-Server");
  Settings::setValue(Settings::Gui::CheckForUpdates, false);
  Settings::setValue(Settings::Gui::AutoStartCore, false);
}
void R3OptionContractTests::actionStableIdsAndFields_data()
{
  QTest::addColumn<int>("row");
  for (int row = 0; row < 8; ++row)
    QTest::newRow(actionLabels[row]) << row;
}
void R3OptionContractTests::actionStableIdsAndFields()
{
  QFETCH(int, row);
  const int id = actionIds[row];
  synergy::gui::ServerConfig config;
  config.addClient("Studio");
  config.addClient("Workstation");
  Hotkey hotkey;
  Action action = makeAction(id);
  ActionDialog editor(nullptr, config, hotkey, action);
  showForEvents(editor);
  auto *types = combo(editor, "comboActionType");
  QVERIFY(types);
  QCOMPARE(types->count(), 8);
  for (int i = 0; i < 8; ++i) {
    QCOMPARE(types->itemData(i).toInt(), actionIds[i]);
    QCOMPARE(types->itemText(i), QString::fromLatin1(actionLabels[i]));
  }
  QCOMPARE(types->currentIndex(), row);
  QCOMPARE(types->currentData().toInt(), id);
  QCOMPARE(editor.findChild<KeySequenceWidget *>()->isVisibleTo(&editor), id < 3);
  for (const auto &field :
       {qMakePair("comboSwitchToScreen", 3), qMakePair("comboSwitchInDirection", 4),
        qMakePair("comboLockCursorToScreen", 6)})
    QCOMPARE(combo(editor, field.first)->isVisibleTo(&editor), id == field.second);
  QVERIFY(saveButton(editor)->isEnabled());
  QTest::mouseClick(saveButton(editor), Qt::LeftButton);
  QCOMPARE(editor.result(), int(QDialog::Accepted));
  QCOMPARE(action.type(), id);
  QTemporaryDir dir;
  QSettings saved(dir.filePath("saved.ini"), QSettings::IniFormat);
  action.saveSettings(saved);
  saved.sync();
  Action reloaded;
  reloaded.loadSettings(saved);
  QCOMPARE(reloaded, action);
  ActionDialog reopened(nullptr, config, hotkey, reloaded);
  QCOMPARE(combo(reopened, "comboActionType")->currentData().toInt(), id);
  reopened.reject();
}
void R3OptionContractTests::legacyAndUnknownAreEntirelyReadOnly_data()
{
  QTest::addColumn<int>("id");
  for (int id : {8, 9, 10, -1, 11, 2000000})
    QTest::newRow(qPrintable(QString::number(id))) << id;
}
void R3OptionContractTests::legacyAndUnknownAreEntirelyReadOnly()
{
  QFETCH(int, id);
  synergy::gui::ServerConfig config;
  config.addClient("Studio");
  Hotkey hotkey;
  Action action = makeAction(id, 3, 2);
  const auto original = action;
  ActionDialog editor(nullptr, config, hotkey, action);
  showForEvents(editor);
  auto *types = combo(editor, "comboActionType");
  QVERIFY(types);
  QCOMPARE(types->currentData().toInt(), id);
  QVERIFY(types->currentText().contains(QString::number(id)));
  QVERIFY(!saveButton(editor)->isEnabled());
  for (auto *control : editor.findChildren<QComboBox *>())
    QVERIFY2(!control->isEnabled(), qPrintable(control->objectName()));
  for (auto *control : editor.findChildren<KeySequenceWidget *>())
    QVERIFY(!control->isEnabled());
  for (auto *control : editor.findChildren<QListWidget *>())
    QVERIFY(!control->isEnabled());
  QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
  QVERIFY(editor.result() != int(QDialog::Accepted));
  QCOMPARE(action, original);
  editor.reject();
  QCOMPARE(action, original);
}
// Additive UI-014 unavailable-target regression; no backend/protocol change.
void R3OptionContractTests::absentStoredTargetNeedsExplicitReplacement()
{
  synergy::gui::ServerConfig config;
  config.addClient("Studio");
  config.addClient("Workstation");
  Hotkey hotkey;
  auto action = makeAction(3, 0, 0, "Absent-Fixture");
  const auto original = action;
  {
    ActionDialog editor(nullptr, config, hotkey, action);
    showForEvents(editor);
    auto *targets = combo(editor, "comboSwitchToScreen");
    QVERIFY(targets);
    QVERIFY(targets->currentText().contains("Absent-Fixture"));
    QVERIFY(!saveButton(editor)->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
    QVERIFY(editor.result() != int(QDialog::Accepted));
    QCOMPARE(action, original);
    editor.reject();
    QCOMPARE(action, original);
  }
  {
    ActionDialog editor(nullptr, config, hotkey, action);
    showForEvents(editor);
    auto *targets = combo(editor, "comboSwitchToScreen");
    const int valid = targets->findText("Studio");
    QVERIFY(valid >= 0);
    QVERIFY2(activatePopup(targets, valid, true), "Explicit valid target must be chosen through the actual popup");
    QVERIFY(saveButton(editor)->isEnabled());
    QTest::mouseClick(saveButton(editor), Qt::LeftButton);
    QCOMPARE(editor.result(), int(QDialog::Accepted));
    QCOMPARE(action.switchScreenName(), QString("Studio"));
  }
  QTemporaryDir directory;
  QSettings persisted(directory.filePath("replacement.ini"), QSettings::IniFormat);
  action.saveSettings(persisted);
  persisted.sync();
  Action loaded;
  loaded.loadSettings(persisted);
  ActionDialog reopened(nullptr, config, hotkey, loaded);
  QCOMPARE(combo(reopened, "comboSwitchToScreen")->currentText(), QString("Studio"));
  QCOMPARE(loaded.switchScreenName(), QString("Studio"));
  reopened.reject();
}
void R3OptionContractTests::directionLockTriggerAndTarget()
{
  synergy::gui::ServerConfig config;
  config.addClient("Studio");
  config.addClient("Workstation");
  Hotkey hotkey;
  const char *directions[]{"right", "left", "up", "down"};
  const int ids[]{1, 0, 2, 3};
  for (int row = 0; row < 4; ++row) {
    auto action = makeAction(4, ids[row]);
    ActionDialog editor(nullptr, config, hotkey, action);
    auto *direction = combo(editor, "comboSwitchInDirection");
    for (int i = 0; i < 4; ++i)
      QCOMPARE(direction->itemData(i).toInt(), ids[i]);
    QCOMPARE(direction->currentIndex(), row);
    QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
    QCOMPARE(action.text(), QStringLiteral("switchInDirection(%1)").arg(QString::fromLatin1(directions[row])));
  }
  const char *locks[]{"toggle", "on", "off"};
  for (int id = 0; id < 3; ++id) {
    auto action = makeAction(6, 0, id);
    ActionDialog editor(nullptr, config, hotkey, action);
    QCOMPARE(combo(editor, "comboLockCursorToScreen")->currentData().toInt(), id);
    QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
    QCOMPARE(action.text(), QStringLiteral("lockCursorToScreen(%1)").arg(QString::fromLatin1(locks[id])));
  }
  for (bool release : {false, true}) {
    auto action = makeAction(3);
    ActionDialog editor(nullptr, config, hotkey, action);
    auto *trigger = combo(editor, "comboTriggerOn");
    QCOMPARE(trigger->itemData(0).toBool(), false);
    QCOMPARE(trigger->itemData(1).toBool(), true);
    trigger->setCurrentIndex(trigger->findData(release));
    auto *target = combo(editor, "comboSwitchToScreen");
    target->setCurrentIndex(target->findData("Workstation"));
    QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
    QCOMPARE(action.activeOnRelease(), release);
    QCOMPARE(action.switchScreenName(), QString("Workstation"));
    QString command;
    QTextStream stream(&command);
    stream << action;
    QCOMPARE(command, release ? QString(";switchToScreen(Workstation)") : QString("switchToScreen(Workstation)"));
  }
  // Invalid hidden fields remain irrelevant and must not become -1 when saving another type.
  auto unrelated = makeAction(3, 400, 500);
  ActionDialog editor(nullptr, config, hotkey, unrelated);
  QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
  QCOMPARE(unrelated.switchDirection(), 400);
  QCOMPARE(unrelated.lockCursorMode(), 500);
  QCOMPARE(unrelated.text(), QString("switchToScreen(Studio)"));
}
void R3OptionContractTests::defaultActionAndInvalidRelevantFields()
{
  Action defaults;
  QVERIFY(!defaults.restartServer());
  synergy::gui::ServerConfig config;
  Hotkey hotkey;
  for (int type : {4, 6}) {
    for (int invalid : {-1, 4, 100000}) {
      Action action = makeAction(type, type == 4 ? invalid : 0, type == 6 ? invalid : 0);
      const auto original = action;
      ActionDialog editor(nullptr, config, hotkey, action);
      QVERIFY(!saveButton(editor)->isEnabled());
      QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
      QVERIFY(editor.result() != int(QDialog::Accepted));
      QCOMPARE(action, original);
      editor.reject();
      QCOMPARE(action, original);
    }
  }
}
void R3OptionContractTests::popupSaveReopenAndEscape_data()
{
  QTest::addColumn<bool>("mouse");
  QTest::newRow("full-mouse-press-release") << true;
  QTest::newRow("keyboard-home-down-return") << false;
}
void R3OptionContractTests::popupSaveReopenAndEscape()
{
  QFETCH(bool, mouse);
  synergy::gui::ServerConfig config;
  config.addClient("Studio");
  Hotkey hotkey;
  for (int row = 0; row < 8; ++row) {
    auto action = makeAction(row == 0 ? 4 : 3);
    ActionDialog editor(nullptr, config, hotkey, action);
    showForEvents(editor);
    auto *types = combo(editor, "comboActionType");
    QSignalSpy changed(types, QOverload<int>::of(&QComboBox::currentIndexChanged));
    QVERIFY2(activatePopup(types, row, mouse), "Actual popup selection/activation did not occur");
    QVERIFY(!changed.isEmpty());
    QCOMPARE(types->currentData().toInt(), actionIds[row]);
    QTest::mouseClick(saveButton(editor), Qt::LeftButton);
    QCOMPARE(action.type(), actionIds[row]);
    const auto saved = action;
    ActionDialog reopened(nullptr, config, hotkey, action);
    showForEvents(reopened);
    auto *again = combo(reopened, "comboActionType");
    QCOMPARE(again->currentData().toInt(), actionIds[row]);
    again->showPopup();
    QCoreApplication::processEvents();
    QVERIFY(again->view()->isVisible());
    QTest::keyClick(again->view(), Qt::Key_Down);
    QTest::keyClick(again->view(), Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(again->currentData().toInt(), actionIds[row]);
    reopened.reject();
    QCOMPARE(action, saved);
  }
}
void R3OptionContractTests::explicitRecipientsExcludeFutureComputer()
{
  synergy::gui::ServerConfig config;
  config.addClient("Studio");
  config.addClient("Workstation");
  Hotkey hotkey;
  auto action = makeAction(2);
  QStringList selected;
  {
    ActionDialog editor(nullptr, config, hotkey, action);
    auto *scope = combo(editor, "comboRecipients");
    QCOMPARE(scope->count(), 2);
    QCOMPARE(scope->itemData(0).toInt(), 0);
    QCOMPARE(scope->itemData(1).toInt(), 1);
    scope->setCurrentIndex(scope->findData(1));
    auto *list = editor.findChild<QListWidget *>("listScreens");
    for (int i = 0; i < list->count(); ++i) {
      list->item(i)->setCheckState(Qt::Checked);
      selected << list->item(i)->text();
    }
    QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
  }
  QVERIFY(action.haveScreens());
  QCOMPARE(std::as_const(action).typeScreenNames(), selected);
  QVERIFY(!action.text().contains(",*"));
  config.addClient("Future");
  ActionDialog reopened(nullptr, config, hotkey, action);
  QCOMPARE(combo(reopened, "comboRecipients")->currentData().toInt(), 1);
  auto *list = reopened.findChild<QListWidget *>("listScreens");
  bool found = false;
  for (int i = 0; i < list->count(); ++i)
    if (list->item(i)->text() == "Future") {
      found = true;
      QCOMPARE(list->item(i)->checkState(), Qt::Unchecked);
    }
  QVERIFY(found);
  reopened.reject();
}
void R3OptionContractTests::modifierMappings_data()
{
  QTest::addColumn<int>("source");
  QTest::addColumn<int>("target");
  for (int source = 0; source < 6; ++source)
    for (int target = 0; target < 7; ++target)
      QTest::newRow(qPrintable(QStringLiteral("%1-to-%2").arg(source).arg(target))) << source << target;
}
void R3OptionContractTests::modifierMappings()
{
  QFETCH(int, source);
  QFETCH(int, target);
  FixtureScreen screen("Studio");
  const char *labels[]{"Shift", "Ctrl", "Alt", "Meta", "Super", "Alt Gr", "None"};
  ScreenSettingsDialog editor(nullptr, &screen, nullptr);
  auto *control = combo(editor, modifierControls[source]);
  QVERIFY(control);
  QCOMPARE(control->count(), 7);
  for (int i = 0; i < 7; ++i) {
    QCOMPARE(control->itemData(i).toInt(), i);
    QCOMPARE(control->itemText(i), QString::fromLatin1(labels[i]));
  }
  control->setCurrentIndex(control->findData(target));
  QVERIFY(QMetaObject::invokeMethod(&editor, "accept", Qt::DirectConnection));
  QCOMPARE(screen.modifier(source), target);
  ScreenSettingsDialog reopened(nullptr, &screen, nullptr);
  QCOMPARE(combo(reopened, modifierControls[source])->currentData().toInt(), target);
  reopened.reject();
  const char *protocol[]{"shift", "ctrl", "alt", "meta", "super", "altgr", "none"};
  if (source != target)
    QVERIFY(screen.screensSection().contains(QStringLiteral("%1 = %2").arg(protocol[source], protocol[target])));
}
void R3OptionContractTests::modifierDefaultAndInvalidPreservation()
{
  for (int source = 0; source < 6; ++source) {
    FixtureScreen screen("Studio");
    screen.setModifier(static_cast<synergy::gui::ScreenConfig::Modifier>(source), -1);
    QCOMPARE(screen.modifier(source), source);
    ScreenSettingsDialog editor(nullptr, &screen, nullptr);
    QCOMPARE(combo(editor, modifierControls[source])->currentData().toInt(), source);
    editor.reject();
    QCOMPARE(std::as_const(screen).modifiers().at(source), -1);
    for (int invalid : {-2, 7, 100000}) {
      screen.setModifier(static_cast<synergy::gui::ScreenConfig::Modifier>(source), invalid);
      const auto original = screen;
      ScreenSettingsDialog unsupported(nullptr, &screen, nullptr);
      QVERIFY(!saveButton(unsupported)->isEnabled());
      for (auto *field : unsupported.findChildren<QComboBox *>())
        QVERIFY(!field->isEnabled());
      QVERIFY(QMetaObject::invokeMethod(&unsupported, "accept", Qt::DirectConnection));
      QVERIFY(unsupported.result() != int(QDialog::Accepted));
      unsupported.reject();
      QCOMPARE(std::as_const(screen).modifiers(), std::as_const(original).modifiers());
    }
  }
}
void R3OptionContractTests::preferencesTypedValuesSaveCancelAndReopen()
{
  synergy::gui::ServerConfig config;
  Settings::setValue(Settings::Gui::CloseToTray, false);
  Settings::setValue(Settings::Gui::Autohide, false);
  Settings::setValue(Settings::Gui::SymbolicTrayIcon, false);
  {
    SettingsDialog editor(nullptr, config);
    auto *close = combo(editor, "comboCloseBehavior");
    QCOMPARE(close->itemData(0).toBool(), true);
    QCOMPARE(close->itemData(1).toBool(), false);
    QCOMPARE(close->currentData().toBool(), false);
    close->setCurrentIndex(close->findData(true));
    QVERIFY(editor.findChild<QRadioButton *>("rbCloseToTray")->isChecked());
    combo(editor, "comboStartBehavior")->setCurrentIndex(combo(editor, "comboStartBehavior")->findData(true));
    combo(editor, "comboTrayIcon")->setCurrentIndex(combo(editor, "comboTrayIcon")->findData(true));
    editor.reject();
    QVERIFY(!Settings::value(Settings::Gui::CloseToTray).toBool());
    QVERIFY(!Settings::value(Settings::Gui::Autohide).toBool());
  }
  {
    SettingsDialog editor(nullptr, config);
    for (const auto *name : {"comboCloseBehavior", "comboStartBehavior", "comboTrayIcon"}) {
      auto *field = combo(editor, name);
      QCOMPARE(field->count(), 2);
      QVERIFY(field->findData(false) >= 0);
      QVERIFY(field->findData(true) >= 0);
      field->setCurrentIndex(field->findData(true));
    }
    auto *bits = combo(editor, "comboTlsKeyLength");
    QCOMPARE(bits->itemData(0).toInt(), 2048);
    QCOMPARE(bits->itemData(1).toInt(), 4096);
    QTest::mouseClick(saveButton(editor), Qt::LeftButton);
    QCOMPARE(editor.result(), int(QDialog::Accepted));
  }
  QVERIFY(Settings::value(Settings::Gui::CloseToTray).toBool());
  QVERIFY(Settings::value(Settings::Gui::Autohide).toBool());
  QVERIFY(Settings::value(Settings::Gui::SymbolicTrayIcon).toBool());
  SettingsDialog reopened(nullptr, config);
  for (const auto *name : {"comboCloseBehavior", "comboStartBehavior", "comboTrayIcon"})
    QCOMPARE(combo(reopened, name)->currentData().toBool(), true);
  auto *interface = combo(reopened, "comboInterface");
  for (int i = 1; i < interface->count(); ++i)
    QCOMPARE(interface->itemData(i).toString(), interface->itemText(i));
  auto *log = combo(reopened, "comboLogLevel");
  for (int i = 0; i < log->count(); ++i)
    QVERIFY(!log->itemData(i).toString().isEmpty());
  reopened.reject();
}
void R3OptionContractTests::languagePreviewCancelAndSystemValue()
{
  Settings::setValue(Settings::Core::Language, QString());
  synergy::gui::ServerConfig config;
  const QString original = I18N::currentLanguage();
  {
    SettingsDialog editor(nullptr, config);
    auto *languages = combo(editor, "comboLanguage");
    QVERIFY(languages->findData(QString()) >= 0);
    QCOMPARE(languages->currentData().toString(), QString());
    const int chinese = languages->findData(QStringLiteral("zh_CN"));
    QVERIFY2(chinese >= 0, "Packaged test translations must include zh_CN");
    languages->setCurrentIndex(chinese);
    QVERIFY(I18N::currentLanguage().startsWith("zh"));
    QVERIFY(Settings::value(Settings::Core::Language).toString().isEmpty());
    editor.reject();
    QCOMPARE(I18N::currentLanguage(), original);
    QVERIFY(Settings::value(Settings::Core::Language).toString().isEmpty());
  }
  SettingsDialog editor(nullptr, config);
  auto *languages = combo(editor, "comboLanguage");
  languages->setCurrentIndex(languages->findData(QStringLiteral("en")));
  QTest::mouseClick(saveButton(editor), Qt::LeftButton);
  QCOMPARE(Settings::value(Settings::Core::Language).toString(), QStringLiteral("en"));
  SettingsDialog reopened(nullptr, config);
  QCOMPARE(combo(reopened, "comboLanguage")->currentData().toString(), QStringLiteral("en"));
  reopened.reject();
  {
    SettingsDialog toSystem(nullptr, config);
    auto *cancelledSystemLanguages = combo(toSystem, "comboLanguage");
    cancelledSystemLanguages->setCurrentIndex(cancelledSystemLanguages->findData(QString()));
    QCOMPARE(Settings::value(Settings::Core::Language).toString(), QStringLiteral("en"));
    toSystem.reject();
    QCOMPARE(Settings::value(Settings::Core::Language).toString(), QStringLiteral("en"));
  }
  {
    SettingsDialog toSystem(nullptr, config);
    auto *acceptedSystemLanguages = combo(toSystem, "comboLanguage");
    acceptedSystemLanguages->setCurrentIndex(acceptedSystemLanguages->findData(QString()));
    QTest::mouseClick(saveButton(toSystem), Qt::LeftButton);
    QCOMPARE(toSystem.result(), int(QDialog::Accepted));
    QVERIFY(Settings::value(Settings::Core::Language).toString().isEmpty());
  }
  SettingsDialog systemReopened(nullptr, config);
  QCOMPARE(combo(systemReopened, "comboLanguage")->currentData().toString(), QString());
  systemReopened.reject();
}
// UI-008/UI-014 native finding successor: real injected controls must follow
// actual language-popup preview without discarding the unsaved scope choice.
void R3OptionContractTests::injectedScopeRetranslatesInSameDialog_data()
{
  QTest::addColumn<QString>("originalLanguage");
  QTest::addColumn<int>("width");
  for (const auto &language : {QStringLiteral("en"), QStringLiteral("zh_CN")})
    for (int width : {540, 600, 820})
      QTest::newRow(qPrintable(language + "-" + QString::number(width))) << language << width;
}
void R3OptionContractTests::injectedScopeRetranslatesInSameDialog()
{
  QFETCH(QString, originalLanguage);
  QFETCH(int, width);
  Settings::setValue(Settings::Core::Language, originalLanguage);
  I18N::setLanguage(originalLanguage);
  const auto originalSettingsPath = Settings::settingsFile();
  const bool originalPreferSystem = synergy::gui::SettingsScope::preferSystem();
  synergy::gui::ServerConfig config;
  SettingsDialog editor(nullptr, config);
  showForEvents(editor);
  editor.resize(width, editor.height());
  auto *label = editor.findChild<QLabel *>("scopeLabel");
  auto *scope = combo(editor, "comboSettingsScope");
  auto *languages = combo(editor, "comboLanguage");
  QVERIFY(label && scope && languages);
  QCOMPARE(scope->count(), 2);
  QCOMPARE(scope->itemData(0).toBool(), false);
  QCOMPARE(scope->itemData(1).toBool(), true);
  const auto initialScope = scope->currentData();
  const auto initialScopeProperty = scope->property("originalScope");
  QVERIFY(!initialScope.toBool());
  scope->setCurrentIndex(scope->findData(true));
  QVERIFY(saveButton(editor)->isEnabled()); // Existing scope dirty tracking stays intact; never save/promote.
  for (const auto &language : {QStringLiteral("en"), QStringLiteral("zh_CN"), QStringLiteral("en")}) {
    const int row = languages->findData(language);
    QVERIFY(row >= 0);
    QVERIFY2(activatePopup(languages, row, true), "Language preview must use the real popup activation path");
    QCOMPARE(I18N::currentLanguage(), language);
    QCOMPARE(Settings::value(Settings::Core::Language).toString(), originalLanguage);
    QCOMPARE(scope->itemData(0).toBool(), false);
    QCOMPARE(scope->itemData(1).toBool(), true);
    QCOMPARE(scope->currentData().toBool(), true);
    QCOMPARE(scope->property("originalScope"), initialScopeProperty);
    const bool chinese = language == QStringLiteral("zh_CN");
    // R4-P03 reference wording only; every typed/draft/Cancel assertion stays.
    QCOMPARE(label->text(), chinese ? QString::fromUtf8("设置作用域") : QString("Settings scope"));
    QCOMPARE(
        scope->itemText(scope->findData(false)), chinese ? QString::fromUtf8("当前用户") : QString("Current user")
    );
    QCOMPARE(scope->itemText(scope->findData(true)), chinese ? QString::fromUtf8("所有用户") : QString("All users"));
    const auto expectedTooltip =
        chinese ? QString::fromUtf8("选择 %1 存储设置的位置。更改将在 %1 重启后生效。")
                : QString("Choose where %1 stores its settings. Changes take effect when %1 restarts.");
    QCOMPARE(scope->toolTip(), expectedTooltip.arg(QApplication::applicationName()));
    QVERIFY(saveButton(editor)->isEnabled());
    QCOMPARE(Settings::settingsFile(), originalSettingsPath);
    QCOMPARE(synergy::gui::SettingsScope::preferSystem(), originalPreferSystem);
  }
  auto *buttons = editor.findChild<QDialogButtonBox *>();
  QVERIFY(buttons && buttons->button(QDialogButtonBox::Cancel));
  QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel), Qt::LeftButton);
  QCOMPARE(editor.result(), int(QDialog::Rejected));
  QCOMPARE(I18N::currentLanguage(), originalLanguage);
  QCOMPARE(Settings::value(Settings::Core::Language).toString(), originalLanguage);
  QCOMPARE(Settings::settingsFile(), originalSettingsPath);
  QCOMPARE(synergy::gui::SettingsScope::preferSystem(), originalPreferSystem);
  SettingsDialog reopened(nullptr, config);
  QCOMPARE(combo(reopened, "comboSettingsScope")->currentData(), initialScope);
  QCOMPARE(combo(reopened, "comboLanguage")->currentData().toString(), originalLanguage);
  reopened.reject();
}
void R3OptionContractTests::controlledSystemLocaleFallback_data()
{
  QTest::addColumn<QString>("locale");
  QTest::addColumn<QString>("expected");
  QTest::newRow("unsupported-french-falls-back-to-english") << QString("fr_FR") << QString("en");
  QTest::newRow("japanese-system-default-is-english") << QString("ja_JP") << QString("en");
  QTest::newRow("simplified-chinese") << QString("zh_CN") << QString("zh_CN");
  // C-UI-R3-COMPLETE-LOCALE-R2: PM-approved successor; the shipped catalogue
  // inventory has no Traditional catalogue, so preserve I18N's China fallback.
  // Original unconditional zh_TW expectation and its RED remain in baseline logs.
  QTest::newRow("traditional-missing-catalog-falls-back-to-simplified") << QString("zh_TW") << QString("zh_CN");
}
void R3OptionContractTests::controlledSystemLocaleFallback()
{
  QFETCH(QString, locale);
  QFETCH(QString, expected);
  if (locale == QStringLiteral("zh_TW"))
    QVERIFY2(
        !QFileInfo::exists(QCoreApplication::applicationDirPath() + "/translations/synergy_zh_TW.qm"),
        "Successor fallback oracle requires the explicit absence of a Traditional catalogue"
    );
  QProcess process;
  process.start(QCoreApplication::applicationFilePath(), {"--r3-system-locale", locale, expected});
  QVERIFY(process.waitForStarted(5000));
  QVERIFY(process.waitForFinished(15000));
  QCOMPARE(process.exitStatus(), QProcess::NormalExit);
  QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
}

int main(int argc, char **argv)
{
  QTemporaryDir directory;
  if (!directory.isValid())
    return 1;
  const auto root = QFileInfo(directory.path()).canonicalFilePath();
  QString contextError;
  if (!synergy::runtime::configureIsolatedRuntime(
          {root, "tsr3-" + QUuid::createUuid().toString(QUuid::WithoutBraces)}, &contextError
      ))
    return 2;
  qputenv("XDG_CONFIG_HOME", root.toUtf8());
  qputenv("XDG_STATE_HOME", root.toUtf8());
  QApplication app(argc, argv);
  Settings::setSettingsFile(root + "/user/test.conf");
  Settings::setStateFile(root + "/state/app.state");
  if (argc == 4 && QByteArray(argv[1]) == "--r3-system-locale") {
    QLocale::setDefault(QLocale(QString::fromLocal8Bit(argv[2])));
    Settings::setValue(Settings::Core::Language, QString());
    const QString expected = QString::fromLocal8Bit(argv[3]);
    if (I18N::currentLanguage() != expected)
      return 40;
    synergy::gui::ServerConfig config;
    SettingsDialog editor(nullptr, config);
    auto *languages = combo(editor, "comboLanguage");
    if (!languages || !languages->currentData().toString().isEmpty())
      return 41;
    const int explicitEnglish = languages->findData(QStringLiteral("en"));
    if (explicitEnglish < 0)
      return 42;
    languages->setCurrentIndex(explicitEnglish);
    if (!Settings::value(Settings::Core::Language).toString().isEmpty())
      return 43;
    editor.reject();
    if (I18N::currentLanguage() != expected || !Settings::value(Settings::Core::Language).toString().isEmpty())
      return 44;
    return 0;
  }
  app.setStyleSheet(synergy::gui::appStyleSheet());
  R3OptionContractTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "R3OptionContractTests.moc"
