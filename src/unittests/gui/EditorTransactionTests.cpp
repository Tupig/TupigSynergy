/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/I18N.h"
#include "common/NetworkProtocol.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/config/Screen.h"
#include "gui/config/ScreenList.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/widgets/LogWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTranslator>
#include <QUuid>
#include <QtTest>

#include <cstdio>

class EditorTransactionTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void clientCancelDoesNotWrite();
  void clientSaveWrites();
  void screenAliasesReplaceRatherThanAppend();
  void logsFilterCountAndBoundHistory();
  void localizingEngineLogPreservesRawHistory();
  void largeLogHistoryWrapsAfterResizeFontAndFilter();
  void serverCancelRestoresProtocol();
  void protocolValuesSupportStoredAndDefaultTypes();
  void traySelectorCancelDoesNotWrite();
  void traySelectorSaveWrites();
  void cancelPreservesSystemLanguageSetting();
  void saveOtherSettingPreservesSystemLanguage();
};

void EditorTransactionTests::clientCancelDoesNotWrite()
{
  Settings::setValue(Settings::Client::InvertXScroll, false);
  ClientConfigDialog editor(nullptr);
  editor.findChild<QCheckBox *>(QStringLiteral("cbXScrollInvert"))->setChecked(true);
  editor.reject();
  QVERIFY(!Settings::value(Settings::Client::InvertXScroll).toBool());
}

void EditorTransactionTests::clientSaveWrites()
{
  Settings::setValue(Settings::Client::InvertXScroll, false);
  ClientConfigDialog editor(nullptr);
  editor.findChild<QCheckBox *>(QStringLiteral("cbXScrollInvert"))->setChecked(true);
  auto *buttons = editor.findChild<QDialogButtonBox *>();
  QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
  buttons->button(QDialogButtonBox::Save)->click();
  QVERIFY(Settings::value(Settings::Client::InvertXScroll).toBool());
  QCOMPARE(editor.result(), static_cast<int>(QDialog::Accepted));
}

void EditorTransactionTests::screenAliasesReplaceRatherThanAppend()
{
  synergy::gui::Screen screen(QStringLiteral("test-screen"));
  ScreenSettingsDialog initial(nullptr, &screen, nullptr);
  initial.findChild<QListWidget *>(QStringLiteral("listAliases"))->addItem(QStringLiteral("old-alias"));
  initial.accept();
  synergy::gui::ScreenList screens;
  screens.append(screen);
  ScreenSettingsDialog editor(nullptr, &screen, &screens);
  auto *aliases = editor.findChild<QListWidget *>(QStringLiteral("listAliases"));
  aliases->clear();
  aliases->addItem(QStringLiteral("new-alias"));
  editor.accept();
  QCOMPARE(editor.result(), static_cast<int>(QDialog::Accepted));
  QCOMPARE(std::as_const(screen).aliases(), QStringList{QStringLiteral("new-alias")});
}

void EditorTransactionTests::protocolValuesSupportStoredAndDefaultTypes()
{
  QCOMPARE(networkProtocolFromValue(QVariant::fromValue(NetworkProtocol::Barrier)), NetworkProtocol::Barrier);
  QCOMPARE(networkProtocolFromValue(QStringLiteral("barrier")), NetworkProtocol::Barrier);
  QCOMPARE(networkProtocolFromValue(QStringLiteral("synergy")), NetworkProtocol::Synergy);
}

void EditorTransactionTests::serverCancelRestoresProtocol()
{
  Settings::setValue(Settings::Server::Protocol, networkProtocolToOption(NetworkProtocol::Synergy));
  synergy::gui::ServerConfig config;
  ServerConfigDialog editor(nullptr, config);
  editor.findChild<QRadioButton *>(QStringLiteral("rbProtocolBarrier"))->setChecked(true);
  QCOMPARE(networkProtocolFromValue(Settings::value(Settings::Server::Protocol)), NetworkProtocol::Barrier);
  editor.reject();
  QCOMPARE(networkProtocolFromValue(Settings::value(Settings::Server::Protocol)), NetworkProtocol::Synergy);
}

