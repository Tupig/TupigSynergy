/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "UiRenderTestSupport.h"
#include "common/I18N.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/Action.h"
#include "gui/AppMenus.h"
#include "gui/AppStyle.h"
#include "gui/Hotkey.h"
#include "gui/TlsUtility.h"
#include "gui/config/Screen.h"
#include "gui/config/ScreenList.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/AboutDialog.h"
#include "gui/dialogs/ActionDialog.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/FingerprintDialog.h"
#include "gui/dialogs/HotkeyDialog.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/update/UpdateDialog.h"
#include "gui/widgets/KeySequenceWidget.h"
#include "gui/widgets/MessagePage.h"
#include "gui/widgets/PageHost.h"
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFocusEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTranslator>
#include <QUuid>
#include <QVBoxLayout>
#include <QtTest>

namespace {
class ScopedLanguage
{
public:
  explicit ScopedLanguage(const QString &language)
      : m_setting(Settings::value(Settings::Core::Language)),
        m_language(I18N::currentLanguage())
  {
    Settings::setValue(Settings::Core::Language, language);
    I18N::setLanguage(language);
  }
  ~ScopedLanguage()
  {
    I18N::setLanguage(m_language);
    Settings::setValue(Settings::Core::Language, m_setting);
  }

private:
  QVariant m_setting;
  QString m_language;
};
class ScopedSetting
{
public:
  ScopedSetting(QString key, const QVariant &value) : m_key(std::move(key)), m_value(Settings::value(m_key))
  {
    Settings::setValue(m_key, value);
  }
  ~ScopedSetting()
  {
    Settings::setValue(m_key, m_value);
  }

private:
  QString m_key;
  QVariant m_value;
};

void reportSettingsMismatches(SettingsDialog &editor)
{
  const auto mismatch = [](const char *key, bool different) {
    if (different)
      qWarning() << "R2 Reset mismatch:" << key;
  };
  const auto check = [&](const char *name, const QString &key) {
    auto *control = editor.findChild<QCheckBox *>(QString::fromLatin1(name));
    if (control)
      mismatch(name, control->isChecked() != Settings::value(key).toBool());
  };
  const auto radio = [&](const char *name, const QString &key) {
    auto *control = editor.findChild<QRadioButton *>(QString::fromLatin1(name));
    if (control)
      mismatch(name, control->isChecked() != Settings::value(key).toBool());
  };
  const auto line = [&](const char *name, const QString &key) {
    auto *control = editor.findChild<QLineEdit *>(QString::fromLatin1(name));
    if (control)
      mismatch(name, control->text() != Settings::value(key).toString());
  };
  check("cbPreventSleep", Settings::Core::PreventSleep);
  check("cbElevateDaemon", Settings::Daemon::Elevate);
  check("cbGuiDebug", Settings::Log::GuiDebug);
  check("cbShowVersion", Settings::Gui::ShowVersionInTitle);
  check("cbRequireClientCert", Settings::Security::CheckPeers);
  check("cbRunEnterCommand", Settings::Core::EnableEnterCommand);
  check("cbRunExitCommand", Settings::Core::EnableExitCommand);
  check("checkForUpdates", Settings::Gui::CheckForUpdates);
  radio("rbAutoHide", Settings::Gui::Autohide);
  radio("rbCloseToTray", Settings::Gui::CloseToTray);
  radio("rbIconMono", Settings::Gui::SymbolicTrayIcon);
  line("lineLogFilename", Settings::Log::File);
  line("lineTlsCertPath", Settings::Security::Certificate);
  line("lineCommandEnter", Settings::Core::ScreenEnterCommand);
  line("lineCommandExit", Settings::Core::ScreenExitCommand);
  mismatch(
      "sbPort",
      editor.findChild<QSpinBox *>(QStringLiteral("sbPort"))->value() != Settings::value(Settings::Core::Port).toInt()
  );
  mismatch(
      "comboLogLevel",
      editor.findChild<QComboBox *>(QStringLiteral("comboLogLevel"))->currentData() != Settings::logLevelText()
  );
  mismatch(
      "groupLogToFile", editor.findChild<QGroupBox *>(QStringLiteral("groupLogToFile"))->isChecked() !=
                            Settings::value(Settings::Log::ToFile).toBool()
  );
  mismatch(
      "groupSecurity", editor.findChild<QGroupBox *>(QStringLiteral("groupSecurity"))->isChecked() !=
                           Settings::value(Settings::Security::TlsEnabled).toBool()
  );
  mismatch(
      "groupService",
      editor.findChild<QGroupBox *>(QStringLiteral("groupService"))->isChecked() !=
          (Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>() == Settings::Service)
  );
  mismatch(
      "comboTlsKeyLength", editor.findChild<QComboBox *>(QStringLiteral("comboTlsKeyLength"))->currentText() !=
                               Settings::value(Settings::Security::KeySize).toString()
  );
  mismatch(
      "comboLanguage",
      I18N::nativeTo639Name(editor.findChild<QComboBox *>(QStringLiteral("comboLanguage"))->currentText()) !=
          I18N::currentLanguage()
  );
  auto *scope = editor.findChild<QComboBox *>(QStringLiteral("comboSettingsScope"));
  if (scope)
    mismatch("comboSettingsScope", scope->currentIndex() != scope->property("originalScopeIndex").toInt());
  auto *interface = editor.findChild<QComboBox *>(QStringLiteral("comboInterface"));
  if (interface->currentIndex() > 0)
    mismatch("comboInterface", interface->currentText() != Settings::value(Settings::Core::Interface).toString());
}

void settle()
{
  for (int i = 0; i < 8; ++i) {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
  }
}
void prepare(PageHost &host, int width)
{
  host.setAttribute(Qt::WA_DontShowOnScreen);
  auto *layout = new QVBoxLayout(&host);
  layout->setContentsMargins(0, 0, 0, 0);
  auto *stack = new QStackedWidget(&host);
  stack->setObjectName(QStringLiteral("pageStack"));
  stack->addWidget(new QWidget);
  layout->addWidget(stack);
  host.resize(width, 100);
  host.show();
}
void fit(PageHost &host, int width)
{
  settle();
  host.resize(width, qMin(660, host.preferredContentHeight(width)));
  settle();
}
QPushButton *saveButton(QDialog &dialog)
{
  auto *box = dialog.findChild<QDialogButtonBox *>();
  if (!box)
    return nullptr;
  auto *save = box->button(QDialogButtonBox::Save);
  return save ? save : box->button(QDialogButtonBox::Ok);
}
// Measure leaf controls, never a stretched ancestor/card's bottom edge.
int visibleContentBottom(QDialog &dialog, QDialogButtonBox *box)
{
  int bottom = -1;
  for (auto *widget : dialog.findChildren<QWidget *>()) {
    if (!widget->isVisibleTo(&dialog) || widget == box || (box && box->isAncestorOf(widget)))
      continue;
    const bool leaf = qobject_cast<QLabel *>(widget) || qobject_cast<QLineEdit *>(widget) ||
                      qobject_cast<QComboBox *>(widget) || qobject_cast<QAbstractSpinBox *>(widget) ||
                      qobject_cast<QAbstractButton *>(widget) || qobject_cast<QListWidget *>(widget) ||
                      qobject_cast<QTextEdit *>(widget);
    if (leaf)
      bottom = qMax(bottom, widget->mapTo(&dialog, QPoint(0, widget->height() - 1)).y());
  }
  return bottom;
}
QDialog *syntheticPage(int height)
{
  auto *dialog = new QDialog;
  auto *layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(0, 0, 0, 0);
  auto *body = new QLabel(QStringLiteral("Synthetic body"));
  body->setFixedHeight(height);
  layout->addWidget(body);
  layout->addWidget(new QDialogButtonBox(QDialogButtonBox::Cancel));
  return dialog;
}
QStringList menuNames(const QList<QAction *> &actions)
{
  QStringList result;
  for (auto *action : actions)
    result << (action->isSeparator() ? QStringLiteral("|") : action->objectName());
  return result;
}
} // namespace

class LayoutAcceptanceTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void activePageHeightIgnoresHiddenTallPages();
  void activeTabShrinksAndInvalidates();
  void actualPagesNaturalFit_data();
  void actualPagesNaturalFit();
  void aliasListGrowthAndShrink();
  void actionTypesShowRelevantFieldsAndSave_data();
  void actionTypesShowRelevantFieldsAndSave();
  void recordingEscapeRestoresAndSaveIsGated();
  void accessibleRecordingActivation_data();
  void accessibleRecordingActivation();
  void actionRecordingBlocksSaveAndCancelPreservesDraft();
  void specifiedAllRecipientsRemainExplicitAfterReload();
  void settingsResetCancelAndSave();
  void tlsKeySizeRemainsDraftUntilSave();
  void tlsRestoreDefaultsRemainsDraft();
  void explicitRegenerationCommitsOnlyMatchingMetadata();
  void failedRegenerationPreservesStoredMetadata();
  void menusShareActionsAndRouteOnce_data();
  void menusShareActionsAndRouteOnce();
};

void LayoutAcceptanceTests::activePageHeightIgnoresHiddenTallPages()
{
  PageHost host;
  prepare(host, 600);
  auto *shortPage = syntheticPage(45);
  host.push(shortPage);
  fit(host, 600);
  const int shortHeight = host.preferredContentHeight(600);
  // Independent bound derives from the 45px body + one button row + gaps/inset.
  QVERIFY2(shortHeight < 160, "A short page must not inherit an arbitrary minimum viewport height");
  auto *tallPage = syntheticPage(470);
  host.push(tallPage);
  fit(host, 600);
  const int tallHeight = host.preferredContentHeight(600);
  QVERIFY(tallHeight > shortHeight + 350);
  tallPage->reject();
  fit(host, 600);
  QCOMPARE(host.preferredContentHeight(600), shortHeight);
  QCOMPARE(host.height(), shortHeight);
  shortPage->reject();
  settle();
}

void LayoutAcceptanceTests::activeTabShrinksAndInvalidates()
{
  PageHost host;
  prepare(host, 600);
  auto *editor = new QDialog;
  auto *layout = new QVBoxLayout(editor);
  auto *tabs = new QTabWidget;
  for (const auto height : {430, 40}) {
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    auto *label = new QLabel(QStringLiteral("Tab content"));
    label->setFixedHeight(height);
    pageLayout->addWidget(label);
    tabs->addTab(page, QString::number(height));
  }
  layout->addWidget(tabs);
  layout->addWidget(new QDialogButtonBox(QDialogButtonBox::Cancel));
  host.push(editor);
  fit(host, 600);
  const auto tall = host.height();
  QSignalSpy changed(&host, &PageHost::contentSizeChanged);
  tabs->setCurrentIndex(1);
  fit(host, 600);
  QVERIFY(changed.count() > 0);
  QVERIFY2(host.height() < tall - 300, "Hidden tall tab must not contribute natural size");
  QVERIFY(host.height() < 220);
  editor->reject();
  settle();
}

void LayoutAcceptanceTests::actualPagesNaturalFit_data()
{
  QTest::addColumn<QString>("kind");
  QTest::addColumn<QString>("language");
  QTest::addColumn<int>("width");
  const QStringList pages{QStringLiteral("server"),   QStringLiteral("client"),      QStringLiteral("preferences"),
                          QStringLiteral("computer"), QStringLiteral("hotkey"),      QStringLiteral("action"),
                          QStringLiteral("about"),    QStringLiteral("fingerprint"), QStringLiteral("update"),
                          QStringLiteral("message")};
  for (const auto &language : {QStringLiteral("en"), QStringLiteral("zh_CN")})
    for (const int width : {540, 600, 820})
      for (const auto &kind : pages)
        QTest::newRow(qPrintable(QStringLiteral("%1-%2-%3").arg(kind, language).arg(width)))
            << kind << language << width;
}

