/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "UiRenderTestSupport.h"
#include "common/I18N.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include "gui/Action.h"
#include "gui/AppStyle.h"
#include "gui/Hotkey.h"
#include "gui/config/Screen.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ActionDialog.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/FingerprintDialog.h"
#include "gui/dialogs/HotkeyDialog.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/widgets/PageHost.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QUuid>
#include <QVBoxLayout>
#include <QtTest>
#include <memory>

namespace {
QColor pixel(const QImage &image, QPoint point)
{
  return image.pixelColor(qRound(point.x() * image.devicePixelRatio()), qRound(point.y() * image.devicePixelRatio()));
}
void fit(PageHost &host, int width)
{
  QCoreApplication::processEvents();
  host.resize(width, qMin(660, host.preferredContentHeight(width)));
  QCoreApplication::processEvents();
}
} // namespace
class R3VisualContractTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void logCardGeometryAndTransactions_data()
  {
    QTest::addColumn<int>("width");
    QTest::addColumn<QString>("language");
    for (int width : {540, 600, 820})
      for (const char *language : {"en", "zh_CN"})
        QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(language).arg(width)))
            << width << QString::fromLatin1(language);
  }
  void logCardGeometryAndTransactions()
  {
    QFETCH(int, width);
    QFETCH(QString, language);
    I18N::setLanguage(language);
    const QString syntheticPath = Settings::userDir() + "/logs/fixture.log";
    Settings::setValue(Settings::Log::File, syntheticPath);
    Settings::setValue(Settings::Log::ToFile, false);
    PageHost host;
    auto *stack = new QStackedWidget(&host);
    stack->setObjectName("pageStack");
    stack->addWidget(new QWidget);
    auto *hostLayout = new QVBoxLayout(&host);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->addWidget(stack);
    host.setAttribute(Qt::WA_DontShowOnScreen);
    host.resize(width, 100);
    host.show();
    synergy::gui::ServerConfig config;
    auto *editor = new SettingsDialog(nullptr, config);
    host.push(editor);
    auto *tabs = editor->findChild<QTabWidget *>();
    QVERIFY(tabs);
    auto *logPage = editor->findChild<QWidget *>("tabLog");
    QVERIFY(logPage);
    tabs->setCurrentWidget(logPage);
    fit(host, width);
    auto *card = editor->findChild<QGroupBox *>("logCard");
    auto *fileFlag = editor->findChild<QCheckBox *>("cbLogToFile");
    auto *path = editor->findChild<QLineEdit *>("lineLogFilename");
    auto *browse = editor->findChild<QPushButton *>("btnBrowseLog");
    QVERIFY(card && fileFlag && path && browse);
    int visibleCards = 0;
    for (auto *group : logPage->findChildren<QGroupBox *>())
      if (group->isVisibleTo(editor))
        ++visibleCards;
    QCOMPARE(visibleCards, 1);
    QVERIFY(card->isAncestorOf(fileFlag));
    QVERIFY(card->isAncestorOf(path));
    QVERIFY(!fileFlag->isChecked());
    QVERIFY(!path->isEnabled());
    QVERIFY(!browse->isEnabled());
    QStyleOptionButton option;
    option.initFrom(fileFlag);
    option.text = fileFlag->text();
    const auto indicator = fileFlag->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, fileFlag);
    const auto indicatorPosition = fileFlag->mapTo(card, indicator.topLeft());
    qInfo() << "R4 checkbox geometry diagnostic" << language << width << "widgetRect" << fileFlag->rect()
            << "widgetMapped" << fileFlag->mapTo(card, QPoint()) << "optionRect" << option.rect << "indicatorLocal"
            << indicator << "indicatorMapped" << indicatorPosition << "cardRect" << card->rect() << "cardContents"
            << card->contentsRect() << "layoutMargins" << card->layout()->contentsMargins() << "style"
            << fileFlag->style()->metaObject()->className();
    const auto diagnosticRoot = qEnvironmentVariable("SYNERGY_R3_VISUAL_DIAGNOSTIC_DIR");
    if (!diagnosticRoot.isEmpty()) {
      QVERIFY(QDir().mkpath(diagnosticRoot));
      QVERIFY(host.grab().save(diagnosticRoot + "/log-indicator-" + language + "-" + QString::number(width) + ".png"));
    }
    QVERIFY2(indicatorPosition.x() >= 12, "Checkbox indicator must be inside the card, not on its border");
    QVERIFY(indicatorPosition.y() >= 10);
    QVERIFY(path->width() >= 180);
    QCOMPARE(browse->width(), 32);
    const auto pathRect = QRect(path->mapTo(card, QPoint()), path->size());
    const auto browseRect = QRect(browse->mapTo(card, QPoint()), browse->size());
    QCOMPARE(browseRect.left() - pathRect.right() - 1, 8);
    QVERIFY(card->rect().adjusted(10, 10, -10, -10).contains(pathRect));
    QVERIFY(card->rect().adjusted(10, 10, -10, -10).contains(browseRect));
    QTest::mouseClick(fileFlag, Qt::LeftButton);
    QVERIFY(fileFlag->isChecked());
    QVERIFY(path->isEnabled());
    QVERIFY(browse->isEnabled());
    auto *buttons = editor->findChild<QDialogButtonBox *>();
    QVERIFY(buttons);
    auto *save = buttons->button(QDialogButtonBox::Save);
    QVERIFY(save && save->isEnabled());
    QVERIFY(save->property("primary").toBool());
    for (auto role : {QDialogButtonBox::Cancel, QDialogButtonBox::Reset, QDialogButtonBox::RestoreDefaults})
      QVERIFY(!buttons->button(role)->property("primary").toBool());
    QVERIFY2(settleStaticRender(host), "Static capture did not settle within 1000ms");
    const auto saveImage = save->grab().toImage();
    const auto background = pixel(saveImage, QPoint(8, save->height() / 2));
    QVERIFY(background == QColor("#4cc38a") || background == QColor("#2f9e6b"));
    const auto evidence = qEnvironmentVariable("SYNERGY_R3_EVIDENCE_DIR");
    if (!evidence.isEmpty()) {
      QVERIFY(QDir().mkpath(evidence + "/visual"));
      QVERIFY(host.grab().save(evidence + QStringLiteral("/visual/log-%1-%2.png").arg(language).arg(width)));
    }
    QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel), Qt::LeftButton);
    QVERIFY(!Settings::value(Settings::Log::ToFile).toBool());
    QCOMPARE(Settings::value(Settings::Log::File).toString(), syntheticPath);
  }
  // Additive UI-005/UI-011 endpoint check before local hash-format correction.
  void localFingerprintCodeBlockIsCompleteAndFits_data()
  {
    QTest::addColumn<int>("width");
    QTest::addColumn<QString>("language");
    for (int width : {540, 600, 820})
      for (const char *language : {"en", "zh_CN"})
        QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(language).arg(width)))
            << width << QString::fromLatin1(language);
  }
  void localFingerprintCodeBlockIsCompleteAndFits()
  {
    QFETCH(int, width);
    QFETCH(QString, language);
    I18N::setLanguage(language);
    PageHost host;
    auto *stack = new QStackedWidget(&host);
    stack->setObjectName("pageStack");
    stack->addWidget(new QWidget);
    auto *layout = new QVBoxLayout(&host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(stack);
    host.setAttribute(Qt::WA_DontShowOnScreen);
    host.resize(width, 100);
    host.show();
    const QByteArray bytes(32, '\x5a');
    auto *editor = new FingerprintDialog(nullptr, {QCryptographicHash::Sha256, bytes});
    host.push(editor);
    fit(host, width);
    QVERIFY(settleStaticRender(host));
    auto *hash = editor->findChild<QLabel *>("fingerprintHash");
    QVERIFY(hash && hash->isVisibleTo(editor));
    QVERIFY(hash->alignment().testFlag(Qt::AlignLeft));
    const auto lines = hash->text().split('\n', Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 2);
    QString complete;
    for (auto line : lines) {
      QVERIFY2(hash->fontMetrics().horizontalAdvance(line) <= hash->width(), "Fingerprint line clips horizontally");
      line.remove(QRegularExpression("[^0-9a-fA-F]"));
      QCOMPARE(line.size(), 32); // Sixteen bytes per line, complete SHA-256.
      complete += line.toLower();
    }
    QCOMPARE(complete, QString::fromLatin1(bytes.toHex()));
    for (auto *area : editor->findChildren<QScrollArea *>())
      if (area->isVisibleTo(editor))
        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
    for (auto *label : editor->findChildren<QLabel *>())
      QVERIFY2(
          !label->isVisibleTo(editor) || !label->text().isEmpty(), "Empty local footer must not reserve visible height"
      );
    auto *box = editor->findChild<QDialogButtonBox *>();
    QVERIFY(box && box->button(QDialogButtonBox::Ok));
    QVERIFY(box->button(QDialogButtonBox::Ok)->property("primary").toBool());
    const auto evidence = qEnvironmentVariable("SYNERGY_R3_EVIDENCE_DIR");
    if (!evidence.isEmpty()) {
      QVERIFY(QDir().mkpath(evidence + "/visual"));
      QVERIFY(host.grab().save(evidence + QStringLiteral("/visual/fingerprint-%1-%2.png").arg(language).arg(width)));
    }
    editor->reject();
  }
  // UI-004 bounded native alignment finding: visible form content starts near
  // the card's inner edge; inspect real geometry rather than a layout flag.
  void actionShortFormStartsAtCardInnerEdge_data()
  {
    QTest::addColumn<int>("width");
    QTest::addColumn<QString>("language");
    for (int width : {540, 600, 820})
      for (const char *language : {"en", "zh_CN"})
        QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(language).arg(width)))
            << width << QString::fromLatin1(language);
  }
  void actionShortFormStartsAtCardInnerEdge()
  {
    QFETCH(int, width);
    QFETCH(QString, language);
    I18N::setLanguage(language);
    synergy::gui::ServerConfig config;
    config.addClient("Studio");
    Hotkey hotkey;
    QSettings fixture(Settings::userDir() + "/action-alignment.ini", QSettings::IniFormat);
    fixture.setValue(SettingsKeys::ActionType, 3);
    fixture.setValue(SettingsKeys::SwitchToScreen, "Studio");
    Action action;
    action.loadSettings(fixture);
    PageHost host;
    auto *stack = new QStackedWidget(&host);
    stack->setObjectName("pageStack");
    stack->addWidget(new QWidget);
    auto *layout = new QVBoxLayout(&host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(stack);
    host.setAttribute(Qt::WA_DontShowOnScreen);
    host.resize(width, 100);
    host.show();
    auto *editor = new ActionDialog(nullptr, config, hotkey, action);
    host.push(editor);
    fit(host, width);
    QVERIFY(settleStaticRender(host));
    auto *card = editor->findChild<QGroupBox *>("actionCard");
    auto *label = editor->findChild<QLabel *>("labelTrigger");
    auto *field = editor->findChild<QComboBox *>("comboTriggerOn");
    QVERIFY(card && label && field);
    QVERIFY(label->isVisibleTo(editor) && field->isVisibleTo(editor));
    const QRect labelRect(label->mapTo(card, QPoint()), label->size());
    const QRect fieldRect(field->mapTo(card, QPoint()), field->size());
    qInfo() << "Action form geometry" << language << width << "label" << labelRect << "field" << fieldRect << "card"
            << card->rect();
    QVERIFY2(
        labelRect.left() >= 10 && labelRect.left() <= 24,
        qPrintable(
            QStringLiteral("Trigger label starts %1px inside card; expected inner-edge flow").arg(labelRect.left())
        )
    );
    QVERIFY(labelRect.right() < fieldRect.left());
    QCOMPARE(field->width(), 180);
    QVERIFY(card->rect().adjusted(10, 0, -10, 0).contains(fieldRect));
    for (auto *area : editor->findChildren<QScrollArea *>())
      if (area->isVisibleTo(editor))
        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
    const auto evidence = qEnvironmentVariable("SYNERGY_R3_EVIDENCE_DIR");
    if (!evidence.isEmpty()) {
      QVERIFY(QDir().mkpath(evidence + "/visual"));
      QVERIFY(host.grab().save(evidence + QStringLiteral("/visual/action-alignment-%1-%2.png").arg(language).arg(width))
      );
    }
    editor->reject();
  }
  void tabsHaveTransparentBlankAreaAndNoNativeBaseline()
  {
    QWidget workspace;
    workspace.setObjectName("r3Workspace");
    workspace.setStyleSheet("QWidget#r3Workspace { background: #123456; }");
    auto *layout = new QVBoxLayout(&workspace);
    auto *tabs = new QTabWidget;
    tabs->addTab(new QWidget, "One");
    tabs->addTab(new QWidget, "Two");
    layout->addWidget(tabs);
    workspace.resize(600, 220);
    workspace.setAttribute(Qt::WA_DontShowOnScreen);
    workspace.show();
    QCoreApplication::processEvents();
    auto *bar = tabs->tabBar();
    const auto image = workspace.grab().toImage();
    const int start = tabs->mapTo(&workspace, QPoint()).x() + bar->tabRect(1).right() + 16;
    const int end = tabs->mapTo(&workspace, QPoint(tabs->width() - 12, 0)).x();
    QVERIFY(end - start > 100);
    const int top = bar->mapTo(&workspace, QPoint()).y();
    for (int y : {top, top + bar->height() / 2, top + bar->height() - 1, top + bar->height() + 1})
      for (int x = start; x < end; ++x)
        QCOMPARE(pixel(image, QPoint(x, y)), QColor("#123456"));
  }
  void everyEditorKeepsCardsAndActionTiers_data()
  {
    QTest::addColumn<QString>("kind");
    for (const char *kind : {"preferences", "server", "client", "computer", "hotkey", "action"})
      QTest::newRow(kind) << QString::fromLatin1(kind);
  }
  void everyEditorKeepsCardsAndActionTiers()
  {
    QFETCH(QString, kind);
    synergy::gui::ServerConfig config;
    config.addClient("Studio");
    synergy::gui::Screen screen("Studio");
    Hotkey hotkey;
    Action action;
    QSettings empty(Settings::userDir() + "/visual-action.ini", QSettings::IniFormat);
    action.loadSettings(empty);
    QDialog *dialog = nullptr;
    if (kind == "preferences")
      dialog = new SettingsDialog(nullptr, config);
    if (kind == "server")
      dialog = new ServerConfigDialog(nullptr, config);
    if (kind == "client")
      dialog = new ClientConfigDialog(nullptr);
    if (kind == "computer")
      dialog = new ScreenSettingsDialog(nullptr, &screen, nullptr);
    if (kind == "hotkey")
      dialog = new HotkeyDialog(nullptr, hotkey);
    if (kind == "action")
      dialog = new ActionDialog(nullptr, config, hotkey, action);
    QVERIFY(dialog);
    std::unique_ptr<QDialog> editor(dialog);
    dialog->setAttribute(Qt::WA_DontShowOnScreen);
    dialog->show();
    auto *tabs = dialog->findChild<QTabWidget *>();
    auto *box = dialog->findChild<QDialogButtonBox *>();
    QVERIFY(box);
    for (int index = 0; index < (tabs ? tabs->count() : 1); ++index) {
      if (tabs)
        tabs->setCurrentIndex(index);
      QCoreApplication::processEvents();
      QVERIFY(box->isVisibleTo(dialog));
      for (auto *group : dialog->findChildren<QGroupBox *>()) {
        if (!group->isVisibleTo(dialog))
          continue;
        QVERIFY2(group->title().isEmpty(), qPrintable(kind + ": native border title " + group->objectName()));
        QVERIFY2(!group->isCheckable(), qPrintable(kind + ": native checkable group " + group->objectName()));
      }
      for (auto role : {QDialogButtonBox::Save, QDialogButtonBox::Ok})
        if (auto *button = box->button(role))
          QVERIFY(button->property("primary").toBool());
      for (auto role : {QDialogButtonBox::Cancel, QDialogButtonBox::Reset, QDialogButtonBox::RestoreDefaults})
        if (auto *button = box->button(role))
          QVERIFY(!button->property("primary").toBool());
      if (kind == "preferences") {
        auto *activePage = tabs->currentWidget();
        QVERIFY(activePage->isAncestorOf(box));
        QGroupBox *containingCard = nullptr;
        for (auto *group : activePage->findChildren<QGroupBox *>())
          if (group->isVisibleTo(dialog) && group->isAncestorOf(box))
            containingCard = group;
        QVERIFY(containingCard);
        QVERIFY(containingCard->rect().contains(QRect(box->mapTo(containingCard, QPoint()), box->size())));
      }
    }
    dialog->reject();
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
          {root, "tsr3-" + QUuid::createUuid().toString(QUuid::WithoutBraces)}, &error
      ))
    return 2;
  QApplication application(argc, argv);
  Settings::setSettingsFile(root + "/user/visual.conf");
  Settings::setStateFile(root + "/state/app.state");
  Settings::setValue(Settings::Core::ComputerName, "Visual-Fixture");
  Settings::setValue(Settings::Gui::AutoStartCore, false);
  Settings::setValue(Settings::Gui::CheckForUpdates, false);
  application.setStyleSheet(synergy::gui::appStyleSheet());
  R3VisualContractTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "R3VisualContractTests.moc"
