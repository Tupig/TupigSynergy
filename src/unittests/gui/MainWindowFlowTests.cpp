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
#include "gui/StyleUtils.h"
#include "gui/TlsUtility.h"
#include "gui/dialogs/AboutDialog.h"
#include "gui/dialogs/ActionDialog.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/FingerprintDialog.h"
#include "gui/dialogs/HotkeyDialog.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/update/UpdateChecker.h"
#include "gui/update/UpdateDialog.h"
#include "gui/update/UpdateDownloader.h"
#include "gui/widgets/LogPanel.h"
#include "gui/widgets/LogWidget.h"
#include "gui/widgets/MessagePage.h"
#include "gui/widgets/ModeCard.h"
#include "gui/widgets/PageHost.h"
#include "gui/widgets/ScreenSetupView.h"
#include "gui/widgets/StatusBar.h"
#include "net/FingerprintDatabase.h"
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHostAddress>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScreen>
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
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QUuid>
#include <utility>

// Same observer-only seam as R2UiPreview: no production migration hook runs.
namespace synergy::gui::migration {
void showNoticeIfPending(QWidget *)
{
}
} // namespace synergy::gui::migration

#ifdef Q_OS_MACOS
namespace {
bool allowIsolatedCoreAction = false;
}

// Replace the GUI permission check in this test executable only.
bool checkOSXAccessibilityPermission(bool)
{
  return allowIsolatedCoreAction;
}
#endif