void EditorTransactionTests::logsFilterCountAndBoundHistory()
{
  QElapsedTimer elapsed;
  elapsed.start();
  const auto phase = [&](const char *name, int appended = -1) {
    std::fprintf(
        stderr, "EL1 phase=%s elapsed_ms=%lld appended=%d\n", name, static_cast<long long>(elapsed.elapsed()), appended
    );
    std::fflush(stderr);
  };
  phase("enter");
  LogWidget log;
  log.appendLine(QStringLiteral("[2026-11-18T18:17:28.518] INFO: first"));
  log.appendLine(QStringLiteral("[time] WARNING: second"));
  log.appendLine(QStringLiteral("[time] ERROR: third"));
  auto *table = log.findChild<QTableWidget *>();
  QVERIFY(table != nullptr);
  QCOMPARE(table->item(0, 0)->text(), QStringLiteral("2026-11-18 18:17:28"));
  QVERIFY(log.allText().startsWith(QStringLiteral("[2026-11-18T18:17:28.518] INFO: first\n")));
  QCOMPARE(table->item(0, 0)->font(), table->item(0, 1)->font());
  QCOMPARE(table->item(0, 1)->font(), table->item(0, 2)->font());
  const std::array<int, 3> counts{1, 1, 1};
  QCOMPARE(log.levelCounts(), counts);
  QSignalSpy history(&log, &LogWidget::historyChanged);
  log.setLevelFilter(QStringLiteral("warn"));
  QCOMPARE(history.last().first().toString(), QStringLiteral("[time] WARNING: second"));
  QVERIFY(log.allText().contains(QStringLiteral("second")));
  QVERIFY(!log.allText().contains(QStringLiteral("first")));
  log.setLevelFilter(QString());
  QCOMPARE(table->item(0, 0)->text(), QStringLiteral("2026-11-18 18:17:28"));
  log.clear();
  for (int i = 0; i < 10003; ++i) {
    if (i == 0)
      phase("append-0", 0);
    log.appendLine(QStringLiteral("[time] INFO: record-%1").arg(i));
    if ((i + 1) % 1000 == 0)
      phase("append-progress", i + 1);
  }
  phase("append-loop-end", 10003);
  QCOMPARE(log.levelCounts()[0], 10000);
  QVERIFY(!log.allText().contains(QStringLiteral("record-0\n")));
  QVERIFY(log.allText().contains(QStringLiteral("record-10002")));
  log.clear();
  QCOMPARE(log.levelCounts()[0], 0);
  QVERIFY(log.allText().isEmpty());
  phase("long-append-before");
  log.appendLine(QStringLiteral("[time] ERROR: ") + QString(20000, QLatin1Char('x')));
  phase("long-append-after");
  QVERIFY(log.maximumHeight() <= 236);
  phase("resize-before");
  log.resize(320, 236);
  phase("resize-after");
  phase("show-before");
  log.show();
  phase("show-after");
  phase("process-events-before");
  QCoreApplication::processEvents();
  phase("process-events-after");
  QCOMPARE(table->textElideMode(), Qt::ElideNone);
  QVERIFY(table->rowHeight(table->rowCount() - 1) > table->fontMetrics().height() * 2);
}