void LayoutAcceptanceTests::actualPagesNaturalFit()
{
  QFETCH(QString, kind);
  QFETCH(QString, language);
  QFETCH(int, width);
  const ScopedLanguage languageScope(language);
  QCOMPARE(I18N::currentLanguage(), language);
  QTranslator translator;
  if (language == QStringLiteral("zh_CN")) {
    QVERIFY(translator.load(QStringLiteral("synergy_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
  }
  synergy::gui::ServerConfig config;
  config.addClient(QStringLiteral("Studio"));
  config.addClient(QStringLiteral("Workstation"));
  synergy::gui::Screen screen(QStringLiteral("Studio"));
  synergy::gui::ScreenList screens;
  screens.append(screen);
  QVERIFY(!std::as_const(config).hotkeys().isEmpty());
  Hotkey hotkey = std::as_const(config).hotkeys().first();
  QVERIFY(!std::as_const(hotkey).actions().isEmpty());
  Action action = std::as_const(hotkey).actions().first();
  PageHost host;
  prepare(host, width);
  QDialog *editor = nullptr;
  if (kind == QStringLiteral("server"))
    editor = new ServerConfigDialog(nullptr, config);
  else if (kind == QStringLiteral("client"))
    editor = new ClientConfigDialog(nullptr);
  else if (kind == QStringLiteral("preferences"))
    editor = new SettingsDialog(nullptr, config);
  else if (kind == QStringLiteral("computer"))
    editor = new ScreenSettingsDialog(nullptr, &screen, &screens);
  else if (kind == QStringLiteral("hotkey"))
    editor = new HotkeyDialog(nullptr, hotkey);
  else if (kind == QStringLiteral("action"))
    editor = new ActionDialog(nullptr, config, hotkey, action);
  else if (kind == QStringLiteral("about"))
    editor = new AboutDialog(nullptr);
  else if (kind == QStringLiteral("fingerprint"))
    editor = new FingerprintDialog(nullptr, Fingerprint(QCryptographicHash::Sha256, QByteArray(32, '\x5a')));
  else if (kind == QStringLiteral("update")) {
    synergy::gui::update::UpdateInfo info;
    info.version = QStringLiteral("1.22.0");
    info.notes = QStringLiteral(
        "Realistic offline release notes with a long sentence that wraps at narrow widths.\nSecond release change."
    );
    info.asset.name = QStringLiteral("synergy_1.22.0_mac_arm64.dmg");
    editor = new synergy::gui::update::UpdateDialog(info, nullptr);
  } else {
    auto *message = new MessagePage(
        nullptr, MessagePage::Kind::Question, QStringLiteral("Confirm operation"),
        QStringLiteral("A realistic message explains the pending operation and its consequences before confirmation.")
    );
    message->addCheckBox(QStringLiteral("Do not ask again"));
    message->addButton(QStringLiteral("Cancel"));
    message->addButton(QStringLiteral("Continue"), true);
    editor = message;
  }
  host.push(editor);
  auto *tabs = editor->findChild<QTabWidget *>();
  auto *actionSelector =
      kind == QStringLiteral("action") ? editor->findChild<QComboBox *>(QStringLiteral("comboActionType")) : nullptr;
  const int count = actionSelector ? actionSelector->count() : (tabs ? tabs->count() : 1);
  for (int tab = 0; tab < count; ++tab) {
    if (tabs)
      tabs->setCurrentIndex(tab);
    if (actionSelector)
      actionSelector->setCurrentIndex(tab);
    fit(host, width);
    const int preferred = host.preferredContentHeight(width);
    QVERIFY(preferred > 0);
    QCOMPARE(host.width(), width);
    QCOMPARE(editor->layout()->contentsMargins(), QMargins());
    QVERIFY2(editor->width() <= width - 20, qPrintable(kind));
    for (auto *widget : editor->findChildren<QWidget *>()) {
      if (!widget->isVisibleTo(editor) ||
          (!qobject_cast<QLineEdit *>(widget) && !qobject_cast<QComboBox *>(widget) &&
           !qobject_cast<QAbstractButton *>(widget) && !qobject_cast<QAbstractSpinBox *>(widget)))
        continue;
      const QRect bounds(widget->mapTo(editor, QPoint()), widget->size());
      QVERIFY2(
          bounds.left() >= 0 && bounds.right() < editor->width(),
          qPrintable(kind + QStringLiteral(" clips ") + widget->objectName())
      );
    }
    for (auto *area : editor->findChildren<QScrollArea *>())
      if (area->isVisibleTo(editor))
        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
    for (auto *list : editor->findChildren<QListWidget *>()) {
      if (!list->isVisibleTo(editor))
        continue;
      const int row = list->count() ? list->sizeHintForRow(0) : list->fontMetrics().height() + 8;
      QVERIFY2(
          list->height() <= qMax(1, qMin(5, list->count())) * row + 12, "Lists must be row-sized, including empty lists"
      );
    }
    const auto directory =
        qEnvironmentVariable("SYNERGY_UI_EVIDENCE_DIR", QDir::temp().filePath(QStringLiteral("tupig-r2-layout")));
    QVERIFY(QDir().mkpath(directory));
    QVERIFY2(settleStaticRender(host), "Static capture did not settle within 1000ms");
    QPixmap image(host.size());
    image.fill(Qt::transparent);
    host.render(&image);
    QVERIFY(image.save(
        QDir(directory).filePath(QStringLiteral("%1-%2-%3-tab%4.png").arg(kind, language).arg(width).arg(tab))
    ));
    if (kind == QStringLiteral("client")) {
      const QList<QStringList> rows{
          {QStringLiteral("cbXScrollInvert"), QStringLiteral("lblXScrollSpeed"), QStringLiteral("sbXScrollScale")},
          {QStringLiteral("cbYScrollInvert"), QStringLiteral("lblScrollSpeed"), QStringLiteral("sbYScrollScale")}
      };
      for (const auto &row : rows) {
        auto *invert = editor->findChild<QCheckBox *>(row.at(0));
        auto *scaleLabel = editor->findChild<QLabel *>(row.at(1));
        auto *scale = editor->findChild<QAbstractSpinBox *>(row.at(2));
        QVERIFY(invert);
        QVERIFY(scaleLabel);
        QVERIFY(scale);
        invert->ensurePolished();
        QStyleOptionButton option;
        option.initFrom(invert);
        option.text = invert->text();
        const QRect contents = invert->style()->subElementRect(QStyle::SE_CheckBoxContents, &option, invert);
        const int renderedTextEnd =
            invert->mapTo(editor, contents.topLeft()).x() + invert->fontMetrics().horizontalAdvance(invert->text());
        const int labelStart = scaleLabel->mapTo(editor, QPoint()).x();
        const int gap = labelStart - renderedTextEnd;
        const QRect checkboxBounds(invert->mapTo(editor, QPoint()), invert->size());
        const QRect labelBounds(scaleLabel->mapTo(editor, QPoint()), scaleLabel->size());
        const QRect scaleBounds(scale->mapTo(editor, QPoint()), scale->size());
        qInfo() << "R2 client rendered gap" << row.at(0) << "fontPixels" << invert->font().pixelSize() << "textWidth"
                << invert->fontMetrics().horizontalAdvance(invert->text()) << "minimumWidth" << invert->minimumWidth()
                << "sizeHint" << invert->sizeHint() << "actualBounds" << checkboxBounds << "contents" << contents
                << "scaleLabel" << labelBounds << "scale" << scaleBounds << "gap" << gap;
        if (gap < 8) {
          const auto layoutItem = invert->style()->subElementRect(QStyle::SE_CheckBoxLayoutItem, &option, invert);
          auto *field = invert->parentWidget();
          qWarning() << "R2 client layout-item diagnostic" << row.at(0) << "styleLayoutItem" << layoutItem
                     << "useWidgetRect" << invert->testAttribute(Qt::WA_LayoutUsesWidgetRect) << "fieldMinimum"
                     << field->minimumWidth() << "fieldActual" << field->size() << "fieldHint" << field->sizeHint()
                     << "layoutGeometry" << field->layout()->geometry() << "layoutHint" << field->layout()->sizeHint()
                     << "spacing" << field->layout()->spacing() << "checkboxItemGeometry"
                     << field->layout()->itemAt(0)->geometry();
        }
        QVERIFY2(gap >= 8, qPrintable(row.at(0) + QStringLiteral(" rendered caption-to-Scale gap=%1px").arg(gap)));
        QVERIFY(checkboxBounds.right() < labelBounds.left());
        QVERIFY(labelBounds.right() < scaleBounds.left());
      }
    }
    if (kind == QStringLiteral("preferences")) {
      auto *certificateRow = editor->findChild<QWidget *>(QStringLiteral("certificateControls"));
      auto *keyLengthRow = editor->findChild<QWidget *>(QStringLiteral("keyLengthControls"));
      QVERIFY(certificateRow);
      QVERIFY(keyLengthRow);
      if (certificateRow->isVisibleTo(editor) && keyLengthRow->isVisibleTo(editor)) {
        const int certificateBottom = certificateRow->mapTo(editor, QPoint(0, certificateRow->height() - 1)).y();
        const int keyTop = keyLengthRow->mapTo(editor, QPoint()).y();
        const int gap = keyTop - certificateBottom - 1;
        QVERIFY2(
            gap >= 0 && gap <= 16, qPrintable(QStringLiteral("TLS certificate-to-key controls gap=%1px").arg(gap))
        );
      }
    }
    if (preferred <= 660) {
      auto *box = editor->findChild<QDialogButtonBox *>();
      if (box && box->isVisibleTo(editor)) {
        const int bottom = visibleContentBottom(*editor, box);
        const int gap = box->mapTo(editor, QPoint()).y() - bottom - 1;
        if (gap < 0 || gap > 24) {
          qWarning() << "R2 gap diagnostic" << kind << "state" << tab << "editor" << editor->geometry() << "buttonBox"
                     << box->geometry() << "bottom" << bottom;
          if (tabs)
            qWarning() << "tabs" << tabs->geometry() << "current" << tabs->currentWidget()->geometry() << "hint"
                       << tabs->currentWidget()->sizeHint();
          for (auto *field : editor->findChildren<QWidget *>())
            if (field->objectName().contains(QStringLiteral("Name"), Qt::CaseInsensitive) ||
                field->objectName().contains(QStringLiteral("Error"), Qt::CaseInsensitive))
              qWarning() << field->objectName() << "visible" << field->isVisibleTo(editor) << "bounds"
                         << QRect(field->mapTo(editor, QPoint()), field->size()) << "hint" << field->sizeHint()
                         << "text"
                         << (qobject_cast<QLabel *>(field) ? qobject_cast<QLabel *>(field)->text() : QString());
        }
        QVERIFY2(
            gap >= 0 && gap <= 24, qPrintable(QStringLiteral("%1 tab %2: action gap=%3px").arg(kind).arg(tab).arg(gap))
        );
      }
    }
  }
  editor->reject();
  settle();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void LayoutAcceptanceTests::aliasListGrowthAndShrink()
{
  synergy::gui::Screen screen(QStringLiteral("Studio"));
  synergy::gui::ScreenList screens;
  screens.append(screen);
  PageHost host;
  prepare(host, 600);
  auto *editor = new ScreenSettingsDialog(nullptr, &screen, &screens);
  host.push(editor);
  auto *list = editor->findChild<QListWidget *>(QStringLiteral("listAliases"));
  QVERIFY(list);
  auto *tabs = editor->findChild<QTabWidget *>();
  QVERIFY(tabs);
  for (int i = 0; i < tabs->count(); ++i) {
    tabs->setCurrentIndex(i);
    settle();
    if (list->isVisibleTo(editor))
      break;
  }
  QVERIFY(list->isVisibleTo(editor));
  fit(host, 600);
  const int empty = host.height();
  qInfo() << "R2 alias initial" << "host" << host.height() << "list" << list->height() << "frame" << list->frameWidth()
          << "fontHeight" << list->fontMetrics().height() << "pixelSize" << list->font().pixelSize();
  QSignalSpy changed(&host, &PageHost::contentSizeChanged);
  for (int i = 0; i < 4; ++i)
    list->addItem(QStringLiteral("studio-alias-%1").arg(i));
  fit(host, 600);
  QVERIFY(changed.count() > 0);
  QVERIFY(host.height() > empty + 30);
  list->clear();
  fit(host, 600);
  qInfo() << "R2 alias cleared" << "host" << host.height() << "list" << list->height() << "frame" << list->frameWidth()
          << "fontHeight" << list->fontMetrics().height() << "pixelSize" << list->font().pixelSize();
  QCOMPARE(host.height(), empty);
  editor->reject();
  settle();
  QVERIFY(std::as_const(screen).aliases().isEmpty());
}

void LayoutAcceptanceTests::actionTypesShowRelevantFieldsAndSave_data()
{
  QTest::addColumn<int>("type");
  for (int i = 0; i < 8; ++i)
    QTest::newRow(qPrintable(QStringLiteral("type-%1").arg(i))) << i;
}
void LayoutAcceptanceTests::actionTypesShowRelevantFieldsAndSave()
{
  QFETCH(int, type);
  synergy::gui::ServerConfig config;
  config.addClient(QStringLiteral("Studio"));
  config.addClient(QStringLiteral("Workstation"));
  Hotkey hotkey;
  Action action;
  ActionDialog editor(nullptr, config, hotkey, action);
  editor.setAttribute(Qt::WA_DontShowOnScreen);
  editor.show();
  auto *selector = editor.findChild<QComboBox *>(QStringLiteral("comboActionType"));
  QVERIFY(selector);
  QCOMPARE(selector->count(), 8);
  // R3 semantic lineage: display order changed; R2 field/geometry contract remains.
  selector->setCurrentIndex(selector->findData(type));
  settle();
  auto *keys = editor.findChild<KeySequenceWidget *>();
  QVERIFY(keys);
  QCOMPARE(keys->isVisibleTo(&editor), type < 3);
  for (const auto &pair :
       {qMakePair(QStringLiteral("comboSwitchToScreen"), 3), qMakePair(QStringLiteral("comboSwitchInDirection"), 4),
        qMakePair(QStringLiteral("comboLockCursorToScreen"), 6)}) {
    auto *field = editor.findChild<QComboBox *>(pair.first);
    QVERIFY(field);
    QCOMPARE(field->isVisibleTo(&editor), type == pair.second);
  }
  auto *recipients = editor.findChild<QComboBox *>(QStringLiteral("comboRecipients"));
  QVERIFY2(recipients, "The backend supports All/Specified recipients only");
  QCOMPARE(recipients->count(), 2);
  if (type < 3) {
    KeySequence sequence;
    QVERIFY(sequence.appendKey(Qt::Key_K, Qt::ControlModifier));
    keys->setKeySequence(sequence);
    QMetaObject::invokeMethod(keys, "keySequenceChanged", Qt::DirectConnection);
    recipients->setCurrentIndex(1);
    auto *list = editor.findChild<QListWidget *>(QStringLiteral("listScreens"));
    QVERIFY(list);
    QVERIFY(list->count() >= 2);
    for (int i = 0; i < list->count(); ++i)
      list->item(i)->setCheckState(i == 0 ? Qt::Checked : Qt::Unchecked);
  }
  auto *save = saveButton(editor);
  QVERIFY(save);
  QVERIFY(save->isEnabled());
  save->click();
  QCOMPARE(editor.result(), int(QDialog::Accepted));
  QCOMPARE(action.type(), type);
  if (type < 3)
    QCOMPARE(std::as_const(action).typeScreenNames().size(), 1);
  if (type == 7)
    QVERIFY(action.restartServer());
}

void LayoutAcceptanceTests::recordingEscapeRestoresAndSaveIsGated()
{
  Hotkey hotkey;
  HotkeyDialog editor(nullptr, hotkey);
  editor.setAttribute(Qt::WA_DontShowOnScreen);
  editor.show();
  settle();
  auto *keys = editor.findChild<KeySequenceWidget *>();
  QVERIFY(keys);
  KeySequence original;
  QVERIFY(original.appendKey(Qt::Key_K, Qt::ControlModifier));
  keys->setKeySequence(original);
  QMetaObject::invokeMethod(keys, "keySequenceChanged", Qt::DirectConnection);
  auto *save = saveButton(editor);
  QVERIFY(save);
  QVERIFY(save->isEnabled());
  QSignalSpy recording(keys, &KeySequenceWidget::recordingChanged);
  QTest::mouseClick(keys, Qt::LeftButton);
  QVERIFY(keys->isRecording());
  QVERIFY(!save->isEnabled());
  QTest::keyClick(keys, Qt::Key_Escape);
  QVERIFY(!keys->isRecording());
  QCOMPARE(keys->keySequence(), original);
  QVERIFY(save->isEnabled());
  QCOMPARE(recording.count(), 2);
  QCOMPARE(recording.at(0).at(0).toBool(), true);
  QCOMPARE(recording.at(1).at(0).toBool(), false);
  const auto completeText = keys->text();
  QTest::mouseClick(keys, Qt::LeftButton);
  QVERIFY(keys->isRecording());
  QVERIFY(!save->isEnabled());
  QFocusEvent focusOut(QEvent::FocusOut);
  QCoreApplication::sendEvent(keys, &focusOut);
  QVERIFY(!keys->isRecording());
  QCOMPARE(keys->keySequence(), original);
  QCOMPARE(keys->text(), completeText);
  QVERIFY(save->isEnabled());
  // A successful capture becomes the backup for the next capture/cancel cycle.
  QTest::mouseClick(keys, Qt::LeftButton);
  QTest::keyClick(keys, Qt::Key_L, Qt::AltModifier);
  QVERIFY(!keys->isRecording());
  const auto latestSequence = keys->keySequence();
  QVERIFY(latestSequence.valid());
  QVERIFY(latestSequence != original);
  const auto latestText = keys->text();
  QTest::mouseClick(keys, Qt::LeftButton);
  QTest::keyClick(keys, Qt::Key_Escape);
  QVERIFY(!keys->isRecording());
  QCOMPARE(keys->keySequence(), latestSequence);
  QCOMPARE(keys->text(), latestText);
  QVERIFY(save->isEnabled());
  save->click();
  QCOMPARE(editor.result(), int(QDialog::Accepted));
  QCOMPARE(std::as_const(hotkey).keySequence(), latestSequence);
  {
    Hotkey mouseHotkey;
    HotkeyDialog mouseEditor(nullptr, mouseHotkey);
    mouseEditor.setAttribute(Qt::WA_DontShowOnScreen);
    mouseEditor.show();
    settle();
    auto *mouseKeys = mouseEditor.findChild<KeySequenceWidget *>();
    QVERIFY(mouseKeys);
    mouseKeys->setKeySequence(original);
    QMetaObject::invokeMethod(mouseKeys, "keySequenceChanged", Qt::DirectConnection);
    auto *mouseSave = saveButton(mouseEditor);
    QVERIFY(mouseSave);
    QSignalSpy mouseRecording(mouseKeys, &KeySequenceWidget::recordingChanged);
    QSignalSpy mouseAccepted(&mouseEditor, &QDialog::accepted);
    QTest::mouseClick(mouseKeys, Qt::LeftButton);
    QVERIFY(mouseKeys->isRecording());
    QVERIFY(!mouseSave->isEnabled());
    QTest::mouseClick(mouseKeys, Qt::RightButton);
    QVERIFY(!mouseKeys->isRecording());
    QVERIFY(mouseSave->isEnabled());
    KeySequence expectedMouse;
    QVERIFY(expectedMouse.appendMouseButton(Qt::RightButton));
    QCOMPARE(mouseKeys->keySequence(), expectedMouse);
    QCOMPARE(mouseRecording.count(), 2);
    QCOMPARE(mouseRecording.at(0).at(0).toBool(), true);
    QCOMPARE(mouseRecording.at(1).at(0).toBool(), false);
    QCOMPARE(mouseAccepted.count(), 0);
    mouseEditor.reject();
  }
}

void LayoutAcceptanceTests::accessibleRecordingActivation_data()
{
  QTest::addColumn<QString>("activation");
  QTest::newRow("accessible-click") << QStringLiteral("click");
  QTest::newRow("keyboard-space") << QStringLiteral("space");
  QTest::newRow("keyboard-return") << QStringLiteral("return");
  QTest::newRow("keyboard-enter") << QStringLiteral("enter");
}

void LayoutAcceptanceTests::accessibleRecordingActivation()
{
  QFETCH(QString, activation);
  Hotkey hotkey;
  HotkeyDialog editor(nullptr, hotkey);
  editor.setAttribute(Qt::WA_DontShowOnScreen);
  editor.show();
  settle();
  auto *keys = editor.findChild<KeySequenceWidget *>();
  QVERIFY(keys);
  KeySequence original;
  QVERIFY(original.appendKey(Qt::Key_K, Qt::ControlModifier));
  keys->setKeySequence(original);
  QMetaObject::invokeMethod(keys, "keySequenceChanged", Qt::DirectConnection);
  auto *save = saveButton(editor);
  QVERIFY(save);
  QVERIFY(save->isEnabled());
  keys->setEnabled(false);
  keys->click();
  QVERIFY(!keys->isRecording());
  QCOMPARE(keys->keySequence(), original);
  keys->setEnabled(true);
  qInfo() << "R2 recording focus policy" << keys->focusPolicy();
  QSignalSpy recording(keys, &KeySequenceWidget::recordingChanged);
  QSignalSpy accepted(&editor, &QDialog::accepted);
  if (activation == QStringLiteral("click"))
    keys->click();
  else if (activation == QStringLiteral("space"))
    QTest::keyClick(keys, Qt::Key_Space);
  else if (activation == QStringLiteral("return"))
    QTest::keyClick(keys, Qt::Key_Return);
  else
    QTest::keyClick(keys, Qt::Key_Enter);
  QCOMPARE(accepted.count(), 0);
  QVERIFY2(keys->isRecording(), "Accessible click and keyboard activation must start capture");
  QVERIFY2(keys->focusPolicy() & Qt::TabFocus, "Recording must be reachable through keyboard focus");
  QVERIFY(!save->isEnabled());
  QCOMPARE(keys->backupSequence(), original);
  QVERIFY2(!keys->keySequence().valid(), "Activation key must not become the captured shortcut");
  QCOMPARE(recording.count(), 1);
  QTest::keyClick(keys, Qt::Key_Escape);
  QVERIFY(!keys->isRecording());
  QCOMPARE(keys->keySequence(), original);
  QVERIFY(save->isEnabled());
  QCOMPARE(recording.count(), 2);
  editor.reject();
}

void LayoutAcceptanceTests::actionRecordingBlocksSaveAndCancelPreservesDraft()
{
  synergy::gui::ServerConfig config;
  config.addClient(QStringLiteral("Studio"));
  Hotkey hotkey;
  Action action;
  QTemporaryDir actionDirectory;
  QSettings actionSettings(actionDirectory.filePath(QStringLiteral("action.ini")), QSettings::IniFormat);
  action.loadSettings(actionSettings);
  const auto original = action;
  ActionDialog editor(nullptr, config, hotkey, action);
  editor.setAttribute(Qt::WA_DontShowOnScreen);
  editor.show();
  settle();
  auto *keys = editor.findChild<KeySequenceWidget *>();
  QVERIFY(keys);
  KeySequence sequence;
  QVERIFY(sequence.appendKey(Qt::Key_J, Qt::AltModifier));
  keys->setKeySequence(sequence);
  QMetaObject::invokeMethod(keys, "keySequenceChanged", Qt::DirectConnection);
  auto *save = saveButton(editor);
  QVERIFY(save);
  QVERIFY(save->isEnabled());
  QTest::mouseClick(keys, Qt::LeftButton);
  QVERIFY(keys->isRecording());
  QVERIFY(!save->isEnabled());
  QTest::keyClick(keys, Qt::Key_Escape);
  QVERIFY(!keys->isRecording());
  QCOMPARE(keys->keySequence(), sequence);
  editor.reject();
  QCOMPARE(action, original);
}

void LayoutAcceptanceTests::specifiedAllRecipientsRemainExplicitAfterReload()
{
  synergy::gui::ServerConfig config;
  config.addClient(QStringLiteral("Studio"));
  config.addClient(QStringLiteral("Workstation"));
  Hotkey hotkey;
  Action action;
  QStringList selectedNames;
  {
    ActionDialog editor(nullptr, config, hotkey, action);
    auto *keys = editor.findChild<KeySequenceWidget *>();
    QVERIFY(keys);
    KeySequence sequence;
    QVERIFY(sequence.appendKey(Qt::Key_K, Qt::ControlModifier));
    keys->setKeySequence(sequence);
    QMetaObject::invokeMethod(keys, "keySequenceChanged", Qt::DirectConnection);
    auto *recipients = editor.findChild<QComboBox *>(QStringLiteral("comboRecipients"));
    QVERIFY(recipients);
    recipients->setCurrentIndex(1);
    auto *list = editor.findChild<QListWidget *>(QStringLiteral("listScreens"));
    QVERIFY(list);
    QVERIFY(list->count() >= 2);
    for (int i = 0; i < list->count(); ++i) {
      list->item(i)->setCheckState(Qt::Checked);
      selectedNames << list->item(i)->text();
    }
    auto *save = saveButton(editor);
    QVERIFY(save);
    QVERIFY(save->isEnabled());
    save->click();
    QCOMPARE(editor.result(), int(QDialog::Accepted));
  }
  QCOMPARE(std::as_const(action).typeScreenNames(), selectedNames);
  QVERIFY(action.haveScreens());
  QVERIFY(!action.text().contains(QStringLiteral(",*")));
  QTemporaryDir saved;
  QSettings settings(saved.filePath(QStringLiteral("action.ini")), QSettings::IniFormat);
  action.saveSettings(settings);
  settings.sync();
  Action reloaded;
  reloaded.loadSettings(settings);
  QCOMPARE(std::as_const(reloaded).typeScreenNames(), selectedNames);
  {
    ActionDialog editor(nullptr, config, hotkey, reloaded);
    auto *recipients = editor.findChild<QComboBox *>(QStringLiteral("comboRecipients"));
    QVERIFY(recipients);
    QCOMPARE(recipients->currentIndex(), 1);
    editor.reject();
  }
  config.addClient(QStringLiteral("Future-Computer"));
  {
    ActionDialog editor(nullptr, config, hotkey, reloaded);
    auto *list = editor.findChild<QListWidget *>(QStringLiteral("listScreens"));
    QVERIFY(list);
    bool foundFuture = false;
    for (int i = 0; i < list->count(); ++i) {
      auto *item = list->item(i);
      if (item->text() == QStringLiteral("Future-Computer")) {
        foundFuture = true;
        QCOMPARE(item->checkState(), Qt::Unchecked);
      } else {
        QVERIFY(selectedNames.contains(item->text()));
        QCOMPARE(item->checkState(), Qt::Checked);
      }
    }
    QVERIFY(foundFuture);
    auto *recipients = editor.findChild<QComboBox *>(QStringLiteral("comboRecipients"));
    QVERIFY(recipients);
    QCOMPARE(recipients->currentIndex(), 1);
    recipients->setCurrentIndex(0);
    auto *save = saveButton(editor);
    QVERIFY(save);
    QVERIFY(save->isEnabled());
    save->click();
  }
  QVERIFY(std::as_const(reloaded).typeScreenNames().isEmpty());
  QVERIFY(reloaded.text().contains(QStringLiteral(",*")));
  {
    ActionDialog editor(nullptr, config, hotkey, reloaded);
    auto *recipients = editor.findChild<QComboBox *>(QStringLiteral("comboRecipients"));
    QVERIFY(recipients);
    QCOMPARE(recipients->currentIndex(), 0);
    editor.reject();
  }
}

void LayoutAcceptanceTests::settingsResetCancelAndSave()
{
  const int storedPort = 25252;
  Settings::setValue(Settings::Core::Port, storedPort);
  synergy::gui::ServerConfig config;
  {
    SettingsDialog editor(nullptr, config);
    auto *port = editor.findChild<QSpinBox *>(QStringLiteral("sbPort"));
    QVERIFY(port);
    auto *box = editor.findChild<QDialogButtonBox *>();
    QVERIFY(box);
    QCOMPARE(port->value(), storedPort);
    port->setValue(storedPort + 1);
    QVERIFY(box->button(QDialogButtonBox::Reset)->isEnabled());
    box->button(QDialogButtonBox::Reset)->click();
    QCOMPARE(port->value(), storedPort);
    if (box->button(QDialogButtonBox::Save)->isEnabled())
      reportSettingsMismatches(editor);
    QVERIFY(!box->button(QDialogButtonBox::Save)->isEnabled());
    auto *level = editor.findChild<QComboBox *>(QStringLiteral("comboLogLevel"));
    QVERIFY(level);
    QVERIFY(level->count() > 1);
    const int storedLevel = level->currentIndex();
    level->setCurrentIndex((storedLevel + 1) % level->count());
    QVERIFY(box->button(QDialogButtonBox::Save)->isEnabled());
    box->button(QDialogButtonBox::Reset)->click();
    QCOMPARE(level->currentIndex(), storedLevel);
    QVERIFY(!box->button(QDialogButtonBox::Save)->isEnabled());
    port->setValue(storedPort + 2);
    editor.reject();
    QCOMPARE(Settings::value(Settings::Core::Port).toInt(), storedPort);
  }
  {
    SettingsDialog editor(nullptr, config);
    auto *port = editor.findChild<QSpinBox *>(QStringLiteral("sbPort"));
    QVERIFY(port);
    auto *box = editor.findChild<QDialogButtonBox *>();
    QVERIFY(box);
    box->button(QDialogButtonBox::RestoreDefaults)->click();
    QCOMPARE(port->value(), Settings::defaultValue(Settings::Core::Port).toInt());
    QCOMPARE(Settings::value(Settings::Core::Port).toInt(), storedPort);
    port->setValue(storedPort + 3);
    box->button(QDialogButtonBox::Save)->click();
    QCOMPARE(editor.result(), int(QDialog::Accepted));
    QCOMPARE(Settings::value(Settings::Core::Port).toInt(), storedPort + 3);
  }
}

void LayoutAcceptanceTests::tlsKeySizeRemainsDraftUntilSave()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const ScopedSetting certificate(Settings::Security::Certificate, temporary.filePath(QStringLiteral("private.pem")));
  const ScopedSetting keySize(Settings::Security::KeySize, 2048);
  const ScopedSetting tls(Settings::Security::TlsEnabled, true);
  const ScopedLanguage language(QStringLiteral("en"));
  synergy::gui::ServerConfig config;
  {
    SettingsDialog editor(nullptr, config);
    auto *combo = editor.findChild<QComboBox *>(QStringLiteral("comboTlsKeyLength"));
    QVERIFY(combo);
    auto *box = editor.findChild<QDialogButtonBox *>();
    QVERIFY(box);
    combo->setCurrentText(QStringLiteral("4096"));
    QCOMPARE(Settings::value(Settings::Security::KeySize).toInt(), 2048);
    QVERIFY(box->button(QDialogButtonBox::Save)->isEnabled());
    auto *security = editor.findChild<QGroupBox *>(QStringLiteral("groupSecurity"));
    QVERIFY(security);
    security->setChecked(false);
    security->setChecked(true);
    QCOMPARE(combo->currentText(), QStringLiteral("4096"));
    auto *service = editor.findChild<QGroupBox *>(QStringLiteral("groupService"));
    QVERIFY(service);
    service->setChecked(!service->isChecked());
    QCOMPARE(combo->currentText(), QStringLiteral("4096"));
    box->button(QDialogButtonBox::Reset)->click();
    QCOMPARE(combo->currentText(), QStringLiteral("2048"));
    QCOMPARE(Settings::value(Settings::Security::KeySize).toInt(), 2048);
    combo->setCurrentText(QStringLiteral("4096"));
    editor.reject();
    QCOMPARE(Settings::value(Settings::Security::KeySize).toInt(), 2048);
  }
  {
    SettingsDialog editor(nullptr, config);
    auto *combo = editor.findChild<QComboBox *>(QStringLiteral("comboTlsKeyLength"));
    QVERIFY(combo);
    combo->setCurrentText(QStringLiteral("4096"));
    auto *save = saveButton(editor);
    QVERIFY(save);
    QVERIFY(save->isEnabled());
    save->click();
    QCOMPARE(editor.result(), int(QDialog::Accepted));
    QCOMPARE(Settings::value(Settings::Security::KeySize).toInt(), 4096);
  }
}

void LayoutAcceptanceTests::tlsRestoreDefaultsRemainsDraft()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto originalCertificate = temporary.filePath(QStringLiteral("private.pem"));
  const ScopedSetting certificate(Settings::Security::Certificate, originalCertificate);
  const ScopedSetting keySize(Settings::Security::KeySize, 4096);
  const ScopedSetting tls(Settings::Security::TlsEnabled, false);
  const ScopedSetting peers(
      Settings::Security::CheckPeers, !Settings::defaultValue(Settings::Security::CheckPeers).toBool()
  );
  const ScopedLanguage language(QStringLiteral("en"));
  synergy::gui::ServerConfig config;
  SettingsDialog editor(nullptr, config);
  auto *box = editor.findChild<QDialogButtonBox *>();
  QVERIFY(box);
  QVERIFY(box->button(QDialogButtonBox::RestoreDefaults)->isEnabled());
  box->button(QDialogButtonBox::RestoreDefaults)->click();
  QCOMPARE(
      editor.findChild<QComboBox *>(QStringLiteral("comboTlsKeyLength"))->currentText(),
      Settings::defaultValue(Settings::Security::KeySize).toString()
  );
  QCOMPARE(
      editor.findChild<QGroupBox *>(QStringLiteral("groupSecurity"))->isChecked(),
      Settings::defaultValue(Settings::Security::TlsEnabled).toBool()
  );
  QCOMPARE(
      editor.findChild<QCheckBox *>(QStringLiteral("cbRequireClientCert"))->isChecked(),
      Settings::defaultValue(Settings::Security::CheckPeers).toBool()
  );
  QCOMPARE(
      editor.findChild<QLineEdit *>(QStringLiteral("lineTlsCertPath"))->text(),
      Settings::defaultValue(Settings::Security::Certificate).toString()
  );
  QCOMPARE(Settings::value(Settings::Security::KeySize).toInt(), 4096);
  QCOMPARE(Settings::value(Settings::Security::Certificate).toString(), originalCertificate);
  QVERIFY(!Settings::value(Settings::Security::TlsEnabled).toBool());
  editor.reject();
}

void LayoutAcceptanceTests::explicitRegenerationCommitsOnlyMatchingMetadata()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto storedPath = temporary.filePath(QStringLiteral("stored/private.pem"));
  const auto draftPath = temporary.filePath(QStringLiteral("untouched-draft.pem"));
  const ScopedSetting certificate(Settings::Security::Certificate, storedPath);
  const ScopedSetting keySize(Settings::Security::KeySize, 2048);
  const ScopedSetting tls(Settings::Security::TlsEnabled, true);
  const ScopedLanguage language(QStringLiteral("en"));
  QVERIFY(synergy::gui::TlsUtility::generateCertificateAt(storedPath, 2048));
  QCOMPARE(synergy::gui::TlsUtility::getCertKeyLength(storedPath), 2048);
  const bool storedSleep = Settings::value(Settings::Core::PreventSleep).toBool();
  synergy::gui::ServerConfig config;
  PageHost host;
  prepare(host, 600);
  auto *editor = new SettingsDialog(nullptr, config);
  host.push(editor);
  auto *combo = editor->findChild<QComboBox *>(QStringLiteral("comboTlsKeyLength"));
  QVERIFY(combo);
  combo->setCurrentText(QStringLiteral("4096"));
  editor->findChild<QLineEdit *>(QStringLiteral("lineTlsCertPath"))->setText(draftPath);
  editor->findChild<QCheckBox *>(QStringLiteral("cbPreventSleep"))->setChecked(!storedSleep);
  auto *regenerate = editor->findChild<QPushButton *>(QStringLiteral("btnTlsRegenCert"));
  QVERIFY(regenerate);
  QVERIFY(regenerate->isEnabled());
  regenerate->click();
  settle();
  QCOMPARE(synergy::gui::TlsUtility::getCertKeyLength(storedPath), 4096);
  QCOMPARE(Settings::value(Settings::Security::KeySize).toInt(), 4096);
  QCOMPARE(Settings::value(Settings::Security::Certificate).toString(), storedPath);
  QCOMPARE(Settings::value(Settings::Core::PreventSleep).toBool(), storedSleep);
  QVERIFY(!QFile::exists(draftPath));
  auto *notice = host.findChild<QMessageBox *>();
  QVERIFY(notice);
  notice->accept();
  settle();
  editor->reject();
  settle();
  QCOMPARE(Settings::value(Settings::Security::KeySize).toInt(), 4096);
  QVERIFY(synergy::gui::TlsUtility::isCertValid(storedPath));
}

void LayoutAcceptanceTests::failedRegenerationPreservesStoredMetadata()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QFile blockedParent(temporary.filePath(QStringLiteral("not-a-directory")));
  QVERIFY(blockedParent.open(QIODevice::WriteOnly));
  blockedParent.write("synthetic fixture");
  blockedParent.close();
  const auto blockedPath = blockedParent.fileName() + QStringLiteral("/private.pem");
  const ScopedSetting certificate(Settings::Security::Certificate, blockedPath);
  const ScopedSetting keySize(Settings::Security::KeySize, 2048);
  const ScopedSetting tls(Settings::Security::TlsEnabled, true);
  const ScopedLanguage language(QStringLiteral("en"));
  synergy::gui::ServerConfig config;
  PageHost host;
  prepare(host, 600);
  auto *editor = new SettingsDialog(nullptr, config);
  host.push(editor);
  auto *combo = editor->findChild<QComboBox *>(QStringLiteral("comboTlsKeyLength"));
  QVERIFY(combo);
  combo->setCurrentText(QStringLiteral("4096"));
  auto *regenerate = editor->findChild<QPushButton *>(QStringLiteral("btnTlsRegenCert"));
  QVERIFY(regenerate);
  regenerate->click();
  settle();
  QCOMPARE(Settings::value(Settings::Security::KeySize).toInt(), 2048);
  QCOMPARE(Settings::value(Settings::Security::Certificate).toString(), blockedPath);
  QVERIFY(!QFile::exists(blockedPath));
  QVERIFY(host.findChild<QMessageBox *>() == nullptr);
  editor->reject();
  settle();
}

