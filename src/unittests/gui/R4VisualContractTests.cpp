/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "R4VisualContractTests.h"
#include "R4DragTrace.h"
#include "UiRenderTestSupport.h"
#include "common/I18N.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/Action.h"
#include "gui/Hotkey.h"
#include "gui/KeySequence.h"
#include "gui/MainWindow.h"
#include "gui/StyleUtils.h"
#include "gui/TlsUtility.h"
#include "gui/config/Screen.h"
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
#include "gui/widgets/LogPanel.h"
#include "gui/widgets/MessagePage.h"
#include "gui/widgets/PageHost.h"
#include "gui/widgets/StatusBar.h"
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QtTest>
#include <climits>
#include <iterator>

namespace synergy::testing {
namespace {
// Dynamic observation context avoids teaching lupdate new product messages from
// test oracle lookups. The supplied context names match each real QObject class.
QString observedText(const char *context, const char *source)
{
  return QCoreApplication::translate(context, source);
}
// Independent oracle: inert HTML SHA63895000..., CSS lines44-152 and page
// functions487-713. Object names only locate real fields; they do not define
// expected geometry, card count, heading count, order or action ownership.
struct ReferencePage
{
  const char *kind;
  int tab;
  int cards;
  int headings;
  bool actionsInside;
  const char *orderedFields;
  const char *reference;
};
const ReferencePage pages[]{
    {"main", 0, 1, 0, false, "", "HTML487-508 server; log expanded/collapsed"},
    {"main", 1, 1, 0, false, "", "HTML487-508 client; log expanded/collapsed"},
    {"preferences", 0, 1, 0, true, "cbPreventSleep,checkForUpdates,comboLanguage,comboTrayIcon,comboSettingsScope",
     "HTML559-569"},
    {"preferences", 1, 1, 0, true, "comboStartBehavior,comboCloseBehavior", "HTML570-577"},
    {"preferences", 2, 1, 0, true, "comboLogLevel,lineLogFilename,cbLogToFile,cbGuiDebug", "HTML578-588"},
    {"preferences", 3, 1, 0, true,
     "comboInterface,sbPort,lineTlsCertPath,comboTlsKeyLength,cbTlsEnabled,cbRequireClientCert", "HTML589-599"},
    {"preferences", 4, 1, 0, true, "cbRunEnterCommand,lineCommandEnter,cbRunExitCommand,lineCommandExit",
     "HTML600-602"},
    {"server", 0, 1, 1, false, "screenSetupView", "HTML510-521; necessary existing commit controls retained"},
    {"server", 1, 2, 2, false, "listHotkeys,listActions", "HTML522-529; necessary existing commit controls retained"},
    {"server", 2, 1, 0, true, "cbEnableClipboard,cbRelativeMouseMoves,cbDefaultLockToScreenState,cbDisableLockToScreen",
     "HTML530-545"},
    {"server", 3, 1, 0, true, "cbExternalConfig,lineConfigFile", "HTML546-549"},
    {"computer", 0, 1, 0, true, "lineNameEdit", "HTML604-606"},
    {"computer", 1, 1, 0, true, "comboShift,comboCtrl,comboAlt,comboMeta,comboSuper,comboAltGr",
     "HTML607; retain all legal modifier choices"},
    {"computer", 2, 1, 1, true, "sbSwitchCornerSize,chkFixScrollLock,chkFixCapsLock,chkFixNumLock", "HTML609-615"},
    {"computer", 3, 1, 0, true, "listAliases,lineAddAlias", "HTML608"},
    {"client", 0, 2, 2, false, "cbDynamicConnectTime,cbLanguageSync,cbXScrollInvert,cbYScrollInvert", "HTML551-557"},
    {"hotkey", 0, 1, 2, true, "m_pKeySequenceWidgetHotkey,listAssociatedActions", "HTML618-627"},
    {"action", 0, 1, 0, true, "comboTriggerOn,comboActionType",
     "HTML629-640; relevant fields only, All/Specified retained"},
    {"about", 0, 2, 2, false, "lblName,lblVersion", "HTML698-703; retained notices may be disclosed"},
    {"fingerprint", 0, 1, 1, false, "", "HTML642-648 local certificate portion; remote trust semantics inherited"},
    {"fingerprint", 1, 2, 2, false, "", "HTML642-648 comparison; both real fingerprints and reject default retained"},
    {"update", 0, 1, 1, true, "", "HTML673-696 available-update card; download semantics inherited"},
    {"update", 1, 1, 1, true, "", "HTML679-691 actual loopback partial download"},
    {"update", 2, 1, 1, true, "", "HTML679-691 actual stalled download"},
    {"message", 0, 1, 1, true, "", "HTML705-706 permission explanation; no OS action connected"},
    {"message", 1, 1, 1, true, "", "HTML707 existing-process explanation; no process action connected"},
    {"message", 2, 1, 1, true, "", "HTML708 port-error explanation; synthetic text only"},
    {"message", 3, 1, 1, true, "", "HTML709 clear explanation; no data-loss action connected"},
    {"message", 4, 1, 1, true, "", "HTML710 close-to-tray explanation; fixture only"}
};
// The same controlled partial-byte harness used by existing flow checks. Only
// loopback is listened to; payload is deliberately incomplete and can never pass
// the declared digest or reach the installer. Cancel must destroy the transfer.
class PartialDownload final : public QTcpServer
{
public:
  QList<QPointer<QTcpSocket>> peers;
  int requests = 0;
  explicit PartialDownload(bool enabled)
  {
    if (!enabled)
      return;
    listen(QHostAddress::LocalHost, 0);
    connect(this, &QTcpServer::newConnection, this, [this] {
      while (hasPendingConnections()) {
        auto *socket = nextPendingConnection();
        peers << socket;
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
          socket->readAll();
          if (!socket->property("counted").toBool()) {
            socket->setProperty("counted", true);
            ++requests;
          }
        });
      }
    });
  }
  QUrl url() const
  {
    return QUrl(QString("http://127.0.0.1:%1/fixture").arg(serverPort()));
  }
  void send()
  {
    for (const auto &socket : std::as_const(peers))
      if (socket) {
        socket->write("HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n");
        socket->write(QByteArray(65536, 'r'));
        socket->flush();
      }
  }
};
QString outputDirectory(const QString &root)
{
  const auto requested = qEnvironmentVariable("SYNERGY_R4_EVIDENCE_DIR");
  return requested.isEmpty() ? root + "/visual-r4" : requested;
}
bool writeJson(const QString &path, const QJsonObject &value)
{
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
         file.write(QJsonDocument(value).toJson(QJsonDocument::Indented)) > 0;
}
QRect bounds(QWidget *widget, QWidget *root)
{
  return {widget->mapTo(root, QPoint()), widget->size()};
}
QJsonArray rectJson(const QRect &r)
{
  return {r.x(), r.y(), r.width(), r.height()};
}
QColor pixel(const QImage &image, const QPoint &logical)
{
  return image.pixelColor(
      qRound(logical.x() * image.devicePixelRatio()), qRound(logical.y() * image.devicePixelRatio())
  );
}
bool nearColor(QColor actual, QColor expected, int tolerance = 3)
{
  return qAbs(actual.red() - expected.red()) <= tolerance && qAbs(actual.green() - expected.green()) <= tolerance &&
         qAbs(actual.blue() - expected.blue()) <= tolerance;
}
// Reuse the established real-view activation contract: exactly once immediately;
// bounded wait is only for the stock popup/container close animation.
bool chooseData(QComboBox *control, int value)
{
  if (!control || !settleStaticRender(*control->window()))
    return false;
  const int row = control->findData(value);
  if (row < 0)
    return false;
  QSignalSpy activated(control, qOverload<int>(&QComboBox::activated));
  control->showPopup();
  QCoreApplication::processEvents();
  auto *view = control->view();
  if (!view || !view->isVisible())
    return false;
  const auto index = control->model()->index(row, control->modelColumn(), control->rootModelIndex());
  view->scrollTo(index);
  QCoreApplication::processEvents();
  QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualRect(index).center());
  QCoreApplication::processEvents();
  const bool immediate = activated.count() == 1 && control->currentData().toInt() == value;
  auto *container = view->parentWidget();
  QElapsedTimer timer;
  timer.start();
  while (immediate && (view->isVisible() || (container && container->isVisible())) && timer.elapsed() < 1000)
    QTest::qWait(10);
  return immediate && activated.count() == 1 && !view->isVisible() && (!container || !container->isVisible());
}
// Same bounded command-blocking pattern as the independently reviewed R2 native
// harness. It changes capability availability, never appearance/theme or layout.
bool blockRuntimeCommands(MainWindow *window)
{
  // afterStartupProbePreInit explicitly seeded en before production I18N.
  // Fail closed if that prerequisite ever changes; never guess a localized label.
  if (I18N::currentLanguage() != QStringLiteral("en"))
    return false;
  int blocked = 0;
  for (auto *action : window->findChildren<QAction *>()) {
    const auto key = action->shortcut();
    const bool clear = action->text() == QStringLiteral("Clear settings");
    if (!clear && key != QKeySequence(Qt::CTRL | Qt::Key_S) && key != QKeySequence(Qt::CTRL | Qt::Key_R) &&
        key != QKeySequence(Qt::CTRL | Qt::Key_U))
      continue;
    ++blocked;
    QObject::disconnect(action, &QAction::triggered, window, nullptr);
    const auto disable = [action] {
      if (action->isEnabled())
        action->setEnabled(false);
    };
    QObject::connect(action, &QAction::changed, action, disable);
    disable();
  }
  for (const char *name : {"btnToggleCore", "btnRestartCore", "btnUpdatesShortcut"}) {
    auto *button = window->findChild<QPushButton *>(QString::fromLatin1(name));
    if (!button)
      return false;
    QObject::disconnect(button, &QPushButton::clicked, nullptr, nullptr);
    button->setEnabled(false);
  }
  auto *hostname = window->findChild<QLineEdit *>("lineHostname");
  if (!hostname)
    return false;
  QObject::disconnect(hostname, &QLineEdit::returnPressed, nullptr, nullptr);
  QObject::connect(Settings::instance(), &Settings::settingsChanged, window, [](const QString &) {
    for (const auto &key : {Settings::Gui::AutoStartCore, Settings::Gui::CheckForUpdates})
      if (Settings::value(key).toBool())
        Settings::setValue(key, false);
  });
  return blocked == 4 && !Settings::value(Settings::Gui::AutoStartCore).toBool() &&
         !Settings::value(Settings::Gui::CheckForUpdates).toBool() && window->findChildren<QProcess *>().isEmpty();
}
QJsonObject appearance(MainWindow *window, const QString &root)
{
  const auto palette = qApp->palette();
  return {
      {"pid", qApp->applicationPid()},
      {"observed_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
      {"executable", qApp->applicationFilePath()},
      {"entry", "production synergy-gui.cpp + real onPreInit/onMainWindow/onAppStart"},
      {"test_difference", "existing isolated RuntimeContext; fixture safety overrides after migration; canonical "
                          "dangerous commands blocked"},
      {"capture",
       "QWidget::grab from visible native MainWindow; compositor/native CUA observation remains independent"},
      {"qpa", QGuiApplication::platformName()},
      {"style_class", qApp->style()->metaObject()->className()},
      {"style_name", qApp->style()->objectName()},
      {"app_font_pixels", qApp->font().pixelSize()},
      {"window_color", palette.color(QPalette::Window).name()},
      {"text_color", palette.color(QPalette::WindowText).name()},
      {"qss_sha256",
       QString::fromLatin1(QCryptographicHash::hash(qApp->styleSheet().toUtf8(), QCryptographicHash::Sha256).toHex())},
      {"dpr", window->devicePixelRatioF()},
      {"visible", window->isVisible()},
      {"root", root},
      {"settings", Settings::settingsFile()},
      {"state", runtime::runtimeStateFile()},
      {"byte_identical_distributed_executable", false},
      {"auto_core", Settings::value(Settings::Gui::AutoStartCore).toBool()},
      {"auto_updates", Settings::value(Settings::Gui::CheckForUpdates).toBool()}
  };
}
} // namespace

bool prepareR4VisualFixture(const QString &root)
{
  // Existing bootstrap already bound every path and identity before QApplication.
  if (runtime::runtimeRoot() != root || root.isEmpty())
    return false;
  Settings::setValue(Settings::Core::ComputerName, "R4-Fixture");
  Settings::setValue(Settings::Core::CoreMode, int(Settings::Server));
  Settings::setValue(Settings::Core::ScreenEnterCommand, QStringLiteral(""));
  Settings::setValue(Settings::Core::ScreenExitCommand, QStringLiteral(""));
  Settings::setValue(Settings::Log::Level, QStringLiteral("INFO"));
  if (!gui::TlsUtility::generateCertificateAt(root + "/user/fixture.pem", 2048))
    return false;
  gui::ServerConfig config;
  config.addClient("R4-Studio");
  config.commit();
  Settings::save();
  return true;
}

