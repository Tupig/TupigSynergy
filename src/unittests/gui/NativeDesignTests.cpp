/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "UiRenderTestSupport.h"
#include "common/Constants.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"
#include "gui/Action.h"
#include "gui/AppStyle.h"
#include "gui/Hotkey.h"
#include "gui/MainWindowDesign.h"
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
#include "gui/update/UpdateDownloader.h"
#include "gui/widgets/LogPanel.h"
#include "gui/widgets/LogWidget.h"
#include "gui/widgets/PageHost.h"
#include <QFileInfo>
#include <QUuid>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDate>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QHostAddress>
#include <QImage>
#include <QLabel>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QMainWindow>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QStyleOptionSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTranslator>
#include <QVBoxLayout>
#include <QtTest>

namespace {

void diagnoseGeometryFailure(const QWidget *target, const char *check)
{
  for (int depth = 0; target && depth < 4; target = target->parentWidget(), ++depth) {
    const auto *layout = target->layout();
    const auto policy = target->sizePolicy();
    qInfo().nospace() << "WF1 geometry check=" << check << " depth=" << depth
                      << " type=" << target->metaObject()->className() << " object=" << target->objectName()
                      << " geometry=" << target->geometry() << " minimum=" << target->minimumSize()
                      << " minimumHint=" << target->minimumSizeHint() << " hint=" << target->sizeHint()
                      << " policy=" << static_cast<int>(policy.horizontalPolicy()) << ','
                      << static_cast<int>(policy.verticalPolicy())
                      << " layoutMinimum=" << (layout ? layout->minimumSize() : QSize(-1, -1));
  }
}

void applyTestPalette(bool dark)
{
  auto palette = QApplication::style()->standardPalette();
  const QColor text(dark ? "#e9e9ea" : "#1c1c1e");
  palette.setColor(QPalette::Window, QColor(dark ? "#1c1c1e" : "#f5f5f7"));
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::Base, QColor(dark ? "#1b1c20" : "#ffffff"));
  palette.setColor(QPalette::Button, QColor(dark ? "#2a2a2e" : "#ececf0"));
  qApp->setPalette(palette);
  qApp->setStyleSheet(synergy::gui::appStyleSheet());
}

void verifyTextAction(const QPushButton *button)
{
  const auto detail = QStringLiteral("%1 \"%2\": height %3, font %4px, width %5, hinted width %6")
                          .arg(button->objectName(), button->text())
                          .arg(button->height())
                          .arg(button->font().pixelSize())
                          .arg(button->width())
                          .arg(button->sizeHint().width());
  QVERIFY2(button->height() == 32, qPrintable(detail));
  QVERIFY2(button->font().pixelSize() == 13, qPrintable(detail));
  QVERIFY2(button->width() >= button->minimumSizeHint().width(), qPrintable(detail));
}

QStackedWidget *prepareHost(PageHost &host)
{
  host.setAttribute(Qt::WA_DontShowOnScreen);
  host.resize(600, 520);
  auto *layout = new QVBoxLayout(&host);
  layout->setContentsMargins(0, 0, 0, 0);
  auto *stack = new QStackedWidget(&host);
  stack->setObjectName(QStringLiteral("pageStack"));
  stack->addWidget(new QWidget);
  layout->addWidget(stack);
  host.show();
  return stack;
}

void capture(QWidget &widget, const QString &name)
{
  QVERIFY2(settleStaticRender(widget), "Static capture did not settle within 1000ms");
  const auto directory = qEnvironmentVariable(
      "SYNERGY_UI_EVIDENCE_DIR", QDir(QDir::tempPath()).filePath(QStringLiteral("tupig-native-design"))
  );
  QVERIFY(QDir().mkpath(directory));
  QPixmap image(widget.size());
  image.fill(Qt::transparent);
  widget.render(&image);
  QVERIFY2(image.save(QDir(directory).filePath(name + QStringLiteral(".png"))), qPrintable(name));
}

QString normalizedHash(QString value)
{
  value.remove(QRegularExpression(QStringLiteral("[^0-9a-fA-F]")));
  return value.toLower();
}

synergy::gui::update::UpdateInfo updateInfo()
{
  synergy::gui::update::UpdateInfo info;
  info.version = QStringLiteral("1.22.0");
  info.tag = QStringLiteral("v1.22.0");
  info.notes =
      QStringLiteral("A real release note with a long line that should wrap within the window.\nSecond change.");
  info.releaseUrl = QUrl(QStringLiteral("https://github.com/Tupig/TupigSynergy/releases/tag/v1.22.0"));
  info.asset.name = QStringLiteral("synergy_1.22.0_mac_arm64.dmg");
  return info;
}

} // namespace

class NativeDesignTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void mainShellResponsiveWidths_data();
  void mainShellResponsiveWidths();
  void clientAddressAndPortFit_data();
  void clientAddressAndPortFit();
  void mainShellUsesCompactCards_data();
  void mainShellUsesCompactCards();
  void embeddedPages_data();
  void embeddedPages();
  void windowSelectorsKeepTransactions();
  void shortPreferenceFieldsStayCompact();
  void fingerprintsShowFullHashAndRejectByDefault();
  void aboutUsesBuildVersionAndCompactTitle();
  void aboutNoticesRemainReadableAndComplete_data();
  void aboutNoticesRemainReadableAndComplete();
  void collapsedLogShowsOneLatestMessage();
  void collapsedLogBadgeHasRoundedCorners();
  void coreLogUsesChineseCatalogue_data();
  void coreLogUsesChineseCatalogue();
  void logFollowsTailWithoutInterruptingHistory();
  void leavingUpdateCancelsPendingDownload();
};