void EditorTransactionTests::localizingEngineLogPreservesRawHistory()
{
  class FixtureTranslator final : public QTranslator
  {
  public:
    bool isEmpty() const override
    {
      return false;
    }

    QString translate(const char *context, const char *source, const char *, int) const override
    {
      if (QByteArrayView(context) == "EngineLogMessages" && QByteArrayView(source) == "fixture connection accepted")
        return QStringLiteral("已接受客户端连接");
      if (QByteArrayView(context) == "EngineLogFormats") {
        const QByteArrayView format(source);
        if (format == "failed to open trusted fingerprints file: %s")
          return QStringLiteral("打开受信任指纹文件失败：%s");
        if (format == "file transfer: wrote %s (%zu bytes)")
          return QStringLiteral("文件传输：已写入 %s（%zu 字节）");
        if (format == "error communicating with client \"%s\": %s")
          return QStringLiteral("与客户端“%s”通信时出错：%s");
        if (format == "failed to connect to server=%s, trying next resolved address")
          return QStringLiteral("连接服务器 %s 失败，正在尝试下一个解析地址");
        if (format == "failed to connect to server=%s, trying next server in list")
          return QStringLiteral("连接服务器 %s 失败，正在尝试列表中的下一个服务器");
        if (format == "format specifier %%I%d has invalid length")
          return QStringLiteral("格式说明符 %%I%d 的长度无效");
        if (format == "file transfer: server is offering %zu file(s)")
          return QStringLiteral("文件传输：服务器正在提供 %s 个文件"); // Deliberately invalid translation type.
      }
      return {};
    }
  };

  LogWidget log;
  auto *table = log.findChild<QTableWidget *>();
  QVERIFY(table);
  const QString raw = QStringLiteral("[2026-11-18T18:17:28.518] INFO: fixture connection accepted");
  log.appendLine(raw);
  const QString unknown = QStringLiteral("[time] WARNING: unknown engine detail");
  log.appendLine(unknown);
  QCOMPARE(table->item(0, 2)->text(), QStringLiteral("fixture connection accepted"));
  FixtureTranslator translator;
  QVERIFY(QCoreApplication::installTranslator(&translator));
  QTRY_COMPARE(table->item(0, 2)->text(), QStringLiteral("已接受客户端连接"));
  QCOMPARE(table->item(1, 2)->text(), QStringLiteral("unknown engine detail"));
  QCOMPARE(log.allText(), raw + QLatin1Char('\n') + unknown);
  log.setLevelFilter(QStringLiteral("info"));
  QVERIFY(log.latestDisplayText().endsWith(QStringLiteral("已接受客户端连接")));
  QCOMPARE(log.allText(), raw);
  QVERIFY(QCoreApplication::removeTranslator(&translator));
  QTRY_COMPARE(table->item(0, 2)->text(), QStringLiteral("fixture connection accepted"));

  LogWidget dynamicLog;
  auto *dynamicTable = dynamicLog.findChild<QTableWidget *>();
  QVERIFY(dynamicTable);
  const auto path = QStringLiteral("C:\\临时目录\\%1:指纹:数据库.txt");
  const auto client = QStringLiteral("客户端%1:名字");
  const auto detail = QStringLiteral("对端:连接断开 %2:保留");
  const auto host = QStringLiteral("服务器%1:24800");
  const auto ambiguous = QStringLiteral("error communicating with client \"first\": second\": third");
  const auto overlong = QStringLiteral("failed to open trusted fingerprints file: ") + QString(16385, QLatin1Char('x'));
  const QList<QPair<QString, QString>> examples{
      {QStringLiteral("failed to open trusted fingerprints file: ") + path,
       QStringLiteral("打开受信任指纹文件失败：") + path},
      {QStringLiteral("file transfer: wrote ") + path + QStringLiteral(" (4294967297 bytes)"),
       QStringLiteral("文件传输：已写入 ") + path + QStringLiteral("（4294967297 字节）")},
      {QStringLiteral("error communicating with client \"") + client + QStringLiteral("\": ") + detail,
       QStringLiteral("与客户端“") + client + QStringLiteral("”通信时出错：") + detail},
      {QStringLiteral("failed to connect to server=") + host + QStringLiteral(", trying next resolved address"),
       QStringLiteral("连接服务器 ") + host + QStringLiteral(" 失败，正在尝试下一个解析地址")},
      {QStringLiteral("failed to connect to server=") + host + QStringLiteral(", trying next server in list"),
       QStringLiteral("连接服务器 ") + host + QStringLiteral(" 失败，正在尝试列表中的下一个服务器")},
      {QStringLiteral("format specifier %I8 has invalid length"), QStringLiteral("格式说明符 %I8 的长度无效")},
      {QStringLiteral("file transfer: wrote ") + path + QStringLiteral(" (not-a-number bytes)"),
       QStringLiteral("file transfer: wrote ") + path + QStringLiteral(" (not-a-number bytes)")},
      {ambiguous, ambiguous}, // Either occurrence of the delimiter yields two valid string fields.
      {QStringLiteral("file transfer: server is offering 12 file(s)"),
       QStringLiteral("file transfer: server is offering 12 file(s)")},
      {overlong, overlong},
      {QStringLiteral("unknown engine detail: ") + detail, QStringLiteral("unknown engine detail: ") + detail}
  };
  QStringList rawLines;
  for (const auto &example : examples) {
    rawLines.append(QStringLiteral("[2026-11-18T18:17:28.518] INFO: ") + example.first);
    dynamicLog.appendLine(rawLines.last());
  }
  QCOMPARE(dynamicTable->rowCount(), examples.size());
  for (int row = 0; row < examples.size(); ++row)
    QCOMPARE(dynamicTable->item(row, 2)->text(), examples.at(row).first);
  QVERIFY(QCoreApplication::installTranslator(&translator));
  QTRY_COMPARE(dynamicTable->item(0, 2)->text(), examples.first().second);
  for (int row = 0; row < examples.size(); ++row)
    QCOMPARE(dynamicTable->item(row, 2)->text(), examples.at(row).second);
  // Copy uses allText(), so it must retain the exact original lines without touching the host clipboard.
  QCOMPARE(dynamicLog.allText(), rawLines.join(QLatin1Char('\n')));
  QVERIFY(dynamicLog.latestDisplayText().endsWith(examples.last().second));
  QVERIFY(QCoreApplication::removeTranslator(&translator));
  QTRY_COMPARE(dynamicTable->item(0, 2)->text(), examples.first().first);
  for (int row = 0; row < examples.size(); ++row)
    QCOMPARE(dynamicTable->item(row, 2)->text(), examples.at(row).first);
  QCOMPARE(dynamicLog.allText(), rawLines.join(QLatin1Char('\n')));
}