class R4VisualChecks : public QObject
{
  Q_OBJECT
public:
  R4VisualChecks(MainWindow *window, QString root) : m_window(window), m_root(std::move(root))
  {
  }
  Q_INVOKABLE void observeUrl(const QUrl &url)
  {
    m_observedUrls << url;
  }

private:
  MainWindow *m_window;
  QString m_root;
  QJsonArray m_receipts;
  QStringList m_errors;
  QList<QUrl> m_observedUrls;
  void check(bool valid, const QString &message)
  {
    if (!valid) {
      m_errors << message;
      qWarning().noquote() << message;
    }
  }
  void saveReceipt()
  {
    writeJson(outputDirectory(m_root) + "/observations.json", {{"pages", m_receipts}});
  }
  QWidget *owningCard(QWidget *widget, const QList<QWidget *> &cards)
  {
    if (!widget)
      return nullptr;
    for (auto *card : cards)
      if (card == widget || card->isAncestorOf(widget))
        return card;
    return nullptr;
  }
  QJsonArray controlTokens(QWidget *editor, const QString &key, bool assertTokens)
  {
    QJsonArray controls;
    for (auto *widget : editor->findChildren<QWidget *>()) {
      if (!widget->isVisibleTo(editor))
        continue;
      // A spinbox's implementation-owned line editor is not another input row.
      if (qobject_cast<QLineEdit *>(widget) && qobject_cast<QAbstractSpinBox *>(widget->parentWidget()))
        continue;
      const bool captureField = qobject_cast<KeySequenceWidget *>(widget);
      const bool input = captureField || qobject_cast<QLineEdit *>(widget) || qobject_cast<QComboBox *>(widget) ||
                         qobject_cast<QAbstractSpinBox *>(widget);
      auto *button = qobject_cast<QPushButton *>(widget);
      const bool label = qobject_cast<QLabel *>(widget);
      const bool checkbox = qobject_cast<QCheckBox *>(widget);
      if (!input && !button && !label && !checkbox)
        continue;
      const auto name = widget->objectName();
      const bool primary = button && button->property("primary").toBool();
      const bool badge = name == "logBadge";
      const bool meterValue =
          name == "downloadAmountValue" || name == "downloadRateValue" || name == "downloadEtaValue";
      const bool chip = button && button->property("levelChip").toBool();
      const bool search = input && widget->accessibleName() == observedText("LogPanel", "Search log");
      const bool heading =
          widget->property("sectionHeading").toBool() || name == "cardHeading" || name == "messageTitle" ||
          name == "pageTitle" || name == "lblComputerName" || name == "lblName" || name == "logTitle" ||
          (qobject_cast<FingerprintDialog *>(editor) && label &&
           (static_cast<QLabel *>(widget)->text() ==
                observedText("FingerprintDialog", "Local computer's fingerprint") ||
            static_cast<QLabel *>(widget)->text() == observedText("FingerprintDialog", "Server Fingerprint") ||
            static_cast<QLabel *>(widget)->text() == observedText("FingerprintDialog", "Client Fingerprint")));
      int expectedHeight = input ? 30 : (button ? 32 : 0);
      if (search)
        expectedHeight = 26;
      if (button && button->property("small").toBool())
        expectedHeight = 26;
      if (button && button->property("iconButton").toBool())
        expectedHeight = 24;
      if (button && button->property("levelChip").toBool())
        expectedHeight = 20;
      controls.append(
          QJsonObject{
              {"name", name},
              {"class", widget->metaObject()->className()},
              {"rect", rectJson(bounds(widget, editor))},
              {"font_pixels", widget->font().pixelSize()},
              {"font_points", widget->font().pointSizeF()},
              {"font_weight", int(widget->font().weight())},
              {"font_family", widget->font().family()},
              {"font_bold", widget->font().bold()},
              {"expected_height", expectedHeight},
              {"primary", primary},
              {"heading", heading}
          }
      );
      if (!assertTokens)
        continue;
      // Compact icon/chip dimensions inherited from their separate main-shell
      // contract; the reference's editor outer boxes are exactly32/30/26.
      if (expectedHeight > 0 && expectedHeight != 24 && expectedHeight != 20)
        check(
            widget->height() == expectedHeight,
            key + QString(": outer height %1 actual%2 expected%3").arg(name).arg(widget->height()).arg(expectedHeight)
        );
      if (input || (button && !button->text().isEmpty()) || checkbox) {
        const int pixels = widget->font().pixelSize();
        // Qt's integral pixelSize may truncate/round a fractional CSS pixel.
        // Each variant keeps its own reference value, never a generic fallback.
        const int minPixels =
            captureField ? 12
                         : (chip ? 10 : (search ? 11 : (button ? (button->property("small").toBool() ? 12 : 13) : 12)));
        const int maxPixels = chip ? 11 : (search ? 12 : (button && button->property("small").toBool() ? 12 : 13));
        check(
            pixels >= minPixels && pixels <= maxPixels,
            key + ": reference control font " + name + " actual=" + QString::number(pixels)
        );
        if (!primary)
          check(
              widget->font().weight() == QFont::Normal, key + ": ordinary control weight must be400 " + name +
                                                            " actual=" + QString::number(widget->font().weight())
          );
        else
          check(widget->font().weight() == QFont::DemiBold, key + ": primary weight must be600 " + name);
      }
      // Ordinary field/subtle labels must not inherit a group-title's600 weight.
      if (label && !heading && !badge && !meterValue && name != "messageIcon" && name != "lblIcon")
        check(
            widget->font().weight() == QFont::Normal,
            key + ": ordinary label weight must be400 " + name + " actual=" + QString::number(widget->font().weight())
        );
      if (label && meterValue)
        check(widget->font().weight() == QFont::DemiBold, key + ": reference meter b value weight600 " + name);
      if (badge) {
        check(widget->font().weight() == QFont::DemiBold, key + ": reference badge weight600");
        check(
            widget->font().pixelSize() >= 10 && widget->font().pixelSize() <= 11, key + ": reference badge font10.5px"
        );
      }
      if (name == "fingerprintHash")
        check(widget->font().pixelSize() == 11, key + ": reference fingerprint code11px");
      if (name == "lblName")
        check(widget->font().pixelSize() == 22, key + ": reference About brand22px");
    }
    return controls;
  }
  void mainShell(const ReferencePage &expected, const QString &language, int width)
  {
    // Let normal language/resize/layout delivery finish before sending the real
    // click; do not force checked state or reinterpret an unactivated click.
    QVERIFY(settleStaticRender(*m_window));
    auto *mode = m_window->findChild<QAbstractButton *>(expected.tab == 0 ? "rbModeServer" : "rbModeClient");
    QVERIFY(mode);
    QTest::mouseClick(mode, Qt::LeftButton);
    QVERIFY(mode->isChecked());
    auto *status = m_window->findChild<StatusBar *>();
    auto *log = m_window->findChild<LogPanel *>();
    QVERIFY(status && log);
    const auto key = QString("main-mode%1-%2-%3").arg(expected.tab).arg(language).arg(width);
    for (bool collapsed : {false, true}) {
      if (log->isCollapsed() != collapsed) {
        const auto tooltip = observedText("LogPanel", collapsed ? "Collapse Log" : "Expand Log");
        QPushButton *toggle = nullptr;
        for (auto *button : log->findChildren<QPushButton *>())
          if (button->toolTip() == tooltip && button->isVisibleTo(log))
            toggle = button;
        QVERIFY(toggle);
        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(log->isCollapsed(), collapsed);
      }
      QVERIFY(settleStaticRender(*m_window));
      const auto image = m_window->grab().toImage();
      const QString capture = key + (collapsed ? "-collapsed" : "-expanded");
      check(image.save(outputDirectory(m_root) + "/" + capture + ".png"), capture + ": screenshot save");
      check(status->height() == 32, capture + ": status outer height32");
      const QColor title("#232326");
      const bool dark = gui::isDarkMode();
      QJsonArray samples;
      for (const auto point : {QPoint(status->width() / 3, 3), QPoint(status->width() * 2 / 3, status->height() - 4)}) {
        const auto actual = pixel(image, status->mapTo(m_window, point));
        samples.append(
            QJsonObject{
                {"x", point.x()},
                {"y", point.y()},
                {"actual", actual.name()},
                {"expected", dark ? title.name() : QStringLiteral("no light reference golden")},
                {"asserted", dark}
            }
        );
        if (dark)
          check(
              nearColor(actual, title),
              capture + ": actual StatusBar flat background=" + actual.name() + " expected#232326"
          );
      }
      const auto controls = controlTokens(m_window, capture, true);
      m_receipts.append(
          QJsonObject{
              {"key", capture},
              {"reference", expected.reference},
              {"status_samples", samples},
              {"controls", controls},
              {"errors", QJsonArray::fromStringList(m_errors)}
          }
      );
    }
    saveReceipt();
    QVERIFY2(m_errors.isEmpty(), qPrintable(m_errors.join('\n')));
  }
  void tabStrip(QDialog *editor, QTabWidget *tabs, const QString &key)
  {
    auto *bar = tabs->tabBar();
    const auto image = m_window->grab().toImage();
    const QRect strip = QRect(tabs->mapTo(m_window, QPoint()), QSize(tabs->width(), bar->height() + 3));
    const QPoint origin = bar->mapTo(m_window, QPoint());
    const bool dark = gui::isDarkMode();
    const QColor background("#1c1c1e");
    const QColor selected("#31443a");
    QJsonArray samples;
    const auto sample = [&](QPoint point, QColor expected, const QString &role) {
      if (!image.rect().contains(
              QPoint(qRound(point.x() * image.devicePixelRatio()), qRound(point.y() * image.devicePixelRatio()))
          )) {
        check(false, key + ": tab sample outside image " + role);
        return;
      }
      const QColor actual = pixel(image, point);
      samples.append(
          QJsonObject{
              {"role", role},
              {"x", point.x()},
              {"y", point.y()},
              {"actual", actual.name()},
              {"expected", dark ? expected.name() : QStringLiteral("no light reference golden")},
              {"asserted", dark}
          }
      );
      if (dark)
        check(
            nearColor(actual, expected), key + ": tab " + role + " at " + QString::number(point.x()) + "," +
                                             QString::number(point.y()) + " actual=" + actual.name() +
                                             " expected=" + expected.name()
        );
    };
    // 3px channel tolerance is only for flat pixels. Text glyphs and rounded
    // antialiased edges are excluded; all coordinates are logical then DPR-mapped.
    for (int i = 0; i < bar->count(); ++i) {
      const QRect tab = bar->tabRect(i).translated(origin);
      sample(
          {tab.left() + 6, tab.center().y()}, i == bar->currentIndex() ? selected : background,
          QString("tab%1-interior").arg(i)
      );
      sample({tab.right(), tab.center().y()}, background, QString("tab%1-gap").arg(i));
      sample({tab.center().x(), strip.top() + bar->height() + 1}, background, QString("tab%1-under-label").arg(i));
    }
    const int last = bar->tabRect(bar->count() - 1).translated(origin).right();
    for (int x = strip.left() + 3; x < strip.right() - 2; x += 8) {
      sample({x, strip.top() + bar->height() + 1}, background, "full-width-baseline");
      if (x > last + 3)
        sample({x, strip.top() + bar->height() / 2}, background, "full-width-tail");
    }
    const auto dpr = image.devicePixelRatio();
    const QRect physical(
        qRound(strip.x() * dpr), qRound(strip.y() * dpr), qRound(strip.width() * dpr), qRound(strip.height() * dpr)
    );
    check(image.copy(physical).save(outputDirectory(m_root) + "/" + key + "-tabstrip.png"), key + ": tabstrip save");
    int firstCardTop = INT_MAX;
    for (auto *card : tabs->currentWidget()->findChildren<QGroupBox *>())
      if (card->isVisibleTo(editor))
        firstCardTop = qMin(firstCardTop, card->mapTo(m_window, QPoint()).y());
    const int gap = firstCardTop - (origin.y() + bar->height());
    check(firstCardTop != INT_MAX && gap == 8, key + QString(": reference tab-to-card gap8 actual%1").arg(gap));
    m_receipts.append(
        QJsonObject{
            {"key", key},
            {"reference", "HTML.tabs CSS145-148"},
            {"strip", rectJson(strip)},
            {"tab_to_card_gap", gap},
            {"samples", samples}
        }
    );
  }
private Q_SLOTS:
  void initTestCase()
  {
    QVERIFY(m_window && m_window->isVisible());
    QVERIFY(runtime::runtimeRoot() == m_root);
    QVERIFY(!Settings::value(Settings::Gui::AutoStartCore).toBool());
    QVERIFY(!Settings::value(Settings::Gui::CheckForUpdates).toBool());
    QVERIFY(QDir().mkpath(outputDirectory(m_root)));
    QVERIFY(writeJson(outputDirectory(m_root) + "/appearance.json", appearance(m_window, m_root)));
    QDesktopServices::setUrlHandler("https", this, "observeUrl");
    QCOMPARE(qApp->font().pixelSize(), 13);
  }
  void referencePages_data()
  {
    QTest::addColumn<int>("page");
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("actionType");
    QTest::addColumn<int>("recipients");
    for (int i = 0; i < int(std::size(pages)); ++i) {
      const QString kind = QString::fromLatin1(pages[i].kind);
      const auto group = qEnvironmentVariable("SYNERGY_R4_GROUP");
      if (group == "group2" && kind != "preferences" && kind != "computer" && kind != "hotkey" && kind != "action" &&
          kind != "client")
        continue;
      if (group == "common" && kind != "main" && kind != "preferences" && kind != "computer" && kind != "server")
        continue;
      if (group == "remaining" && kind != "server" && kind != "about" && kind != "fingerprint" && kind != "update" &&
          kind != "message")
        continue;
      if (group == "presentation" && (kind != "preferences" || pages[i].tab != 0))
        continue;
      if (group == "computer-actions" && kind != "computer")
        continue;
      if (group == "sharing-fields" && (kind != "server" || pages[i].tab != 2))
        continue;
      const QList<int> types = kind == "action" ? QList<int>{3, 4, 2, 0, 1, 5, 6, 7} : QList<int>{-1};
      for (int type : types)
        for (int recipients : (type >= 0 && type <= 2 ? QList<int>{0, 1} : QList<int>{0}))
          for (const char *language : {"en", "zh_CN"})
            for (int width : {540, 600, 820})
              QTest::newRow(qPrintable(QString("%1-tab%2-type%3-recipients%4-%5-%6")
                                           .arg(pages[i].kind)
                                           .arg(pages[i].tab)
                                           .arg(type)
                                           .arg(recipients)
                                           .arg(language)
                                           .arg(width))
              ) << i
                << QString::fromLatin1(language) << width << type << recipients;
    }
  }
  void referencePages()
  {
    QFETCH(int, page);
    QFETCH(QString, language);
    QFETCH(int, width);
    QFETCH(int, actionType);
    QFETCH(int, recipients);
    m_errors.clear();
    const auto &expected = pages[page];
    I18N::setLanguage(language);
    m_window->resize(width, m_window->height());
    if (QString::fromLatin1(expected.kind) == "main") {
      mainShell(expected, language, width);
      return;
    }
    auto *host = m_window->findChild<PageHost *>();
    QVERIFY(host);
    auto &config = m_window->serverConfig();
    gui::Screen screen("R4-Studio");
    const gui::Screen originalScreen = screen;
    gui::ScreenList screens;
    screens.append(screen);
    Hotkey hotkey;
    Action action;
    QSettings actionFixture(m_root + "/r4-action.ini", QSettings::IniFormat);
    KeySequence sequence;
    sequence.appendKey(Qt::Key_K, Qt::ControlModifier);
    sequence.saveSettings(actionFixture);
    actionFixture.setValue(SettingsKeys::ActionType, 0);
    action.loadSettings(actionFixture);
    const Action originalAction = action;
    QDialog *editor = nullptr;
    const QString kind = QString::fromLatin1(expected.kind);
    if (kind == "preferences")
      editor = new SettingsDialog(m_window, config);
    if (kind == "server")
      editor = new ServerConfigDialog(m_window, config);
    if (kind == "computer")
      editor = new ScreenSettingsDialog(m_window, &screen, &screens);
    if (kind == "client")
      editor = new ClientConfigDialog(m_window);
    if (kind == "hotkey")
      editor = new HotkeyDialog(m_window, hotkey);
    if (kind == "action")
      editor = new ActionDialog(m_window, config, hotkey, action);
    if (kind == "about")
      editor = new AboutDialog(m_window);
    if (kind == "fingerprint")
      editor = new FingerprintDialog(
          m_window, Fingerprint(QCryptographicHash::Sha256, QByteArray(32, '\x5a')),
          expected.tab == 0 ? FingerprintDialogMode::Local : FingerprintDialogMode::Server,
          Fingerprint(QCryptographicHash::Sha256, QByteArray(32, '\x6b')),
          expected.tab == 0 ? m_root + "/user/fixture.pem" : QString()
      );
    PartialDownload download(kind == "update");
    if (kind == "update") {
      QVERIFY(download.isListening());
      gui::update::UpdateInfo info;
      info.version = "99.0.0";
      info.notes = "Offline visual fixture; no external network or installer.";
      info.asset.name = "r4-fixture.dmg";
      info.asset.url = download.url();
      info.asset.sha256 = QString(64, '0');
      info.asset.size = 1048576; // known-size fixture agrees with its HTTP Content-Length
      editor = new gui::update::UpdateDialog(info, m_window, {"http", "127.0.0.1", "/"});
    }
    if (kind == "message") {
      const char *titles[]{
          "Permission explanation fixture", "Existing process fixture", "Port error fixture", "Clear settings fixture",
          "Close to tray fixture"
      };
      auto *p = new MessagePage(
          m_window, expected.tab == 2 ? MessagePage::Kind::Warning : MessagePage::Kind::Question,
          QString::fromLatin1(titles[expected.tab]), "Synthetic explanation; no host operation is connected."
      );
      if (expected.tab == 4)
        p->addCheckBox("Do not show this fixture again");
      if (expected.tab != 2 && expected.tab != 4)
        p->addButton("Cancel");
      p->addButton(
          expected.tab == 3 ? "Confirm clear fixture" : "Continue fixture", expected.tab != 3, expected.tab == 3
      );
      editor = p;
    }
    QVERIFY(editor);
    const QPointer<QDialog> guardedEditor(editor);
    const auto cleanup = qScopeGuard([guardedEditor] {
      if (!guardedEditor)
        return;
      // A failed row must not leave its page or a retained transfer in the shared host.
      guardedEditor->QDialog::reject();
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });
    host->push(editor);
    QVERIFY(settleStaticRender(*m_window));
    auto *tabs = editor->findChild<QTabWidget *>();
    if (tabs) {
      QVERIFY(expected.tab < tabs->count());
      QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier, tabs->tabBar()->tabRect(expected.tab).center());
      QCOMPARE(tabs->currentIndex(), expected.tab);
    }
    if (kind == "action") {
      QVERIFY(chooseData(editor->findChild<QComboBox *>("comboActionType"), actionType));
      if (actionType >= 0 && actionType <= 2)
        QVERIFY(chooseData(editor->findChild<QComboBox *>("comboRecipients"), recipients));
    }
    QPointer<gui::update::UpdateDownloader> transfer;
    if (kind == "update" && expected.tab > 0) {
      transfer = editor->findChild<gui::update::UpdateDownloader *>();
      QVERIFY(transfer);
      QSignalSpy progress(transfer, &gui::update::UpdateDownloader::progress);
      QPushButton *start = nullptr;
      const auto text = observedText("synergy::gui::update::UpdateDialog", "Download and Install");
      for (auto *button : editor->findChildren<QPushButton *>())
        if (button->text() == text)
          start = button;
      QVERIFY(start);
      QTest::mouseClick(start, Qt::LeftButton);
      QTRY_COMPARE_WITH_TIMEOUT(download.requests, 1, 2000);
      download.send();
      QTRY_VERIFY_WITH_TIMEOUT(progress.count() > 0, 2000);
      if (expected.tab == 2) {
        // Observe the real named state, not elapsed time alone. The existing
        // flow contract allows3s for1.5s staleness plus500ms timer delivery.
        const auto waiting = observedText("synergy::gui::update::UpdateDialog", "Waiting for network");
        auto *rate = editor->findChild<QLabel *>("downloadRateValue");
        auto *eta = editor->findChild<QLabel *>("downloadEtaValue");
        QVERIFY(rate && eta);
        QVERIFY(rate->isVisibleTo(editor) && eta->isVisibleTo(editor));
        QTRY_COMPARE_WITH_TIMEOUT(rate->text(), waiting, 3000);
        check(
            !rate->text().contains("MB/s") && !rate->text().contains('%'),
            "actual stalled rate cell has no positive rate: " + rate->text()
        );
        check(
            !eta->text().contains(QRegularExpression(QStringLiteral("[0-9]+"))),
            "actual stalled ETA cell has no finite remaining time: " + eta->text()
        );
        m_receipts.append(
            QJsonObject{
                {"key", "update-stalled-meter-" + language + "-" + QString::number(width)},
                {"rate_cell", rate->objectName()},
                {"rate", rate->text()},
                {"eta_cell", eta->objectName()},
                {"eta", eta->text()},
                {"received_bytes", progress.last().at(0).toLongLong()}
            }
        );
        check(progress.last().at(0).toLongLong() == 65536, "stalled fixture must report actual65536 body bytes");
      }
    }
    QVERIFY(settleStaticRender(*m_window));
    const QString key = QString("%1-tab%2-type%3-recipients%4-%5-%6")
                            .arg(kind)
                            .arg(expected.tab)
                            .arg(actionType)
                            .arg(recipients)
                            .arg(language)
                            .arg(width);
    check(m_window->grab().save(outputDirectory(m_root) + "/" + key + ".png"), key + ": screenshot save");
    QWidget *active = tabs ? tabs->currentWidget() : editor;
    QList<QWidget *> cards;
    for (auto *widget : active->findChildren<QWidget *>()) {
      if (!widget->isVisibleTo(editor) || (!qobject_cast<QGroupBox *>(widget) && widget->objectName() != "card"))
        continue;
      bool nested = false;
      for (auto *parent = widget->parentWidget(); parent && parent != active; parent = parent->parentWidget())
        if (qobject_cast<QGroupBox *>(parent) || parent->objectName() == "card")
          nested = true;
      if (!nested)
        cards << widget;
    }
    int headings = 0;
    for (auto *label : active->findChildren<QLabel *>())
      if (label->isVisibleTo(editor) &&
          (label->objectName() == "cardHeading" || label->objectName() == "messageTitle" ||
           label->objectName() == "lblName" || label->property("sectionHeading").toBool() ||
           (kind == "fingerprint" &&
            (label->text() == observedText("FingerprintDialog", "Local computer's fingerprint") ||
             label->text() == observedText("FingerprintDialog", "Server Fingerprint") ||
             label->text() == observedText("FingerprintDialog", "Client Fingerprint")))))
        ++headings;
    const bool composition = qEnvironmentVariable("SYNERGY_R4_GROUP") != "common";
    if (composition) {
      check(
          cards.size() == expected.cards,
          key + QString(": HTML card count expected%1 actual%2").arg(expected.cards).arg(cards.size())
      );
      check(
          headings == expected.headings,
          key + QString(": HTML heading count expected%1 actual%2").arg(expected.headings).arg(headings)
      );
      if (kind == "preferences" && expected.tab == 0) {
        const bool chinese = language == "zh_CN";
        const QList<QPair<QString, QString>> labels{
            {"lblLanguage", chinese ? QString::fromUtf8("界面语言") : QString("Language")},
            {"lblTrayIconStyle", chinese ? QString::fromUtf8("托盘图标") : QString("Tray icon")},
            {"scopeLabel", chinese ? QString::fromUtf8("设置作用域") : QString("Settings scope")},
            {"cbPreventSleep", chinese ? QString::fromUtf8("阻止此计算机进入睡眠") : QString("Prevent sleep")}
        };
        for (const auto &entry : labels) {
          auto *widget = editor->findChild<QWidget *>(entry.first);
          QString visible;
          if (auto *label = qobject_cast<QLabel *>(widget))
            visible = label->text();
          if (auto *box = qobject_cast<QCheckBox *>(widget))
            visible = box->text();
          check(
              widget && widget->isVisibleTo(editor) && visible.remove('&') == entry.second,
              key + ": reference General label " + entry.first + " expected=" + entry.second + " actual=" + visible
          );
        }
      }
      for (auto *group : active->findChildren<QGroupBox *>())
        if (group->isVisibleTo(editor)) {
          check(group->title().isEmpty(), key + ": native border title " + group->objectName());
          check(!group->isCheckable(), key + ": checkable border group " + group->objectName());
        }
      auto *box = editor->findChild<QDialogButtonBox *>();
      if ((expected.actionsInside && kind != "update" && kind != "message") || kind == "client")
        check(box && box->isVisibleTo(editor), key + ": reference action box must exist and be visible");
      if (box && expected.actionsInside) {
        bool inside = false;
        for (auto *card : cards)
          if (card->isAncestorOf(box) && card->rect().contains(bounds(box, card)))
            inside = true;
        check(inside, key + ": reference actions must be inside the content card");
      }
      if (box && kind == "computer") {
        auto *card = owningCard(box, cards);
        check(card, key + ": computer actions have actual owning card");
        if (card) {
          int rightmost = -1;
          for (auto *button : box->findChildren<QPushButton *>())
            if (button->isVisibleTo(editor))
              rightmost = qMax(rightmost, bounds(button, card).right() + 1);
          const int expectedRight = card->width() - 12;
          check(
              rightmost == expectedRight, key + QString(": Save/Cancel row right edge%1 must equal card inner right%2")
                                                    .arg(rightmost)
                                                    .arg(expectedRight)
          );
          m_receipts.append(
              QJsonObject{
                  {"key", key + "-actions-edge"},
                  {"card_width", card->width()},
                  {"rightmost_button_edge", rightmost},
                  {"expected_inner_right", expectedRight},
                  {"box_rect", rectJson(bounds(box, card))}
              }
          );
        }
      }
      if (box && kind == "client") {
        for (auto *card : cards)
          check(!card->isAncestorOf(box), key + ": client reference actions stay outside both cards");
      }
      if (kind == "action") {
        auto *label = editor->findChild<QLabel *>("labelTrigger");
        auto *field = editor->findChild<QComboBox *>("comboTriggerOn");
        check(label && field, key + ": action reference label/control");
        QWidget *formCard = label ? label->parentWidget() : nullptr;
        while (formCard && !qobject_cast<QGroupBox *>(formCard))
          formCard = formCard->parentWidget();
        check(formCard, key + ": action form card must exist");
        if (label && field && formCard) {
          check(bounds(label, formCard).left() == 12, key + ": action label starts at12px inset");
          check(
              bounds(field, formCard).left() - bounds(label, formCard).left() == 130,
              key + ": action120px label column +10px gap"
          );
          check(field->width() == 180, key + ": action180px control column");
        }
        auto *recipientCombo = editor->findChild<QComboBox *>("comboRecipients");
        if (recipientCombo && recipientCombo->isVisibleTo(editor))
          check(
              formCard && formCard->isAncestorOf(recipientCombo), key + ": recipients belong to the same action card"
          );
        const QList<QPair<QString, bool>> fields{
            {"keySequenceWidget", actionType >= 0 && actionType <= 2},
            {"comboSwitchToScreen", actionType == 3},
            {"comboSwitchInDirection", actionType == 4},
            {"comboLockCursorToScreen", actionType == 6},
            {"comboRecipients", actionType >= 0 && actionType <= 2},
            {"listScreens", actionType >= 0 && actionType <= 2 && recipients == 1}
        };
        for (const auto &entry : fields) {
          auto *widget = editor->findChild<QWidget *>(entry.first);
          check(
              widget && widget->isVisibleTo(editor) == entry.second, key + ": relevant field visibility " + entry.first
          );
          if (widget && widget->isVisibleTo(editor))
            check(formCard && formCard->isAncestorOf(widget), key + ": related field same-card " + entry.first);
        }
      }
      int previousBottom = -1;
      for (const auto &name : QString::fromLatin1(expected.orderedFields).split(',', Qt::SkipEmptyParts)) {
        auto *field = editor->findChild<QWidget *>(name);
        check(field && field->isVisibleTo(editor), key + ": missing reference field " + name);
        if (field && field->isVisibleTo(editor)) {
          const QRect row = bounds(field, editor);
          check(row.top() > previousBottom, key + ": reference rows must be strictly below preceding row " + name);
          previousBottom = row.bottom();
        }
      }
      // Reference row alignment is independent of natural/native label widths.
      if (kind == "server" && expected.tab == 2) {
        const QList<QPair<QString, QString>> fieldLabels{
            {"sbClipboardSizeLimit", "label_7"}, {"sbSwitchCornerSize", "label"},
            {"sbHeartbeat", "cbHeartbeat"},      {"comboProtocol", "labelProtocol"},
            {"sbSwitchDelay", "cbSwitchDelay"},  {"sbSwitchDoubleTap", "cbSwitchDoubleTap"}
        };
        QJsonArray columns;
        for (const auto &entry : fieldLabels) {
          auto *field = editor->findChild<QWidget *>(entry.first);
          auto *label = editor->findChild<QWidget *>(entry.second);
          check(
              field && field->isVisibleTo(editor) && label && label->isVisibleTo(editor),
              key + ": visible real sharing field/label " + entry.first
          );
          if (!field || !label)
            continue;
          auto *card = owningCard(field, cards);
          check(card, key + ": sharing field inside actual content card " + entry.first);
          if (!card)
            continue;
          const auto fieldRect = bounds(field, card);
          const auto labelRect = bounds(label, card);
          check(
              fieldRect.left() == 142 && fieldRect.width() == 180,
              key + QString(": sharing field %1 actual left%2 width%3 requires card142/180")
                        .arg(entry.first)
                        .arg(fieldRect.left())
                        .arg(fieldRect.width())
          );
          QString text;
          QRect textRect;
          QRect available = label->contentsRect();
          if (auto *plain = qobject_cast<QLabel *>(label)) {
            text = plain->text();
            textRect = plain->fontMetrics().boundingRect(
                available, Qt::TextShowMnemonic | (plain->wordWrap() ? Qt::TextWordWrap : Qt::TextSingleLine), text
            );
          }
          if (auto *checkBox = qobject_cast<QCheckBox *>(label)) {
            text = checkBox->text();
            QStyleOptionButton option;
            option.initFrom(checkBox);
            option.text = text;
            available = checkBox->style()->subElementRect(QStyle::SE_CheckBoxContents, &option, checkBox);
            textRect = checkBox->fontMetrics().boundingRect(available, Qt::TextShowMnemonic, text);
          }
          check(
              !text.isEmpty() && textRect.width() <= available.width() && textRect.height() <= available.height(),
              key + ": sharing label must be fully readable without clipping " + entry.second
          );
          columns.append(
              QJsonObject{
                  {"field", entry.first},
                  {"field_rect", rectJson(fieldRect)},
                  {"label", entry.second},
                  {"label_rect", rectJson(labelRect)},
                  {"text", text},
                  {"text_rect", rectJson(textRect)},
                  {"text_available", rectJson(available)}
              }
          );
        }
        m_receipts.append(QJsonObject{{"key", key + "-sharing-columns"}, {"columns", columns}});
        const QStringList names{
            "cbEnableClipboard", "cbRelativeMouseMoves", "cbDefaultLockToScreenState", "cbDisableLockToScreen"
        };
        int left = -1;
        for (const auto &name : names)
          if (auto *field = editor->findChild<QWidget *>(name)) {
            const int x = bounds(field, editor).left();
            if (left >= 0)
              check(x == left, key + ": sharing checkbox common left edge " + name);
            left = x;
          }
      }
      if (kind == "about") {
        QWidget *brand = nullptr;
        for (const auto &name : {"lblName", "lblVersion"}) {
          auto *field = editor->findChild<QWidget *>(name);
          check(field && field->isVisibleTo(editor), key + ": visible brand content " + name);
          if (field) {
            auto *card = owningCard(field, cards);
            check(card, key + ": brand content inside a card " + name);
            if (!brand)
              brand = card;
            else
              check(card == brand, key + ": brand/version share first card " + name);
          }
        }
        for (const auto &actionText :
             {observedText("AboutDialog", "Project home"), observedText("AboutDialog", "Contributors")}) {
          QPushButton *actionButton = nullptr;
          for (auto *candidate : editor->findChildren<QPushButton *>())
            if (candidate->isVisibleTo(editor) && candidate->text() == actionText)
              actionButton = candidate;
          check(actionButton, key + ": visible semantic About action " + actionText);
          if (actionButton) {
            check(owningCard(actionButton, cards) == brand, key + ": project/contributor actions in brand card");
            m_observedUrls.clear();
            QTest::mouseClick(actionButton, Qt::LeftButton);
            const QString destination = actionText == observedText("AboutDialog", "Contributors")
                                            ? "https://github.com/Tupig/TupigSynergy/graphs/contributors"
                                            : "https://github.com/Tupig/TupigSynergy";
            check(
                m_observedUrls == QList<QUrl>{QUrl(destination)},
                key + ": actual About route; HTTPS intercepted inside this test process"
            );
          }
        }
        auto *notices = editor->findChild<QPushButton *>("aboutNoticesButton");
        check(
            notices && notices->isVisibleTo(editor) && owningCard(notices, cards) &&
                owningCard(notices, cards) != brand,
            key + ": visible retained-notice disclosure in separate second card"
        );
      }
      if (kind == "fingerprint") {
        const auto hashes = editor->findChildren<QLabel *>("fingerprintHash");
        check(hashes.size() == (expected.tab == 0 ? 1 : 2), key + ": real complete fingerprint count");
        for (int index = 0; index < hashes.size(); ++index) {
          auto *hash = hashes.at(index);
          auto *card = owningCard(hash, cards);
          check(card, key + ": fingerprint code inside card");
          const QByteArray bytes(32, index == 0 ? '\x5a' : '\x6b');
          const auto exact = QString::fromLatin1(bytes.left(16).toHex(':').toUpper()) + '\n' +
                             QString::fromLatin1(bytes.mid(16).toHex(':').toUpper());
          check(
              hash->text() == exact,
              key + QString(": fingerprint%1 must equal its complete5a/6b fixture bytes in two16-byte lines").arg(index)
          );
          check(hash->alignment().testFlag(Qt::AlignLeft), key + ": fingerprint code left aligned");
        }
        if (expected.tab == 0)
          for (auto *label : editor->findChildren<QLabel *>())
            if (label->text() == observedText("FingerprintDialog", "Local computer's fingerprint"))
              check(
                  owningCard(label, cards) == owningCard(hashes.value(0), cards),
                  key + ": local fingerprint heading belongs inside hash card"
              );
        if (expected.tab == 1) {
          check(box && box->isVisibleTo(editor), key + ": comparison actions visible");
          if (box) {
            auto *reject = box->button(QDialogButtonBox::No);
            auto *trust = box->button(QDialogButtonBox::Yes);
            check(
                reject && trust && reject->isDefault() && !trust->autoDefault(),
                key + ": reject remains default; no acceptance invoked"
            );
          }
        }
      }
      if (kind == "update" || kind == "message") {
        QList<QPushButton *> actions;
        for (auto *button : editor->findChildren<QPushButton *>())
          if (button->isVisibleTo(editor)) {
            if (kind == "message" ||
                button->text() == observedText("synergy::gui::update::UpdateDialog", "Open Release Page") ||
                button->text() == observedText("synergy::gui::update::UpdateDialog", "Later") ||
                button->text() == observedText("synergy::gui::update::UpdateDialog", "Cancel download") ||
                button->text() == observedText("synergy::gui::update::UpdateDialog", "Download and Install"))
              actions << button;
          }
        const int expectedActions = kind == "update" ? 3 : ((expected.tab == 2 || expected.tab == 4) ? 1 : 2);
        check(actions.size() == expectedActions, key + ": reference action count");
        for (auto *button : actions)
          check(
              owningCard(button, cards),
              key + ": reference message/update actions inside content card " + button->text()
          );
        if (kind == "update") {
          auto *notes = editor->findChild<QWidget *>("releaseNotes");
          check(notes && owningCard(notes, cards), key + ": update notes inside same card");
          if (notes)
            for (auto *button : actions)
              check(owningCard(button, cards) == owningCard(notes, cards), key + ": update notes/actions one card");
          if (expected.tab > 0) {
            auto *progress = editor->findChild<QProgressBar *>();
            if (progress)
              m_receipts.append(
                  QJsonObject{
                      {"key", key + "-progress"},
                      {"declared_asset_size", 1048576},
                      {"minimum", progress->minimum()},
                      {"maximum", progress->maximum()},
                      {"value", progress->value()},
                      {"visible", progress->isVisibleTo(editor)}
                  }
              );
            check(
                progress && progress->isVisibleTo(editor) && progress->value() > 0,
                key + ": actual partial-byte progress visible"
            );
          }
        }
      }
    }
    const auto controls = controlTokens(editor, key, qEnvironmentVariable("SYNERGY_R4_GROUP") != "group2");
    if (qEnvironmentVariable("SYNERGY_R4_GROUP") != "group2") {
      const auto image = editor->grab().toImage();
      for (auto *card : cards) {
        const QRect r = bounds(card, editor);
        if (gui::isDarkMode()) {
          check(
              nearColor(pixel(image, r.topLeft() + QPoint(1, 1)), QColor("#1c1c1e")),
              key + ":11px rounded card corner must expose app background"
          );
          check(
              nearColor(pixel(image, r.topLeft() + QPoint(r.width() / 2, 4)), QColor("#242427")),
              key + ": card flat background must be#242427"
          );
        }
        if (auto *layout = card->layout()) {
          const auto margins = layout->contentsMargins();
          check(
              margins.left() == 12 && margins.right() == 12 && margins.top() >= 10 && margins.top() <= 12 &&
                  margins.bottom() >= 10 && margins.bottom() <= 12,
              key + ": reference painted-card inset12/10..12"
          );
        }
      }
    }
    if (tabs && qEnvironmentVariable("SYNERGY_R4_GROUP") != "group2")
      tabStrip(editor, tabs, key);
    m_receipts.append(
        QJsonObject{
            {"key", key},
            {"reference", expected.reference},
            {"cards", cards.size()},
            {"headings", headings},
            {"controls", controls}
        }
    );
    if (kind == "update" && expected.tab > 0) {
      // Back leaves a live download running. Use the explicit Cancel action
      // to verify that the incomplete transfer is destroyed.
      const auto cancelText = observedText("synergy::gui::update::UpdateDialog", "Cancel download");
      QPushButton *cancel = nullptr;
      for (auto *button : editor->findChildren<QPushButton *>()) {
        if (button->isVisibleTo(editor) && button->text() == cancelText)
          cancel = button;
      }
      QVERIFY(cancel != nullptr);
      QTest::mouseClick(cancel, Qt::LeftButton);
    } else {
      editor->reject();
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(settleStaticRender(*m_window));
    if (kind == "action")
      check(action == originalAction, key + ": Cancel must preserve the original Action value");
    if (kind == "computer")
      QCOMPARE(screen, originalScreen);
    if (kind == "update" && expected.tab > 0)
      check(!transfer, key + ": Cancel destroys actual incomplete transfer; installer never reached");
    auto receipt = m_receipts.last().toObject();
    receipt["errors"] = QJsonArray::fromStringList(m_errors);
    m_receipts[m_receipts.size() - 1] = receipt;
    saveReceipt();
    QVERIFY2(m_errors.isEmpty(), qPrintable(m_errors.join('\n')));
  }
  void readableShortcutsAndRecording_data()
  {
    QTest::addColumn<int>("fixture");
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("width");
    for (int fixture = 0; fixture < 5; ++fixture)
      for (const char *language : {"en", "zh_CN"})
        for (int width : {540, 600, 820})
          QTest::newRow(qPrintable(QString("fixture%1-%2-%3").arg(fixture).arg(language).arg(width)))
              << fixture << QString::fromLatin1(language) << width;
  }
  void readableShortcutsAndRecording()
  {
    QFETCH(int, fixture);
    QFETCH(QString, language);
    QFETCH(int, width);
    m_errors.clear();
    I18N::setLanguage(language);
    m_window->resize(width, m_window->height());
    KeySequence seeded;
    if (fixture > 0)
      seeded.appendKey(Qt::Key_Control, Qt::ControlModifier);
    if (fixture == 1)
      seeded.appendKey(Qt::Key_K, Qt::ControlModifier);
    if (fixture == 2) {
      seeded.appendKey(Qt::Key_Alt, Qt::ControlModifier | Qt::AltModifier);
      seeded.appendKey(Qt::Key_Right, Qt::ControlModifier | Qt::AltModifier);
    }
    if (fixture == 3)
      seeded.appendMouseButton(Qt::XButton1);
    if (fixture == 4) {
      seeded.appendKey(Qt::Key_K, Qt::ControlModifier);
      seeded.appendKey(Qt::Key_L, Qt::ControlModifier);
    }
    const QString key = QString("presentation-keys%1-%2-%3").arg(fixture).arg(language).arg(width);
    const auto snapshot = [&](const KeySequence &sequence, const QString &suffix) {
      QSettings store(m_root + "/" + key + suffix + ".ini", QSettings::IniFormat);
      store.clear();
      sequence.saveSettings(store);
      store.sync();
      QMap<QString, QVariant> result;
      for (const auto &name : store.allKeys())
        result.insert(name, store.value(name));
      return result;
    };
    QSettings fixtureStore(m_root + "/" + key + "-fixture.ini", QSettings::IniFormat);
    fixtureStore.clear();
    seeded.saveSettings(fixtureStore);
    Hotkey hotkey;
    hotkey.loadSettings(fixtureStore);
    const Hotkey originalHotkey = hotkey;
    const KeySequence original = std::as_const(hotkey).keySequence();
    QCOMPARE(original.sequence(), std::as_const(seeded).sequence());
    QCOMPARE(original.toString(), seeded.toString());
    const auto before = snapshot(original, "-before");
    const auto protocol = original.toString();
    QPointer<HotkeyDialog> editor = new HotkeyDialog(m_window, hotkey);
    const auto dismiss = qScopeGuard([&] {
      if (editor) {
        editor->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      }
    });
    auto *host = m_window->findChild<PageHost *>();
    QVERIFY(host);
    host->push(editor);
    QVERIFY(settleStaticRender(*m_window));
    auto *keys = editor->findChild<KeySequenceWidget *>();
    auto *box = editor->findChild<QDialogButtonBox *>();
    QVERIFY(keys && box && box->button(QDialogButtonBox::Ok));
    const auto stopped = keys->text();
    check(
        keys->height() == 30,
        key + ": real KeySequenceWidget is the reference30px readonly field; Record is a separate32px action"
    );
    check(
        !stopped.trimmed().isEmpty() &&
            !stopped.contains(QRegularExpression(QStringLiteral("(?:keystroke|mousebutton)\\s*\\("))),
        key + ": readable shortcut caption, no protocol call"
    );
    if (fixture == 4)
      check(
          stopped.contains(QRegularExpression(QStringLiteral("\\bK\\b"), QRegularExpression::CaseInsensitiveOption)) &&
              stopped.contains(QRegularExpression(QStringLiteral("\\bL\\b"), QRegularExpression::CaseInsensitiveOption)
              ),
          key + ": persisted multi-key caption retains independent K and L tokens"
      );
    if (fixture == 1)
      check(stopped.contains('K', Qt::CaseInsensitive), key + ": caption retains K identity");
    if (fixture == 2)
      check(
          stopped.contains(QString::fromUtf8("→")) || stopped.contains("Right", Qt::CaseInsensitive) ||
              stopped.contains(QString::fromUtf8("右")),
          key + ": caption retains Right identity"
      );
    if (fixture == 3)
      check(
          stopped.contains("Mouse", Qt::CaseInsensitive) || stopped.contains("X1", Qt::CaseInsensitive) ||
              stopped.contains(QString::fromUtf8("鼠标")),
          key + ": caption identifies mouse shortcut"
      );
    if (fixture > 0)
      check(
          stopped.contains("Ctrl", Qt::CaseInsensitive) || stopped.contains("Control", Qt::CaseInsensitive) ||
              stopped.contains(QString::fromUtf8("⌃")) || stopped.contains(QString::fromUtf8("⌘")),
          key + ": caption retains Control modifier identity"
      );
    if (fixture == 2)
      check(
          stopped.contains("Alt", Qt::CaseInsensitive) || stopped.contains(QString::fromUtf8("⌥")),
          key + ": caption retains Alt modifier identity"
      );
    QPushButton *record = nullptr;
    for (auto *candidate : editor->findChildren<QPushButton *>())
      if (candidate != keys && candidate->isVisibleTo(editor) &&
          (candidate->text().contains("Record", Qt::CaseInsensitive) ||
           candidate->text().contains(QString::fromUtf8("录制"))))
        record = candidate;
    check(record && record != keys, key + ": independent localized visible Record action required");
    if (record && record != keys)
      check(record->height() == 32, key + ": explicit Record action outer32px");
    const bool explicitRecord = record && record != keys;
    check(m_window->grab().save(outputDirectory(m_root) + "/" + key + "-stopped.png"), key + ": stopped capture");
    QSignalSpy recording(keys, &KeySequenceWidget::recordingChanged);
    // Baseline may lack the explicit control; still verify its existing field
    // route independently, while retaining the missing-Record failure above.
    QTest::mouseClick(record ? record : keys, Qt::LeftButton);
    QVERIFY(keys->isRecording());
    QVERIFY(!box->button(QDialogButtonBox::Ok)->isEnabled());
    QVERIFY(settleStaticRender(*m_window));
    check(m_window->grab().save(outputDirectory(m_root) + "/" + key + "-recording.png"), key + ": recording capture");
    QTest::keyClick(keys, Qt::Key_Escape);
    QVERIFY(!keys->isRecording());
    QCOMPARE(recording.count(), 2);
    QVERIFY(recording.first().at(0).toBool());
    QVERIFY(!recording.last().at(0).toBool());
    QCOMPARE(keys->keySequence(), original);
    QCOMPARE(keys->keySequence().toString(), protocol);
    QCOMPARE(snapshot(keys->keySequence(), "-after"), before);
    QCOMPARE(keys->text(), stopped);
    QCOMPARE(box->button(QDialogButtonBox::Ok)->isEnabled(), original.valid());
    QVERIFY(settleStaticRender(*m_window));
    check(m_window->grab().save(outputDirectory(m_root) + "/" + key + "-restored.png"), key + ": restored capture");
    editor->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(hotkey, originalHotkey);
    m_receipts.append(
        QJsonObject{
            {"key", key},
            {"reference", "HTML618-627; P01 worksheet548f7592"},
            {"stopped_caption", stopped},
            {"explicit_record", explicitRecord},
            {"core_string_before_after", protocol},
            {"saved_key_count", before.size()},
            {"record_signal_count", recording.count()},
            {"errors", QJsonArray::fromStringList(m_errors)}
        }
    );
    saveReceipt();
    QVERIFY2(m_errors.isEmpty(), qPrintable(m_errors.join('\n')));
  }
  void protocolControls_data()
  {
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("initial");
    QTest::addColumn<bool>("mouse");
    for (const char *language : {"en", "zh_CN"})
      for (int width : {540, 600, 820})
        for (int initial : {0, 1})
          for (bool mouse : {true, false})
            QTest::newRow(qPrintable(QString("protocol-%1-%2-initial%3-%4")
                                         .arg(language)
                                         .arg(width)
                                         .arg(initial)
                                         .arg(mouse ? "mouse" : "keyboard"))
            ) << QString::fromLatin1(language)
              << width << initial << mouse;
  }
  void protocolControls()
  {
    QFETCH(QString, language);
    QFETCH(int, width);
    QFETCH(int, initial);
    QFETCH(bool, mouse);
    m_errors.clear();
    const QString key =
        QString("protocol-%1-%2-initial%3-%4").arg(language).arg(width).arg(initial).arg(mouse ? "mouse" : "keyboard");
    const QString oldSettings = Settings::settingsFile();
    const auto restore = qScopeGuard([&] { Settings::setSettingsFile(oldSettings); });
    Settings::setSettingsFile(m_root + "/" + key + ".conf");
    Settings::setValue(Settings::Core::Language, language);
    I18N::setLanguage(language);
    const QStringList literals{QStringLiteral("synergy"), QStringLiteral("barrier")};
    const QList<NetworkProtocol> ids{NetworkProtocol::Synergy, NetworkProtocol::Barrier};
    Settings::setValue(Settings::Core::ComputerName, "Protocol-Host");
    Settings::setValue(Settings::Gui::AutoStartCore, false);
    Settings::setValue(Settings::Gui::CheckForUpdates, false);
    Settings::setValue(Settings::Server::ExternalConfig, false);
    Settings::setValue(Settings::Server::ExternalConfigFile, QString());
    Settings::setValue(Settings::Server::Protocol, literals[initial]);
    gui::ServerConfig config;
    config.addClient("Protocol-Peer");
    config.commit();
    const gui::ServerConfig original = config;
    const auto settingsMap = [] {
      auto &store = Settings::proxy().get();
      store.sync();
      QVariantMap result;
      for (const auto &name : store.allKeys())
        result.insert(name, store.value(name));
      return result;
    };
    const auto wire = [](const gui::ServerConfig &value) {
      QString text;
      QTextStream stream(&text);
      stream << value;
      stream.flush();
      return text;
    };
    const auto baseline = settingsMap();
    const QString baselineWire = wire(config);
    const QString oldLine = "\tprotocol = " + literals[initial] + "\n";
    const int target = 1 - initial;
    const QString newLine = "\tprotocol = " + literals[target] + "\n";
    QCOMPARE(baselineWire.count(oldLine), 1);
    QCOMPARE(config.protocol(), ids[initial]);
    const auto root = outputDirectory(m_root);
    QVERIFY(writeJson(root + "/" + key + "-settings-before.json", QJsonObject::fromVariantMap(baseline)));
    QFile wireFile(root + "/" + key + "-core-before.txt");
    QVERIFY(wireFile.open(QIODevice::WriteOnly));
    QCOMPARE(wireFile.write(baselineWire.toUtf8()), qint64(baselineWire.toUtf8().size()));
    wireFile.close();
    m_window->resize(width, m_window->height());
    auto *host = m_window->findChild<PageHost *>();
    QVERIFY(host);
    QPointer<ServerConfigDialog> editor;
    const auto dismiss = qScopeGuard([&] {
      if (editor) {
        editor->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      }
    });
    bool allHaveDropdown = true;
    QJsonArray routes;
    const auto open = [&]() {
      editor = new ServerConfigDialog(m_window, config);
      host->push(editor);
      if (!settleStaticRender(*m_window))
        return false;
      auto *tabs = editor->findChild<QTabWidget *>();
      if (!tabs || tabs->count() < 3)
        return false;
      QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier, tabs->tabBar()->tabRect(2).center());
      if (tabs->currentIndex() != 2 || !settleStaticRender(*m_window))
        return false;
      auto *combo = editor->findChild<QComboBox *>("comboProtocol");
      allHaveDropdown = allHaveDropdown && combo && combo->isVisibleTo(editor);
      check(combo && combo->isVisibleTo(editor), key + ": reference requires visible two-choice protocol QComboBox");
      if (combo) {
        check(combo->count() == 2, key + ": exactly two supported protocol choices");
        for (int i = 0; i < combo->count(); ++i) {
          check(
              i < 2 && combo->itemData(i).userType() == qMetaTypeId<NetworkProtocol>(),
              key + ": protocol itemData keeps typed enum"
          );
          if (i < 2) {
            check(combo->itemData(i).value<NetworkProtocol>() == ids[i], key + ": fixed enum ID order");
            check(
                combo->itemText(i) == (i == 0 ? QStringLiteral("Synergy") : QStringLiteral("Barrier")),
                key + ": protocol visible name"
            );
          }
        }
      }
      return true;
    };
    const auto selection = [&](int expected) {
      if (auto *combo = editor->findChild<QComboBox *>("comboProtocol"))
        return combo->currentData().userType() == qMetaTypeId<NetworkProtocol>() &&
               combo->currentData().value<NetworkProtocol>() == ids[expected];
      auto *radio = editor->findChild<QRadioButton *>(expected == 0 ? "rbProtocolSynergy" : "rbProtocolBarrier");
      return radio && radio->isVisibleTo(editor) && radio->isChecked();
    };
    const auto activate = [&](int expected) {
      if (auto *combo = editor->findChild<QComboBox *>("comboProtocol")) {
        const int row = combo->findData(QVariant::fromValue(ids[expected]));
        if (row < 0)
          return false;
        QSignalSpy activated(combo, qOverload<int>(&QComboBox::activated));
        combo->showPopup();
        QCoreApplication::processEvents();
        auto *view = combo->view();
        if (!view || !view->isVisible())
          return false;
        const auto index = combo->model()->index(row, combo->modelColumn(), combo->rootModelIndex());
        view->scrollTo(index);
        QCoreApplication::processEvents();
        if (mouse)
          QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualRect(index).center());
        else {
          QTest::keyClick(view, Qt::Key_Home);
          for (int i = 0; i < row; ++i)
            QTest::keyClick(view, Qt::Key_Down);
          QTest::keyClick(view, Qt::Key_Return);
        }
        QCoreApplication::processEvents();
        const bool immediate = activated.count() == 1 && selection(expected);
        auto *container = view->parentWidget();
        QElapsedTimer timer;
        timer.start();
        while (immediate && (view->isVisible() || (container && container->isVisible())) && timer.elapsed() < 1000)
          QTest::qWait(10);
        routes.append(
            QJsonObject{
                {"widget", "QComboBox"},
                {"mouse", mouse},
                {"target_enum", expected},
                {"immediate_activations", activated.count()}
            }
        );
        return immediate && activated.count() == 1 && !view->isVisible() && (!container || !container->isVisible());
      }
      auto *radio = editor->findChild<QRadioButton *>(expected == 0 ? "rbProtocolSynergy" : "rbProtocolBarrier");
      if (!radio || !radio->isVisibleTo(editor))
        return false;
      QSignalSpy clicked(radio, &QRadioButton::clicked);
      if (mouse)
        QTest::mouseClick(radio, Qt::LeftButton, Qt::NoModifier, QPoint(8, radio->height() / 2));
      else {
        radio->setFocus();
        QTest::keyClick(radio, Qt::Key_Space);
      }
      QCoreApplication::processEvents();
      routes.append(
          QJsonObject{
              {"widget", "legacy QRadioButton fallback"},
              {"mouse", mouse},
              {"target_enum", expected},
              {"clicks", clicked.count()}
          }
      );
      return clicked.count() == 1 && selection(expected);
    };
    const auto close = [&](QDialogButtonBox::StandardButton which) {
      auto *box = editor->findChild<QDialogButtonBox *>();
      if (!box)
        return false;
      auto *button = box->button(which);
      if (!button || !button->isEnabled())
        return false;
      QTest::mouseClick(button, Qt::LeftButton);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      return true;
    };
    QVERIFY(open());
    QVERIFY(selection(initial));
    QVERIFY(activate(target));
    QCOMPARE(config, original);
    QCOMPARE(Settings::value(Settings::Server::Protocol).toString(), literals[target]);
    QVERIFY(settleStaticRender(*m_window));
    QVERIFY(m_window->grab().save(root + "/" + key + "-preview.png"));
    QVERIFY(close(QDialogButtonBox::Cancel));
    QCOMPARE(config, original);
    QCOMPARE(settingsMap(), baseline);
    QCOMPARE(wire(config), baselineWire);
    QVERIFY(open());
    QVERIFY(selection(initial));
    QVERIFY(activate(target));
    QVERIFY(close(QDialogButtonBox::Ok));
    config.commit();
    QCOMPARE(config.protocol(), ids[target]);
    QCOMPARE(Settings::value(Settings::Server::Protocol).toString(), literals[target]);
    auto expectedSettings = baseline;
    expectedSettings.insert(QStringLiteral("server/protocol"), literals[target]);
    QCOMPARE(settingsMap(), expectedSettings);
    QString expectedWire = baselineWire;
    expectedWire.replace(oldLine, newLine);
    QCOMPARE(wire(config), expectedWire);
    gui::ServerConfig reloaded;
    QCOMPARE(reloaded, config);
    QCOMPARE(wire(reloaded), expectedWire);
    QVERIFY(writeJson(root + "/" + key + "-settings-saved.json", QJsonObject::fromVariantMap(settingsMap())));
    QFile savedWire(root + "/" + key + "-core-saved.txt");
    QVERIFY(savedWire.open(QIODevice::WriteOnly));
    QCOMPARE(savedWire.write(expectedWire.toUtf8()), qint64(expectedWire.toUtf8().size()));
    savedWire.close();
    const gui::ServerConfig saved = config;
    QVERIFY(open());
    QVERIFY(selection(target));
    QVERIFY(settleStaticRender(*m_window));
    QVERIFY(m_window->grab().save(root + "/" + key + "-reopened.png"));
    QVERIFY(activate(initial));
    QVERIFY(close(QDialogButtonBox::Cancel));
    QCOMPARE(config, saved);
    QCOMPARE(settingsMap(), expectedSettings);
    QCOMPARE(wire(config), expectedWire);
    m_receipts.append(
        QJsonObject{
            {"key", key},
            {"reference", "HTML530-544; fixed Synergy0/Barrier1 and literal protocol encodings"},
            {"dropdown_present", allHaveDropdown},
            {"initial_enum", initial},
            {"saved_enum", target},
            {"settings_key_count", baseline.size()},
            {"full_model_settings_wire_transactions", true},
            {"routes", routes},
            {"errors", QJsonArray::fromStringList(m_errors)}
        }
    );
    saveReceipt();
    QVERIFY2(m_errors.isEmpty(), qPrintable(m_errors.join('\n')));
  }
  void summaryCaptures_data()
  {
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("type");
    QTest::addColumn<int>("variant");
    QTest::addColumn<bool>("release");
    // Each actual variant has an independent enum/meaning identity. Captions are
    // observations for accepted P02 manual comparison, never formatter oracles.
    const int requested = qEnvironmentVariableIntValue("SYNERGY_R4_CAPTURE_WIDTH");
    const bool captionState = qEnvironmentVariable("SYNERGY_R4_GROUP") == "caption-state";
    const bool unknownWrap = qEnvironmentVariable("SYNERGY_R4_GROUP") == "unknown-wrap";
    for (const char *language : {"en", "zh_CN"})
      for (int width : {540, 600, 820}) {
        if (requested && requested != width)
          continue;
        if (captionState && width != 600)
          continue;
        for (int type : {3, 4, 2, 0, 1, 5, 6, 7, 8, 9, 10, 77}) {
          if (unknownWrap && type < 8)
            continue;
          if (captionState && type != 4 && type != 6 && type < 8)
            continue;
          const int variants = type == 3 ? 2 : (type == 4 ? 4 : (type == 6 ? 3 : ((type >= 0 && type <= 2) ? 2 : 1)));
          for (int variant = (type == 4 || type == 6) ? -1 : 0; variant < variants; ++variant)
            for (bool release : {false, true}) {
              if (unknownWrap && release)
                continue;
              if (captionState && (release || ((type == 4 || type == 6) && variant != -1)))
                continue;
              QTest::newRow(qPrintable(QString("summary-%1-%2-type%3-variant%4-release%5")
                                           .arg(language)
                                           .arg(width)
                                           .arg(type)
                                           .arg(variant)
                                           .arg(release))
              ) << QString::fromLatin1(language)
                << width << type << variant << release;
            }
        }
      }
  }
  void summaryCaptures()
  {
    QFETCH(QString, language);
    QFETCH(int, width);
    QFETCH(int, type);
    QFETCH(int, variant);
    QFETCH(bool, release);
    m_errors.clear();
    const QString key = QString("summary-%1-%2-type%3-variant%4-release%5")
                            .arg(language)
                            .arg(width)
                            .arg(type)
                            .arg(variant)
                            .arg(release);
    const auto output = outputDirectory(m_root);
    const QString oldSettings = Settings::settingsFile();
    const auto restore = qScopeGuard([&] { Settings::setSettingsFile(oldSettings); });
    Settings::setSettingsFile(m_root + "/" + key + ".conf");
    Settings::setValue(Settings::Core::ComputerName, "Summary-Host");
    Settings::setValue(Settings::Gui::AutoStartCore, false);
    Settings::setValue(Settings::Gui::CheckForUpdates, false);
    Settings::setValue(Settings::Server::ExternalConfig, false);
    Settings::setValue(Settings::Server::ExternalConfigFile, QString());
    Settings::setValue(Settings::Server::Protocol, QStringLiteral("synergy"));
    gui::ServerConfig seed;
    seed.addClient("R4-Studio");
    seed.commit();
    auto &store = Settings::proxy().get();
    store.beginGroup("internalConfig");
    store.beginWriteArray("hotkeys", 1);
    store.setArrayIndex(0);
    KeySequence trigger;
    trigger.appendKey(Qt::Key_Control, Qt::ControlModifier);
    trigger.appendKey(Qt::Key_K, Qt::ControlModifier);
    trigger.saveSettings(store);
    store.beginWriteArray("actions", 1);
    store.setArrayIndex(0);
    trigger.saveSettings(store);
    store.setValue(SettingsKeys::ActionType, type);
    store.setValue(SettingsKeys::SwitchDirection, type == 4 ? variant : 0);
    store.setValue(SettingsKeys::LockToScreen, type == 6 ? variant : 0);
    store.setValue(SettingsKeys::ActiveOnRelease, release);
    store.setValue(SettingsKeys::SwitchToScreen, type == 3 && variant == 1 ? "R4-Missing" : "R4-Studio");
    store.setValue(SettingsKeys::HasScreens, type >= 0 && type <= 2 && variant == 1);
    store.setValue(SettingsKeys::RestartServer, type == 7);
    store.beginWriteArray(SettingsKeys::ScreenNames, 2);
    store.setArrayIndex(0);
    store.setValue(SettingsKeys::ScreenName, "R4-Studio");
    store.setArrayIndex(1);
    store.setValue(SettingsKeys::ScreenName, "R4-Missing");
    store.endArray();
    store.endArray();
    store.endArray();
    store.endGroup();
    store.sync();
    gui::ServerConfig config;
    const gui::ServerConfig original = config;
    QCOMPARE(original.hotkeys().size(), 1);
    QCOMPARE(original.hotkeys().first().actions().size(), 1);
    const auto originalHotkey = original.hotkeys().first();
    const auto originalAction = originalHotkey.actions().first();
    QCOMPARE(originalAction.type(), type);
    QCOMPARE(originalAction.activeOnRelease(), release);
    if (type == 4)
      QCOMPARE(originalAction.switchDirection(), variant);
    if (type == 6)
      QCOMPARE(originalAction.lockCursorMode(), variant);
    if (type >= 0 && type <= 2)
      QCOMPARE(originalAction.haveScreens(), variant == 1);
    const auto savedMap = [&](const Hotkey &hotkey, const QString &suffix) {
      QSettings snap(m_root + "/" + key + suffix + ".ini", QSettings::IniFormat);
      snap.clear();
      hotkey.saveSettings(snap);
      snap.sync();
      QVariantMap result;
      for (const auto &name : snap.allKeys())
        result.insert(name, snap.value(name));
      return result;
    };
    const auto wire = [](const gui::ServerConfig &value) {
      QString text;
      QTextStream stream(&text);
      stream << value;
      stream.flush();
      return qMakePair(text, int(stream.status()));
    };
    const auto beforeMap = savedMap(originalHotkey, "-before");
    const auto beforeWire = wire(original);
    QVERIFY(writeJson(output + "/" + key + "-saved-before.json", QJsonObject::fromVariantMap(beforeMap)));
    I18N::setLanguage(language);
    m_window->resize(width, m_window->height());
    auto *host = m_window->findChild<PageHost *>();
    QVERIFY(host);
    QList<QPointer<QDialog>> openedPages;
    const auto dismiss = qScopeGuard([&] {
      for (auto it = openedPages.rbegin(); it != openedPages.rend(); ++it)
        if (*it) {
          (*it)->reject();
          QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
    });
    auto *server = new ServerConfigDialog(m_window, config);
    openedPages << server;
    host->push(server);
    QVERIFY(settleStaticRender(*m_window));
    auto *tabs = server->findChild<QTabWidget *>();
    QVERIFY(tabs);
    QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier, tabs->tabBar()->tabRect(1).center());
    QCOMPARE(tabs->currentIndex(), 1);
    auto *hotkeys = server->findChild<QListWidget *>("listHotkeys");
    auto *actions = server->findChild<QListWidget *>("listActions");
    QVERIFY(hotkeys && actions);
    QCOMPARE(hotkeys->count(), 1);
    hotkeys->setCurrentRow(0);
    QCOMPARE(actions->count(), 1);
    actions->setCurrentRow(0);
    QVERIFY(settleStaticRender(*m_window));
    const QString serverCaption = actions->item(0)->text();
    const QString hotkeyCaption = hotkeys->item(0)->text();
    const bool serverIcon = !actions->item(0)->icon().isNull();
    QVERIFY(m_window->grab().save(output + "/" + key + "-server.png"));
    auto *editHotkey = server->findChild<QPushButton *>("btnEditHotkey");
    QVERIFY(editHotkey && editHotkey->isEnabled());
    QTest::mouseClick(editHotkey, Qt::LeftButton);
    HotkeyDialog *hotkey = nullptr;
    for (auto *candidate : m_window->findChildren<HotkeyDialog *>())
      if (candidate->isVisibleTo(m_window))
        hotkey = candidate;
    QVERIFY(hotkey);
    openedPages << hotkey;
    QVERIFY(settleStaticRender(*m_window));
    auto *associated = hotkey->findChild<QListWidget *>("listAssociatedActions");
    QVERIFY(associated);
    QCOMPARE(associated->count(), 1);
    associated->setCurrentRow(0);
    const QString associatedCaption = associated->item(0)->text();
    const bool associatedIcon = !associated->item(0)->icon().isNull();
    QVERIFY(m_window->grab().save(output + "/" + key + "-hotkey.png"));
    auto *editAction = hotkey->findChild<QPushButton *>("btnConfigureAction");
    QVERIFY(editAction && editAction->isEnabled());
    QTest::mouseClick(editAction, Qt::LeftButton);
    ActionDialog *action = nullptr;
    for (auto *candidate : m_window->findChildren<ActionDialog *>())
      if (candidate->isVisibleTo(m_window))
        action = candidate;
    QVERIFY(action);
    openedPages << action;
    QVERIFY(settleStaticRender(*m_window));
    auto *types = action->findChild<QComboBox *>("comboActionType");
    auto *box = action->findChild<QDialogButtonBox *>();
    QVERIFY(types && box);
    QCOMPARE(types->currentData().toInt(), type);
    QVERIFY(box->button(QDialogButtonBox::Ok));
    const bool knownInvalid = (type == 4 || type == 6) && variant == -1;
    const auto claimsReadOnly = [](const QString &caption) {
      return caption.contains("read-only", Qt::CaseInsensitive) || caption.contains(QString::fromUtf8("只读"));
    };
    if (knownInvalid) {
      QVERIFY(types->isEnabled());
      QVERIFY(!box->button(QDialogButtonBox::Ok)->isEnabled());
      check(
          !claimsReadOnly(serverCaption) && !claimsReadOnly(associatedCaption),
          key + ": supported repairable action must not claim read-only"
      );
    }
    if (type >= 8) {
      QVERIFY(!types->isEnabled());
      QVERIFY(!box->button(QDialogButtonBox::Ok)->isEnabled());
      check(
          claimsReadOnly(serverCaption) && claimsReadOnly(associatedCaption),
          key + ": unsupported legacy/unknown captions identify read-only"
      );
      const auto explanation = observedText("ActionDialog", "Unsupported action (%1) — read-only").arg(type);
      QLabel *fullExplanation = nullptr;
      for (auto *candidate : action->findChildren<QLabel *>())
        if (candidate->isVisibleTo(action) && candidate->text() == explanation)
          fullExplanation = candidate;
      check(
          fullExplanation && fullExplanation->wordWrap(),
          key + ": unsupported type needs independent visible complete wrapped read-only explanation"
      );
      if (fullExplanation) {
        const QRect available = fullExplanation->contentsRect();
        const auto needed =
            fullExplanation->fontMetrics().boundingRect(available, Qt::TextWordWrap, fullExplanation->text());
        check(
            needed.width() <= available.width() && needed.height() <= available.height(),
            key + ": complete unknown explanation must fit without clipping"
        );
      }
    }
    QVERIFY(m_window->grab().save(output + "/" + key + "-action.png"));
    QJsonArray controls;
    for (auto *control : action->findChildren<QWidget *>())
      if (control->isVisibleTo(action) && (qobject_cast<QComboBox *>(control) || qobject_cast<QCheckBox *>(control) ||
                                           qobject_cast<KeySequenceWidget *>(control)))
        controls.append(
            QJsonObject{
                {"name", control->objectName()},
                {"class", control->metaObject()->className()},
                {"enabled", control->isEnabled()},
                {"rect", rectJson(bounds(control, action))}
            }
        );
    for (auto it = openedPages.rbegin(); it != openedPages.rend(); ++it) {
      auto *buttons = (*it)->findChild<QDialogButtonBox *>();
      QVERIFY(buttons && buttons->button(QDialogButtonBox::Cancel));
      QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel), Qt::LeftButton);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    QCOMPARE(config, original);
    QCOMPARE(std::as_const(config).hotkeys(), original.hotkeys());
    QCOMPARE(savedMap(std::as_const(config).hotkeys().first(), "-after"), beforeMap);
    QCOMPARE(wire(config), beforeWire);
    QCOMPARE(originalAction.text(), std::as_const(config).hotkeys().first().actions().first().text());
    QVERIFY(writeJson(
        output + "/" + key + "-saved-after.json",
        QJsonObject::fromVariantMap(savedMap(std::as_const(config).hotkeys().first(), "-after"))
    ));
    m_receipts.append(
        QJsonObject{
            {"key", key},
            {"observation_only", true},
            {"type", type},
            {"variant", variant},
            {"release", release},
            {"target", originalAction.switchScreenName()},
            {"specified", originalAction.haveScreens()},
            {"recipients", QJsonArray::fromStringList(originalAction.typeScreenNames())},
            {"server_hotkey_caption", hotkeyCaption},
            {"server_action_caption", serverCaption},
            {"associated_action_caption", associatedCaption},
            {"server_action_icon", serverIcon},
            {"associated_action_icon", associatedIcon},
            {"core_action_literal", originalAction.text()},
            {"full_core_text", beforeWire.first},
            {"core_stream_status", beforeWire.second},
            {"settings_key_count", beforeMap.size()},
            {"cancel_model_full_save_wire_unchanged", true},
            {"unknown_read_only", type >= 8},
            {"repairable_known_invalid", knownInvalid},
            {"action_controls", controls},
            {"errors", QJsonArray::fromStringList(m_errors)}
        }
    );
    saveReceipt();
    QVERIFY2(m_errors.isEmpty(), qPrintable(m_errors.join('\n')));
  }
  void openEditorLanguage_data()
  {
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("initialLanguage");
    for (const char *kind : {"fingerprint-local", "fingerprint-compare", "update", "about"})
      for (const char *language : {"en", "zh_CN"})
        QTest::newRow(qPrintable(QString::fromLatin1(kind) + "-" + QString::fromLatin1(language)))
            << QString::fromLatin1(kind) << QString::fromLatin1(language);
  }
  void openEditorLanguage()
  {
    QFETCH(QString, kind);
    QFETCH(QString, initialLanguage);
    m_errors.clear();
    I18N::setLanguage(initialLanguage);
    m_window->resize(540, m_window->height());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    QVERIFY(settleStaticRender(*m_window));
    const auto originalConfig = m_window->serverConfig();
    auto *host = m_window->findChild<PageHost *>();
    QVERIFY(host);
    QPointer<QDialog> editor;
    const auto dismiss = qScopeGuard([&] {
      if (editor) {
        editor->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      }
    });
    if (kind == "fingerprint-local" || kind == "fingerprint-compare")
      editor = new FingerprintDialog(
          m_window, Fingerprint(QCryptographicHash::Sha256, QByteArray(32, '\x5a')),
          kind == "fingerprint-local" ? FingerprintDialogMode::Local : FingerprintDialogMode::Server,
          Fingerprint(QCryptographicHash::Sha256, QByteArray(32, '\x6b'))
      );
    if (kind == "about")
      editor = new AboutDialog(m_window);
    if (kind == "update") {
      gui::update::UpdateInfo info;
      info.version = "99.0.0";
      info.notes = "Owned translation fixture; no update request.";
      info.asset.name = "fixture.dmg";
      info.asset.size = 1048576;
      info.asset.url = QUrl("http://127.0.0.1:1/fixture");
      info.asset.sha256 = QString(64, '0');
      editor = new gui::update::UpdateDialog(info, m_window, {"http", "127.0.0.1", "/"});
    }
    QVERIFY(editor);
    host->push(editor);
    QVERIFY(settleStaticRender(*m_window));
    auto *pageTitle = m_window->findChild<QLabel *>("pageTitle");
    QVERIFY(pageTitle);
    QSignalSpy accepted(editor, &QDialog::accepted);
    QSignalSpy rejected(editor, &QDialog::rejected);
    const QString key = "live-language-" + kind + "-" + initialLanguage;
    bool phaseComplete = false;
    const auto observe = [&](const QString &phase, const QString &language) {
      phaseComplete = false;
      check(
          pageTitle->text() == editor->windowTitle(),
          key + ": visible navigation title equals the real active editor during " + phase
      );
      QJsonObject receipt{
          {"key", key + "-" + phase},
          {"language", language},
          {"width", 540},
          {"active_title", editor->windowTitle()},
          {"visible_title", pageTitle->text()}
      };
      if (kind.startsWith("fingerprint")) {
        auto *box = editor->findChild<QDialogButtonBox *>();
        QVERIFY(box && box->button(QDialogButtonBox::Help));
        const auto help = box->button(QDialogButtonBox::Help)->text();
        check(
            help == observedText("FingerprintDialog", "Show image"),
            key + ": fingerprint mode caption preserves actual Show image during " + phase
        );
        receipt["help_caption"] = help;
        const auto hashes = editor->findChildren<QLabel *>("fingerprintHash");
        QCOMPARE(hashes.size(), kind == "fingerprint-local" ? 1 : 2);
        for (int i = 0; i < hashes.size(); ++i) {
          const QByteArray bytes(32, i == 0 ? '\x5a' : '\x6b');
          const auto expected = QString::fromLatin1(bytes.left(16).toHex(':').toUpper()) + '\n' +
                                QString::fromLatin1(bytes.mid(16).toHex(':').toUpper());
          QCOMPARE(hashes[i]->text(), expected);
        }
        if (kind == "fingerprint-compare") {
          auto *no = box->button(QDialogButtonBox::No);
          auto *yes = box->button(QDialogButtonBox::Yes);
          QVERIFY(no && yes);
          check(
              no->text() == observedText("FingerprintDialog", "Reject"),
              key + ": explicit Reject caption survives " + phase
          );
          check(
              yes->text() == observedText("FingerprintDialog", "Trust after verification"),
              key + ": explicit Trust after verification caption survives " + phase
          );
          QVERIFY(no->isDefault());
          QVERIFY(!yes->autoDefault());
          receipt["reject_caption"] = no->text();
          receipt["trust_caption"] = yes->text();
        }
      }
      QCOMPARE(accepted.count(), 0);
      QCOMPARE(rejected.count(), 0);
      QCOMPARE(m_window->serverConfig(), originalConfig);
      check(
          m_window->grab().save(outputDirectory(m_root) + "/" + key + "-" + phase + ".png"),
          key + ": rendered phase capture"
      );
      receipt["errors"] = QJsonArray::fromStringList(m_errors);
      m_receipts.append(receipt);
      saveReceipt();
      phaseComplete = true;
    };
    observe("stable-construction", initialLanguage);
    QVERIFY(phaseComplete);
    const QString other = initialLanguage == "en" ? QStringLiteral("zh_CN") : QStringLiteral("en");
    for (const auto &language : {other, initialLanguage}) {
      I18N::setLanguage(language);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
      QVERIFY(settleStaticRender(*m_window));
      observe("changed-" + language, language);
      QVERIFY(phaseComplete);
    }
    editor->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(accepted.count(), 0);
    QCOMPARE(m_window->serverConfig(), originalConfig);
    QVERIFY2(m_errors.isEmpty(), qPrintable(m_errors.join('\n')));
  }
  void recipientInteraction_data()
  {
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("width");
    for (const char *language : {"en", "zh_CN"})
      for (int width : {540, 600, 820})
        QTest::newRow(qPrintable(QString::fromLatin1(language) + "-" + QString::number(width)))
            << QString::fromLatin1(language) << width;
  }
  void recipientInteraction()
  {
    QFETCH(QString, language);
    QFETCH(int, width);
    m_errors.clear();
    I18N::setLanguage(language);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    QVERIFY(settleStaticRender(*m_window));
    m_window->resize(width, m_window->height());
    const QString key = "recipient-input-" + language + "-" + QString::number(width);
    auto &config = m_window->serverConfig();
    const auto configBefore = config;
    const auto restoreConfig = qScopeGuard([&] { config = configBefore; });
    config.addClient("Recipient-Studio");
    QSettings fixture(m_root + "/" + key + "-fixture.ini", QSettings::IniFormat);
    fixture.clear();
    KeySequence keys;
    keys.appendKey(Qt::Key_Control, Qt::ControlModifier);
    keys.appendKey(Qt::Key_K, Qt::ControlModifier);
    keys.saveSettings(fixture);
    fixture.setValue(SettingsKeys::ActionType, 2);
    fixture.setValue(SettingsKeys::HasScreens, true);
    fixture.beginWriteArray(SettingsKeys::ScreenNames, 1);
    fixture.setArrayIndex(0);
    fixture.setValue(SettingsKeys::ScreenName, "Recipient-Studio");
    fixture.endArray();
    Action action;
    action.loadSettings(fixture);
    const Action original = action;
    Hotkey hotkey;
    const auto map = [&](const Action &value, const QString &suffix) {
      QSettings store(m_root + "/" + key + suffix + ".ini", QSettings::IniFormat);
      store.clear();
      value.saveSettings(store);
      store.sync();
      QVariantMap result;
      for (const auto &n : store.allKeys())
        result.insert(n, store.value(n));
      return result;
    };
    const auto originalMap = map(action, "-before");
    const auto originalCore = action.text();
    auto *host = m_window->findChild<PageHost *>();
    QVERIFY(host);
    QPointer<ActionDialog> editor;
    const auto dismiss = qScopeGuard([&] {
      if (editor) {
        editor->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      }
    });
    const auto open = [&] {
      editor = new ActionDialog(m_window, config, hotkey, action);
      host->push(editor);
      return settleStaticRender(*m_window);
    };
    QVERIFY(open());
    auto *scope = editor->findChild<QComboBox *>("comboRecipients");
    QVERIFY(scope);
    QVERIFY(chooseData(scope, 0));
    QVERIFY(chooseData(scope, 1));
    auto *list = editor->findChild<QListWidget *>("listScreens");
    QVERIFY(list && list->count() >= 2);
    list->setCurrentRow(0);
    list->scrollToItem(list->item(0));
    QVERIFY(settleStaticRender(*m_window));
    auto *item = list->item(0);
    const QRect row = list->visualItemRect(item);
    // Independent reference box: list item left padding10,14px affordance,
    // vertical center from the actual visual row. Never ask a future delegate
    // for the rectangle that defines its own expected position.
    const QRect expectedBox(row.left() + 10, row.center().y() - 7, 14, 14);
    const QPoint click = expectedBox.center();
    const auto stateBefore = item->checkState();
    QJsonArray states;
    const auto capture = [&](const QString &phase) {
      const auto image = list->viewport()->grab().toImage();
      const QColor backing = pixel(image, QPoint(row.right() - 4, row.center().y()));
      int visible = 0;
      for (int y = expectedBox.top(); y <= expectedBox.bottom(); ++y)
        for (int x = expectedBox.left(); x <= expectedBox.right(); ++x) {
          const auto color = pixel(image, QPoint(x, y));
          if (!nearColor(color, backing, 12))
            ++visible;
        }
      // Contrast in an overlapping corner does not prove a centered box.
      // Scan only the icon-free left32px band, excluding the outermost row
      // perimeter (native rounded selection/background may differ there).
      QRect painted;
      int foreground = 0;
      for (int y = row.top() + 1; y < row.bottom(); ++y)
        for (int x = row.left() + 1; x < row.left() + 32; ++x) {
          const auto color = pixel(image, QPoint(x, y));
          if (!nearColor(color, backing, 12)) {
            const QRect dot(x, y, 1, 1);
            painted = painted.isNull() ? dot : painted.united(dot);
            ++foreground;
          }
        }
      const bool complete = foreground >= 12 && painted.width() >= 10 && painted.height() >= 10;
      check(complete, key + ": checked and unchecked checkbox needs a complete visible paint box during " + phase);
      check(
          complete && qAbs(painted.center().y() - row.center().y()) <= 2,
          key + ": actual checkbox paint box must be centered on visual row during " + phase
      );
      check(
          complete && qAbs(painted.center().x() - expectedBox.center().x()) <= 2,
          key + ": actual checkbox paint box must respect list left padding during " + phase
      );
      check(
          m_window->grab().save(outputDirectory(m_root) + "/" + key + "-" + phase + ".png"), key + ": capture " + phase
      );
      states.append(
          QJsonObject{
              {"phase", phase},
              {"state", int(item->checkState())},
              {"selected", item->isSelected()},
              {"row_rect", rectJson(row)},
              {"expected_indicator_rect", rectJson(expectedBox)},
              {"click_center", QJsonArray{click.x(), click.y()}},
              {"paint_contrast_pixels", visible},
              {"paint_bbox", rectJson(painted)},
              {"paint_foreground_pixels", foreground}
          }
      );
    };
    list->clearSelection();
    capture("before-unselected");
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, click);
    QCoreApplication::processEvents();
    const auto flipped = stateBefore == Qt::Checked ? Qt::Unchecked : Qt::Checked;
    check(item->checkState() == flipped, key + ": real centered indicator mouse click toggles exactly once");
    capture("mouse");
    const auto beforeSpace = item->checkState();
    list->setFocus();
    QTest::keyClick(list, Qt::Key_Space);
    QCOMPARE(item->checkState(), beforeSpace == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    capture("space");
    QTest::keyClick(list, Qt::Key_Space);
    QCOMPARE(item->checkState(), beforeSpace);
    // Reach a saved checked state through real keyboard input even if the
    // baseline mouse hit area is misplaced; the mouse failure stays recorded.
    if (item->checkState() != Qt::Checked)
      QTest::keyClick(list, Qt::Key_Space);
    QCOMPARE(item->checkState(), Qt::Checked);
    QStringList selected;
    for (int i = 0; i < list->count(); ++i)
      if (list->item(i)->checkState() == Qt::Checked)
        selected << list->item(i)->text();
    auto *box = editor->findChild<QDialogButtonBox *>();
    QVERIFY(box && box->button(QDialogButtonBox::Ok)->isEnabled());
    QTest::mouseClick(box->button(QDialogButtonBox::Ok), Qt::LeftButton);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(action.haveScreens());
    QCOMPARE(action.type(), 2);
    QCOMPARE(std::as_const(action).keySequence(), original.keySequence());
    QCOMPARE(action.activeOnRelease(), original.activeOnRelease());
    QCOMPARE(std::as_const(action).typeScreenNames(), selected);
    QCOMPARE(action.text(), QString("keystroke(Control+k,%1)").arg(selected.join(':')));
    const auto savedMap = map(action, "-saved");
    Action saved = action;
    QSettings persisted(m_root + "/" + key + "-saved.ini", QSettings::IniFormat);
    Action reloaded;
    reloaded.loadSettings(persisted);
    QCOMPARE(reloaded, saved);
    QCOMPARE(map(reloaded, "-reloaded"), savedMap);
    QCOMPARE(reloaded.text(), saved.text());
    config.addClient("Recipient-Future");
    QVERIFY(open());
    list = editor->findChild<QListWidget *>("listScreens");
    QVERIFY(list);
    bool future = false;
    for (int i = 0; i < list->count(); ++i) {
      auto *entry = list->item(i);
      if (entry->text() == "Recipient-Future") {
        future = true;
        QCOMPARE(entry->checkState(), Qt::Unchecked);
      } else
        QCOMPARE(entry->checkState() == Qt::Checked, selected.contains(entry->text()));
    }
    QVERIFY(future);
    QCOMPARE(editor->findChild<QComboBox *>("comboRecipients")->currentData().toInt(), 1);
    list->setCurrentRow(0);
    list->setFocus();
    QTest::keyClick(list, Qt::Key_Space);
    editor->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(action, saved);
    QCOMPARE(map(action, "-cancel"), savedMap);
    QCOMPARE(action.text(), saved.text());
    m_receipts.append(
        QJsonObject{
            {"key", key},
            {"states", states},
            {"selected_names", QJsonArray::fromStringList(selected)},
            {"core_before", originalCore},
            {"core_saved", saved.text()},
            {"original_map_keys", originalMap.size()},
            {"saved_map_keys", savedMap.size()},
            {"save_reopen_future_cancel", true},
            {"errors", QJsonArray::fromStringList(m_errors)}
        }
    );
    saveReceipt();
    QVERIFY2(m_errors.isEmpty(), qPrintable(m_errors.join('\n')));
  }
  void cleanupTestCase()
  {
    saveReceipt();
    QDesktopServices::unsetUrlHandler("https");
  }
};

