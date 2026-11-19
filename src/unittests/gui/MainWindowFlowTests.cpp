/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "UiRenderTestSupport.h"
#include "common/Constants.h"
#include "common/I18N.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/AppStyle.h"
#include "gui/Hotkey.h"
#include "gui/KeySequence.h"
#include "gui/MainWindow.h"
#include "gui/ScreenSetupModel.h"
#include "gui/TlsUtility.h"
#include "gui/dialogs/ActionDialog.h"
#include "gui/dialogs/FingerprintDialog.h"
#include "gui/dialogs/HotkeyDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/update/UpdateChecker.h"
#include "gui/update/UpdateDialog.h"
#include "gui/update/UpdateDownloader.h"
#include "gui/widgets/MessagePage.h"
#include "gui/widgets/ModeCard.h"
#include "gui/widgets/ScreenSetupView.h"
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <utility>

// Same observer-only seam as R2UiPreview: no production migration hook runs.
namespace synergy::gui::migration {
void showNoticeIfPending(QWidget *)
{
}
} // namespace synergy::gui::migration

namespace {
class ControlledHttp final : public QTcpServer
{
public:
  ControlledHttp()
  {
    listen(QHostAddress::LocalHost, 0);
    connect(this, &QTcpServer::newConnection, this, [this] {
      while (hasPendingConnections()) {
        auto *socket = nextPendingConnection();
        peers.append(socket);
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
          socket->readAll();
          if (!socket->property("requestCounted").toBool()) {
            socket->setProperty("requestCounted", true);
            ++requests;
          }
        });
      }
    });
  }
  QUrl url() const
  {
    return QUrl(QStringLiteral("http://127.0.0.1:%1/fixture").arg(serverPort()));
  }
  void write(const QByteArray &bytes, bool finish = false)
  {
    for (const auto &socket : std::as_const(peers))
      if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->write(bytes);
        socket->flush();
        if (finish)
          socket->disconnectFromHost();
      }
  }
  int requests = 0;
  QList<QPointer<QTcpSocket>> peers;
};

class IsolatedWindow final : public MainWindow
{
public:
  explicit IsolatedWindow(ControlledHttp &http, int width, bool showOnScreen = false)
      : MainWindow("ts-flow-" + QUuid::createUuid().toString(QUuid::WithoutBraces)),
        server(http)
  {
    // Test-only boundaries: no production core command can run. Update command
    // remains real and can retrieve only this object's loopback factory URL.
    for (auto *action : findChildren<QAction *>()) {
      const auto key = action->shortcut();
      if (key != QKeySequence(Qt::CTRL | Qt::Key_S) && key != QKeySequence(Qt::CTRL | Qt::Key_R))
        continue;
      QObject::disconnect(action, &QAction::triggered, this, nullptr);
      action->setEnabled(false);
    }
    for (const auto &name : {"btnToggleCore", "btnRestartCore"})
      if (auto *button = findChild<QPushButton *>(name)) {
        QObject::disconnect(button, &QPushButton::clicked, nullptr, nullptr);
        button->setEnabled(false);
      }
    if (auto *hostname = findChild<QLineEdit *>("lineHostname"))
      QObject::disconnect(hostname, &QLineEdit::returnPressed, nullptr, nullptr);
    setAttribute(Qt::WA_DontShowOnScreen, !showOnScreen);
    open();
    // Matrix widths represent a manual resize after the compact production startup.
    if (width > 0)
      resize(width, height());
  }
  int checks = 0;

protected:
  synergy::gui::update::UpdateChecker *createUpdateChecker() override
  {
    ++checks;
    auto *checker = new synergy::gui::update::UpdateChecker(this);
    checker->setApiUrl(server.url());
    return checker;
  }

private:
  ControlledHttp &server;
};

// Test-only actual activation helper; never changes a combo index directly.
bool chooseLanguagePopup(QComboBox *control, const QString &language)
{
  if (!control)
    return false;
  const int row = control->findData(language);
  if (row < 0)
    return false;
  // Existing endpoint-settle seam prepares the real page; it never hides a
  // popup or changes selection. Require stable, visible geometry before input.
  QElapsedTimer readiness;
  readiness.start();
  if (!settleStaticRender(*control->window()))
    return false;
  QRect previous;
  int stable = 0;
  while (readiness.elapsed() < 1000) {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
    const QRect current(control->mapTo(control->window(), QPoint()), control->size());
    if (control->isVisibleTo(control->window()) && current.width() > 0 && current.height() > 0 && current == previous)
      ++stable;
    else
      stable = 0;
    previous = current;
    if (stable >= 2)
      break;
    QTest::qWait(10);
  }
  qInfo() << "Main language popup readiness" << language << readiness.elapsed() << "stable" << stable << "geometry"
          << previous;
  if (stable < 2)
    return false;
  QSignalSpy activated(control, qOverload<int>(&QComboBox::activated));
  control->showPopup();
  QCoreApplication::processEvents();
  auto *view = control->view();
  if (!view || !view->isVisible()) {
    qWarning() << "Main language popup view unavailable" << language;
    return false;
  }
  const auto index = control->model()->index(row, control->modelColumn(), control->rootModelIndex());
  view->scrollTo(index);
  QCoreApplication::processEvents();
  const auto point = view->visualRect(index).center();
  if (!view->viewport()->rect().contains(point))
    return false;
  QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, point);
  QCoreApplication::processEvents();
  const bool immediate = activated.count() == 1 && control->currentData().toString() == language;
  qInfo() << "Main language popup activation" << language << "count" << activated.count() << "selected"
          << control->currentData() << "visible" << view->isVisible();
  auto *container = view->parentWidget();
  QElapsedTimer closing;
  closing.start();
  while (immediate && (view->isVisible() || (container && container->isVisible())) && closing.elapsed() < 1000)
    QTest::qWait(10);
  return immediate && activated.count() == 1 && control->currentData().toString() == language && !view->isVisible() &&
         (!container || !container->isVisible());
}

