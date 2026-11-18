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
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QHostAddress>
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
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
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
  void mainShellUsesCompactCards_data();
  void mainShellUsesCompactCards();
  void embeddedPages_data();
  void embeddedPages();
  void windowSelectorsKeepTransactions();
  void shortPreferenceFieldsStayCompact();
  void fingerprintsShowFullHashAndRejectByDefault();
  void aboutUsesBuildVersionAndCompactTitle();
  void collapsedLogShowsOneLatestMessage();
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
    for (int width : {540, 600, 820})
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
  capture(
      window, QStringLiteral("main-responsive-%1-%2-%3").arg(language).arg(width).arg(server ? "server" : "client")
  );
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