void LayoutAcceptanceTests::menusShareActionsAndRouteOnce_data()
{
  QTest::addColumn<bool>("mac");
  QTest::newRow("macOS") << true;
  QTest::newRow("WindowsLinux") << false;
}
void LayoutAcceptanceTests::menusShareActionsAndRouteOnce()
{
  QFETCH(bool, mac);
  using namespace synergy::gui;
  const ScopedLanguage languageScope(QStringLiteral("en"));
  QWidget owner;
  QMenuBar bar(&owner);
  QMenu tray(&owner);
  AppMenuActions bundle;
  QList<QAction *> all;
  auto action = [&](const QString &name) {
    auto *a = new QAction(name, &owner);
    a->setObjectName(name);
    all << a;
    return a;
  };
  bundle.open = action(QStringLiteral("open"));
  bundle.start = action(QStringLiteral("start"));
  bundle.restart = action(QStringLiteral("restart"));
  bundle.stop = action(QStringLiteral("stop"));
  bundle.quit = action(QStringLiteral("quit"));
  bundle.settings = action(QStringLiteral("settings"));
  bundle.clearSettings = action(QStringLiteral("clear"));
  bundle.updates = action(QStringLiteral("updates"));
  bundle.about = action(QStringLiteral("about"));
  bundle.log = action(QStringLiteral("log"));
  bundle.help = action(QStringLiteral("help"));
  populateMainMenus(&bar, bundle, mac ? MenuPlatform::MacOS : MenuPlatform::WindowsLinux);
  populateTrayMenu(&tray, bundle);
  // UI-019 lineage: NoRole tray proxy retains the same Quit command in position seven.
  QCOMPARE(menuNames(tray.actions()), QStringList({"open", "|", "start", "restart", "stop", "|", "trayQuitAction"}));
  QStringList titles;
  QSet<QAction *> menuActions;
  for (auto *top : bar.actions()) {
    QVERIFY(top->menu());
    titles << top->text().remove(QLatin1Char('&'));
    for (auto *a : top->menu()->actions())
      if (!a->isSeparator())
        menuActions.insert(a);
  }
  if (mac)
    QCOMPARE(titles, QStringList({"TuPig Synergy", "File", "View", "Support"}));
  else
    QCOMPARE(titles, QStringList({"File", "Edit", "View", "Help"}));
  auto *file = bar.findChild<QMenu *>(QStringLiteral("menuFile"));
  auto *view = bar.findChild<QMenu *>(QStringLiteral("menuView"));
  auto *help = bar.findChild<QMenu *>(QStringLiteral("menuHelp"));
  QVERIFY(file);
  QVERIFY(view);
  QVERIFY(help);
  QCOMPARE(menuNames(view->actions()), QStringList({"log"}));
  if (mac) {
    auto *application = bar.findChild<QMenu *>(QStringLiteral("menuApplication"));
    QVERIFY(application);
    QCOMPARE(
        menuNames(application->actions()), QStringList({"about", "updates", "|", "settings", "clear", "|", "quit"})
    );
    QCOMPARE(menuNames(file->actions()), QStringList({"start", "restart", "stop"}));
    QCOMPARE(menuNames(help->actions()), QStringList({"help"}));
    QVERIFY(bar.findChild<QMenu *>(QStringLiteral("menuEdit")) == nullptr);
  } else {
    auto *edit = bar.findChild<QMenu *>(QStringLiteral("menuEdit"));
    QVERIFY(edit);
    QCOMPARE(menuNames(file->actions()), QStringList({"start", "restart", "stop", "|", "quit"}));
    QCOMPARE(menuNames(edit->actions()), QStringList({"settings"}));
    QCOMPARE(menuNames(help->actions()), QStringList({"updates", "help", "|", "clear", "|", "about"}));
    QVERIFY(bar.findChild<QMenu *>(QStringLiteral("menuApplication")) == nullptr);
  }
  for (auto *a :
       {bundle.start, bundle.restart, bundle.stop, bundle.settings, bundle.updates, bundle.about, bundle.log,
        bundle.help, bundle.quit})
    QVERIFY(menuActions.contains(a));
  QCOMPARE(bundle.about->menuRole(), QAction::AboutRole);
  QCOMPARE(bundle.settings->menuRole(), QAction::PreferencesRole);
  QCOMPARE(bundle.quit->menuRole(), QAction::QuitRole);
  // Qt logical Control maps to Command in native macOS menus.
  const auto modifier = Qt::CTRL;
  QCOMPARE(bundle.updates->shortcut(), QKeySequence(modifier | Qt::Key_U));
  QCOMPARE(bundle.settings->shortcut(), QKeySequence(modifier | Qt::Key_Comma));
  QCOMPARE(bundle.log->shortcut(), QKeySequence(modifier | Qt::Key_J));
  QSet<QString> shortcuts;
  for (auto *a : all)
    for (const auto &key : a->shortcuts()) {
      if (key.isEmpty())
        continue;
      QVERIFY2(!shortcuts.contains(key.toString()), "Duplicate active application shortcut");
      shortcuts.insert(key.toString());
    }
  for (auto *a : {bundle.start, bundle.restart, bundle.stop})
    QVERIFY(!a->shortcut().isEmpty());
  QSignalSpy routed(bundle.start, &QAction::triggered);
  bundle.start->setEnabled(false);
  bundle.restart->setEnabled(true);
  bundle.stop->setEnabled(true);
  QVERIFY(!tray.actions().at(2)->isEnabled());
  QVERIFY(tray.actions().at(3)->isEnabled());
  QVERIFY(tray.actions().first()->isEnabled());
  bundle.start->trigger();
  QCOMPARE(routed.count(), 0);
  bundle.start->setEnabled(true);
  tray.actions().at(2)->trigger();
  QCOMPARE(routed.count(), 1);
  // Rebuilding presentation must not duplicate routing or QAction objects.
  populateMainMenus(&bar, bundle, mac ? MenuPlatform::MacOS : MenuPlatform::WindowsLinux);
  populateTrayMenu(&tray, bundle);
  tray.actions().at(2)->trigger();
  QCOMPARE(routed.count(), 2);
}

