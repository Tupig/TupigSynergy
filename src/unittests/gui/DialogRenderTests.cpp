/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "UiRenderTestSupport.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/Action.h"
#include "gui/AppStyle.h"
#include "gui/Hotkey.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ActionDialog.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/FingerprintDialog.h"
#include "gui/dialogs/HotkeyDialog.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/update/UpdateChecker.h"
#include "gui/update/UpdateDialog.h"
#include "gui/widgets/KeySequenceWidget.h"
#include "update/TestHttpServer.h"
#include <QFileInfo>
#include <QUuid>

#include <QApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QListWidget>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QString updateText(const char *source)
{
  return QCoreApplication::translate("synergy::gui::update::UpdateDialog", source);
}

/// 离屏渲染页面并保存截图，避免向系统注入真实输入。
void renderToFile(QWidget &widget, const QString &path)
{
  widget.ensurePolished();
  widget.adjustSize();
  QVERIFY2(settleStaticRender(widget), "Static capture did not settle within 1000ms");
  QPixmap pixmap(widget.size());
  pixmap.fill(Qt::transparent);
  widget.render(&pixmap);
  QVERIFY2(pixmap.save(path), qPrintable(path));
}

} // namespace

class DialogRenderTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void rendersHotkeyAndAction();
  void recordsHotkeyWithSyntheticKeys();
  void downloadsUpdateFromLoopback();
  void rendersUpdateAndFingerprint();
  void rendersConfigurationPages();
  void animatedButtonStillDeliversClick();
};

void DialogRenderTests::rendersHotkeyAndAction()
{
  const QString outputDir = qEnvironmentVariable(
      "SYNERGY_UI_EVIDENCE_DIR", QDir(QDir::tempPath()).filePath(QStringLiteral("tupig-dialog-render"))
  );
  QDir().mkpath(outputDir);

  Hotkey hotkey;
  HotkeyDialog hotkeyDialog(nullptr, hotkey);
  renderToFile(hotkeyDialog, QDir(outputDir).filePath(QStringLiteral("hotkey.png")));
  QVERIFY(hotkeyDialog.width() > 0);

  synergy::gui::ServerConfig config;
  Action action;
  ActionDialog actionDialog(nullptr, config, hotkey, action);
  renderToFile(actionDialog, QDir(outputDir).filePath(QStringLiteral("action.png")));
  QVERIFY(actionDialog.width() > 0);
  // 屏幕列表来自服务器配置，必须始终存在。
  QVERIFY(actionDialog.findChild<QListWidget *>() != nullptr);

  qInfo() << "rendered:" << outputDir;
}

void DialogRenderTests::rendersUpdateAndFingerprint()
{
  synergy::gui::update::UpdateInfo info;
  info.version = QStringLiteral("1.22.0");
  info.tag = QStringLiteral("v1.22.0");
  info.notes = QStringLiteral("- 修复日志面板高度\n- 新增卡片式配置页");
  info.releaseUrl = QUrl(QStringLiteral("https://github.com/Tupig/TupigSynergy/releases/tag/v1.22.0"));
  info.asset.name = QStringLiteral("synergy_1.22.0_mac_arm64.dmg");
  const QString outputDir = qEnvironmentVariable(
      "SYNERGY_UI_EVIDENCE_DIR", QDir(QDir::tempPath()).filePath(QStringLiteral("tupig-dialog-render"))
  );
  QDir().mkpath(outputDir);

  synergy::gui::update::UpdateDialog updateDialog(info, nullptr);
  renderToFile(updateDialog, QDir(outputDir).filePath(QStringLiteral("update.png")));
  QVERIFY(updateDialog.width() > 0);

  Fingerprint local(QCryptographicHash::Sha256, QByteArray(32, '\x5a'));
  Fingerprint remote(QCryptographicHash::Sha256, QByteArray(32, '\x7e'));
  FingerprintDialog fingerprint(nullptr, local, FingerprintDialogMode::Server, remote);
  renderToFile(fingerprint, QDir(outputDir).filePath(QStringLiteral("fingerprint.png")));
  QVERIFY(fingerprint.width() > 0);
}

void DialogRenderTests::recordsHotkeyWithSyntheticKeys()
{
  Hotkey hotkey;
  HotkeyDialog dialog(nullptr, hotkey);
  auto *sequence = dialog.findChild<KeySequenceWidget *>(QStringLiteral("m_pKeySequenceWidgetHotkey"));
  QVERIFY(sequence != nullptr);

  // 点击进入录制，再用 Qt 合成按键事件，避免向系统注入真实输入。
  QTest::mouseClick(sequence, Qt::LeftButton);
  QTest::keyClick(sequence, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
  QVERIFY(sequence->valid());
  QVERIFY(!sequence->keySequence().toString().isEmpty());

  QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection);
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
  QVERIFY(!hotkey.text().isEmpty());
}