namespace {
class WorkspaceWidthProbe final : public QObject
{
public:
  int widthChanges = 0;

protected:
  bool eventFilter(QObject *, QEvent *event) override
  {
    if (event->type() != QEvent::Resize)
      return false;
    const auto *resize = static_cast<QResizeEvent *>(event);
    if (resize->size().width() != resize->oldSize().width())
      ++widthChanges;
    // Stop a runaway layout recursion only after the invariant has already failed.
    return widthChanges > 8;
  }
};

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

void placeFullScreenCapFixtureAtScreenTop(MainWindow &window)
{
  auto *screen = window.screen();
  QVERIFY(screen);
  const int width = window.width();
  const QPoint position(window.frameGeometry().left(), screen->availableGeometry().top());
  window.move(position);
  QVERIFY(settleStaticRender(window));
  QTRY_COMPARE_WITH_TIMEOUT(window.frameGeometry().topLeft(), position, 1000);
  QCOMPARE(window.screen(), screen);
  QCOMPARE(window.width(), width);
}

void verifyEmbeddedFooter(MainWindow &window, QWidget *bottom, QWidget *actions, const QString &detail)
{
  auto *pages = window.findChild<QStackedWidget *>("pageStack");
  auto *status = window.findChild<StatusBar *>("statusBar");
  QVERIFY(pages && status && window.screen());
  auto *area = qobject_cast<QScrollArea *>(pages->currentWidget());
  QVERIFY(area && area->widget());
  QVERIFY2(bottom && actions && bottom->isVisibleTo(&window) && actions->isVisibleTo(&window), qPrintable(detail));
  QVERIFY(area->widget()->isAncestorOf(bottom));
  auto *viewport = area->viewport();
  const auto rect = [viewport](QWidget *widget) { return QRect(widget->mapTo(viewport, QPoint()), widget->size()); };
  QVERIFY(settleStaticRender(window));
  QList<QRect> previousGeometry;
  QElapsedTimer stable;
  const auto geometryStable = [&] {
    const QList<QRect> geometry{window.rect(), viewport->rect(), rect(bottom), rect(actions)};
    if (!stable.isValid() || geometry != previousGeometry) {
      previousGeometry = geometry;
      stable.start();
    }
    return stable.elapsed() >= 50;
  };
  QTRY_VERIFY_WITH_TIMEOUT(geometryStable(), 1000);
  QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
  auto *vertical = area->verticalScrollBar();
  const auto movePage = [&](Qt::Key direction, int end) {
    area->setFocus();
    QCOMPARE(window.focusWidget(), area);
    while (vertical->value() != end) {
      const int previous = vertical->value();
      QTest::keyClick(area, direction);
      QVERIFY(qAbs(vertical->value() - end) < qAbs(previous - end));
    }
  };
  const int overflow = vertical->maximum();
  if (overflow > 0) {
    // Only screen-capped content may use the workspace scrollbar.
    const int chrome = window.frameGeometry().height() - area->height();
    const int cap = qBound(1, window.screen()->availableGeometry().height() - chrome, 660);
    QVERIFY2(qAbs(area->height() - cap) <= 2, qPrintable(detail));
    QVERIFY2(area->widget()->height() > viewport->height(), qPrintable(detail));
    movePage(Qt::Key_PageDown, overflow);
    if (QTest::currentTestFailed())
      return;
  }
  const int gap = status->mapTo(&window, QPoint()).y() - bottom->mapTo(&window, QPoint()).y() - bottom->height();
  qInfo().noquote() << detail << "height" << window.height() << "bottom gap" << gap << "vertical overflow" << overflow;
  QVERIFY2(viewport->rect().contains(rect(actions)), qPrintable(detail));
  QVERIFY2(qAbs(gap - 10) <= 2, qPrintable(detail + QStringLiteral(" bottom gap %1 px").arg(gap)));
  if (overflow > 0)
    movePage(Qt::Key_PageUp, 0);
}

void verifyDialogFooter(MainWindow &window, QDialog *editor, const QString &detail)
{
  QVERIFY(editor);
  auto *buttons = editor->findChild<QDialogButtonBox *>();
  QVERIFY(buttons);
  auto *card = qobject_cast<QGroupBox *>(buttons->parentWidget());
  verifyEmbeddedFooter(window, card ? static_cast<QWidget *>(card) : buttons, buttons, detail);
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
  void coreStartFailureDescribesConfigPermissions()
  {
    const auto customPath = QDir(Settings::userDir()).filePath(QStringLiteral("custom-server.conf"));
    Settings::setValue(Settings::Server::ExternalConfigFile, customPath);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(pages);

    for (const bool externalConfig : {false, true}) {
      Settings::setValue(Settings::Server::ExternalConfig, externalConfig);
      window.m_coreProcess.error(synergy::gui::CoreProcess::Error::StartFailed);
      auto *editor = pages->currentWidget()->property("pageEditor").value<QDialog *>();
      auto *warning = qobject_cast<QMessageBox *>(editor);
      QVERIFY(warning);
      const auto mode = externalConfig ? QCoreApplication::translate("MainWindow", "read")
                                       : QCoreApplication::translate("MainWindow", "read and write");
      const auto configPath =
          externalConfig ? customPath : Settings::defaultValue(Settings::Server::ExternalConfigFile).toString();
      const auto expected = QCoreApplication::translate(
                                "MainWindow", "\nAdditionally, check you are able to %1 the server config file: %2"
      )
                                .arg(mode, configPath);
      QVERIFY2(warning->text().endsWith(expected), qPrintable(warning->text()));
      warning->reject();
      QCOMPARE(pages->currentIndex(), 0);
    }
  }
  void manualCoreActionReopensDeclinedPeerFingerprint_data()
  {
    QTest::addColumn<Settings::CoreMode>("role");
    QTest::newRow("client-connect") << Settings::Client;
    QTest::newRow("server-restart") << Settings::Server;
  }
  void manualCoreActionReopensDeclinedPeerFingerprint()
  {
    using synergy::core::ProcessState;
    using synergy::gui::CoreProcess;
    QFETCH(Settings::CoreMode, role);
    const bool isClient = role == Settings::Client;
    Settings::setValue(Settings::Core::CoreMode, role);
    Settings::setValue(Settings::Client::RemoteHost, QStringLiteral("192.0.2.1"));

    QTemporaryDir fixture(synergy::runtime::runtimeRoot() + QStringLiteral("/peer-fingerprint-XXXXXX"));
    QVERIFY(fixture.isValid());
    const auto isolatedSettings = fixture.filePath(QStringLiteral("settings.conf"));
    QVERIFY(QFile::copy(Settings::settingsFile(), isolatedSettings));
    Settings::setSettingsFile(isolatedSettings);

    MainWindow window("ts-flow-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    window.setAttribute(Qt::WA_DontShowOnScreen);
    auto &core = window.m_coreProcess;
    core.m_appPath = fixture.filePath(QStringLiteral("absent-core"));
    QVERIFY(!QFileInfo::exists(core.m_appPath));
#ifdef Q_OS_MACOS
    const QScopedValueRollback<bool> allowFixtureAction(allowIsolatedCoreAction, true);
#endif
    // The normal Start and Restart actions can run, but no core binary is available.
    QObject::disconnect(&core, &CoreProcess::error, &window, nullptr);
    window.open();
    QCOMPARE(core.mode(), role);

    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    auto *back = window.findChild<QPushButton *>("btnBack");
    auto *toggle = window.findChild<QPushButton *>("btnToggleCore");
    QVERIFY(pages && back && toggle);
    const auto currentPrompt = [pages] {
      return qobject_cast<FingerprintDialog *>(pages->currentWidget()->property("pageEditor").value<QDialog *>());
    };
    const auto drainStateUpdates = [&window] { QCoreApplication::sendPostedEvents(&window, QEvent::MetaCall); };
    const Fingerprint declined{QCryptographicHash::Sha256, QByteArray(32, '\x5a')};
    const auto peer = QString::fromLatin1(declined.data.toHex(':'));
    const auto otherRole = isClient ? Settings::Server : Settings::Client;
    const auto serverDb = Settings::tlsTrustedServersDb();
    const auto clientDb = Settings::tlsTrustedClientsDb();
    QVERIFY(!QFileInfo::exists(serverDb));
    QVERIFY(!QFileInfo::exists(clientDb));

    core.peerFingerprint(peer);
    auto *prompt = currentPrompt();
    QVERIFY(prompt);
    auto *buttons = prompt->findChild<QDialogButtonBox *>();
    QVERIFY(buttons && buttons->button(QDialogButtonBox::No));
    if (isClient)
      buttons->button(QDialogButtonBox::No)->click();
    else
      back->click();
    QCOMPARE(pages->count(), 1);
    core.peerFingerprint(peer);
    QCOMPARE(pages->count(), 1);

    // A new startup through the automatic path must keep the prior decision.
    QSignalSpy stateChanges(&core, &CoreProcess::processStateChanged);
    Settings::setValue(Settings::Gui::AutoStartCore, true);
    window.open();
    Settings::setValue(Settings::Gui::AutoStartCore, false);
    QVERIFY(!stateChanges.isEmpty());
    QCOMPARE(core.processState(), ProcessState::Stopped);
    drainStateUpdates();
    core.peerFingerprint(peer);
    QCOMPARE(pages->count(), 1);

    // Suppression belongs to the local role, even for an identical hash.
    core.setMode(otherRole);
    core.peerFingerprint(peer);
    QVERIFY(currentPrompt());
    back->click();
    core.setMode(role);

    auto *action = isClient ? window.m_actionStartCore : window.m_actionRestartCore;
    QVERIFY(window.m_menuFile->actions().contains(action));
    QVERIFY(window.m_trayIcon->contextMenu()->actions().contains(action));
    stateChanges.clear();
    if (isClient) {
      QVERIFY(toggle->isEnabled());
      toggle->click();
    } else {
      core.setProcessState(ProcessState::RetryPending);
      drainStateUpdates();
      stateChanges.clear();
      QVERIFY(action->isEnabled());
      action->trigger();
    }
    QVERIFY(!stateChanges.isEmpty());
    QCOMPARE(core.processState(), ProcessState::Stopped);
    drainStateUpdates();

    core.peerFingerprint(peer);
    prompt = currentPrompt();
    QVERIFY2(prompt, "A manual connection action should allow reconsidering this fingerprint.");
    if (!isClient) {
      core.setProcessState(ProcessState::RetryPending);
      drainStateUpdates();
    }
    QVERIFY(action->isEnabled());
    action->trigger();
    drainStateUpdates();
    core.peerFingerprint(peer);
    QCOMPARE(currentPrompt(), prompt);
    QCOMPARE(pages->count(), 2);
    back->click();
    core.peerFingerprint(peer);
    QCOMPARE(pages->count(), 1);

    core.setMode(otherRole);
    core.peerFingerprint(peer);
    QCOMPARE(pages->count(), 1);
    QVERIFY(!QFileInfo::exists(serverDb));
    QVERIFY(!QFileInfo::exists(clientDb));

    // Trusting a different peer persists only that fingerprint. The client
    // continuation starts the core without clearing the earlier rejection.
    core.setMode(role);
    const Fingerprint accepted{QCryptographicHash::Sha256, QByteArray(32, '\x6b')};
    const auto acceptedPeer = QString::fromLatin1(accepted.data.toHex(':'));
    core.peerFingerprint(acceptedPeer);
    prompt = currentPrompt();
    QVERIFY(prompt);
    buttons = prompt->findChild<QDialogButtonBox *>();
    QVERIFY(buttons && buttons->button(QDialogButtonBox::Yes));
    stateChanges.clear();
    buttons->button(QDialogButtonBox::Yes)->click();
    QCOMPARE(pages->count(), 1);
    QCOMPARE(!stateChanges.isEmpty(), isClient);
    drainStateUpdates();
    core.peerFingerprint(peer);
    core.peerFingerprint(acceptedPeer);
    QCOMPARE(pages->count(), 1);
    FingerprintDatabase trusted;
    trusted.read(isClient ? serverDb : clientDb);
    QVERIFY(trusted.isTrusted(accepted));
    QVERIFY(!trusted.isTrusted(declined));
    QVERIFY(!QFileInfo::exists(isClient ? clientDb : serverDb));
  }
  void serverLabelKeepsTheExplicitAddressWhenTheNetworkChanges()
  {
    const auto configured = QStringLiteral("192.0.2.2");
    Settings::setValue(Settings::Core::Interface, configured);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *monitor = window.findChild<synergy::gui::NetworkMonitor *>();
    auto *label = window.findChild<QLabel *>("lblIpAddresses");
    QVERIFY(monitor && label);

    monitor->setIpAddresses({QStringLiteral("192.0.2.20")});
    QCOMPARE(label->text(), QCoreApplication::translate("MainWindow", "Local IP") + QStringLiteral("  ") + configured);
    QCOMPARE(Settings::value(Settings::Core::Interface).toString(), configured);
  }
  void automaticServerLabelFollowsCandidatesWithoutSavingAnAddress()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *monitor = window.findChild<synergy::gui::NetworkMonitor *>();
    auto *label = window.findChild<QLabel *>("lblIpAddresses");
    QVERIFY(monitor && label);

    const auto prefix = QCoreApplication::translate("MainWindow", "Local IP") + QStringLiteral("  ");
    monitor->setIpAddresses({QStringLiteral("192.0.2.10")});
    QCOMPARE(label->text(), prefix + QStringLiteral("192.0.2.10"));
    monitor->setIpAddresses({QStringLiteral("192.0.2.11")});
    QCOMPARE(label->text(), prefix + QStringLiteral("192.0.2.11"));
    monitor->setIpAddresses({});
    QCOMPARE(label->text(), prefix + QStringLiteral("127.0.0.1"));
    QVERIFY(Settings::value(Settings::Core::Interface).toString().isEmpty());
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
    auto *general = editor->findChild<QGroupBox *>("generalCard");
    auto *buttons = editor->findChild<QDialogButtonBox *>("buttonBox");
    auto *status = window.findChild<StatusBar *>("statusBar");
    auto *area = qobject_cast<QScrollArea *>(pages->currentWidget());
    QVERIFY(tabs && general && buttons && status && area && window.screen());
    QCOMPARE(area->property("pageEditor").value<QDialog *>(), static_cast<QDialog *>(editor));
    QTRY_COMPARE(tabs->currentIndex(), 0);
    QCOMPARE(tabs->currentWidget()->objectName(), QStringLiteral("tabGeneral"));
    auto *tabBar = tabs->tabBar();
    QVERIFY(tabBar);
    QTRY_VERIFY(editor->isVisibleTo(&window) && tabBar->isVisibleTo(&window));
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
    QCOMPARE(window.width(), m_width);
    const auto generalSize = window.size();
    const auto verifyActiveTabFit = [&](const char *phase) {
      auto *card = qobject_cast<QGroupBox *>(buttons->parentWidget());
      const auto tabName = tabs->currentWidget()->objectName();
      const auto detail = tabName + QStringLiteral(" / ") + QString::fromLatin1(phase);
      QVERIFY2(card && tabs->currentWidget()->isAncestorOf(card) && card->isVisibleTo(&window), qPrintable(detail));
      QCOMPARE(window.width(), m_width);
      verifyEmbeddedFooter(window, card, buttons, detail);
      if (!QTest::currentTestFailed() && m_width == 500 && QLatin1String(phase) == QLatin1String("baseline"))
        capture(window, "preferences-fit-" + tabName);
    };
    const QStringList toggleNames{
        "cbPreventSleep", "cbShowVersion", "cbLogToFile", "cbTlsEnabled", "cbRunEnterCommand"
    };
    QCOMPARE(tabs->count(), toggleNames.size());
    for (int index = 0; index < tabs->count(); ++index) {
      QTest::mouseClick(tabBar, Qt::LeftButton, Qt::NoModifier, tabBar->tabRect(index).center());
      QTRY_COMPARE(tabs->currentIndex(), index);
      verifyActiveTabFit("baseline");
      if (QTest::currentTestFailed())
        return;
      auto *toggle = tabs->currentWidget()->findChild<QCheckBox *>(toggleNames.at(index));
      QVERIFY(toggle && toggle->isVisibleTo(&window) && toggle->isEnabled());
      const auto original = toggle->isChecked();
      QStyleOptionButton option;
      option.initFrom(toggle);
      const auto indicator = toggle->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, toggle);
      QVERIFY(!indicator.isEmpty());
      const auto click = indicator.center();
      QVERIFY(area->viewport()->rect().contains(toggle->mapTo(area->viewport(), click)));
      QTest::mouseClick(toggle, Qt::LeftButton, Qt::NoModifier, click);
      QTRY_COMPARE(toggle->isChecked(), !original);
      verifyActiveTabFit("toggled");
      if (QTest::currentTestFailed())
        return;
      QTest::mouseClick(toggle, Qt::LeftButton, Qt::NoModifier, click);
      QTRY_COMPARE(toggle->isChecked(), original);
      verifyActiveTabFit("restored");
      if (QTest::currentTestFailed())
        return;
    }
    QTest::mouseClick(tabBar, Qt::LeftButton, Qt::NoModifier, tabBar->tabRect(0).center());
    QTRY_COMPARE(tabs->currentIndex(), 0);
    QTRY_COMPARE(window.size(), generalSize);
    QTRY_VERIFY_WITH_TIMEOUT(topInsetSettled(), 1000);
    capture(window, "preferences-general-top-inset");
    auto *back = window.findChild<QPushButton *>("btnBack");
    QVERIFY(back);
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);
    QTRY_COMPARE(window.width(), m_width);
  }
  void compactHomeAndPreferencesRenderWithoutClipping()
  {
    Settings::setValue(Settings::Gui::LogExpanded, true);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    const auto mappedRect = [&window](QWidget *widget) {
      return QRect(widget->mapTo(&window, QPoint()), widget->size());
    };
    const auto imageMatchesWindow = [&window](const QImage &image) {
      const qreal dpr = window.devicePixelRatioF();
      return !image.isNull() && qAbs(image.devicePixelRatio() - dpr) < 0.001 &&
             qAbs(image.width() - window.width() * dpr) <= 1 && qAbs(image.height() - window.height() * dpr) <= 1;
    };
    const auto pixel = [](const QImage &image, const QPoint &logical) {
      const qreal dpr = image.devicePixelRatio();
      return image.pixelColor(qRound(logical.x() * dpr), qRound(logical.y() * dpr));
    };
    const auto colorNear = [](QColor actual, QColor expected) {
      return actual.alpha() == 255 && qAbs(actual.red() - expected.red()) <= 2 &&
             qAbs(actual.green() - expected.green()) <= 2 && qAbs(actual.blue() - expected.blue()) <= 2;
    };
    const auto distinctPaintPixels = [&pixel](const QImage &image, const QRect &rect) {
      const auto background = pixel(image, rect.topLeft() + QPoint(4, rect.height() / 2));
      int count = 0;
      for (int y = rect.top() + 4; y < rect.bottom() - 3; ++y)
        for (int x = rect.left() + 6; x < rect.right() - 5; ++x) {
          const auto color = pixel(image, QPoint(x, y));
          count += qAbs(color.red() - background.red()) + qAbs(color.green() - background.green()) +
                       qAbs(color.blue() - background.blue()) >
                   24;
        }
      return count;
    };
    QList<QRect> previousLayout;
    QElapsedTimer stableLayout;
    const auto layoutSettled = [&] {
      QList<QRect> current{window.rect()};
      for (auto *widget : window.findChildren<QWidget *>())
        if (widget->isVisibleTo(&window))
          current.append(mappedRect(widget));
      if (!stableLayout.isValid() || current != previousLayout) {
        previousLayout = current;
        stableLayout.start();
      }
      return stableLayout.elapsed() >= 50;
    };
    auto *workspace = window.findChild<QScrollArea *>("workspace");
    auto *status = window.findChild<QWidget *>("statusBar");
    auto *logPanel = window.findChild<QWidget *>("logPanel");
    auto *card = window.findChild<QFrame *>("card");
    auto *preferences = window.findChild<QPushButton *>("btnSettingsShortcut");
    auto *serverMode = window.findChild<ModeCard *>("rbModeServer");
    auto *clientMode = window.findChild<ModeCard *>("rbModeClient");
    auto *configure = window.findChild<QPushButton *>("btnConfigureServer");
    auto *start = window.findChild<QPushButton *>("btnToggleCore");
    auto *restart = window.findChild<QPushButton *>("btnRestartCore");
    QVERIFY(
        workspace && status && logPanel && card && preferences && serverMode && clientMode && configure && start &&
        restart
    );
    QTRY_COMPARE(window.width(), m_width);
    QVERIFY(settleStaticRender(window));
    QTRY_VERIFY_WITH_TIMEOUT(layoutSettled(), 1000);
    const auto home = window.grab().toImage();
    QVERIFY(imageMatchesWindow(home));
    QCOMPARE(window.size(), QSize(m_width, 390));
    capture(window, "strict-home");
    const QColor panel(synergy::gui::isDarkMode() ? "#242427" : "#ffffff");
    const QColor idleMode(synergy::gui::isDarkMode() ? "#28292e" : "#f0f0f3");
    QVERIFY(colorNear(pixel(home, card->mapTo(&window, QPoint(card->width() / 2, 5))), panel));
    QVERIFY(colorNear(pixel(home, clientMode->mapTo(&window, QPoint(clientMode->width() - 8, 16))), idleMode));
    const QList<QWidget *> homeControls{serverMode, clientMode, configure, start, restart};
    for (int i = 0; i < homeControls.size(); ++i) {
      auto *control = homeControls.at(i);
      QVERIFY(control->isVisibleTo(&window));
      QVERIFY2(mappedRect(workspace->viewport()).contains(mappedRect(control)), qPrintable(control->objectName()));
      QVERIFY2(distinctPaintPixels(home, mappedRect(control)) > 8, qPrintable(control->objectName()));
      for (int j = i + 1; j < homeControls.size(); ++j)
        QVERIFY(!mappedRect(control).intersects(mappedRect(homeControls.at(j))));
    }
    QCOMPARE(workspace->horizontalScrollBar()->maximum(), 0);
    const int homeBottomGap = mappedRect(status).top() - mappedRect(logPanel).bottom() - 1;
    QVERIFY2(homeBottomGap >= 0 && homeBottomGap <= 100, qPrintable(QString::number(homeBottomGap)));

    QTest::mouseClick(preferences, Qt::LeftButton);
    QTRY_VERIFY(window.findChild<SettingsDialog *>() != nullptr);
    auto *editor = window.findChild<SettingsDialog *>();
    auto *general = editor->findChild<QGroupBox *>("generalCard");
    auto *tabs = editor->findChild<QTabWidget *>("tabWidget");
    auto *buttons = editor->findChild<QDialogButtonBox *>("buttonBox");
    auto *language = editor->findChild<QComboBox *>("comboLanguage");
    auto *tray = editor->findChild<QComboBox *>("comboTrayIcon");
    auto *sleep = editor->findChild<QCheckBox *>("cbPreventSleep");
    auto *updates = editor->findChild<QCheckBox *>("checkForUpdates");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(general && tabs && buttons && language && tray && sleep && updates && pages);
    auto *area = qobject_cast<QScrollArea *>(pages->currentWidget());
    QVERIFY(area);
    QTRY_COMPARE(window.width(), m_width);
    QVERIFY(settleStaticRender(window));
    stableLayout.invalidate();
    QTRY_VERIFY_WITH_TIMEOUT(layoutSettled(), 1000);
    const auto settings = window.grab().toImage();
    QVERIFY(imageMatchesWindow(settings));
    capture(window, "strict-preferences");
    QVERIFY(colorNear(pixel(settings, general->mapTo(&window, QPoint(6, general->height() / 2))), panel));
    const QList<QWidget *> settingsControls{tabs->tabBar(), sleep, updates, language, tray, buttons};
    for (int i = 0; i < settingsControls.size(); ++i) {
      auto *control = settingsControls.at(i);
      QVERIFY(control->isVisibleTo(&window));
      QVERIFY2(mappedRect(area->viewport()).contains(mappedRect(control)), qPrintable(control->objectName()));
      QVERIFY2(distinctPaintPixels(settings, mappedRect(control)) > 8, qPrintable(control->objectName()));
      for (int j = i + 1; j < settingsControls.size(); ++j)
        QVERIFY(!mappedRect(control).intersects(mappedRect(settingsControls.at(j))));
    }
    QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
    QCOMPARE(area->verticalScrollBar()->maximum(), 0);
    const int settingsBottomGap = mappedRect(status).top() - mappedRect(general).bottom() - 1;
    QVERIFY2(qAbs(settingsBottomGap - 10) <= 2, qPrintable(QString::number(settingsBottomGap)));
    const auto settingsSize = window.size();
    window.hide();
    window.open();
    QTRY_COMPARE(window.width(), m_width);
    QTRY_COMPARE(window.size(), settingsSize);
    auto *back = window.findChild<QPushButton *>("btnBack");
    QVERIFY(back);
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);
    QTRY_COMPARE(window.width(), m_width);
    window.hide();
    window.open();
    QTRY_COMPARE(window.width(), m_width);
  }
  void viewMenuTogglesTheActualLogPanel()
  {
    Settings::setValue(Settings::Gui::LogExpanded, true);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *view = window.findChild<QMenu *>("menuView");
    auto *log = window.findChild<LogPanel *>("logPanel");
    QVERIFY(view && log);
    QCOMPARE(view->actions().size(), 1);
    auto *toggle = view->actions().first();
    QVERIFY(toggle->isCheckable());
    for (const bool collapsed : {true, false}) {
      QCOMPARE(log->isCollapsed(), !collapsed);
      view->popup(window.mapToGlobal(QPoint(12, 36)));
      QTRY_VERIFY(view->isVisible());
      const QRect item = view->actionGeometry(toggle);
      QVERIFY(item.isValid());
      QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, item.center());
      QTRY_COMPARE(log->isCollapsed(), collapsed);
      QTRY_VERIFY(!view->isVisible());
      QCOMPARE(toggle->isChecked(), !collapsed);
    }
  }
  void logActionsExplainEmptyCopyAndConfirmClear()
  {
    Settings::setValue(Settings::Gui::LogExpanded, true);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *log = window.findChild<LogPanel *>("logPanel");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(log && pages);
    auto *hint = log->findChild<QLabel *>("logHint");
    auto *records = log->findChild<LogWidget *>();
    const auto button = [log](const char *source) -> QPushButton * {
      const auto label = QCoreApplication::translate("LogPanel", source);
      for (auto *action : log->findChildren<QPushButton *>())
        if (action->accessibleName() == label)
          return action;
      return nullptr;
    };
    auto *copy = button("Copy Log");
    auto *clear = button("Clear Log");
    QVERIFY(hint && records && copy && clear);
    records->clear();
    const auto emptyFeedback = QCoreApplication::translate("LogPanel", "No log entries to copy");
    QTest::mouseClick(copy, Qt::LeftButton);
    QCOMPARE(hint->text(), emptyFeedback);
    QTest::mouseClick(clear, Qt::LeftButton);
    QCOMPARE(pages->count(), 1);

    log->appendLine(QStringLiteral("[00:00:01] INFO: keep until confirmed"));
    const auto history = records->allText();
    QVERIFY(history.contains(QStringLiteral("keep until confirmed")));
    records->setLevelFilter(QStringLiteral("error"));
    QVERIFY(records->allText().isEmpty());
    QTest::mouseClick(copy, Qt::LeftButton);
    QCOMPARE(hint->text(), emptyFeedback);

    for (const auto choice : {QMessageBox::Cancel, QMessageBox::Yes}) {
      QTest::mouseClick(clear, Qt::LeftButton);
      auto *prompt = window.findChild<QMessageBox *>("confirmClearLog");
      QVERIFY(prompt);
      QCOMPARE(pages->count(), 2);
      QCOMPARE(prompt->defaultButton(), prompt->button(QMessageBox::Cancel));
      QVERIFY(records->hasHistory());
      QTest::mouseClick(prompt->button(choice), Qt::LeftButton);
      QTRY_VERIFY(!window.findChild<QMessageBox *>("confirmClearLog"));
      QCOMPARE(pages->count(), 1);
      records->setLevelFilter({});
      QCOMPARE(records->allText(), choice == QMessageBox::Cancel ? history : QString());
      records->setLevelFilter(QStringLiteral("error"));
    }
  }
  void collapsedLogFitsHomeWithoutExtraSpace()
  {
    Settings::setValue(Settings::Gui::LogExpanded, true);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *workspace = window.findChild<QScrollArea *>("workspace");
    auto *log = window.findChild<LogPanel *>("logPanel");
    auto *status = window.findChild<StatusBar *>("statusBar");
    auto *addresses = window.findChild<QLabel *>("lblIpAddresses");
    auto *preferences = window.findChild<QPushButton *>("btnSettingsShortcut");
    auto *back = window.findChild<QPushButton *>("btnBack");
    QVERIFY(workspace && log && status && addresses && preferences && back);
    addresses->setText(QCoreApplication::translate("MainWindow", "Local IP") + QStringLiteral("  192.0.2.10"));
    status->setStatus(ConnectionState::Listening, ProcessState::Started, true);
    auto *statusText = status->findChild<QLabel *>("statusText");
    QVERIFY(statusText);
    log->appendLine(QStringLiteral("[10:15:00] INFO: ") + statusText->text());
    const auto logButton = [log](const char *label) -> QPushButton * {
      const auto tooltip = QCoreApplication::translate("LogPanel", label);
      for (auto *control : log->findChildren<QPushButton *>())
        if (control->toolTip() == tooltip)
          return control;
      return nullptr;
    };
    auto *collapse = logButton("Collapse Log");
    auto *expand = logButton("Expand Log");
    QVERIFY(collapse && expand);
    QVERIFY(settleStaticRender(window));
    const int expandedHeight = window.height();
    QTest::mouseClick(collapse, Qt::LeftButton);
    QTRY_VERIFY(log->isCollapsed());
    QVERIFY(settleStaticRender(window));

    const int gap = status->mapTo(&window, QPoint()).y() - log->mapTo(&window, QPoint()).y() - log->height();
    QVERIFY2(qAbs(gap - 10) <= 2, qPrintable(QStringLiteral("Gap below collapsed log: %1 px").arg(gap)));
    QVERIFY(window.height() < expandedHeight);
    QCOMPARE(window.width(), m_width);
    QCOMPARE(window.minimumWidth(), 500);
    QCOMPARE(workspace->verticalScrollBar()->maximum(), 0);
    const QRect logRect(log->mapTo(workspace->viewport(), QPoint()), log->size());
    QVERIFY(workspace->viewport()->rect().contains(logRect));
    capture(window, "home-collapsed-log");

    const int collapsedHeight = window.height();
    QTest::mouseClick(preferences, Qt::LeftButton);
    QTRY_VERIFY(back->isVisibleTo(&window));
    QVERIFY(settleStaticRender(window));
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_COMPARE(window.height(), collapsedHeight);
    QTest::mouseClick(expand, Qt::LeftButton);
    QTRY_VERIFY(!log->isCollapsed());
    QTRY_COMPARE(window.height(), expandedHeight);
    QCOMPARE(window.minimumHeight(), 390);
    QCOMPARE(window.width(), m_width);
  }
  void measuringHomePagePreservesScrollOwnedWidth()
  {
    Settings::setValue(Settings::Gui::LogExpanded, true);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *host = window.findChild<PageHost *>();
    auto *workspace = window.findChild<QScrollArea *>("workspace");
    auto *log = window.findChild<LogPanel *>("logPanel");
    auto *preferences = window.findChild<QPushButton *>("btnSettingsShortcut");
    auto *back = window.findChild<QPushButton *>("btnBack");
    QVERIFY(host && workspace && log && preferences && back);
    auto *content = workspace->widget();
    auto *table = log->findChild<QTableWidget *>("logRecords");
    QVERIFY(content && workspace->widgetResizable() && table);

    const auto message = QStringLiteral("Workspace width measurement and wrapped log fixture ").repeated(10);
    for (int index = 0; index < 9; ++index)
      log->appendLine(QStringLiteral("[10:15:00] INFO: ") + message + QString::number(index));
    const auto rows = table->findItems(message + QStringLiteral("8"), Qt::MatchExactly);
    QCOMPARE(rows.size(), 1);
    const int row = rows.first()->row();
    QTRY_VERIFY(table->isVisibleTo(&window));
    QTRY_VERIFY(table->rowHeight(row) > table->fontMetrics().height() * 2);
    QTRY_VERIFY(table->verticalScrollBar()->maximum() > 0);

    const int managedWidth = content->width();
    QVERIFY(managedWidth > 0);
    WorkspaceWidthProbe probe;
    content->installEventFilter(&probe);
    const int preferredHeight = host->preferredContentHeight(managedWidth + 160);
    content->removeEventFilter(&probe);

    QVERIFY(preferredHeight > 0);
    QCOMPARE(probe.widthChanges, 0);
    QCOMPARE(content->width(), managedWidth);

    const int resizedWindowWidth = m_width == 500 ? 720 : 500;
    window.resize(resizedWindowWidth, window.height());
    QTRY_COMPARE(window.width(), resizedWindowWidth);
    QTRY_VERIFY(content->width() != managedWidth);
    QTRY_COMPARE(content->width(), workspace->viewport()->width());
    QTRY_VERIFY(table->rowHeight(row) > table->fontMetrics().height() * 2);

    QTest::mouseClick(preferences, Qt::LeftButton);
    QTRY_VERIFY(host->hasEditor());
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_VERIFY(!host->hasEditor());
    QTRY_VERIFY(table->isVisibleTo(&window));
    QCOMPARE(table->item(row, 2)->text(), message + QStringLiteral("8"));
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
    auto *addresses = window.findChild<QLabel *>("lblIpAddresses");
    QVERIFY(edit && save && cancel && field && label && addresses);
    addresses->setText(QCoreApplication::translate("MainWindow", "Local IP") + QStringLiteral("  192.0.2.10"));
    QTest::mouseClick(edit, Qt::LeftButton);
    QVERIFY(field->isVisibleTo(&window));
    QCOMPARE(window.width(), m_width);
    field->setText("Uncommitted-Fixture");
    cancel->setFocus();
    QCoreApplication::processEvents();
    QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), QString("Flow-Fixture"));
    QVERIFY(settleStaticRender(window));
    QVERIFY(save->isVisibleTo(&window) && cancel->isVisibleTo(&window));
    QVERIFY(save->isEnabled());
    if (m_width == 500)
      capture(window, "name-editing");

    const auto editing = window.grab().toImage();
    QVERIFY(!editing.isNull());
    // The upper center of each button is inside the fill and clear of the icon and text.
    const auto backgroundPoint = [&window, &editing](const QPushButton *control) {
      const auto point = control->mapTo(&window, QPoint(control->width() / 2, 5));
      return (QPointF(point) * editing.devicePixelRatio()).toPoint();
    };
    const auto savePoint = backgroundPoint(save);
    const auto cancelPoint = backgroundPoint(cancel);
    QVERIFY(editing.rect().contains(savePoint) && editing.rect().contains(cancelPoint));
    const auto saveColor = editing.pixelColor(savePoint);
    const auto cancelColor = editing.pixelColor(cancelPoint);
    const bool dark = synergy::gui::isDarkMode();
    qInfo() << "Name editor button backgrounds" << "Save" << saveColor.name() << "Cancel" << cancelColor.name();
    QCOMPARE(cancelColor, QColor(dark ? "#2a2a2e" : "#ececf0"));
    QCOMPARE(saveColor, QColor(dark ? "#4cc38a" : "#2f9e6b"));

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
  void keyboardOpensOccupiedScreenWithoutCommittingDraft()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    const QString originalName = QStringLiteral("Keyboard-Fixture");
    const QString draftName = QStringLiteral("Keyboard-Fixture-Draft");
    window.serverConfig().addClient(originalName);

    auto *open = window.findChild<QPushButton *>("btnConfigureServer");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(open && pages);
    QTest::mouseClick(open, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 1);
    auto *parent = window.findChild<ServerConfigDialog *>();
    QVERIFY(parent);
    auto *view = parent->findChild<ScreenSetupView *>();
    QVERIFY(view && view->model());
    auto *model = view->model();

    QModelIndex occupied;
    QModelIndex blank;
    for (int row = 0; row < model->rowCount(); ++row) {
      for (int column = 0; column < model->columnCount(); ++column) {
        const auto index = model->index(row, column);
        const auto contents = model->data(index);
        if (contents.toString() == originalName)
          occupied = index;
        if (!blank.isValid() && !contents.isValid())
          blank = index;
      }
    }
    QVERIFY(occupied.isValid() && blank.isValid());
    QVERIFY(settleStaticRender(window));
    const auto center = view->visualRect(occupied).center();
    QVERIFY(view->viewport()->rect().contains(center));
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, center);
    QCOMPARE(view->currentIndex(), occupied);
    QCOMPARE(pages->count(), 2);
    QTest::keyClick(view, Qt::Key_Space);
    QCOMPARE(pages->count(), 2);

    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
      const auto modifiers = key == Qt::Key_Enter ? Qt::KeypadModifier : Qt::NoModifier;
      view->setCurrentIndex(occupied);
      view->setFocus();
      QTest::keyClick(view, key, modifiers);
      QTRY_COMPARE(pages->currentIndex(), 2);
      QCOMPARE(pages->count(), 3);
      auto *pageEditor = pages->currentWidget()->property("pageEditor").value<QDialog *>();
      auto *child = qobject_cast<ScreenSettingsDialog *>(pageEditor);
      QVERIFY(child);
      auto *name = child->findChild<QLineEdit *>("lineNameEdit");
      auto *actions = child->findChild<QDialogButtonBox *>();
      QVERIFY(name && actions && actions->button(QDialogButtonBox::Cancel));
      QCOMPARE(name->text(), originalName);
      name->setText(draftName);
      QTest::mouseClick(actions->button(QDialogButtonBox::Cancel), Qt::LeftButton);
      QTRY_COMPARE(pages->currentIndex(), 1);
      QCOMPARE(pages->count(), 2);
      QCOMPARE(model->data(occupied).toString(), originalName);
      QVERIFY(window.serverConfig().screenExists(originalName));
      QVERIFY(!window.serverConfig().screenExists(draftName));

      view->setCurrentIndex(occupied);
      view->setFocus();
      QCOMPARE(view->currentIndex(), occupied);
      QKeyEvent repeat(QEvent::KeyPress, key, modifiers, {}, true);
      QCoreApplication::sendEvent(view, &repeat);
      QCOMPARE(pages->currentIndex(), 1);
      QCOMPARE(pages->count(), 2);

      view->setCurrentIndex(blank);
      view->setFocus();
      QTest::keyClick(view, key, modifiers);
      QCOMPARE(pages->currentIndex(), 1);
      QCOMPARE(pages->count(), 2);
      QVERIFY(!model->data(blank).isValid());

      view->setCurrentIndex(QModelIndex());
      QVERIFY(!view->currentIndex().isValid());
      QTest::keyClick(view, key, modifiers);
      QCOMPARE(pages->currentIndex(), 1);
      QCOMPARE(pages->count(), 2);
    }
    window.close();
  }
  void sharingTabFitsBelowCurrentWindowPosition()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width, true);
    QVERIFY2(QTest::qWaitForWindowExposed(&window), "Fixture window was not exposed");
    auto *screen = window.screen();
    QVERIFY(screen);
    const QRect available = screen->availableGeometry();
    constexpr int inset = 4;
    const bool useNarrowFixture = QGuiApplication::platformName() == "offscreen" && m_width == 820 &&
                                  window.frameGeometry().width() + 2 * inset > available.width();
    const int width = useNarrowFixture ? 600 : m_width;
    if (useNarrowFixture)
      window.resize(width, window.height());
    QTRY_COMPARE(window.width(), width);
    const int frameWidth = window.frameGeometry().width();
    QVERIFY2(frameWidth + 2 * inset <= available.width(), "Fixture width does not fit the work area");
    auto *open = window.findChild<QPushButton *>("btnConfigureServer");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    auto *status = window.findChild<StatusBar *>("statusBar");
    QVERIFY(open && pages && status);
    auto *home = qobject_cast<QScrollArea *>(pages->currentWidget());
    QVERIFY(home);

    const auto globalRect = [](QWidget *widget) { return QRect(widget->mapToGlobal(QPoint()), widget->size()); };
    const auto settle = [&](QScrollArea *page) {
      QVERIFY(settleStaticRender(window));
      QList<QRect> previous;
      QElapsedTimer stable;
      const auto geometryStable = [&] {
        const QList<QRect> current{window.frameGeometry(), globalRect(status), globalRect(page->viewport())};
        if (!stable.isValid() || current != previous) {
          previous = current;
          stable.start();
        }
        return stable.elapsed() >= 50;
      };
      QTRY_VERIFY_WITH_TIMEOUT(geometryStable(), 1000);
    };
    const auto checkFrame = [&](const char *phase, const QPoint &position, QScrollArea *page) {
      const QRect frame = window.frameGeometry();
      const QRect statusRect = globalRect(status);
      qInfo() << phase << QGuiApplication::platformName() << I18N::currentLanguage() << "width" << width << "available"
              << available << "expected position" << position << "frame" << frame << "viewport"
              << globalRect(page->viewport()) << "status" << statusRect << "scroll"
              << page->verticalScrollBar()->value() << "of" << page->verticalScrollBar()->maximum();
      QCOMPARE(window.screen(), screen);
      QCOMPARE(screen->availableGeometry(), available);
      QCOMPARE(window.width(), width);
      QCOMPARE(frame.width(), frameWidth);
      QVERIFY2(frame.topLeft() == position, "Content fitting moved the window");
      QVERIFY2(available.contains(frame), "Content fitting placed the window frame outside the work area");
      QVERIFY(status->isVisibleTo(&window) && !statusRect.isEmpty());
      QVERIFY(available.contains(statusRect) && frame.contains(statusRect));
    };

    const QPoint top(available.left() + (available.width() - frameWidth) / 2, available.top() + inset);
    window.move(top);
    settle(home);
    if (QTest::currentTestFailed())
      return;
    checkFrame("top home", top, home);
    if (QTest::currentTestFailed())
      return;
    const int homeHeight = window.frameGeometry().height();
    const int homeMinimumFrameHeight = window.minimumHeight() + homeHeight - window.height();
    QVERIFY(open->isVisibleTo(&window) && globalRect(home->viewport()).contains(globalRect(open)));
    QTest::mouseClick(open, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 1);
    auto *area = qobject_cast<QScrollArea *>(pages->currentWidget());
    QVERIFY(area && area->widget());
    auto *editor = qobject_cast<ServerConfigDialog *>(area->property("pageEditor").value<QDialog *>());
    QVERIFY(editor);
    auto *tabs = editor->findChild<QTabWidget *>("tabWidget");
    auto *shortTab = editor->findChild<QWidget *>("tabConfigFile");
    auto *longTab = editor->findChild<QWidget *>("tabAdvanced");
    auto *buttons = editor->findChild<QDialogButtonBox *>("buttonBox");
    QVERIFY(tabs && shortTab && longTab && buttons);
    auto *ok = buttons->button(QDialogButtonBox::Ok);
    auto *cancel = buttons->button(QDialogButtonBox::Cancel);
    QVERIFY(ok && cancel);
    auto *scroll = area->verticalScrollBar();

    const auto selectTab = [&](QWidget *tab) {
      auto *bar = tabs->tabBar();
      const int index = tabs->indexOf(tab);
      QVERIFY(index >= 0 && bar->isVisibleTo(&window));
      const QPoint click = bar->tabRect(index).center();
      QVERIFY(bar->rect().contains(click));
      QVERIFY(globalRect(area->viewport()).intersected(available).contains(bar->mapToGlobal(click)));
      QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, click);
      QTRY_COMPARE(tabs->currentWidget(), tab);
      QVERIFY(pages->currentWidget() == area);
      settle(area);
    };
    const auto scrollPage = [&](Qt::Key key, int end) {
      area->setFocus();
      QCOMPARE(window.focusWidget(), area);
      while (scroll->value() != end) {
        const int before = scroll->value();
        QTest::keyClick(area, key);
        QVERIFY(qAbs(scroll->value() - end) < qAbs(before - end));
      }
    };
    const auto checkFooter = [&] {
      const QRect viewport = globalRect(area->viewport()).intersected(available);
      const QRect okRect = globalRect(ok);
      const QRect cancelRect = globalRect(cancel);
      qInfo() << "footer" << "visible viewport" << viewport << "OK" << okRect << "Cancel" << cancelRect;
      QVERIFY(ok->isVisibleTo(&window) && cancel->isVisibleTo(&window));
      QVERIFY(!okRect.isEmpty() && viewport.contains(okRect));
      QVERIFY(!cancelRect.isEmpty() && viewport.contains(cancelRect));
      const auto *card = qobject_cast<QGroupBox *>(buttons->parentWidget());
      QVERIFY(card && tabs->currentWidget()->isAncestorOf(card));
      const int gap = globalRect(status).top() - card->mapToGlobal(QPoint()).y() - card->height();
      QVERIFY2(qAbs(gap - 10) <= 2, qPrintable(QStringLiteral("Editor footer gap: %1 px").arg(gap)));
      QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
    };
    const auto revealLongFooter = [&](const char *phase, const QPoint &position, bool requiresScroll) {
      checkFrame(phase, position, area);
      if (QTest::currentTestFailed())
        return;
      QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
      if (requiresScroll) {
        QVERIFY2(scroll->maximum() > 0, "The capped editor must scroll its outer workspace");
        QVERIFY(area->widget()->height() > area->viewport()->height());
      }
      if (scroll->maximum() > 0) {
        QVERIFY(scroll->value() < scroll->maximum());
        scrollPage(Qt::Key_PageDown, scroll->maximum());
        if (QTest::currentTestFailed())
          return;
        settle(area);
        QCOMPARE(scroll->value(), scroll->maximum());
      }
      checkFooter();
      checkFrame(phase, position, area);
    };

    settle(area);
    selectTab(shortTab);
    checkFrame("top short", top, area);
    checkFooter();
    if (QTest::currentTestFailed())
      return;
    const int shortHeight = window.frameGeometry().height();
    const int shortScrollRange = scroll->maximum();
    const int shortScrollValue = scroll->value();
    selectTab(longTab);
    if (QTest::currentTestFailed())
      return;
    revealLongFooter("top long", top, false);
    if (QTest::currentTestFailed())
      return;
    scrollPage(Qt::Key_PageUp, 0);
    if (QTest::currentTestFailed())
      return;
    const int longHeight = window.frameGeometry().height();

    const QSize longFrameSize(frameWidth, longHeight);
    const int centeredY = available.top() + (available.height() - longHeight) / 2;
    QPoint ordinary(top.x(), qMax(top.y() + 1, centeredY));
    const bool verticalControl = available.contains(QRect(ordinary, longFrameSize));
    if (!verticalControl) {
      const int left = available.left() + inset;
      ordinary = QPoint(left == top.x() ? top.x() + 1 : left, top.y());
    }
    QVERIFY(ordinary != top && available.contains(QRect(ordinary, longFrameSize)));
    qInfo() << (verticalControl ? "Vertical position control"
                                : "Horizontal position control; vertical ordinary unavailable")
            << "top" << top << "second position" << ordinary << "measured long frame size" << longFrameSize;
    selectTab(shortTab);
    window.move(ordinary);
    settle(area);
    checkFrame("second position short", ordinary, area);
    if (QTest::currentTestFailed())
      return;
    QCOMPARE(window.frameGeometry().height(), shortHeight);
    selectTab(longTab);
    if (QTest::currentTestFailed())
      return;
    revealLongFooter("second position long", ordinary, false);
    if (QTest::currentTestFailed())
      return;
    scrollPage(Qt::Key_PageUp, 0);
    if (QTest::currentTestFailed())
      return;
    QCOMPARE(window.frameGeometry().height(), longHeight);

    selectTab(shortTab);
    if (QTest::currentTestFailed())
      return;
    const int decorations = window.frameGeometry().height() - window.height();
    const int lowerBound = qMax(qMax(shortHeight, homeHeight), qMax(390 + decorations, homeMinimumFrameHeight));
    QVERIFY2(longHeight - lowerBound > 2 * inset, "Fixture has no distinct short and tall window heights");
    const int budget = lowerBound + (longHeight - lowerBound) / 2;
    const QPoint low(top.x(), available.bottom() + 1 - budget);
    window.move(low);
    settle(area);
    checkFrame("low short precondition", low, area);
    if (QTest::currentTestFailed())
      return;
    QCOMPARE(window.frameGeometry().height(), shortHeight);
    const int remaining = available.bottom() + 1 - window.frameGeometry().top();
    qInfo() << "low precondition" << "remaining" << remaining << "minimum" << lowerBound << "short" << shortHeight
            << "long" << longHeight;
    QVERIFY(remaining >= lowerBound + inset && remaining <= longHeight - inset);

    selectTab(longTab);
    if (QTest::currentTestFailed())
      return;
    revealLongFooter("low long", low, true);
    if (QTest::currentTestFailed())
      return;
    const bool originalDoubleTap = window.serverConfig().hasSwitchDoubleTap();
    auto *doubleTap = longTab->findChild<QCheckBox *>("cbSwitchDoubleTap");
    QVERIFY(doubleTap && doubleTap->isVisibleTo(&window) && doubleTap->isEnabled());
    QCOMPARE(doubleTap->isChecked(), originalDoubleTap);
    QStyleOptionButton option;
    option.initFrom(doubleTap);
    const QRect indicator = doubleTap->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, doubleTap);
    QVERIFY(!indicator.isEmpty());
    QVERIFY(globalRect(area->viewport()).intersected(available).contains(doubleTap->mapToGlobal(indicator.center())));
    QTest::mouseClick(doubleTap, Qt::LeftButton, Qt::NoModifier, indicator.center());
    QCOMPARE(doubleTap->isChecked(), !originalDoubleTap);
    QVERIFY(ok->isEnabled());
    scrollPage(Qt::Key_PageUp, 0);
    if (QTest::currentTestFailed())
      return;
    selectTab(shortTab);
    checkFrame("low short restored", low, area);
    QCOMPARE(window.frameGeometry().height(), shortHeight);
    QCOMPARE(scroll->maximum(), shortScrollRange);
    QCOMPARE(scroll->value(), shortScrollValue);
    checkFooter();
    if (QTest::currentTestFailed())
      return;

    QSignalSpy finished(editor, &QDialog::finished);
    QVERIFY(cancel->isEnabled());
    QTest::mouseClick(cancel, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toInt(), int(QDialog::Rejected));
    QCOMPARE(window.serverConfig().hasSwitchDoubleTap(), originalDoubleTap);
    settle(home);
    checkFrame("low home restored", low, home);
    QCOMPARE(window.frameGeometry().height(), homeHeight);
  }
  void selectedScreenDeletionIsDraftAndPrimaryProtected()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    placeFullScreenCapFixtureAtScreenTop(window);
    if (QTest::currentTestFailed())
      return;
    window.serverConfig().addClient("Delete-Fixture");
    auto *open = window.findChild<QPushButton *>("btnConfigureServer");
    auto *back = window.findChild<QPushButton *>("btnBack");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(open && back && pages);
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
      if (!save && m_width == 500) {
        auto *tabs = editor->findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 4);
        for (int index = 0; index < tabs->count(); ++index) {
          QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier, tabs->tabBar()->tabRect(index).center());
          QTRY_COMPARE(tabs->currentIndex(), index);
          const auto name = "server-fit-" + tabs->currentWidget()->objectName();
          verifyDialogFooter(window, editor, name);
          if (QTest::currentTestFailed())
            return;
          capture(window, name);
        }
        auto *file = editor->findChild<QLineEdit *>("lineConfigFile");
        auto *actions = editor->findChild<QDialogButtonBox *>();
        QVERIFY(file && actions);
        const auto invalid = Settings::userDir() + "/missing-server.conf";
        QTest::mouseClick(external, Qt::LeftButton, Qt::NoModifier, QPoint(8, external->height() / 2));
        QVERIFY(external->isChecked());
        for (int index = 0; index < 3; ++index)
          QVERIFY(!tabs->isTabEnabled(index));
        auto *hint = editor->findChild<QLabel *>("label_6");
        QVERIFY(
            hint && hint->text().contains(
                        QCoreApplication::translate(
                            "ServerConfigDialog", "Clear the checkbox to edit the layout, hotkeys and sharing here."
                        )
                    )
        );
        QTest::mouseClick(external, Qt::LeftButton, Qt::NoModifier, QPoint(8, external->height() / 2));
        QVERIFY(!external->isChecked());
        for (int index = 0; index < 3; ++index)
          QVERIFY(tabs->isTabEnabled(index));
        external->setChecked(true);
        file->setText(invalid);
        auto *saveConfig = actions->button(QDialogButtonBox::Ok);
        QVERIFY(saveConfig && saveConfig->isEnabled());
        QTest::mouseClick(saveConfig, Qt::LeftButton);
        QTRY_COMPARE(pages->currentIndex(), 2);
        auto *confirmation = window.findChild<QMessageBox *>();
        verifyDialogFooter(window, confirmation, "server-fit-invalid-file");
        if (QTest::currentTestFailed())
          return;
        capture(window, "server-fit-invalid-file");
        QEvent restyle(QEvent::StyleChange);
        QCoreApplication::sendEvent(confirmation, &restyle);
        verifyDialogFooter(window, confirmation, "server-fit-invalid-file-after-style-change");
        if (QTest::currentTestFailed())
          return;
        QTest::mouseClick(back, Qt::LeftButton);
        QTRY_COMPARE(pages->currentIndex(), 1);
        QCOMPARE(file->text(), invalid);
        external->setChecked(false);
        QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier, tabs->tabBar()->tabRect(0).center());
        QTRY_COMPARE(tabs->currentIndex(), 0);
        QVERIFY(settleStaticRender(window));
        QVERIFY(select(view, "Delete-Fixture"));
        const auto computer = view->visualRect(view->currentIndex()).center();
        QVERIFY(view->viewport()->rect().contains(computer));
        QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, computer);
        QTRY_COMPARE(pages->currentIndex(), 2);
        auto *screen = window.findChild<ScreenSettingsDialog *>();
        QVERIFY(screen);
        auto *screenTabs = screen->findChild<QTabWidget *>();
        QVERIFY(screenTabs);
        QCOMPARE(screenTabs->count(), 4);
        for (int index = 0; index < screenTabs->count(); ++index) {
          auto *bar = screenTabs->tabBar();
          QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(index).center());
          QTRY_COMPARE(screenTabs->currentIndex(), index);
          const auto name = "screen-fit-tab-" + QString::number(index);
          verifyDialogFooter(window, screen, name);
          if (QTest::currentTestFailed())
            return;
          capture(window, name);
        }
        QTest::mouseClick(back, Qt::LeftButton);
        QTRY_COMPARE(pages->currentIndex(), 1);
      }
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
    if (m_width == 500) {
      auto *clientMode = window.findChild<ModeCard *>("rbModeClient");
      auto *clientSetup = window.findChild<QPushButton *>("btnConfigureClient");
      QVERIFY(clientMode && clientSetup);
      QTest::mouseClick(clientMode, Qt::LeftButton);
      QTRY_VERIFY(clientSetup->isVisibleTo(&window));
      QTest::mouseClick(clientSetup, Qt::LeftButton);
      QTRY_COMPARE(pages->currentIndex(), 1);
      auto *client = window.findChild<ClientConfigDialog *>();
      verifyDialogFooter(window, client, "client-fit");
      if (QTest::currentTestFailed())
        return;
      capture(window, "client-fit");
      QTest::mouseClick(back, Qt::LeftButton);
      QTRY_COMPARE(pages->currentIndex(), 0);
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
    if (m_width == 500) {
      verifyDialogFooter(window, hotkey, "hotkey-fit");
      if (QTest::currentTestFailed())
        return;
      capture(window, "hotkey-fit");
    }
    auto *associated = hotkey->findChild<QListWidget *>("listAssociatedActions");
    QVERIFY(associated);
    associated->setCurrentRow(0);
    QTest::mouseClick(hotkey->findChild<QPushButton *>("btnRemoveAction"), Qt::LeftButton);
    QCOMPARE(associated->count(), 1);
    checkRestartCaption(associated->item(0)->text());
    QTest::mouseClick(hotkey->findChild<QPushButton *>("btnAddAction"), Qt::LeftButton);
    auto *addition = window.findChild<ActionDialog *>();
    QVERIFY(addition);
    if (m_width == 500) {
      verifyDialogFooter(window, addition, "action-fit-initial");
      if (QTest::currentTestFailed())
        return;
      capture(window, "action-fit-initial");
    }
    auto *type = addition->findChild<QComboBox *>("comboActionType");
    auto *direction = addition->findChild<QComboBox *>("comboSwitchInDirection");
    QVERIFY(type && direction);
    type->setCurrentIndex(type->findData(4));
    direction->setCurrentIndex(direction->findData(2));
    if (m_width == 500) {
      verifyDialogFooter(window, addition, "action-fit-switch-direction");
      if (QTest::currentTestFailed())
        return;
      capture(window, "action-fit-switch-direction");
    }
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
  void certificateResultStaysInNetworkSettings()
  {
    QTemporaryDir certificates;
    QVERIFY(certificates.isValid());
    const auto saved = certificates.filePath("saved.pem");
    const auto draft = certificates.filePath("draft.pem");
    Settings::setValue(Settings::Security::Certificate, saved);
    Settings::setValue(Settings::Security::KeySize, 2048);
    Settings::setValue(Settings::Security::TlsEnabled, true);
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    auto *preferences = window.findChild<QPushButton *>("btnSettingsShortcut");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    auto *title = window.findChild<QLabel *>("pageTitle");
    QVERIFY(preferences && pages && title);
    QTest::mouseClick(preferences, Qt::LeftButton);
    auto *editor = window.findChild<SettingsDialog *>();
    QVERIFY(editor);
    auto *tabs = editor->findChild<QTabWidget *>("tabWidget");
    auto *network = editor->findChild<QWidget *>("tabNetwork");
    auto *scroll = editor->findChild<QScrollArea *>("networkScroll");
    auto *path = editor->findChild<QLineEdit *>("lineTlsCertPath");
    auto *regenerate = editor->findChild<QPushButton *>("btnTlsRegenCert");
    QVERIFY(tabs && network && scroll && path && regenerate);
    tabs->setCurrentWidget(network);
    path->setText(draft);
    QVERIFY(settleStaticRender(window));
    scroll->ensureWidgetVisible(regenerate);
    auto *activePage = qobject_cast<QScrollArea *>(pages->currentWidget());
    QVERIFY(activePage);
    const auto editorTitle = title->text();
    // Exercise the outer scrollbar with an editor capped at the compact height.
    if (m_width == 600) {
      window.setMaximumHeight(window.minimumHeight());
      QVERIFY(settleStaticRender(window));
      activePage->ensureWidgetVisible(regenerate, 0, 0);
    }
    const auto editorHeight = window.height();
    if (m_width != 600) {
      verifyDialogFooter(window, editor, "tls-fit-before-regeneration");
      if (QTest::currentTestFailed())
        return;
    }
    QSignalSpy navigation(pages, &QStackedWidget::currentChanged);
    QVERIFY(regenerate->isVisibleTo(&window) && regenerate->isEnabled());
    QTest::mouseClick(regenerate, Qt::LeftButton);
    capture(window, "tls-result");

    QCOMPARE(pages->currentWidget(), activePage);
    QCOMPARE(navigation.count(), 0);
    QCOMPARE(title->text(), editorTitle);
    QCOMPARE(tabs->currentWidget(), network);
    QCOMPARE(window.width(), m_width);
    auto *notice = network->findChild<QFrame *>("tlsRegenerationNotice");
    auto *text = network->findChild<QLabel *>("lblTlsRegenerationResult");
    auto *ok = network->findChild<QPushButton *>("btnDismissTlsRegeneration");
    QVERIFY(notice && text && ok);
    QVERIFY(notice->isVisibleTo(&window) && ok->isVisibleTo(&window));
    QCOMPARE(
        text->text(),
        I18N::currentLanguage() == "zh_CN" ? "TLS 证书重新生成成功。" : "TLS certificate regenerated successfully."
    );
    QVERIFY(notice->height() <= 56);
    QVERIFY(text->contentsRect().width() >= text->fontMetrics().horizontalAdvance(text->text()));
    QVERIFY(ok->width() <= ok->sizeHint().width());
    QVERIFY(window.height() <= editorHeight + notice->height() + 8);
    const auto viewport = scroll->viewport();
    QTRY_VERIFY(viewport->rect().contains(QRect(notice->mapTo(viewport, QPoint()), notice->size())));
    const auto outerViewport = activePage->viewport();
    if (m_width == 600)
      QVERIFY(activePage->verticalScrollBar()->maximum() > 0);
    QTRY_VERIFY(outerViewport->rect().contains(QRect(notice->mapTo(outerViewport, QPoint()), notice->size())));
    QVERIFY(synergy::gui::TlsUtility::isCertValid(saved));
    QVERIFY(!QFile::exists(draft));
    QCOMPARE(path->text(), draft);

    if (m_width != 600 && activePage->verticalScrollBar()->maximum() == 0) {
      verifyDialogFooter(window, editor, "tls-fit-notice-revealed");
      if (QTest::currentTestFailed())
        return;
    }
    QTest::mouseClick(ok, Qt::LeftButton);
    QTRY_VERIFY(notice->isHidden());
    QTRY_COMPARE(window.height(), editorHeight);
    QCOMPARE(pages->currentWidget(), activePage);
    QCOMPARE(navigation.count(), 0);
    QCOMPARE(path->text(), draft);
    if (m_width != 600) {
      verifyDialogFooter(window, editor, "tls-fit-notice-dismissed");
      if (!QTest::currentTestFailed() && m_width == 500)
        capture(window, "tls-fit-notice-dismissed");
    }
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
  void backgroundUpdateBadgeKeepsTheCurrentPage()
  {
    ControlledHttp server;
    QVERIFY(server.isListening());
    IsolatedWindow window(server, m_width);
    auto *shortcut = window.findChild<QPushButton *>("btnUpdatesShortcut");
    auto *preferences = window.findChild<QPushButton *>("btnSettingsShortcut");
    auto *back = window.findChild<QPushButton *>("btnBack");
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(shortcut && preferences && back && pages);
    auto *badge = shortcut->findChild<QWidget *>("updateAvailableBadge");
    QVERIFY(badge && badge->isHidden());
    QVERIFY(settleStaticRender(window));
    const auto shortcutSize = shortcut->size();
    const auto respond = [&server](const QByteArray &version) {
      const QByteArray release = "https://github.com/Tupig/TupigSynergy/releases/";
      const QByteArray tag = "v" + version;
      const auto asset = [&release, &tag](const QByteArray &suffix) {
        const auto name = "synergy_" + suffix;
        return "{\"name\":\"" + name + "\",\"browser_download_url\":\"" + release + "download/" + tag + "/" + name +
               "\",\"size\":100,\"digest\":\"sha256:" + QByteArray(64, 'a') + "\"}";
      };
      const QByteArray body = "{\"tag_name\":\"" + tag + "\",\"html_url\":\"" + release + "tag/" + tag +
                              "\",\"assets\":[" + asset("fixture.dmg") + "]}";
      server.write(
          "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" +
              body,
          true
      );
    };
    QTest::mouseClick(preferences, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 1);
    const auto *settingsPage = pages->currentWidget();
    window.checkForUpdates(false);
    window.findChild<synergy::gui::update::UpdateChecker *>()->setAssetSuffixes({"_fixture.dmg"});
    QTRY_COMPARE(server.requests, 1);
    respond("99.0.0");
    QTRY_VERIFY(!badge->isHidden());
    QCOMPARE(pages->currentWidget(), settingsPage);
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_VERIFY(badge->isVisibleTo(&window));
    QCOMPARE(pages->currentIndex(), 0);
    QTRY_COMPARE(shortcut->size(), shortcutSize);
    const auto hint = QCoreApplication::translate("QObject", "Update to %1 available").arg("99.0.0");
    QCOMPARE(shortcut->toolTip(), hint);
    QCOMPARE(shortcut->accessibleDescription(), hint);
    const auto badgeImage = badge->grab().toImage();
    QVERIFY(!badgeImage.isNull());
    QCOMPARE(badgeImage.pixelColor(badgeImage.rect().center()), QColor("#e5484d"));
    if (m_width == 500)
      capture(window, "update-available-badge");

    QTest::mouseClick(shortcut, Qt::LeftButton);
    QTRY_VERIFY(window.findChild<synergy::gui::update::UpdateDialog *>());
    QCOMPARE(server.requests, 1);
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);
    QTRY_VERIFY(badge->isVisibleTo(&window));
    QTRY_VERIFY(window.findChildren<synergy::gui::update::UpdateDialog *>().isEmpty());

    window.checkForUpdates(false);
    QTRY_COMPARE(server.requests, 2);
    server.write("HTTP/1.1 503 Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", true);
    QTRY_VERIFY(window.findChildren<synergy::gui::update::UpdateChecker *>().isEmpty());
    QVERIFY(badge->isVisibleTo(&window));

    window.checkForUpdates(false);
    window.findChild<synergy::gui::update::UpdateChecker *>()->setAssetSuffixes({"_fixture.dmg"});
    QTRY_COMPARE(server.requests, 3);
    window.checkForUpdates(true);
    QVERIFY(message(window, QCoreApplication::translate("MainWindow", "Check for Updates")));
    QCOMPARE(window.checks, 3);
    respond("1.0.0");
    QTRY_VERIFY(badge->isHidden());
    QTRY_VERIFY(message(window, QCoreApplication::translate("MainWindow", "No Updates")));
    QCOMPARE(server.requests, 3);
  }
  void manualCheckCompletionKeepsAnUnrelatedSettingsDraft()
  {
    ControlledHttp server;
    QVERIFY(server.isListening());
    IsolatedWindow window(server, m_width);
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(pages);
    window.checkForUpdates(true);
    QTRY_COMPARE(server.requests, 1);
    QVERIFY(message(window, QCoreApplication::translate("MainWindow", "Check for Updates")));

    window.m_actionSettings->trigger();
    QPointer<SettingsDialog> settings = window.findChild<SettingsDialog *>();
    QVERIFY(settings && settings->isVisibleTo(&window));
    auto *draft = settings->findChild<QLineEdit *>("lineTlsCertPath");
    QVERIFY(draft);
    draft->setText(Settings::userDir() + "/uncommitted-fixture.pem");
    const auto uncommitted = draft->text();
    const auto *settingsPage = pages->currentWidget();

    server.write("HTTP/1.1 503 Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", true);
    QTRY_VERIFY(window.findChildren<synergy::gui::update::UpdateChecker *>().isEmpty());
    QVERIFY(settings && settings->isVisibleTo(&window));
    QCOMPARE(pages->currentWidget(), settingsPage);
    QCOMPARE(draft->text(), uncommitted);
  }
  void windowDestructionCancelsPendingUpdateCheck()
  {
    ControlledHttp server;
    QVERIFY(server.isListening());
    for (const bool cancelFirst : {false, true}) {
      const int expectedRequests = server.requests + 1;
      {
        IsolatedWindow window(server, m_width);
        window.checkForUpdates(true);
        QTRY_COMPARE(server.requests, expectedRequests);
        auto *checking = message(window, QCoreApplication::translate("MainWindow", "Check for Updates"));
        QVERIFY(checking);
        if (cancelFirst) {
          auto *cancel = button(*checking, QCoreApplication::translate("MainWindow", "Cancel"));
          QVERIFY(cancel);
          QTest::mouseClick(cancel, Qt::LeftButton);
        }
      }
      QTRY_VERIFY(server.peers.last()->state() == QAbstractSocket::UnconnectedState);
    }
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
    auto *status = window.findChild<StatusBar *>("statusBar");
    QVERIFY(progress && card && status && card->isAncestorOf(progress));
    const auto bottomGap = [&](QFrame *activeCard) {
      return status->mapTo(&window, QPoint()).y() - activeCard->mapTo(&window, QPoint()).y() - activeCard->height();
    };
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(bottomGap(card) - 10) <= 2, 1000);
    capture(window, "checking");
    server.write("HTTP/1.1 503 Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", true);
    QTRY_VERIFY(message(window, QCoreApplication::translate("MainWindow", "Update Check Failed")));
    auto *failed = message(window, QCoreApplication::translate("MainWindow", "Update Check Failed"));
    auto *retry = button(*failed, QCoreApplication::translate("MainWindow", "Retry"));
    auto *failedCard = failed->findChild<QFrame *>("card");
    QVERIFY(retry && failedCard);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(bottomGap(failedCard) - 10) <= 2, 1000);
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
  void detailPagesRevealAndReturnWithoutClipping()
  {
    ControlledHttp server;
    IsolatedWindow window(server, m_width);
    placeFullScreenCapFixtureAtScreenTop(window);
    if (QTest::currentTestFailed())
      return;
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    auto *aboutButton = window.findChild<QPushButton *>("btnAboutShortcut");
    auto *back = window.findChild<QPushButton *>("btnBack");
    auto *status = window.findChild<StatusBar *>("statusBar");
    QVERIFY(pages && aboutButton && back && status);
    const auto record = [&](const QString &name, QWidget *last) {
      QVERIFY(last && last->isVisibleTo(&window));
      QVERIFY(settleStaticRender(window));
      auto *page = qobject_cast<QScrollArea *>(pages->currentWidget());
      QVERIFY(page && page->widget() && window.screen());
      auto *scroll = page->verticalScrollBar();
      const auto lastRect = [&] { return QRect(last->mapTo(page->viewport(), QPoint()), last->size()); };
      const auto atBodyCap = [&] {
        const int chrome = window.frameGeometry().height() - page->height();
        const int available = window.screen()->availableGeometry().height();
        const int bodyCap = qBound(1, available - chrome, 660);
        return qAbs(page->height() - bodyCap) <= 2;
      };
      QTRY_VERIFY_WITH_TIMEOUT(
          scroll->maximum() > 0 ? atBodyCap() : page->viewport()->rect().contains(lastRect()), 1000
      );
      QCOMPARE(page->horizontalScrollBar()->maximum(), 0);
      if (scroll->maximum() > 0) {
        QVERIFY(page->widget()->height() > page->viewport()->height());
        QVERIFY(scroll->value() < scroll->maximum());
        page->setFocus();
        QCOMPARE(window.focusWidget(), page);
        while (scroll->value() < scroll->maximum()) {
          const int before = scroll->value();
          QTest::keyClick(page, Qt::Key_PageDown);
          QVERIFY2(scroll->value() > before, "Page Down must scroll capped content toward the bottom actions");
        }
        QCOMPARE(scroll->value(), scroll->maximum());
      }
      QTRY_VERIFY_WITH_TIMEOUT(page->viewport()->rect().contains(lastRect()), 1000);
      const int gap = status->mapTo(&window, QPoint()).y() - last->mapTo(&window, QPoint()).y() - last->height();
      qInfo().noquote() << name << "window height" << window.height() << "bottom gap" << gap << "vertical scroll"
                        << scroll->value() << "of" << scroll->maximum();
      QVERIFY2(qAbs(gap - 10) <= 2, qPrintable(QStringLiteral("%1 bottom gap: %2 px").arg(name).arg(gap)));
      QCOMPARE(window.width(), m_width);
      capture(window, name);
    };
    QTest::mouseClick(aboutButton, Qt::LeftButton);
    auto *about = window.findChild<AboutDialog *>();
    QVERIFY(about);
    auto *notices = about->findChild<QPushButton *>("aboutNoticesButton");
    auto *aboutClose = about->findChild<QPushButton *>("btnOk");
    QVERIFY(notices && aboutClose);
    record("about-compact", aboutClose);
    if (QTest::currentTestFailed())
      return;
    const int aboutHeight = window.height();
    QTest::mouseClick(notices, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 2);
    auto *noticeText = window.findChild<QTextBrowser *>("aboutNoticesText");
    QVERIFY(noticeText && !noticeText->toPlainText().isEmpty());
    auto *notice = qobject_cast<QDialog *>(noticeText->parentWidget());
    QVERIFY(notice);
    auto *noticeClose = button(*notice, QCoreApplication::translate("AboutDialog", "&Close"));
    record("about-notices", noticeClose);
    if (QTest::currentTestFailed())
      return;
    QTest::mouseClick(back, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 1);
    QTRY_COMPARE(window.height(), aboutHeight);
    QTest::mouseClick(aboutClose, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);

    const Fingerprint local(QCryptographicHash::Sha256, QByteArray(32, '\x5a'));
    const Fingerprint remote(QCryptographicHash::Sha256, QByteArray(32, '\x7e'));
    for (const auto mode : {FingerprintDialogMode::Local, FingerprintDialogMode::Server}) {
      auto *fingerprints = new FingerprintDialog(&window, local, mode, remote);
      PageHost::present(&window, fingerprints);
      auto *actions = fingerprints->findChild<QDialogButtonBox *>();
      auto *hash = fingerprints->findChild<QLabel *>("fingerprintHash");
      QVERIFY(actions && hash && hash->isVisibleTo(&window));
      auto *toggle = actions->button(QDialogButtonBox::Help);
      QVERIFY(toggle);
      const auto kind = mode == FingerprintDialogMode::Local ? QStringLiteral("local") : QStringLiteral("compare");
      record("fingerprint-" + kind + "-hash", actions);
      if (QTest::currentTestFailed())
        return;
      const int hashHeight = window.height();
      QTest::mouseClick(toggle, Qt::LeftButton);
      QTRY_VERIFY(hash->isHidden());
      record("fingerprint-" + kind + "-image", actions);
      if (QTest::currentTestFailed())
        return;
      QTest::mouseClick(toggle, Qt::LeftButton);
      QTRY_VERIFY(hash->isVisibleTo(&window));
      QTRY_COMPARE(window.height(), hashHeight);
      QTest::mouseClick(back, Qt::LeftButton);
      QTRY_COMPARE(pages->currentIndex(), 0);
    }
  }
  void embeddedUpdateFitsBeforeAndDuringDownload()
  {
    ControlledHttp server;
    QVERIFY(server.isListening());
    IsolatedWindow window(server, m_width);
    synergy::gui::update::UpdateInfo info;
    info.version = "99.0.0";
    info.notes = "Offline controlled update fixture";
    info.asset.name = "flow-fixture.dmg";
    info.asset.url = server.url();
    info.asset.sha256 = QString(64, QLatin1Char('0'));
    info.asset.size = 1048576;
    const synergy::gui::update::DownloadPolicy policy{"http", "127.0.0.1", "/"};
    auto *dialog = new synergy::gui::update::UpdateDialog(info, &window, policy);
    PageHost::present(&window, dialog);
    auto *pages = window.findChild<QStackedWidget *>("pageStack");
    QVERIFY(pages);
    auto *page = qobject_cast<QScrollArea *>(pages->currentWidget());
    auto *card = dialog->findChild<QGroupBox *>("updateSummaryCard");
    auto *status = window.findChild<StatusBar *>("statusBar");
    auto *download = button(*dialog, QCoreApplication::translate("UpdateDialog", "Download and Install"));
    auto *downloader = dialog->findChild<synergy::gui::update::UpdateDownloader *>();
    auto *notes = dialog->findChild<QTextEdit *>("releaseNotes");
    auto *toggleNotes = dialog->findChild<QPushButton *>("toggleReleaseNotes");
    QVERIFY(page && card && status && download && downloader && notes && toggleNotes);
    const auto checkFit = [&] {
      QVERIFY(settleStaticRender(window));
      const QRect cardRect(card->mapTo(page->viewport(), QPoint()), card->size());
      QVERIFY(page->viewport()->rect().contains(cardRect));
      QCOMPARE(page->verticalScrollBar()->maximum(), 0);
      const int gap = status->mapTo(&window, QPoint()).y() - card->mapTo(&window, QPoint()).y() - card->height();
      QVERIFY2(qAbs(gap - 10) <= 2, qPrintable(QStringLiteral("Gap below update card: %1 px").arg(gap)));
      QCOMPARE(window.width(), m_width);
    };
    checkFit();
    QVERIFY(notes->isVisibleTo(&window));
    const int shownHeight = window.height();
    QTest::mouseClick(toggleNotes, Qt::LeftButton);
    QTRY_VERIFY(notes->isHidden());
    QTRY_VERIFY(window.height() < shownHeight);
    checkFit();
    QTest::mouseClick(toggleNotes, Qt::LeftButton);
    QTRY_VERIFY(notes->isVisibleTo(&window));
    QTRY_COMPARE(window.height(), shownHeight);
    QSignalSpy bytes(downloader, &synergy::gui::update::UpdateDownloader::progress);
    QTest::mouseClick(download, Qt::LeftButton);
    QTRY_COMPARE(server.requests, 1);
    server.write("HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n" + QByteArray(65536, 'x'));
    QTRY_VERIFY(!bytes.isEmpty() && bytes.last().at(0).toLongLong() > 0);
    checkFit();
    capture(window, "embedded-update-download");
    auto *cancel = button(*dialog, QCoreApplication::translate("UpdateDialog", "Cancel download"));
    QVERIFY(cancel);
    QTest::mouseClick(cancel, Qt::LeftButton);
    QTRY_COMPARE(pages->currentIndex(), 0);
    QTRY_COMPARE(window.minimumHeight(), 390);
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
  void failedInstallPreparationKeepsVerifiedDownload()
  {
    ControlledHttp server;
    QVERIFY(server.isListening());
    const QByteArray payload("verified fixture");
    synergy::gui::update::UpdateInfo info;
    info.version = "99.0.0";
    info.asset.name = "flow-fixture.dmg";
    info.asset.url = server.url();
    info.asset.sha256 = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
    info.asset.size = payload.size();
    const synergy::gui::update::DownloadPolicy policy{"http", "127.0.0.1", "/"};
    synergy::gui::update::UpdateDialog dialog(info, nullptr, policy);
    dialog.setAttribute(Qt::WA_DontShowOnScreen);
    dialog.show();

    int attempts = 0;
    dialog.setInstallPreparation([&attempts] {
      ++attempts;
      return false;
    });
    auto *install = dialog.findChild<QPushButton *>("downloadInstall");
    auto *feedback = dialog.findChild<QLabel *>("updateProgressText");
    auto *downloader = dialog.findChild<synergy::gui::update::UpdateDownloader *>();
    QVERIFY(install && feedback && downloader);
    QSignalSpy verified(downloader, &synergy::gui::update::UpdateDownloader::finished);
    QSignalSpy handoff(&dialog, &synergy::gui::update::UpdateDialog::installationStarted);
    install->click();
    QTRY_COMPARE(server.requests, 1);
    server.write(
        "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n" +
            payload,
        true
    );
    QTRY_COMPARE(verified.size(), 1);
    const QString downloadedPath = verified.first().first().toString();
    const auto failed =
        synergy::gui::update::UpdateDialog::tr("Sharing did not stop. Stop sharing, then try installing again.");
    for (const int expected : {1, 2}) {
      install->click();
      QCOMPARE(attempts, expected);
      QVERIFY(feedback->isVisibleTo(&dialog));
      QCOMPARE(feedback->text(), failed);
      QVERIFY(install->isEnabled());
      QCOMPARE(server.requests, 1);
      QVERIFY(handoff.isEmpty());
      QVERIFY(QFileInfo::exists(downloadedPath));
    }
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