void EditorTransactionTests::largeLogHistoryWrapsAfterResizeFontAndFilter()
{
  LogWidget log;
  log.resize(640, 236);
  const auto payload = QStringLiteral("wrapped geometry text ").repeated(12).trimmed();
  QStringList expectedAll;
  QStringList expectedInfo;
  QStringList expectedWarning;
  for (int row = 0; row < 20; ++row) {
    const bool warning = row % 2 != 0;
    const auto line = QStringLiteral("[time] %1: row-%2 %3")
                          .arg(warning ? QStringLiteral("WARNING") : QStringLiteral("INFO"))
                          .arg(row)
                          .arg(payload);
    expectedAll.append(line);
    (warning ? expectedWarning : expectedInfo).append(line);
    log.appendLine(line);
  }
  const std::array<int, 3> counts{10, 10, 0};
  auto *table = log.findChild<QTableWidget *>();
  QVERIFY(table != nullptr);
  QCOMPARE(table->font().pixelSize(), 12);
  QCOMPARE(table->rowCount(), 20);
  QCOMPARE(log.allText(), expectedAll.join(QLatin1Char('\n')));
  QCOMPARE(log.levelCounts(), counts);

  QSize previousGeometry;
  QElapsedTimer stableSince;
  const auto lastRowHeight = [&] { return table->rowHeight(table->rowCount() - 1); };
  const auto geometryStable = [&] {
    const QSize geometry(table->columnWidth(2), lastRowHeight());
    if (geometry != previousGeometry || !stableSince.isValid()) {
      previousGeometry = geometry;
      stableSince.start();
      return false;
    }
    return stableSince.elapsed() >= 50;
  };
  log.show();
  QTRY_VERIFY_WITH_TIMEOUT(
      table->columnWidth(2) > 350 && lastRowHeight() > table->fontMetrics().height() * 2 && geometryStable(), 1000
  );
  const int wideColumn = table->columnWidth(2);
  const int wideHeight = lastRowHeight();

  log.resize(320, 236);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->columnWidth(2) < wideColumn && lastRowHeight() > wideHeight && geometryStable(), 1000
  );
  const int narrowHeight = lastRowHeight();
  QCOMPARE(table->rowCount(), 20);
  QCOMPARE(log.allText(), expectedAll.join(QLatin1Char('\n')));
  QCOMPARE(log.levelCounts(), counts);

  auto font = table->font();
  font.setPixelSize(20);
  table->setFont(font);
  QCOMPARE(table->font().pixelSize(), 20);
  QTRY_VERIFY_WITH_TIMEOUT(lastRowHeight() > narrowHeight && geometryStable(), 1000);
  const int mixedColumn = table->columnWidth(2);
  QCOMPARE(table->rowCount(), 20);
  QCOMPARE(log.allText(), expectedAll.join(QLatin1Char('\n')));
  QCOMPARE(log.levelCounts(), counts);

  log.setLevelFilter(QStringLiteral("info"));
  QCOMPARE(table->rowCount(), 10);
  QCOMPARE(log.allText(), expectedInfo.join(QLatin1Char('\n')));
  QCOMPARE(log.levelCounts(), counts);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->columnWidth(2) > mixedColumn && lastRowHeight() > table->fontMetrics().height() * 2 && geometryStable(),
      1000
  );
  const int infoColumn = table->columnWidth(2);
  const int infoHeight = lastRowHeight();

  log.setLevelFilter(QStringLiteral("warn"));
  QCOMPARE(table->rowCount(), 10);
  QCOMPARE(log.allText(), expectedWarning.join(QLatin1Char('\n')));
  QCOMPARE(log.levelCounts(), counts);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->columnWidth(2) < infoColumn && lastRowHeight() >= infoHeight && geometryStable(), 1000
  );
  const int warningColumn = table->columnWidth(2);
  const int warningHeight = lastRowHeight();

  log.setLevelFilter(QString());
  QCOMPARE(table->rowCount(), 20);
  QCOMPARE(log.allText(), expectedAll.join(QLatin1Char('\n')));
  QCOMPARE(log.levelCounts(), counts);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->columnWidth(2) == warningColumn && lastRowHeight() == warningHeight && geometryStable(), 1000
  );
  auto *scrollBar = table->verticalScrollBar();
  QVERIFY(scrollBar->maximum() > 0);
  scrollBar->setValue(0);
  QEvent languageChange(QEvent::LanguageChange);
  QCoreApplication::sendEvent(&log, &languageChange);
  QCOMPARE(scrollBar->value(), 0);
  log.appendLine(QStringLiteral("[time] INFO: extra row"));
  QCoreApplication::processEvents();
  QCOMPARE(scrollBar->value(), 0);
}

