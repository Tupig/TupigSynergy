/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/Action.h"
#include "gui/Hotkey.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ActionDialog.h"
#include "gui/dialogs/FingerprintDialog.h"
#include "gui/dialogs/HotkeyDialog.h"
#include "gui/update/UpdateChecker.h"
#include "gui/update/UpdateDialog.h"

#include <QApplication>
#include <QDir>
#include <QListWidget>
#include <QPixmap>
#include <QtTest>

namespace {

/// 离屏渲染页面并保存截图，避免向系统注入真实输入。
void renderToFile(QWidget &widget, const QString &path)
{
  widget.ensurePolished();
  widget.adjustSize();
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
  void rendersUpdateAndFingerprint();
};

void DialogRenderTests::rendersHotkeyAndAction()
{
  const QString outputDir = QDir(QDir::tempPath()).filePath(QStringLiteral("tupig-dialog-render"));
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
  synergy::gui::update::UpdateDialog updateDialog(info, nullptr);
  QVERIFY(updateDialog.width() > 0);

  Fingerprint local(QCryptographicHash::Sha256, QByteArray(32, '\x5a'));
  Fingerprint remote(QCryptographicHash::Sha256, QByteArray(32, '\x7e'));
  FingerprintDialog fingerprint(nullptr, local, FingerprintDialogMode::Server, remote);
  QVERIFY(fingerprint.width() > 0);
}

QTEST_MAIN(DialogRenderTests)
#include "DialogRenderTests.moc"