void observeR4VisualMain(MainWindow *window, const QString &root)
{
  if (!window || runtime::runtimeRoot() != root || !blockRuntimeCommands(window)) {
    qApp->exit(40);
    return;
  }
  const auto directory = outputDirectory(root);
  if (!QDir().mkpath(directory)) {
    qApp->exit(41);
    return;
  }
  auto metadata = appearance(window, root);
  metadata["manual_hold"] = !qEnvironmentVariableIsSet("SYNERGY_R4_AUTOMATED");
  metadata["commands_blocked"] = "Start/Restart/Updates/Clear settings and hostname Return; no core child";
  if (!writeJson(directory + "/launch.json", metadata) ||
      !writeJson(QDir::tempPath() + "/tupig-r4-preview-latest.json", metadata)) {
    qApp->exit(42);
    return;
  }
  if (!qEnvironmentVariableIsSet("SYNERGY_R4_AUTOMATED")) {
    if (qEnvironmentVariable("SYNERGY_R4_DRAG_TRACE") == "1") {
      auto *trace = new R4DragTrace(window, directory);
      if (!trace->isReady()) {
        qApp->exit(43);
        return;
      }
    }
    return; // visible until native Quit; no test appearance override
  }
  R4VisualChecks checks(window, root);
  const QByteArray result = QByteArray(R4_VISUAL_RESULT_PATH) + ",txt";
  char name[] = "R4VisualContractTests";
  char option[] = "-o";
  char stdoutFormat[] = "-,txt";
  QByteArray resultArg = result;
  char referenceOnly[] = "referencePages";
  char shortcutsOnly[] = "readableShortcutsAndRecording";
  char protocolOnly[] = "protocolControls";
  char capturesOnly[] = "summaryCaptures";
  char languageOnly[] = "openEditorLanguage";
  char recipientsOnly[] = "recipientInteraction";
  char *arguments[]{name,          option,        stdoutFormat, option,       resultArg.data(),
                    referenceOnly, shortcutsOnly, protocolOnly, languageOnly, recipientsOnly};
  const auto group = qEnvironmentVariable("SYNERGY_R4_GROUP");
  if (group == "protocol")
    arguments[5] = protocolOnly;
  if (group == "live-language")
    arguments[5] = languageOnly;
  if (group == "recipient-input")
    arguments[5] = recipientsOnly;
  if (group == "summary-captures" || group == "caption-state" || group == "unknown-wrap")
    arguments[5] = capturesOnly;
  const int count = group == "presentation"
                        ? 7
                        : ((group == "group2" || group == "common" || group == "remaining" || group == "protocol" ||
                            group == "summary-captures" || group == "caption-state" || group == "unknown-wrap" ||
                            group == "live-language" || group == "computer-actions" || group == "sharing-fields" ||
                            group == "recipient-input")
                               ? 6
                               : 10);
  const int code = QTest::qExec(&checks, count, arguments);
  qApp->exit(code);
}
} // namespace synergy::testing
#include "R4VisualContractTests.moc"