void DialogRenderTests::downloadsUpdateFromLoopback()
{
  const QByteArray payload(256 * 1024, 'x');
  TestHttpServer server;
  server.setBinary(payload);
  const auto digest = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());

  synergy::gui::update::UpdateInfo info;
  info.version = QStringLiteral("1.22.0");
  info.tag = QStringLiteral("v1.22.0");
  info.notes = QStringLiteral("- 修复日志面板高度");
  info.releaseUrl = QUrl(QStringLiteral("https://github.com/Tupig/TupigSynergy/releases/tag/v1.22.0"));
  info.asset.name = QStringLiteral("synergy_1.22.0_mac_arm64.dmg");
  info.asset.url = server.url(QStringLiteral("/download/synergy_1.22.0_mac_arm64.dmg"));
  info.asset.size = payload.size();
  info.asset.sha256 = digest;

  synergy::gui::update::DownloadPolicy policy;
  policy.scheme = QStringLiteral("http");
  policy.host = QStringLiteral("127.0.0.1");
  policy.pathPrefix = QStringLiteral("/download/");

  synergy::gui::update::UpdateDialog dialog(info, nullptr, policy);
  auto *install = dialog.findChild<QPushButton *>();
  QVERIFY(install != nullptr);
  // 找到「下载并安装」按钮。
  QPushButton *downloadButton = nullptr;
  for (auto *button : dialog.findChildren<QPushButton *>()) {
    if (button->text() == updateText("Download and Install"))
      downloadButton = button;
  }
  QVERIFY(downloadButton != nullptr);
  QTest::mouseClick(downloadButton, Qt::LeftButton);

  QTRY_COMPARE_WITH_TIMEOUT(downloadButton->text(), updateText("Install Now"), 15000);
  auto *progress = dialog.findChild<QProgressBar *>();
  QVERIFY(progress != nullptr);
  QCOMPARE(progress->value(), 100);
  // 只验证校验完成，不点击安装，避免启动系统安装器。
}

void DialogRenderTests::rendersConfigurationPages()
{
  const QString outputDir = qEnvironmentVariable(
      "SYNERGY_UI_EVIDENCE_DIR", QDir(QDir::tempPath()).filePath(QStringLiteral("tupig-dialog-render"))
  );
  QDir().mkpath(outputDir);

  synergy::gui::ServerConfig config;

  ServerConfigDialog serverDialog(nullptr, config);
  auto *tabs = serverDialog.findChild<QTabWidget *>();
  QVERIFY(tabs != nullptr);
  const QStringList names{
      QStringLiteral("server-layout"), QStringLiteral("server-hotkeys"), QStringLiteral("server-sharing"),
      QStringLiteral("server-config")
  };
  for (int index = 0; index < tabs->count() && index < names.size(); ++index) {
    tabs->setCurrentIndex(index);
    renderToFile(serverDialog, QDir(outputDir).filePath(names.at(index) + QStringLiteral(".png")));
  }

  ClientConfigDialog clientDialog(nullptr);
  renderToFile(clientDialog, QDir(outputDir).filePath(QStringLiteral("client.png")));

  SettingsDialog settingsDialog(nullptr, config);
  renderToFile(settingsDialog, QDir(outputDir).filePath(QStringLiteral("preferences.png")));

  synergy::gui::Screen screen(QStringLiteral("screen"));
  synergy::gui::ScreenList screens;
  screens.append(screen);
  ScreenSettingsDialog screenDialog(nullptr, &screen, &screens);
  renderToFile(screenDialog, QDir(outputDir).filePath(QStringLiteral("screen.png")));
}

void DialogRenderTests::animatedButtonStillDeliversClick()
{
  synergy::gui::installButtonFeedback();
  QDialog dialog;
  QPushButton button(QStringLiteral("Test"), &dialog);
  button.resize(100, 32);
  dialog.resize(120, 52);
  dialog.show();
  QCoreApplication::processEvents();
  QSignalSpy clicked(&button, &QPushButton::clicked);
  QTest::mouseClick(&button, Qt::LeftButton);
  QCOMPARE(clicked.count(), 1);
  auto *overlay = button.findChild<QWidget *>(QStringLiteral("buttonFeedbackOverlay"));
  QVERIFY(overlay != nullptr);
  QVERIFY(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
  QTest::mouseClick(&button, Qt::LeftButton);
  QCOMPARE(clicked.count(), 2);
}

int main(int argc, char **argv)
{
  // 构造任何读取 Settings 的对话框之前隔离设置，避免测试碰到日常配置。
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
  Settings::setSettingsFile(directory.filePath(QStringLiteral("render.conf")));
  Settings::setStateFile(directory.filePath(QStringLiteral("render.state")));
  app.setStyleSheet(synergy::gui::appStyleSheet());
  DialogRenderTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "DialogRenderTests.moc"