QPushButton *button(QWidget &root, const QString &text)
{
  for (auto *candidate : root.findChildren<QPushButton *>())
    if (candidate->text() == text && candidate->isVisibleTo(&root))
      return candidate;
  return nullptr;
}
MessagePage *message(MainWindow &window, const QString &title)
{
  for (auto *page : window.findChildren<MessagePage *>())
    if (page->windowTitle() == title && page->isVisibleTo(&window))
      return page;
  return nullptr;
}
void capture(QWidget &root, const QString &name)
{
  QVERIFY(settleStaticRender(root));
  const auto path = qEnvironmentVariable("SYNERGY_UI_EVIDENCE_DIR");
  if (path.isEmpty())
    return;
  QVERIFY(QDir().mkpath(path));
  QVERIFY(root.grab().save(
      path + "/flow-" + name + "-" + I18N::currentLanguage() + "-" + QString::number(root.width()) + ".png"
  ));
}
} // namespace

// Additive executable counterpart of accepted M-C01, M-C05 and M-C06.
class MainWindowFlowTests : public QObject
{
  Q_OBJECT
private:
  int m_width = 600;
private Q_SLOTS:
  void initTestCase_data()
  {
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("width");
    for (const auto &language : {QStringLiteral("en"), QStringLiteral("zh_CN")})
      for (int width : {500, 600, 820})
        QTest::newRow(qPrintable(language + "-" + QString::number(width))) << language << width;
  }
  void init()
  {
    QFETCH_GLOBAL(QString, language);
    QFETCH_GLOBAL(int, width);
    m_width = width;
    const auto fixtureId =
        QString::fromLatin1(QTest::currentTestFunction()) + "-" + language + "-" + QString::number(width);
    Settings::setSettingsFile(Settings::userDir() + "/" + fixtureId + ".conf");
    Settings::setStateFile(synergy::runtime::runtimeRoot() + "/state/" + fixtureId + ".state");
    I18N::setLanguage(language);
    Settings::setValue(Settings::Core::ComputerName, "Flow-Fixture");
    Settings::setValue(Settings::Core::CoreMode, int(Settings::Server));
    Settings::setValue(Settings::Core::ProcessMode, int(Settings::Desktop));
    Settings::setValue(Settings::Gui::AutoStartCore, false);
    Settings::setValue(Settings::Gui::CheckForUpdates, false);
    Settings::setValue(Settings::Gui::Autohide, false);
    Settings::setValue(Settings::Gui::CloseToTray, false);
    Settings::setValue(Settings::Gui::CloseReminder, false);
    Settings::setValue(Settings::Security::TlsEnabled, false);
    Settings::setValue(Settings::Log::ToFile, false);
  }
  void windowStartsCompactDespiteSavedGeometry()
  {
    Settings::setValue(Settings::Gui::WindowGeometry, QRect(25, 35, m_width + 200, 900));
    ControlledHttp server;
    IsolatedWindow window(server, 0);
    QCOMPARE(window.minimumSize(), QSize(500, 390));
    QTRY_COMPARE(window.width(), 500);
    QVERIFY(window.height() >= window.minimumHeight());
    window.resize(200, 200);
    QCOMPARE(window.size(), QSize(500, 390));
  }
  void windowStartsCompactWithoutSavedGeometry()
  {
    Settings::setValue(Settings::Gui::WindowGeometry);
    ControlledHttp server;
    IsolatedWindow window(server, 0);
    QCOMPARE(window.minimumSize(), QSize(500, 390));
    QTRY_COMPARE(window.width(), 500);
    QVERIFY(window.height() >= 390);
  }
  void pageShortcutReceivesFocusOnTab()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *pageBar = window.findChild<QWidget *>("pageBar");
    auto *preferences = window.findChild<QPushButton *>("btnSettingsShortcut");
    QVERIFY(pageBar && preferences);
    QCOMPARE(window.focusWidget(), pageBar);
    QTest::keyClick(pageBar, Qt::Key_Tab);
    QCOMPARE(window.focusWidget(), preferences);
  }
  void preferencesMouseAndKeyboardNavigationHideHomeShortcuts()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *pageBar = window.findChild<QWidget *>("pageBar");
    auto *preferences = window.findChild<QPushButton *>("btnSettingsShortcut");
    auto *updates = window.findChild<QPushButton *>("btnUpdatesShortcut");
    auto *about = window.findChild<QPushButton *>("btnAboutShortcut");
    auto *back = window.findChild<QPushButton *>("btnBack");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(pageBar && preferences && updates && about && back && pages);
    const auto homeShortcutsVisible = [&] {
      return preferences->isVisibleTo(&window) && updates->isVisibleTo(&window) && about->isVisibleTo(&window);
    };
    const auto homeShortcutsHidden = [&] {
      return preferences->isHidden() && updates->isHidden() && about->isHidden();
    };
    QTRY_VERIFY(homeShortcutsVisible());
    QSignalSpy activated(preferences, &QPushButton::clicked);
    QTest::mouseClick(preferences, Qt::LeftButton);
    QTRY_COMPARE(activated.count(), 1);
    QTRY_VERIFY(window.findChild<SettingsDialog *>() != nullptr);
    QPointer<SettingsDialog> editor = window.findChild<SettingsDialog *>();
    QTRY_VERIFY(editor && editor->isVisibleTo(&window));
    QTRY_VERIFY(back->isVisibleTo(&window));
    QTRY_COMPARE(window.width(), m_width);
    QVERIFY(pageBar->isVisibleTo(&window));
    QTRY_VERIFY(homeShortcutsHidden());
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);
    QTRY_VERIFY(homeShortcutsVisible());
    QTRY_VERIFY(!back->isVisibleTo(&window));
    QTRY_VERIFY(editor.isNull());
    QTRY_COMPARE(window.width(), m_width);

    pageBar->setFocus();
    QTest::keyClick(pageBar, Qt::Key_Tab);
    QTRY_COMPARE(window.focusWidget(), preferences);
    QTest::keyClick(preferences, Qt::Key_Space);
    QTRY_COMPARE(activated.count(), 2);
    QTRY_VERIFY(window.findChild<SettingsDialog *>() != nullptr);
    editor = window.findChild<SettingsDialog *>();
    QTRY_VERIFY(editor && editor->isVisibleTo(&window));
    QTRY_VERIFY(back->isVisibleTo(&window));
    QTRY_COMPARE(window.width(), m_width);
    QTRY_VERIFY(homeShortcutsHidden());
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);
    QTRY_VERIFY(homeShortcutsVisible());
    QTRY_VERIFY(editor.isNull());
    QTRY_COMPARE(window.width(), m_width);
  }
  void preferencesTabStripKeepsWorkspaceInsetAtMinimumHeight()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    window.resize(m_width, 700);
    auto *preferences = window.findChild<QPushButton *>("btnSettingsShortcut");
    auto *pageBar = window.findChild<QWidget *>("pageBar");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(preferences && pageBar && pages);
    QTRY_VERIFY(preferences->isVisibleTo(&window));
    QTest::mouseClick(preferences, Qt::LeftButton);
    QTRY_VERIFY(window.findChild<SettingsDialog *>() != nullptr);
    auto *editor = window.findChild<SettingsDialog *>();
    auto *tabs = editor->findChild<QTabWidget *>("tabWidget");
    auto *area = qobject_cast<QScrollArea *>(pages->currentWidget());
    QVERIFY(tabs && area);
    QCOMPARE(area->property("pageEditor").value<QDialog *>(), static_cast<QDialog *>(editor));
    QTRY_COMPARE(tabs->currentIndex(), 0);
    QCOMPARE(tabs->currentWidget()->objectName(), QStringLiteral("tabGeneral"));
    auto *tabBar = tabs->tabBar();
    QVERIFY(tabBar);
    QTRY_VERIFY(editor->isVisibleTo(&window) && tabBar->isVisibleTo(&window));
    QTRY_COMPARE(window.size(), QSize(m_width, 390));
    QVERIFY(settleStaticRender(window));
    QRect previousTabRect;
    QRect previousViewportRect;
    QElapsedTimer stable;
    const auto topInsetSettled = [&] {
      const QRect tabRect(tabBar->mapTo(&window, QPoint()), tabBar->size());
      const QRect viewportRect(area->viewport()->mapTo(&window, QPoint()), area->viewport()->size());
      if (!stable.isValid() || tabRect != previousTabRect || viewportRect != previousViewportRect) {
        previousTabRect = tabRect;
        previousViewportRect = viewportRect;
        stable.start();
      }
      const int headerBottom = pageBar->mapTo(&window, QPoint()).y() + pageBar->height();
      return stable.elapsed() >= 50 && tabRect.height() > 0 && area->verticalScrollBar()->value() == 0 &&
             qAbs(viewportRect.top() - headerBottom) <= 2 && qAbs(tabRect.top() - viewportRect.top() - 10) <= 2 &&
             qAbs(tabRect.top() - headerBottom - 10) <= 2;
    };
    QTRY_VERIFY_WITH_TIMEOUT(topInsetSettled(), 1000);
    QCOMPARE(window.size(), QSize(m_width, 390));
    QTest::mouseClick(tabBar, Qt::LeftButton, Qt::NoModifier, tabBar->tabRect(1).center());
    QTRY_COMPARE(tabs->currentIndex(), 1);
    QTRY_COMPARE(window.width(), m_width);
    QTest::mouseClick(tabBar, Qt::LeftButton, Qt::NoModifier, tabBar->tabRect(0).center());
    QTRY_COMPARE(tabs->currentIndex(), 0);
    QTRY_COMPARE(window.size(), QSize(m_width, 390));
    QTRY_VERIFY_WITH_TIMEOUT(topInsetSettled(), 1000);
    capture(window, "preferences-general-top-inset");
    auto *back = window.findChild<QPushButton *>("btnBack");
    QVERIFY(back);
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);
    QTRY_COMPARE(window.width(), m_width);
  }
  void nameChangesRequireExplicitCommit()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *edit = window.findChild<QPushButton *>("btnEditName");
    auto *save = window.findChild<QPushButton *>("btnSaveName");
    auto *cancel = window.findChild<QPushButton *>("btnCancelName");
    auto *field = window.findChild<QLineEdit *>("lineEditName");
    auto *label = window.findChild<QLabel *>("lblComputerName");
    QVERIFY(edit && save && cancel && field && label);
    QTest::mouseClick(edit, Qt::LeftButton);
    QVERIFY(field->isVisibleTo(&window));
    QCOMPARE(window.width(), m_width);
    field->setText("Uncommitted-Fixture");
    cancel->setFocus();
    QCoreApplication::processEvents();
    QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), QString("Flow-Fixture"));
    QTest::mouseClick(cancel, Qt::LeftButton);
    QCOMPARE(label->text(), QString("Flow-Fixture"));
    QTest::mouseClick(edit, Qt::LeftButton);
    field->setText("Esc-Fixture");
    QTest::keyClick(field, Qt::Key_Escape);
    QVERIFY(field->isHidden());
    QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), QString("Flow-Fixture"));
    QTest::mouseClick(edit, Qt::LeftButton);
    field->setText("Saved-Fixture");
    QTest::mouseClick(save, Qt::LeftButton);
    QCOMPARE(label->text(), QString("Saved-Fixture"));
    QSettings persisted(Settings::settingsFile(), QSettings::IniFormat);
    QCOMPARE(persisted.value(Settings::Core::ComputerName).toString(), QString("Saved-Fixture"));
    QTest::mouseClick(edit, Qt::LeftButton);
    QCOMPARE(field->text(), QString("Saved-Fixture"));
    field->setText("Enter-Fixture");
    QTest::keyClick(field, Qt::Key_Return);
    QCOMPARE(label->text(), QString("Enter-Fixture"));
    persisted.sync();
    QCOMPARE(persisted.value(Settings::Core::ComputerName).toString(), QString("Enter-Fixture"));
    capture(window, "name-committed");
    window.close();
  }
  // UI-008/UI-002 same-family native finding: one real MainWindow survives
  // language preview Cancel and Save; setup-only text must not remain stale.
  void mainLanguageSaveCancelRefreshesVisibleAndAccessibleText()
  {
    const auto originalLanguage = I18N::currentLanguage();
    const auto changedLanguage = originalLanguage == "en" ? QString("zh_CN") : QString("en");
    Settings::setValue(Settings::Core::Language, originalLanguage);
    Settings::setValue(Settings::Client::RemoteHost, "192.0.2.20");
    Settings::setValue(Settings::Core::Port, 24567);
    // Approved fixture boundary: existing Preferences Save canonicalizes unset
    // command values to empty strings and enum Info to the existing INFO option.
    // Seed those representations before the strict all-effective-values baseline.
    Settings::setValue(Settings::Core::ScreenEnterCommand, QStringLiteral(""));
    Settings::setValue(Settings::Core::ScreenExitCommand, QStringLiteral(""));
    Settings::setValue(Settings::Log::Level, QStringLiteral("INFO"));
    ControlledHttp server;
    IsolatedWindow window(server, m_width, true);
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *serverMode = window.findChild<ModeCard *>("rbModeServer");
    auto *clientMode = window.findChild<ModeCard *>("rbModeClient");
    auto *exportConfig = window.findChild<QPushButton *>("btnSaveServerConfig");
    auto *hostname = window.findChild<QLineEdit *>("lineHostname");
    auto *port = window.findChild<QSpinBox *>("spinPort");
    auto *restart = window.findChild<QPushButton *>("btnRestartCore");
    QVERIFY(serverMode && clientMode && exportConfig && hostname && port && restart);
    const auto originalMode = window.coreMode();
    const bool serverChecked = serverMode->isChecked();
    const bool clientChecked = clientMode->isChecked();
    const auto originalHostText = hostname->text();
    const int originalPortValue = port->value();
    QSignalSpy serverToggled(serverMode, &QRadioButton::toggled);
    QSignalSpy clientToggled(clientMode, &QRadioButton::toggled);
    QMap<QString, QVariant> originalSettings;
    for (const auto &key : Settings::validKeys())
      originalSettings.insert(key, Settings::value(key));
    const auto mismatches = [&](const QString &language, bool languageSaved) {
      QStringList errors;
      const bool chinese = language == "zh_CN";
      const auto expect = [&errors](const QString &name, const QString &actual, const QString &expected) {
        if (actual != expected)
          errors << name + ": actual=" + actual + "; expected=" + expected;
      };
      expect("Server title", serverMode->text(), chinese ? QString::fromUtf8("服务端") : QString("Server"));
      expect("Client title", clientMode->text(), chinese ? QString::fromUtf8("客户端") : QString("Client"));
      const auto exportText = chinese ? QString::fromUtf8("导出服务器配置") : QString("Export server configuration");
      expect("Export tooltip", exportConfig->toolTip(), exportText);
      expect("Export accessibleName", exportConfig->accessibleName(), exportText);
      expect(
          "Hostname accessibleName", hostname->accessibleName(),
          chinese ? QString::fromUtf8("连接到：") : QString("Connect to:")
      );
      expect(
          "Hostname tooltip description", hostname->toolTip(),
          chinese ? QString::fromUtf8("服务端计算机的主机名或 IP 地址，可填写逗号分隔的列表。")
                  : QString("Hostname or IP address of the server computer. May contain a comma separated list.")
      );
      expect("Port accessibleName", port->accessibleName(), chinese ? QString::fromUtf8("端口") : QString("Port"));
      expect("Restart tooltip", restart->toolTip(), chinese ? QString::fromUtf8("重启") : QString("Restart"));
      if (window.coreMode() != originalMode || serverMode->isChecked() != serverChecked ||
          clientMode->isChecked() != clientChecked || hostname->text() != originalHostText ||
          port->value() != originalPortValue)
        errors << "Mode/address/port or checked state changed during language operation";
      if (serverToggled.count() != 0 || clientToggled.count() != 0)
        errors << "Unexpected mode toggled signal";
      for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) {
        const auto expected = it.key() == Settings::Core::Language && languageSaved ? QVariant(language) : it.value();
        if (Settings::value(it.key()) != expected)
          errors << "Unexpected effective setting change: " + it.key();
      }
      return errors;
    };
    auto errors = mismatches(originalLanguage, false);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join("\n")));
    QAction *preferences = nullptr;
    for (auto *action : window.findChildren<QAction *>())
      if (action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_Comma))
        preferences = action;
    QVERIFY(preferences);
    preferences->trigger();
    auto *editor = window.findChild<SettingsDialog *>();
    QVERIFY(editor);
    QVERIFY(chooseLanguagePopup(editor->findChild<QComboBox *>("comboLanguage"), changedLanguage));
    QCOMPARE(I18N::currentLanguage(), changedLanguage);
    QCOMPARE(Settings::value(Settings::Core::Language).toString(), originalLanguage);
    auto *box = editor->findChild<QDialogButtonBox *>();
    QVERIFY(box && box->button(QDialogButtonBox::Cancel));
    QTest::mouseClick(box->button(QDialogButtonBox::Cancel), Qt::LeftButton);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // Normal LanguageChange is queued independently of DeferredDelete. Observe
    // the settled real page; do not force text or alter the strict expectations.
    qInfo() << "Main language page before event settle" << I18N::currentLanguage() << serverMode->text()
            << clientMode->text();
    QVERIFY2(settleStaticRender(window), "Main page did not settle after language return");
    qInfo() << "Main language page after event settle" << I18N::currentLanguage() << serverMode->text()
            << clientMode->text();
    QCOMPARE(I18N::currentLanguage(), originalLanguage);
    QVERIFY(serverMode->isVisibleTo(&window) && clientMode->isVisibleTo(&window));
    errors = mismatches(originalLanguage, false);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join("\n")));
    capture(window, "main-language-cancel-restored");
    preferences->trigger();
    editor = window.findChild<SettingsDialog *>();
    QVERIFY(editor);
    QVERIFY(chooseLanguagePopup(editor->findChild<QComboBox *>("comboLanguage"), changedLanguage));
    box = editor->findChild<QDialogButtonBox *>();
    QVERIFY(box && box->button(QDialogButtonBox::Save));
    QVERIFY(box->button(QDialogButtonBox::Save)->isEnabled());
    QTest::mouseClick(box->button(QDialogButtonBox::Save), Qt::LeftButton);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // Normal LanguageChange is queued independently of DeferredDelete. Observe
    // the settled real page; do not force text or alter the strict expectations.
    qInfo() << "Main language page before event settle" << I18N::currentLanguage() << serverMode->text()
            << clientMode->text();
    QVERIFY2(settleStaticRender(window), "Main page did not settle after language return");
    qInfo() << "Main language page after event settle" << I18N::currentLanguage() << serverMode->text()
            << clientMode->text();
    QCOMPARE(I18N::currentLanguage(), changedLanguage);
    QVERIFY(serverMode->isVisibleTo(&window) && clientMode->isVisibleTo(&window));
    errors = mismatches(changedLanguage, true);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join("\n")));
    capture(window, "main-language-saved");
    preferences->trigger();
    editor = window.findChild<SettingsDialog *>();
    QVERIFY(editor);
    QCOMPARE(editor->findChild<QComboBox *>("comboLanguage")->currentData().toString(), changedLanguage);
    editor->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // Normal LanguageChange is queued independently of DeferredDelete. Observe
    // the settled real page; do not force text or alter the strict expectations.
    qInfo() << "Main language page before event settle" << I18N::currentLanguage() << serverMode->text()
            << clientMode->text();
    QVERIFY2(settleStaticRender(window), "Main page did not settle after language return");
    qInfo() << "Main language page after event settle" << I18N::currentLanguage() << serverMode->text()
            << clientMode->text();
    errors = mismatches(changedLanguage, true);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join("\n")));
    window.close();
  }
  void selectedScreenDeletionIsDraftAndPrimaryProtected()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    window.serverConfig().addClient("Delete-Fixture");
    auto *open = window.findChild<QPushButton *>("btnConfigureServer");
    QVERIFY(open);
    const auto select = [](ScreenSetupView *view, const QString &name) {
      for (int row = 0; row < view->model()->rowCount(); ++row)
        for (int column = 0; column < view->model()->columnCount(); ++column) {
          const auto index = view->model()->index(row, column);
          if (view->model()->data(index).toString() == name) {
            view->setCurrentIndex(index);
            return true;
          }
        }
      return false;
    };
    for (bool save : {false, true}) {
      QTest::mouseClick(open, Qt::LeftButton);
      auto *editor = window.findChild<ServerConfigDialog *>();
      QVERIFY(editor);
      auto *view = editor->findChild<ScreenSetupView *>();
      auto *remove = editor->findChild<QPushButton *>("deleteSelectedComputerButton");
      auto *external = editor->findChild<QCheckBox *>("cbExternalConfig");
      QVERIFY(view && remove && external);
      QVERIFY(select(view, window.serverConfig().getServerName()));
      QVERIFY(!remove->isEnabled());
      QVERIFY(select(view, "Delete-Fixture"));
      QVERIFY(remove->isEnabled());
      QVERIFY(select(view, QString()));
      QVERIFY(!remove->isEnabled());
      QVERIFY(select(view, "Delete-Fixture"));
      external->setChecked(true);
      QVERIFY(!remove->isEnabled());
      external->setChecked(false);
      QVERIFY(remove->isEnabled());
      QTest::mouseClick(remove, Qt::LeftButton);
      QVERIFY(window.serverConfig().screenExists("Delete-Fixture"));
      auto *buttons = editor->findChild<QDialogButtonBox *>();
      QVERIFY(buttons);
      QTest::mouseClick(buttons->button(save ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel), Qt::LeftButton);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      QCOMPARE(window.serverConfig().screenExists("Delete-Fixture"), !save);
    }
    window.close();
  }
  void nestedHotkeyRefreshesParentActionsAndParentCancelPreservesOriginal()
  {
    // C-UI-R3-M03-R2 regression: distinct first/survivor actions expose stale
    // parent row routing after nested deletion and addition, before source fix.
    auto &fixture = Settings::proxy().get();
    fixture.remove("internalConfig");
    fixture.beginGroup("internalConfig");
    fixture.beginWriteArray("hotkeys", 1);
    fixture.setArrayIndex(0);
    KeySequence trigger;
    trigger.appendKey(Qt::Key_K, Qt::ControlModifier);
    trigger.saveSettings(fixture);
    fixture.beginWriteArray("actions", 2);
    for (int i = 0; i < 2; ++i) {
      fixture.setArrayIndex(i);
      fixture.setValue(SettingsKeys::ActionType, i == 0 ? 5 : 7);
      fixture.setValue(SettingsKeys::RestartServer, i == 1);
    }
    fixture.endArray();
    fixture.endArray();
    fixture.endGroup();
    fixture.sync();
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    const auto original = std::as_const(window.serverConfig()).hotkeys();
    QCOMPARE(original.size(), 1);
    QCOMPARE(original.first().actions().size(), 2);
    const auto survivorText = original.first().actions().at(1).text();
    QVERIFY(original.first().actions().at(0).text() != survivorText);
    // R4-P02 changes only the visible-caption oracle. Protocol/model values and
    // the real survivor reopen/parent Cancel checks below remain independent.
    const bool chinese = I18N::currentLanguage() == QStringLiteral("zh_CN");
    const auto restartCaption = [chinese](const QString &text) {
      return chinese ? text.contains(QString::fromUtf8("重启")) && text.contains(QString::fromUtf8("服务端"))
                     : text.contains(QRegularExpression(
                           QStringLiteral("\\brestart\\b.*\\bserver\\b"), QRegularExpression::CaseInsensitiveOption
                       ));
    };
    const auto upCaption = [chinese](const QString &text) {
      return chinese ? text.contains(QString::fromUtf8("切换")) && text.contains(QString::fromUtf8("向上"))
                     : text.contains(QRegularExpression(
                           QStringLiteral("\\bswitch\\b.*\\bup\\b"), QRegularExpression::CaseInsensitiveOption
                       ));
    };
    QStringList captionErrors;
    const auto checkRestartCaption = [&](const QString &text) {
      if (!restartCaption(text))
        captionErrors << QStringLiteral("Expected readable restart-server caption: %1").arg(text);
    };
    QTest::mouseClick(window.findChild<QPushButton *>("btnConfigureServer"), Qt::LeftButton);
    auto *parent = window.findChild<ServerConfigDialog *>();
    QVERIFY(parent);
    auto *tabs = parent->findChild<QTabWidget *>();
    tabs->setCurrentWidget(parent->findChild<QWidget *>("tabHotkeys"));
    auto *hotkeys = parent->findChild<QListWidget *>("listHotkeys");
    auto *actions = parent->findChild<QListWidget *>("listActions");
    QVERIFY(hotkeys && actions);
    hotkeys->setCurrentRow(0);
    QCOMPARE(actions->count(), 2);
    QTest::mouseClick(parent->findChild<QPushButton *>("btnEditHotkey"), Qt::LeftButton);
    auto *hotkey = window.findChild<HotkeyDialog *>();
    QVERIFY(hotkey);
    auto *associated = hotkey->findChild<QListWidget *>("listAssociatedActions");
    QVERIFY(associated);
    associated->setCurrentRow(0);
    QTest::mouseClick(hotkey->findChild<QPushButton *>("btnRemoveAction"), Qt::LeftButton);
    QCOMPARE(associated->count(), 1);
    checkRestartCaption(associated->item(0)->text());
    QTest::mouseClick(hotkey->findChild<QPushButton *>("btnAddAction"), Qt::LeftButton);
    auto *addition = window.findChild<ActionDialog *>();
    QVERIFY(addition);
    auto *type = addition->findChild<QComboBox *>("comboActionType");
    auto *direction = addition->findChild<QComboBox *>("comboSwitchInDirection");
    QVERIFY(type && direction);
    type->setCurrentIndex(type->findData(4));
    direction->setCurrentIndex(direction->findData(2));
    QTest::mouseClick(addition->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok), Qt::LeftButton);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(associated->count(), 2);
    QTest::mouseClick(hotkey->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok), Qt::LeftButton);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(actions->count(), 2);
    checkRestartCaption(actions->item(0)->text());
    if (!upCaption(actions->item(1)->text()))
      captionErrors << QStringLiteral("Expected readable switch-up caption: %1").arg(actions->item(1)->text());
    actions->setCurrentRow(0);
    QTest::mouseClick(parent->findChild<QPushButton *>("btnEditAction"), Qt::LeftButton);
    auto *survivor = window.findChild<ActionDialog *>();
    QVERIFY(survivor);
    QCOMPARE(survivor->findChild<QComboBox *>("comboActionType")->currentData().toInt(), 7);
    survivor->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    parent->reject();
    QCOMPARE(std::as_const(window.serverConfig()).hotkeys(), original);
    synergy::gui::ServerConfig reloaded;
    QCOMPARE(std::as_const(reloaded).hotkeys(), original);
    window.close();
    QVERIFY2(captionErrors.isEmpty(), qPrintable(captionErrors.join('\n')));
  }
  void savedCertificateFingerprintKeepsDistinctDraftPath()
  {
    // C-UI-R3-M04-R2: saved-certificate semantics must be explicit when draft B
    // differs from saved A. Generated fixture private material never leaves temp.
    const auto pathA = Settings::userDir() + "/cert-a.pem";
    const auto pathB = Settings::userDir() + "/cert-b.pem";
    QVERIFY(synergy::gui::TlsUtility::generateCertificateAt(pathA, 2048));
    QVERIFY(synergy::gui::TlsUtility::generateCertificateAt(pathB, 2048));
    const auto hashA = synergy::gui::TlsUtility::certFingerprint(pathA);
    const auto hashB = synergy::gui::TlsUtility::certFingerprint(pathB);
    QCOMPARE(hashA.size(), 32);
    QVERIFY(hashA != hashB);
    Settings::setValue(Settings::Security::Certificate, pathA);
    Settings::setValue(Settings::Security::KeySize, 2048);
    Settings::setValue(Settings::Security::TlsEnabled, true);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    QAction *preferences = nullptr;
    for (auto *action : window.findChildren<QAction *>())
      if (action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_Comma))
        preferences = action;
    QVERIFY(preferences);
    preferences->trigger();
    auto *editor = window.findChild<SettingsDialog *>();
    QVERIFY(editor);
    auto *tabs = editor->findChild<QTabWidget *>();
    tabs->setCurrentWidget(editor->findChild<QWidget *>("tabNetwork"));
    auto *path = editor->findChild<QLineEdit *>("lineTlsCertPath");
    auto *view = editor->findChild<QPushButton *>("btnViewFingerprint");
    QVERIFY(path && view);
    path->setText(pathB);
    QCOMPARE(view->text(), QCoreApplication::translate("SettingsDialog", "View saved certificate fingerprint"));
    QTest::mouseClick(view, Qt::LeftButton);
    auto *fingerprint = window.findChild<FingerprintDialog *>();
    QVERIFY(fingerprint);
    auto *hash = fingerprint->findChild<QLabel *>("fingerprintHash");
    QVERIFY(hash && hash->isVisibleTo(fingerprint));
    auto compactHash = hash->text();
    compactHash.remove(QRegularExpression("[^0-9a-fA-F]"));
    QCOMPARE(compactHash.toLower(), QString::fromLatin1(hashA.toHex()));
    bool savedSourceVisible = false;
    for (auto *label : fingerprint->findChildren<QLabel *>())
      if (label->isVisibleTo(fingerprint) && label->text().contains(pathA))
        savedSourceVisible = true;
    QVERIFY(savedSourceVisible);
    capture(window, "saved-fingerprint-distinct-draft");
    fingerprint->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(path->text(), pathB);
    editor->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(Settings::value(Settings::Security::Certificate).toString(), pathA);
    preferences->trigger();
    editor = window.findChild<SettingsDialog *>();
    QVERIFY(editor);
    QCOMPARE(editor->findChild<QLineEdit *>("lineTlsCertPath")->text(), pathA);
    editor->reject();
    window.close();
  }
  void checkingFailureRetryAndCancelAreRealRoutes()
  {
    ControlledHttp server;
    QVERIFY(server.isListening());
    IsolatedWindow window(server, m_width);
    QAction *updates = nullptr;
    for (auto *action : window.findChildren<QAction *>())
      if (action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_U))
        updates = action;
    QVERIFY(updates);
    updates->trigger();
    QTRY_COMPARE(server.requests, 1);
    auto *checking = message(window, QCoreApplication::translate("MainWindow", "Check for Updates"));
    QVERIFY(checking);
    auto *progress = checking->findChild<QProgressBar *>();
    auto *card = checking->findChild<QFrame *>("card");
    QVERIFY(progress && card && card->isAncestorOf(progress));
    capture(window, "checking");
    server.write("HTTP/1.1 503 Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", true);
    QTRY_VERIFY(message(window, QCoreApplication::translate("MainWindow", "Update Check Failed")));
    auto *failed = message(window, QCoreApplication::translate("MainWindow", "Update Check Failed"));
    auto *retry = button(*failed, QCoreApplication::translate("MainWindow", "Retry"));
    QVERIFY(retry);
    capture(window, "check-failed");
    QTest::mouseClick(retry, Qt::LeftButton);
    QTRY_COMPARE(server.requests, 2);
    QCOMPARE(window.checks, 2);
    checking = message(window, QCoreApplication::translate("MainWindow", "Check for Updates"));
    QVERIFY(checking);
    auto *cancel = button(*checking, QCoreApplication::translate("MainWindow", "Cancel"));
    QVERIFY(cancel);
    QTest::mouseClick(cancel, Qt::LeftButton);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!message(window, QCoreApplication::translate("MainWindow", "Check for Updates")));
    QVERIFY(window.findChildren<synergy::gui::update::UpdateChecker *>().isEmpty());
    QCOMPARE(window.checks, 2);
    window.close();
  }
  void stalledDownloadUsesActualBytesAndCancels()
  {
    ControlledHttp server;
    QVERIFY(server.isListening());
    synergy::gui::update::UpdateInfo info;
    info.version = "99.0.0";
    info.notes = "Offline controlled update fixture";
    info.asset.name = "flow-fixture.dmg";
    info.asset.url = server.url();
    info.asset.sha256 = QString(64, QLatin1Char('0'));
    const synergy::gui::update::DownloadPolicy policy{"http", "127.0.0.1", "/"};
    synergy::gui::update::UpdateDialog dialog(info, nullptr, policy);
    dialog.setAttribute(Qt::WA_DontShowOnScreen);
    dialog.resize(m_width, dialog.sizeHint().height());
    dialog.show();
    auto *download = button(dialog, QCoreApplication::translate("UpdateDialog", "Download and Install"));
    QVERIFY(download);
    QPointer<synergy::gui::update::UpdateDownloader> downloader =
        dialog.findChild<synergy::gui::update::UpdateDownloader *>();
    QVERIFY(downloader);
    QSignalSpy bytes(downloader, &synergy::gui::update::UpdateDownloader::progress);
    QSignalSpy verified(downloader, &synergy::gui::update::UpdateDownloader::finished);
    QTest::mouseClick(download, Qt::LeftButton);
    QTRY_COMPARE(server.requests, 1);
    server.write("HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n");
    const auto waiting =
        QCoreApplication::translate("UpdateDialog", "%1 MB — Waiting for network").section(QChar(0x2014), -1).trimmed();
    const auto progressText = [&dialog, &waiting] {
      for (auto *label : dialog.findChildren<QLabel *>())
        if (label->isVisibleTo(&dialog) && (label->text().contains(waiting) || label->text().contains("MB/s")))
          return label->text();
      return QString();
    };
    QTRY_VERIFY_WITH_TIMEOUT(progressText().contains(waiting), 3000);
    QVERIFY(!progressText().contains("MB/s"));
    QVERIFY(!progressText().contains("%"));
    capture(dialog, "download-waiting");
    QTest::qWait(150);
    server.write(QByteArray(65536, 'x'));
    QTRY_VERIFY_WITH_TIMEOUT(!bytes.isEmpty() && bytes.last().at(0).toLongLong() > 0, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(progressText().contains("MB/s"), 1500);
    capture(dialog, "download-positive-rate");
    QTRY_VERIFY_WITH_TIMEOUT(progressText().contains(waiting), 3000);
    QVERIFY(!progressText().contains("MB/s"));
    QVERIFY(!progressText().contains("%"));
    capture(dialog, "download-stalled");
    for (auto *group : dialog.findChildren<QGroupBox *>()) {
      QVERIFY(group->title().isEmpty());
      QVERIFY(group->findChildren<QLabel *>().size() > 0 || group->objectName() == "updateNotesCard");
    }
    auto *cancel = button(dialog, QCoreApplication::translate("UpdateDialog", "Cancel download"));
    QVERIFY(cancel);
    QCOMPARE(verified.count(), 0);
    QTest::mouseClick(cancel, Qt::LeftButton);
    QVERIFY(downloader.isNull());
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
  }
};

int main(int argc, char **argv)
{
  QTemporaryDir directory;
  if (!directory.isValid())
    return 1;
  const auto root = QFileInfo(directory.path()).canonicalFilePath();
  QString error;
  if (!synergy::runtime::configureIsolatedRuntime(
          {root, "ts-flow-" + QUuid::createUuid().toString(QUuid::WithoutBraces)}, &error
      ))
    return 2;
  QDir().mkpath(root + "/user");
  QSettings fixture(Settings::userSettingsFile(), QSettings::IniFormat);
  fixture.setValue(Settings::Core::ComputerName, "Flow-Fixture");
  fixture.setValue(Settings::Gui::AutoStartCore, false);
  fixture.setValue(Settings::Gui::CheckForUpdates, false);
  fixture.setValue(Settings::Security::TlsEnabled, false);
  fixture.sync();
  QApplication app(argc, argv);
  auto font = app.font();
  font.setPixelSize(13);
  app.setFont(font);
  Settings::setStateFile(synergy::runtime::runtimeStateFile());
  app.setStyleSheet(synergy::gui::appStyleSheet());
  MainWindowFlowTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "MainWindowFlowTests.moc"