void NativeDesignTests::initTestCase()
{
  const auto fontDirectory = qEnvironmentVariable("QT_QPA_FONTDIR");
  const auto families = QFontDatabase::families();
  qInfo().nospace() << "WF1 environment qt=" << qVersion() << " platform=" << QGuiApplication::platformName()
                    << " style=" << QApplication::style()->objectName()
                    << " fontdirSet=" << qEnvironmentVariableIsSet("QT_QPA_FONTDIR")
                    << " fontdirExists=" << (!fontDirectory.isEmpty() && QDir(fontDirectory).exists())
                    << " defaultFontdirExists="
                    << QDir(QLibraryInfo::path(QLibraryInfo::LibrariesPath) + QStringLiteral("/fonts")).exists()
                    << " familyCount=" << families.size();
  qInfo().nospace() << "WF1 families SegoeUI=" << families.contains(QStringLiteral("Segoe UI"), Qt::CaseInsensitive)
                    << " Arial=" << families.contains(QStringLiteral("Arial"), Qt::CaseInsensitive)
                    << " MicrosoftYaHei=" << families.contains(QStringLiteral("Microsoft YaHei"), Qt::CaseInsensitive)
                    << " NotoSansCJKSC=" << families.contains(QStringLiteral("Noto Sans CJK SC"), Qt::CaseInsensitive);
  const auto requested = QApplication::font();
  const QFontInfo resolved(requested);
  qInfo().nospace() << "WF1 font requestedFamily=" << requested.family() << " requestedPixels=" << requested.pixelSize()
                    << " requestedPoints=" << requested.pointSizeF() << " resolvedFamily=" << resolved.family()
                    << " resolvedPixels=" << resolved.pixelSize() << " resolvedPoints=" << resolved.pointSizeF()
                    << " exactMatch=" << resolved.exactMatch();
  const QFontMetrics metrics(requested);
  const auto settingsText = QString::fromUtf8("\xE8\xAE\xBE\xE7\xBD\xAE");
  for (const auto &text :
       {QStringLiteral("iiiiiiii"), QStringLiteral("WWWWWWWW"), QStringLiteral("Settings"), settingsText}) {
    auto output = qInfo().nospace();
    output << "WF1 metrics text=" << text << " advance=" << metrics.horizontalAdvance(text)
           << " bounds=" << metrics.boundingRect(text) << " height=" << metrics.height()
           << " ascent=" << metrics.ascent() << " descent=" << metrics.descent();
    if (text == settingsText)
      output << " firstGlyph=" << metrics.inFontUcs4(0x8BBE) << " secondGlyph=" << metrics.inFontUcs4(0x7F6E);
  }
}

// COMPLETE-R1 additive matrix: original R1 600px oracle remains unchanged.
void NativeDesignTests::mainShellResponsiveWidths_data()
{
  QTest::addColumn<QString>("language");
  QTest::addColumn<int>("width");
  QTest::addColumn<bool>("server");
  for (const auto &language : {QStringLiteral("en"), QStringLiteral("zh_CN")})
    for (int width : {500, 540, 600, 820})
      for (bool server : {false, true})
        QTest::newRow(qPrintable(QStringLiteral("%1-%2-%3").arg(language).arg(width).arg(server ? "server" : "client")))
            << language << width << server;
}