int main(int argc, char **argv)
{
  QTemporaryDir isolated;
  if (!isolated.isValid())
    return 1;
  // COMPLETE-R1 isolation lineage: preserve all inherited assertions and fixtures.
  QString isolationError;
  if (!synergy::runtime::configureIsolatedRuntime(
          {QFileInfo(isolated.path()).canonicalFilePath(),
           "ts-complete-" + QUuid::createUuid().toString(QUuid::WithoutBraces)},
          &isolationError
      ))
    return 2;
  qputenv("XDG_CONFIG_HOME", isolated.path().toUtf8());
  qputenv("XDG_STATE_HOME", isolated.path().toUtf8());
  QApplication app(argc, argv);
  Settings::setSettingsFile(isolated.filePath(QStringLiteral("test.conf")));
  Settings::setStateFile(isolated.filePath(QStringLiteral("test.state")));
  Settings::setValue(Settings::Core::ComputerName, QStringLiteral("Test-Server"));
  Settings::setValue(Settings::Core::Language, QStringLiteral("en"));
  auto &fixture = Settings::proxy().get();
  fixture.beginGroup(QStringLiteral("internalConfig"));
  fixture.beginWriteArray(QStringLiteral("hotkeys"), 1);
  fixture.setArrayIndex(0);
  KeySequence hotkeySequence;
  hotkeySequence.appendKey(Qt::Key_K, Qt::ControlModifier);
  hotkeySequence.saveSettings(fixture);
  fixture.beginWriteArray(QStringLiteral("actions"), 1);
  fixture.setArrayIndex(0);
  KeySequence actionSequence;
  actionSequence.appendKey(Qt::Key_J, Qt::AltModifier);
  actionSequence.saveSettings(fixture);
  fixture.setValue(SettingsKeys::ActionType, int(Action::Type::switchInDirection));
  fixture.setValue(SettingsKeys::SwitchDirection, int(Action::SwitchDirection::right));
  fixture.setValue(SettingsKeys::RestartServer, false);
  fixture.endArray();
  fixture.endArray();
  fixture.endGroup();
  fixture.sync();
  app.setStyleSheet(synergy::gui::appStyleSheet());
  LayoutAcceptanceTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "LayoutAcceptanceTests.moc"