void EditorTransactionTests::traySelectorCancelDoesNotWrite()
{
  Settings::setValue(Settings::Gui::SymbolicTrayIcon, false);
  synergy::gui::ServerConfig config;
  SettingsDialog editor(nullptr, config);
  auto *selector = editor.findChild<QComboBox *>(QStringLiteral("comboTrayIcon"));
  QVERIFY(selector != nullptr);
  QCOMPARE(selector->currentIndex(), 0);
  selector->setCurrentIndex(1);
  QVERIFY(editor.findChild<QRadioButton *>(QStringLiteral("rbIconMono"))->isChecked());
  editor.reject();
  QVERIFY(!Settings::value(Settings::Gui::SymbolicTrayIcon).toBool());
}

void EditorTransactionTests::traySelectorSaveWrites()
{
  Settings::setValue(Settings::Gui::SymbolicTrayIcon, false);
  synergy::gui::ServerConfig config;
  SettingsDialog editor(nullptr, config);
  editor.findChild<QComboBox *>(QStringLiteral("comboTrayIcon"))->setCurrentIndex(1);
  auto *buttons = editor.findChild<QDialogButtonBox *>();
  QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
  buttons->button(QDialogButtonBox::Save)->click();
  QCOMPARE(editor.result(), static_cast<int>(QDialog::Accepted));
  QVERIFY(Settings::value(Settings::Gui::SymbolicTrayIcon).toBool());
}

void EditorTransactionTests::cancelPreservesSystemLanguageSetting()
{
  Settings::setValue(Settings::Core::Language, QString());
  const auto originalLanguage = I18N::currentLanguage();
  synergy::gui::ServerConfig config;
  SettingsDialog editor(nullptr, config);
  Settings::setValue(Settings::Core::Language, QStringLiteral("en"));
  editor.reject();
  QVERIFY(Settings::value(Settings::Core::Language).toString().isEmpty());
  QCOMPARE(I18N::currentLanguage(), originalLanguage);
}

void EditorTransactionTests::saveOtherSettingPreservesSystemLanguage()
{
  Settings::setValue(Settings::Core::Language, QString());
  Settings::setValue(Settings::Gui::SymbolicTrayIcon, false);
  synergy::gui::ServerConfig config;
  SettingsDialog editor(nullptr, config);
  editor.findChild<QComboBox *>(QStringLiteral("comboTrayIcon"))->setCurrentIndex(1);
  editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  QCOMPARE(editor.result(), static_cast<int>(QDialog::Accepted));
  QVERIFY(Settings::value(Settings::Core::Language).toString().isEmpty());
}

int main(int argc, char **argv)
{
  // 在 Settings 首次初始化前隔离设置和状态，不触碰宿主日常配置。
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
  Settings::setSettingsFile(directory.filePath(QStringLiteral("test.conf")));
  Settings::setStateFile(directory.filePath(QStringLiteral("test.state")));
  EditorTransactionTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "EditorTransactionTests.moc"