void NativeDesignTests::mainShellResponsiveWidths()
{
  QFETCH(QString, language);
  QFETCH(int, width);
  QFETCH(bool, server);
  applyTestPalette(true);
  QTranslator translator;
  if (language == QStringLiteral("zh_CN")) {
    QVERIFY(translator.load(QStringLiteral("synergy_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
    qApp->installTranslator(&translator);
  }
  QMainWindow window;
  window.setAttribute(Qt::WA_DontShowOnScreen);
  Ui::MainWindow ui;
  ui.setupUi(&window);
  ui.lineEditName->hide();
  ui.lblComputerName->setText(QStringLiteral("Render-Fixture"));
  ui.lblIpAddresses->setText(QStringLiteral("192.0.2.10"));
  ui.rbModeServer->setChecked(server);
  ui.rbModeClient->setChecked(!server);
  ui.clientOptions->setVisible(!server);
  ui.serverOptions->setVisible(server);
  ui.btnConfigureClient->setVisible(!server);
  ui.btnConfigureServer->setVisible(server);
  ui.lblNoMode->hide();
  ui.lineHostname->setText(QStringLiteral("192.0.2.20"));
  ui.logPanel->setCollapsed(true);
  window.resize(width, 480);
  ui.updateShortcutPresentation(width);
  window.show();
  QCoreApplication::processEvents();
  QCOMPARE(window.width(), width);
  QCOMPARE(ui.pageBar->height(), 36);
  QCOMPARE(ui.statusBar->height(), 32);
  for (auto *field :
       {static_cast<QWidget *>(ui.rbModeServer), static_cast<QWidget *>(ui.rbModeClient),
        static_cast<QWidget *>(ui.btnToggleCore), static_cast<QWidget *>(ui.btnRestartCore),
        static_cast<QWidget *>(server ? ui.btnConfigureServer : ui.btnConfigureClient)}) {
    QVERIFY(field->isVisibleTo(&window));
    if (!window.rect().contains(QRect(field->mapTo(&window, QPoint()), field->size())))
      diagnoseGeometryFailure(field, "responsive-containment");
    QVERIFY2(
        window.rect().contains(QRect(field->mapTo(&window, QPoint()), field->size())), qPrintable(field->objectName())
    );
  }
  if (ui.workspace->horizontalScrollBar()->maximum() != 0)
    diagnoseGeometryFailure(ui.workspace, "responsive-horizontal-scroll");
  QCOMPARE(ui.workspace->horizontalScrollBar()->maximum(), 0);
  for (const auto *button :
       {ui.btnEditName, ui.btnSaveName, ui.btnCancelName, ui.btnToggleCore, ui.btnRestartCore,
        server ? ui.btnConfigureServer : ui.btnConfigureClient}) {
    verifyTextAction(button);
  }
  for (const auto *button : {ui.btnSettingsShortcut, ui.btnUpdatesShortcut, ui.btnAboutShortcut}) {
    QVERIFY(ui.pageBar->rect().contains(QRect(button->mapTo(ui.pageBar, QPoint()), button->size())));
    if (width < 640) {
      QVERIFY(button->text().isEmpty());
      QVERIFY(!button->accessibleName().isEmpty());
      QCOMPARE(button->height(), 24);
    } else {
      QVERIFY(!button->text().isEmpty());
      verifyTextAction(button);
    }
  }
  capture(
      window, QStringLiteral("main-responsive-%1-%2-%3").arg(language).arg(width).arg(server ? "server" : "client")
  );
  qApp->removeTranslator(&translator);
}

void NativeDesignTests::clientAddressAndPortFit_data()
{
  QTest::addColumn<QString>("language");
  QTest::addColumn<bool>("dark");
  QTest::addColumn<int>("width");
  for (const auto &language : {QStringLiteral("en"), QStringLiteral("zh_CN")})
    for (const bool dark : {false, true})
      for (const int width : {500, 820})
        QTest::newRow(qPrintable(QStringLiteral("%1-%2-%3").arg(language, dark ? "dark" : "light").arg(width)))
            << language << dark << width;
}

void NativeDesignTests::clientAddressAndPortFit()
{
  QFETCH(QString, language);
  QFETCH(bool, dark);
  QFETCH(int, width);
  applyTestPalette(dark);
  QTranslator translator;
  if (language == QStringLiteral("zh_CN")) {
    QVERIFY(translator.load(QStringLiteral("synergy_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
    qApp->installTranslator(&translator);
  }

  QMainWindow window;
  window.setAttribute(Qt::WA_DontShowOnScreen);
  Ui::MainWindow ui;
  ui.setupUi(&window);
  ui.lblComputerName->setText(QStringLiteral("Render-Fixture"));
  ui.lblIpAddresses->setText(QStringLiteral("192.0.2.10"));
  ui.lineEditName->hide();
  ui.btnSaveName->hide();
  ui.btnCancelName->hide();
  ui.rbModeClient->setChecked(true);
  ui.clientOptions->show();
  ui.serverOptions->hide();
  ui.btnConfigureClient->show();
  ui.lblNoMode->hide();
  ui.logPanel->setCollapsed(true);
  window.resize(width, 390);
  window.show();
  QVERIFY(settleStaticRender(window));

  QCOMPARE(window.width(), width);
  QCOMPARE(ui.workspace->horizontalScrollBar()->maximum(), 0);
  QVERIFY(ui.clientOptions->rect().contains(ui.lineHostname->geometry()));
  QVERIFY(ui.clientOptions->rect().contains(ui.spinPort->geometry()));
  QVERIFY(!ui.lineHostname->geometry().intersects(ui.spinPort->geometry()));

  const auto textContents = [](QLineEdit *editor) {
    QStyleOptionFrame frame;
    frame.initFrom(editor);
    frame.lineWidth =
        editor->hasFrame() ? editor->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &frame, editor) : 0;
    return editor->style()
        ->subElementRect(QStyle::SE_LineEditContents, &frame, editor)
        .marginsRemoved(editor->textMargins());
  };
  const int addressWidth = ui.lineHostname->width();
  const bool compactAddress = addressWidth <= 240;
  const auto addressContents = textContents(ui.lineHostname);
  QVERIFY(
      addressContents.width() >= ui.lineHostname->fontMetrics().horizontalAdvance(QStringLiteral("255.255.255.255"))
  );

  auto *portEdit = ui.spinPort->findChild<QLineEdit *>();
  QVERIFY(portEdit != nullptr);
  const auto portContents = textContents(portEdit);
  const QRect spinText(portEdit->mapTo(ui.spinPort, portContents.topLeft()), portContents.size());
  QStyleOptionSpinBox spin;
  spin.initFrom(ui.spinPort);
  spin.frame = ui.spinPort->hasFrame();
  spin.buttonSymbols = ui.spinPort->buttonSymbols();
  spin.subControls = QStyle::SC_All;
  for (const auto arrow : {QStyle::SC_SpinBoxUp, QStyle::SC_SpinBoxDown}) {
    const auto arrowBounds = ui.spinPort->style()->subControlRect(QStyle::CC_SpinBox, &spin, arrow, ui.spinPort);
    QVERIFY(!arrowBounds.isEmpty());
    QVERIFY(!spinText.intersects(arrowBounds));
  }
  QStringList clippedPorts;
  for (const int port : {22211, 65535}) {
    ui.spinPort->setValue(port);
    QCOMPARE(portEdit->text(), QString::number(port));
    const int required = portEdit->fontMetrics().horizontalAdvance(portEdit->displayText()) + 4;
    if (required > portContents.width())
      clippedPorts << QStringLiteral("%1 needs %2 px but has %3 px").arg(port).arg(required).arg(portContents.width());
  }

  const auto addressList = QStringLiteral(
      "server.test, 198.51.100.123, 2001:db8:1234:5678:90ab:cdef:0:1234, "
      "fallback-west-gateway.example.test, fallback-east-gateway.example.test, backup.test"
  );
  QVERIFY(ui.lineHostname->fontMetrics().horizontalAdvance(addressList) > addressContents.width());
  QTest::keyClicks(ui.lineHostname, addressList);
  QCOMPARE(ui.lineHostname->text(), addressList);
  ui.lineHostname->home(false);
  QTest::keyClicks(ui.lineHostname, QStringLiteral("primary-"));
  ui.lineHostname->end(false);
  QTest::keyClicks(ui.lineHostname, QStringLiteral(".example"));
  QCOMPARE(ui.lineHostname->text(), QStringLiteral("primary-") + addressList + QStringLiteral(".example"));

  ui.lineHostname->setText(QStringLiteral("198.51.100.123"));
  ui.spinPort->setValue(22211);
  if (width == 500)
    capture(window, QStringLiteral("client-address-port-%1-%2").arg(language, dark ? "dark" : "light"));
  if (!compactAddress)
    clippedPorts << QStringLiteral("Address field occupies %1 px, expected at most 240 px").arg(addressWidth);
  QVERIFY2(clippedPorts.isEmpty(), qPrintable(clippedPorts.join(QStringLiteral("; "))));
  qApp->removeTranslator(&translator);
}

void NativeDesignTests::mainShellUsesCompactCards_data()
{
  QTest::addColumn<QString>("language");
  QTest::addColumn<bool>("dark");
  QTest::addColumn<bool>("server");
  QTest::addColumn<bool>("collapsed");
  for (const auto &language : {QStringLiteral("en"), QStringLiteral("zh_CN")}) {
    for (const auto dark : {false, true}) {
      for (const auto server : {false, true}) {
        for (const auto collapsed : {false, true}) {
          const auto name = QStringLiteral("%1-%2-%3-%4")
                                .arg(
                                    language, dark ? "dark" : "light", server ? "server" : "client",
                                    collapsed ? "collapsed" : "expanded"
                                );
          QTest::newRow(qPrintable(name)) << language << dark << server << collapsed;
        }
      }
    }
  }
}

void NativeDesignTests::mainShellUsesCompactCards()
{
  QFETCH(QString, language);
  QFETCH(bool, dark);
  QFETCH(bool, server);
  QFETCH(bool, collapsed);
  applyTestPalette(dark);
  QTranslator translator;
  QTranslator qtTranslator;
  if (language == QStringLiteral("zh_CN")) {
    QVERIFY(translator.load(QStringLiteral("synergy_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
    QVERIFY(qtTranslator.load(QStringLiteral("qt_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
    qApp->installTranslator(&translator);
    qApp->installTranslator(&qtTranslator);
  }
  QMainWindow window;
  window.setAttribute(Qt::WA_DontShowOnScreen);
  Ui::MainWindow ui;
  ui.setupUi(&window);
  ui.lineEditName->hide();
  ui.lblComputerName->setText(QStringLiteral("Test computer"));
  ui.lblIpAddresses->setText(QStringLiteral("192.0.2.10"));
  ui.rbModeServer->setChecked(server);
  ui.rbModeClient->setChecked(!server);
  ui.clientOptions->setVisible(!server);
  ui.serverOptions->setVisible(server);
  ui.btnConfigureClient->setVisible(!server);
  ui.lineHostname->setText(QStringLiteral("192.0.2.20"));
  ui.lblNoMode->hide();
  // Synthetic render fixtures only; no core or network sharing process is started.
  for (int index = 0; index < 6; ++index) {
    ui.logPanel->appendLine(
        QStringLiteral("[00:00:0%1] INFO: %2")
            .arg(index)
            .arg(
                language == QStringLiteral("zh_CN")
                    ? QStringLiteral("测试日志：完整消息自动换行，保留全部诊断内容；仅用于离线界面验证。")
                    : QStringLiteral(
                          "Test log: complete diagnostics wrap within the message column; render fixture only."
                      )
            )
    );
  }
  ui.logPanel->setCollapsed(collapsed);
  window.resize(600, window.height());
  window.show();
  QCoreApplication::processEvents();
  QCOMPARE(window.width(), 600);
  QCOMPARE(ui.pageBar->height(), 36);
  QCOMPARE(ui.statusBar->height(), 32);
  for (const auto *card : {ui.rbModeServer, ui.rbModeClient}) {
    QVERIFY(card->height() >= 48);
    QVERIFY2(card->height() <= 56, "Mode cards must use the compact reference proportions.");
    if (!window.rect().contains(QRect(card->mapTo(&window, QPoint()), card->size())))
      diagnoseGeometryFailure(card, "compact-card-containment");
    QVERIFY(window.rect().contains(QRect(card->mapTo(&window, QPoint()), card->size())));
  }
  for (const auto *button :
       {ui.btnSettingsShortcut, ui.btnUpdatesShortcut, ui.btnAboutShortcut,
        server ? ui.btnConfigureServer : ui.btnConfigureClient, ui.btnToggleCore, ui.btnRestartCore}) {
    if (!window.rect().contains(QRect(button->mapTo(&window, QPoint()), button->size())))
      diagnoseGeometryFailure(button, "compact-button-containment");
    QVERIFY(window.rect().contains(QRect(button->mapTo(&window, QPoint()), button->size())));
  }
  QCOMPARE(ui.logPanel->isCollapsed(), collapsed);
  auto *records = ui.logPanel->findChild<LogWidget *>();
  QVERIFY(records != nullptr);
  QCOMPARE(records->isVisibleTo(&window), !collapsed);
  if (!collapsed) {
    auto *table = records->findChild<QTableWidget *>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 6);
    QVERIFY(table->rowHeight(0) > table->fontMetrics().height());
  } else {
    auto *tail = ui.logPanel->findChild<QLabel *>(QStringLiteral("logTail"));
    QVERIFY(tail != nullptr);
    QVERIFY(tail->text().contains(QStringLiteral("00:00:05")));
  }
  capture(
      window,
      QStringLiteral("main-%1-%2-%3-%4")
          .arg(language, dark ? "dark" : "light", server ? "server" : "client", collapsed ? "collapsed" : "expanded")
  );
  if (!collapsed) {
    auto *footer = ui.logPanel->findChild<QLabel *>(QStringLiteral("logHint"));
    QVERIFY(footer != nullptr);
    ui.workspace->ensureWidgetVisible(footer, 0, 0);
    QCoreApplication::processEvents();
    QElapsedTimer footerReadiness;
    footerReadiness.start();
    while (footerReadiness.elapsed() < 1000 && !ui.workspace->viewport()->rect().contains(QRect(
                                                   footer->mapTo(ui.workspace->viewport(), QPoint()), footer->size()
                                               ))) {
      ui.workspace->ensureWidgetVisible(footer, 0, 0);
      QCoreApplication::processEvents();
      QTest::qWait(10);
    }
    if (language == QStringLiteral("zh_CN")) {
      auto *table = records->findChild<QTableWidget *>();
      QVERIFY(table != nullptr);
      const auto footerRect = QRect(footer->mapTo(ui.workspace->viewport(), QPoint()), footer->size());
      const auto tableFont = QFontInfo(table->font());
      const auto footerFont = QFontInfo(footer->font());
      const auto tableMetrics = QFontMetrics(table->font());
      const auto footerMetrics = QFontMetrics(footer->font());
      const auto hashText = [](const QString &value) {
        return QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex();
      };
      qInfo().nospace() << "CJK-WF1 languageZh=" << true << " dark=" << dark << " server=" << server
                        << " collapsed=" << collapsed << " window=" << window.size()
                        << " workspace=" << ui.workspace->size() << " viewport=" << ui.workspace->viewport()->size()
                        << " contents=" << ui.workspace->widget()->size() << " logPanel=" << ui.logPanel->size()
                        << " records=" << records->size() << " table=" << table->size() << " footer=" << footer->size()
                        << " footerRect=" << footerRect
                        << " footerVisible=" << ui.workspace->viewport()->rect().contains(footerRect)
                        << " scrollValue=" << ui.workspace->verticalScrollBar()->value()
                        << " scrollMaximum=" << ui.workspace->verticalScrollBar()->maximum()
                        << " scrollPageStep=" << ui.workspace->verticalScrollBar()->pageStep()
                        << " messageColumn=" << table->columnWidth(2) << " rows=" << table->rowCount()
                        << " tableFont=" << tableFont.family() << " tableFontExact=" << tableFont.exactMatch()
                        << " tableFontPixels=" << table->font().pixelSize()
                        << " tableFontHeight=" << tableMetrics.height()
                        << " tableCjkAdvance=" << tableMetrics.horizontalAdvance(QString::fromUtf8("设置"))
                        << " tableLatinAdvance=" << tableMetrics.horizontalAdvance(QStringLiteral("Settings"))
                        << " footerFont=" << footerFont.family() << " footerFontExact=" << footerFont.exactMatch()
                        << " footerFontPixels=" << footer->font().pixelSize()
                        << " footerFontHeight=" << footerMetrics.height()
                        << " footerTextLength=" << footer->text().size()
                        << " footerTextSha256=" << hashText(footer->text());
    }
    QVERIFY(ui.workspace->viewport()->rect().contains(
        QRect(footer->mapTo(ui.workspace->viewport(), QPoint()), footer->size())
    ));
    // The 470px stress window stays scrollable; this taller render-only preview
    // displays the full panel without changing production resize policy.
    window.resize(600, 600);
    QCoreApplication::processEvents();
    capture(
        window, QStringLiteral("main-%1-%2-%3-expanded-fit")
                    .arg(language, dark ? "dark" : "light", server ? "server" : "client")
    );
    // A second render fixture matches the reference's five short rows plus one
    // wrapped row. The all-long stress assertions and captures above remain.
    records->clear();
    for (int index = 0; index < 5; ++index) {
      ui.logPanel->appendLine(QStringLiteral("[00:00:0%1] INFO: %2")
                                  .arg(index)
                                  .arg(
                                      language == QStringLiteral("zh_CN")
                                          ? QStringLiteral("离线界面验证记录。")
                                          : QStringLiteral("Offline interface verification record.")
                                  ));
    }
    ui.logPanel->appendLine(
        QStringLiteral("[00:00:05] INFO: %1")
            .arg(
                language == QStringLiteral("zh_CN")
                    ? QStringLiteral("较长的测试记录：自动换行保留全部内容，使用合成数据验证六条记录与统计页脚的显示。")
                    : QStringLiteral(
                          "Long render fixture: complete diagnostics wrap within the message column, preserving the "
                          "six-record view and its footer."
                      )
            )
    );
    QCoreApplication::processEvents();
    auto *table = records->findChild<QTableWidget *>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 6);
    // clear/reappend posts nested layout requests. Check that the frame has
    // actually grown before capturing; a table can have valid cell geometry
    // while an ancestor still has its previous empty-state height.
    QTRY_VERIFY_WITH_TIMEOUT(ui.logPanel->height() >= ui.logPanel->minimumSizeHint().height(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(ui.logPanel->height() >= records->height() + 52, 3000);
    QVERIFY(table->viewport()->rect().contains(table->visualItemRect(table->item(5, 2))));
    capture(
        window,
        QStringLiteral("main-%1-%2-%3-reference").arg(language, dark ? "dark" : "light", server ? "server" : "client")
    );
  }
  qApp->removeTranslator(&qtTranslator);
  qApp->removeTranslator(&translator);
}

void NativeDesignTests::embeddedPages_data()
{
  QTest::addColumn<QString>("kind");
  QTest::addColumn<QString>("language");
  QTest::addColumn<bool>("dark");
  const QStringList pages{QStringLiteral("server"), QStringLiteral("client"),      QStringLiteral("preferences"),
                          QStringLiteral("screen"), QStringLiteral("hotkey"),      QStringLiteral("action"),
                          QStringLiteral("about"),  QStringLiteral("fingerprint"), QStringLiteral("update")};
  for (const auto &language : {QStringLiteral("en"), QStringLiteral("zh_CN")}) {
    for (const auto &kind : pages) {
      for (const auto dark : {false, true})
        QTest::newRow(qPrintable(kind + QLatin1Char('-') + language + (dark ? "-dark" : "-light")))
            << kind << language << dark;
    }
  }
}

void NativeDesignTests::embeddedPages()
{
  QFETCH(QString, kind);
  QFETCH(QString, language);
  QFETCH(bool, dark);
  applyTestPalette(dark);
  QTranslator translator;
  QTranslator qtTranslator;
  if (language == QStringLiteral("zh_CN")) {
    QVERIFY(translator.load(QStringLiteral("synergy_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
    QVERIFY(qtTranslator.load(QStringLiteral("qt_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
    qApp->installTranslator(&translator);
    qApp->installTranslator(&qtTranslator);
  }
  synergy::gui::ServerConfig config;
  synergy::gui::Screen screen(QStringLiteral("test-screen"));
  synergy::gui::ScreenList screens;
  screens.append(screen);
  Hotkey hotkey;
  Action action;
  PageHost host;
  prepareHost(host);
  QDialog *editor = nullptr;
  if (kind == QStringLiteral("server"))
    editor = new ServerConfigDialog(nullptr, config);
  else if (kind == QStringLiteral("client"))
    editor = new ClientConfigDialog(nullptr);
  else if (kind == QStringLiteral("preferences"))
    editor = new SettingsDialog(nullptr, config);
  else if (kind == QStringLiteral("screen"))
    editor = new ScreenSettingsDialog(nullptr, &screen, &screens);
  else if (kind == QStringLiteral("hotkey"))
    editor = new HotkeyDialog(nullptr, hotkey);
  else if (kind == QStringLiteral("action"))
    editor = new ActionDialog(nullptr, config, hotkey, action);
  else if (kind == QStringLiteral("about"))
    editor = new AboutDialog(nullptr);
  else if (kind == QStringLiteral("fingerprint"))
    editor = new FingerprintDialog(nullptr, Fingerprint(QCryptographicHash::Sha256, QByteArray(32, '\x5a')));
  else if (kind == QStringLiteral("update"))
    editor = new synergy::gui::update::UpdateDialog(updateInfo(), nullptr);
  QVERIFY(editor != nullptr);
  host.push(editor);
  QCoreApplication::processEvents();
  QVERIFY(!editor->isWindow());
  QVERIFY(editor->layout() != nullptr);
  QCOMPARE(editor->layout()->contentsMargins(), QMargins());
  auto *tabs = editor->findChild<QTabWidget *>();
  if (kind == QStringLiteral("screen")) {
    QVERIFY2(tabs != nullptr, "Computer settings must expose four compact tabs.");
    QCOMPARE(tabs->count(), 4);
  }
  const auto count = tabs ? tabs->count() : 1;
  for (int index = 0; index < count; ++index) {
    if (tabs)
      tabs->setCurrentIndex(index);
    QCoreApplication::processEvents();
    if (host.width() != 600)
      diagnoseGeometryFailure(&host, "embedded-host-width");
    QCOMPARE(host.width(), 600);
    if (editor->width() > 580)
      diagnoseGeometryFailure(editor, "embedded-editor-width");
    QVERIFY2(editor->width() <= 580, qPrintable(kind + QStringLiteral(" exceeds the workspace inset.")));
    for (auto *field : editor->findChildren<QWidget *>()) {
      if (!field->isVisibleTo(editor) ||
          (!qobject_cast<QLineEdit *>(field) && !qobject_cast<QComboBox *>(field) &&
           !qobject_cast<QAbstractSpinBox *>(field) && !qobject_cast<QPushButton *>(field)))
        continue;
      const QRect bounds(field->mapTo(editor, QPoint()), field->size());
      const auto detail = QStringLiteral("%1 tab %2 clips %3 at x=%4 width=%5 (page %6)")
                              .arg(kind)
                              .arg(index)
                              .arg(field->objectName())
                              .arg(bounds.x())
                              .arg(bounds.width())
                              .arg(editor->width());
      if (!(bounds.left() >= 0 && bounds.right() < editor->width()))
        diagnoseGeometryFailure(field, "embedded-field-containment");
      QVERIFY2(bounds.left() >= 0 && bounds.right() < editor->width(), qPrintable(detail));
      if (auto *button = qobject_cast<QPushButton *>(field)) {
        if (button->property("keyCapture").toBool()) {
          QCOMPARE(button->height(), 30);
        } else if (!button->text().isEmpty() && !button->property("iconButton").toBool() &&
                   !button->property("iconButton32").toBool()) {
          verifyTextAction(button);
        }
      }
    }
    for (auto *area : editor->findChildren<QScrollArea *>()) {
      if (area->isVisibleTo(editor) && area->horizontalScrollBar()->maximum() != 0)
        diagnoseGeometryFailure(area, "embedded-horizontal-scroll");
      if (area->isVisibleTo(editor))
        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
    }
    capture(host, QStringLiteral("%1-%2-%3-%4").arg(kind, language, dark ? "dark" : "light").arg(index));
  }
  editor->reject();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  qApp->removeTranslator(&qtTranslator);
  qApp->removeTranslator(&translator);
}

void NativeDesignTests::windowSelectorsKeepTransactions()
{
  Settings::setValue(Settings::Gui::Autohide, false);
  Settings::setValue(Settings::Gui::CloseToTray, false);
  synergy::gui::ServerConfig config;
  {
    SettingsDialog dialog(nullptr, config);
    auto *start = dialog.findChild<QComboBox *>(QStringLiteral("comboStartBehavior"));
    auto *close = dialog.findChild<QComboBox *>(QStringLiteral("comboCloseBehavior"));
    QVERIFY(start != nullptr);
    QVERIFY(close != nullptr);
    QCOMPARE(start->count(), 2);
    QCOMPARE(close->count(), 2);
    // R3 semantic lineage: Close now displays Hide before Quit.
    QCOMPARE(start->currentData().toBool(), false);
    QCOMPARE(close->currentData().toBool(), false);
    start->setCurrentIndex(start->findData(true));
    close->setCurrentIndex(close->findData(true));
    QVERIFY(dialog.findChild<QRadioButton *>(QStringLiteral("rbAutoHide"))->isChecked());
    QVERIFY(dialog.findChild<QRadioButton *>(QStringLiteral("rbCloseToTray"))->isChecked());
    dialog.reject();
    QVERIFY(!Settings::value(Settings::Gui::Autohide).toBool());
    QVERIFY(!Settings::value(Settings::Gui::CloseToTray).toBool());
  }
  {
    SettingsDialog dialog(nullptr, config);
    auto *start = dialog.findChild<QComboBox *>(QStringLiteral("comboStartBehavior"));
    auto *close = dialog.findChild<QComboBox *>(QStringLiteral("comboCloseBehavior"));
    start->setCurrentIndex(start->findData(true));
    close->setCurrentIndex(close->findData(true));
    auto *save = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
    QVERIFY(save->isEnabled());
    save->click();
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QVERIFY(Settings::value(Settings::Gui::Autohide).toBool());
    QVERIFY(Settings::value(Settings::Gui::CloseToTray).toBool());
  }
}

void NativeDesignTests::shortPreferenceFieldsStayCompact()
{
  synergy::gui::ServerConfig config;
  PageHost host;
  prepareHost(host);
  auto *dialog = new SettingsDialog(nullptr, config);
  host.push(dialog);
  auto *tabs = dialog->findChild<QTabWidget *>();
  QVERIFY(tabs != nullptr);
  auto *languages = dialog->findChild<QComboBox *>(QStringLiteral("comboLanguage"));
  QVERIFY(languages != nullptr);
  QVERIFY2(languages->count() > 0, "The fixture must stage real application catalogs for I18N.");
  const QStringList names{QStringLiteral("comboLanguage"),      QStringLiteral("comboTrayIcon"),
                          QStringLiteral("comboStartBehavior"), QStringLiteral("comboCloseBehavior"),
                          QStringLiteral("comboLogLevel"),      QStringLiteral("sbPort")};
  for (int index = 0; index < tabs->count(); ++index) {
    tabs->setCurrentIndex(index);
    QCoreApplication::processEvents();
    for (const auto &name : names) {
      auto *field = dialog->findChild<QWidget *>(name);
      QVERIFY2(field != nullptr, qPrintable(name));
      if (field->isVisibleTo(dialog))
        QVERIFY2(field->width() <= 180, qPrintable(name + QStringLiteral(" exceeds compact form width")));
    }
  }
  dialog->reject();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void NativeDesignTests::fingerprintsShowFullHashAndRejectByDefault()
{
  const Fingerprint local(QCryptographicHash::Sha256, QByteArray(32, '\x5a'));
  const Fingerprint remote(QCryptographicHash::Sha256, QByteArray(32, '\x7e'));
  FingerprintDialog dialog(nullptr, local, FingerprintDialogMode::Server, remote);
  auto *buttons = dialog.findChild<QDialogButtonBox *>();
  QVERIFY(buttons != nullptr);
  QVERIFY(buttons->button(QDialogButtonBox::No) != nullptr);
  QVERIFY(buttons->button(QDialogButtonBox::No)->isDefault());
  QVERIFY(!buttons->button(QDialogButtonBox::Yes)->isDefault());
  bool foundLocal = false;
  bool foundRemote = false;
  for (auto *label : dialog.findChildren<QLabel *>()) {
    if (!label->isVisibleTo(&dialog))
      continue;
    const auto value = normalizedHash(label->text());
    foundLocal = foundLocal || value == QString::fromLatin1(local.data.toHex());
    foundRemote = foundRemote || value == QString::fromLatin1(remote.data.toHex());
  }
  QVERIFY2(foundLocal, "The full local SHA-256 must be visible without toggling image mode.");
  QVERIFY2(foundRemote, "The full peer SHA-256 must be visible before any trust decision.");
  buttons->button(QDialogButtonBox::No)->click();
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
}

void NativeDesignTests::aboutUsesBuildVersionAndCompactTitle()
{
  AboutDialog dialog(nullptr);
  auto *version = dialog.findChild<QLabel *>(QStringLiteral("lblVersion"));
  auto *title = dialog.findChild<QLabel *>(QStringLiteral("lblName"));
  QVERIFY(version != nullptr);
  QVERIFY(title != nullptr);
  // V07-R2: the existing overlay appends a genuine compile-date suffix.
  // Compare the entire build-derived base, and validate any allowed metadata.
  const auto base = QString::fromLatin1(kDisplayVersion);
  QCOMPARE(version->text().section(QLatin1Char(','), 0, 0), base);
  if (version->text() != base) {
    const QRegularExpression expression(
        QRegularExpression::escape(base) + QStringLiteral(", built ([0-9]{4}-[0-9]{2}-[0-9]{2})$")
    );
    const auto match = expression.match(version->text());
    QVERIFY(match.hasMatch());
    QVERIFY(QDate::fromString(match.captured(1), Qt::ISODate).isValid());
  }
  QVERIFY(title->minimumHeight() <= 32);
  title->ensurePolished();
  QCOMPARE(title->font().pixelSize(), 22);
  QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("btnCopyVersion")) != nullptr);
  auto *license = dialog.findChild<QLabel *>(QStringLiteral("linkGpl"));
  QVERIFY(license != nullptr);
  QVERIFY(!license->text().trimmed().isEmpty());
}

void NativeDesignTests::aboutNoticesRemainReadableAndComplete_data()
{
  QTest::addColumn<bool>("chinese");
  QTest::newRow("en-light") << false;
  QTest::newRow("zh-dark") << true;
}

void NativeDesignTests::aboutNoticesRemainReadableAndComplete()
{
  QFETCH(bool, chinese);
  QTranslator translator;
  if (chinese) {
    QVERIFY(translator.load(QStringLiteral("synergy_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
    qApp->installTranslator(&translator);
  }
  applyTestPalette(chinese);
  PageHost host;
  auto *pages = prepareHost(host);
  host.resize(500, 380);
  auto *about = new AboutDialog(&host);
  host.push(about);
  auto *open = about->findChild<QPushButton *>(QStringLiteral("aboutNoticesButton"));
  QVERIFY(open);
  QTest::mouseClick(open, Qt::LeftButton);
  QTRY_COMPARE(pages->currentIndex(), 2);
  auto *browser = host.findChild<QTextBrowser *>(QStringLiteral("aboutNoticesText"));
  QVERIFY(browser);
  QCOMPARE(browser->openExternalLinks(), true);

  QStringList headings;
  QSet<QString> links;
  bool hasRegularBodyWithLeading = false;
  for (auto block = browser->document()->firstBlock(); block.isValid(); block = block.next()) {
    const auto format = block.blockFormat();
    if (format.headingLevel() == 2)
      headings.append(block.text());
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      const auto span = it.fragment().charFormat();
      if (span.isAnchor())
        links.insert(span.anchorHref());
      else if (!it.fragment().text().trimmed().isEmpty() && format.headingLevel() == 0 &&
               span.fontWeight() == QFont::Normal && format.lineHeight() >= 130) {
        hasRegularBodyWithLeading = true;
      }
    }
  }
  const auto expectedHeadings =
      chinese
          ? QStringList{QStringLiteral("项目来源"), QStringLiteral("版权"), QStringLiteral("主要开发者"), QStringLiteral("项目许可"), QStringLiteral("第三方许可声明")}
          : QStringList{
                QStringLiteral("Project attribution"), QStringLiteral("Copyright"),
                QStringLiteral("Important Developers"), QStringLiteral("Project license"),
                QStringLiteral("Third-party notices")
            };
  QCOMPARE(headings, expectedHeadings);
  QVERIFY(hasRegularBodyWithLeading);
  const auto body = browser->toPlainText().simplified();
  const auto copyright = QString::fromUtf8(kCopyright).split(QLatin1Char('\n'));
  for (const auto &line : copyright)
    QVERIFY2(body.contains(line), qPrintable(line));
  QFile license(QStringLiteral(":/licenses/Lucide-Feather.txt"));
  QVERIFY(license.open(QIODevice::ReadOnly));
  QVERIFY(body.contains(QString::fromUtf8(license.readAll()).simplified()));
  QVERIFY(links.contains(QStringLiteral("https://www.gnu.org/licenses/old-licenses/gpl-2.0.html")));
  QVERIFY(links.contains(QStringLiteral("https://github.com/Tupig/TupigSynergy")));
  QVERIFY(links.contains(QStringLiteral("https://github.com/Tupig/TupigSynergy/graphs/contributors")));
#ifdef SYNERGY_EXTRA_HEADER
  QVERIFY(links.contains(QString::fromUtf8(kProjectContact)));
  QVERIFY(links.contains(QStringLiteral("https://github.com/Tupig/Tupig_synergy#readme")));
#endif

  auto *close = host.findChild<QPushButton *>(QStringLiteral("aboutNoticesClose"));
  QVERIFY(close);
  QCoreApplication::processEvents();
  host.resize(500, host.preferredContentHeight(500));
  QTRY_VERIFY(browser->verticalScrollBar()->maximum() > 0);
  QCOMPARE(browser->horizontalScrollBar()->maximum(), 0);
  verifyTextAction(close);
  QVERIFY(close->width() <= close->sizeHint().width() + 2);
  const int bottom = close->mapTo(&host, QPoint(0, close->height())).y();
  QVERIFY2(host.height() - bottom >= 0 && host.height() - bottom <= 12, "Trailing space must be only the page inset");
  capture(host, chinese ? QStringLiteral("about-notices-500-zh-dark") : QStringLiteral("about-notices-500-en-light"));
  QTest::mouseClick(close, Qt::LeftButton);
  QTRY_COMPARE(pages->currentIndex(), 1);
}

void NativeDesignTests::collapsedLogShowsOneLatestMessage()
{
  LogPanel panel;
  panel.resize(580, 300);
  panel.appendLine(QStringLiteral("[00:00:01] INFO: first"));
  panel.appendLine(QStringLiteral("[00:00:02] ERROR: last complete message"));
  auto *records = panel.findChild<LogWidget *>();
  QVERIFY(records != nullptr);
  panel.setCollapsed(true);
  QVERIFY(panel.isCollapsed());
  QVERIFY(!records->isVisibleTo(&panel));
  auto *tail = panel.findChild<QLabel *>(QStringLiteral("logTail"));
  QVERIFY(tail != nullptr);
  QVERIFY(tail->text().contains(QStringLiteral("last complete message")));
  panel.setCollapsed(false);
  QVERIFY(!panel.isCollapsed());
  QVERIFY(records->isVisibleTo(&panel));
  QVERIFY(records->allText().contains(QStringLiteral("first")));
  QVERIFY(records->allText().contains(QStringLiteral("last complete message")));
}

void NativeDesignTests::collapsedLogBadgeHasRoundedCorners()
{
  applyTestPalette(true);
  LogPanel panel;
  panel.setAttribute(Qt::WA_DontShowOnScreen);
  panel.resize(500, panel.sizeHint().height());
  panel.setCollapsed(true);
  panel.show();
  QVERIFY(settleStaticRender(panel));

  auto *badge = panel.findChild<QLabel *>(QStringLiteral("logBadge"));
  QVERIFY(badge && badge->isVisibleTo(&panel));
  const QRect bounds(badge->mapTo(&panel, QPoint()) * 2, badge->size() * 2);
  QImage image(panel.size() * 2, QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(2);
  image.fill(Qt::transparent);
  panel.render(&image);
  QVERIFY(image.rect().contains(bounds.adjusted(-2, 0, 0, 0)));
  const auto background = image.pixelColor(bounds.left() - 2, bounds.top());
  for (const auto &corner : {bounds.topLeft(), bounds.topRight(), bounds.bottomLeft(), bounds.bottomRight()}) {
    QCOMPARE(image.pixelColor(corner), background);
  }
  QVERIFY(image.pixelColor(bounds.center().x(), bounds.top()) != background);
}

void NativeDesignTests::coreLogUsesChineseCatalogue_data()
{
  QTest::addColumn<QString>("message");
  QTest::addColumn<QString>("translated");

  QTest::newRow("settings-path") << QStringLiteral("initial settings file: C:\\示例目录\\settings %1.ini")
                                 << QStringLiteral("初始设置文件：C:\\示例目录\\settings %1.ini");
  QTest::newRow("core-start") << QStringLiteral("starting core") << QStringLiteral("正在启动核心进程");
  QTest::newRow("tls-listener") << QStringLiteral("QtTransportListenSocket: listening on 192.0.2.42:22211 (tls=1)")
                                << QStringLiteral("QtTransportListenSocket：正在监听 192.0.2.42:22211（tls=1）");
  QTest::newRow("clipboard") << QStringLiteral("screen \"fixture%1\" grabbed clipboard 0 from \"示例%2\"")
                             << QStringLiteral("屏幕“fixture%1”取得剪贴板 0，来源为“示例%2”");
  QTest::newRow("unrecognized") << QStringLiteral("fixture: unknown system detail %1")
                                << QStringLiteral("fixture: unknown system detail %1");
}

void NativeDesignTests::coreLogUsesChineseCatalogue()
{
  QFETCH(QString, message);
  QFETCH(QString, translated);
  LogPanel panel;
  auto *records = panel.findChild<LogWidget *>();
  auto *table = panel.findChild<QTableWidget *>(QStringLiteral("logRecords"));
  auto *tail = panel.findChild<QLabel *>(QStringLiteral("logTail"));
  QVERIFY(records && table && tail);

  const auto history = QStringLiteral("[2026-01-02T03:04:05.678] INFO: ") + message;
  const auto latest = QStringLiteral("[2026-01-02T03:04:06.789] INFO: ") + message;
  panel.appendLine(history);
  QCOMPARE(table->rowCount(), 1);
  QCOMPARE(table->item(0, 2)->text(), message);

  QTranslator translator;
  QVERIFY(translator.load(QStringLiteral("synergy_zh_CN.qm"), QStringLiteral(UI_TEST_TRANSLATIONS_DIR)));
  QVERIFY(QCoreApplication::installTranslator(&translator));
  QTRY_COMPARE(table->item(0, 2)->text(), translated);
  panel.appendLine(latest);
  QCOMPARE(table->rowCount(), 2);
  QCOMPARE(table->item(1, 0)->text(), QStringLiteral("2026-01-02 03:04:06"));
  QCOMPARE(table->item(1, 2)->text(), translated);
  panel.setCollapsed(true);
  QVERIFY(tail->text().endsWith(translated));
  QCOMPARE(records->allText(), history + QLatin1Char('\n') + latest);

  QVERIFY(QCoreApplication::removeTranslator(&translator));
  QTRY_COMPARE(table->item(0, 2)->text(), message);
  QCOMPARE(table->item(1, 2)->text(), message);
  QVERIFY(tail->text().endsWith(message));
}

void NativeDesignTests::logFollowsTailWithoutInterruptingHistory()
{
  LogWidget log;
  log.setAttribute(Qt::WA_DontShowOnScreen);
  log.resize(580, 236);
  log.show();
  for (int index = 0; index < 12; ++index)
    log.appendLine(QStringLiteral("[00:00:00] INFO: record-%1").arg(index));
  auto *table = log.findChild<QTableWidget *>();
  QVERIFY(table != nullptr);
  auto *scroll = table->verticalScrollBar();
  QTRY_VERIFY(scroll->maximum() > 0);
  QTRY_COMPARE(scroll->value(), scroll->maximum());

  // Programmatic Qt scrollbar changes never inject input into the host.
  scroll->setValue(0);
  log.appendLine(QStringLiteral("[00:00:01] INFO: record-12"));
  QCoreApplication::processEvents();
  QCOMPARE(scroll->value(), 0);

  scroll->setValue(scroll->maximum());
  log.appendLine(QStringLiteral("[00:00:02] INFO: record-13"));
  QTRY_COMPARE(scroll->value(), scroll->maximum());
  QVERIFY(log.allText().contains(QStringLiteral("record-0")));
  QVERIFY(log.allText().contains(QStringLiteral("record-13")));
}

void NativeDesignTests::leavingUpdateCancelsPendingDownload()
{
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  auto info = updateInfo();
  info.asset.url = QUrl(QStringLiteral("http://127.0.0.1:%1/update.dmg").arg(server.serverPort()));
  info.asset.size = 1024;
  info.asset.sha256 = QString(64, QLatin1Char('a'));
  const synergy::gui::update::DownloadPolicy policy{
      QStringLiteral("http"), QStringLiteral("127.0.0.1"), QStringLiteral("/")
  };
  synergy::gui::update::UpdateDialog dialog(info, nullptr, policy);
  QPointer<synergy::gui::update::UpdateDownloader> downloader =
      dialog.findChild<synergy::gui::update::UpdateDownloader *>();
  QVERIFY(downloader != nullptr);
  QPushButton *download = nullptr;
  for (auto *button : dialog.findChildren<QPushButton *>()) {
    if (button->property("primary").toBool())
      download = button;
  }
  QVERIFY(download != nullptr);
  download->click();
  QVERIFY(!download->isEnabled());
  QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 5000);
  auto *socket = server.nextPendingConnection();
  QVERIFY(socket != nullptr);
  QSignalSpy disconnected(socket, &QTcpSocket::disconnected);
  dialog.reject();
  QVERIFY(downloader.isNull());
  QTRY_COMPARE_WITH_TIMEOUT(disconnected.count(), 1, 5000);
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
}

int main(int argc, char **argv)
{
  QTemporaryDir directory;
  if (!directory.isValid())
    return 1;
  // COMPLETE-R1 isolation lineage: preserve all inherited assertions and fixtures.
  QString isolationError;
  if (!synergy::runtime::configureIsolatedRuntime(
          {QFileInfo(directory.path()).canonicalFilePath(),
           "ts-complete-" + QUuid::createUuid().toString(QUuid::WithoutBraces)},
          &isolationError
      ))
    return 2;
  qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
  qputenv("XDG_STATE_HOME", directory.path().toUtf8());
  QApplication app(argc, argv);
  auto font = app.font();
  font.setPixelSize(13);
  app.setFont(font);
  Settings::setSettingsFile(directory.filePath(QStringLiteral("design.conf")));
  Settings::setStateFile(directory.filePath(QStringLiteral("design.state")));
  Settings::setValue(Settings::Core::Language, QStringLiteral("en"));
  Settings::setValue(Settings::Core::ComputerName, QStringLiteral("test-computer"));
  app.setStyleSheet(synergy::gui::appStyleSheet());
  NativeDesignTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "NativeDesignTests.moc"
