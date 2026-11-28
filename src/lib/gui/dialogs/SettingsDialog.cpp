/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsDialog.h"
#include "FingerprintDialog.h"
#include "SettingsDialogDesign.h"
#include "common/LogLevel.h"
#include "common/PlatformInfo.h"
#include "common/RuntimeContext.h"
#include "common/RuntimeLogPaths.h"
#include "gui/StyleUtils.h"
#include "widgets/PageHost.h"

#include "common/Constants.h"
#include "common/I18N.h"
#include "common/Settings.h"
#include "gui/Messages.h"
#include "gui/TlsUtility.h"
#include "gui/core/NetworkMonitor.h"
#include "net/TlsPolicy.h"

#ifdef SYNERGY_EXTRA_HEADER
#include "synergy/hooks/gui_hook.h"
#endif

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QListView>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTimer>
#include <QVariant>
#include <algorithm>

using namespace synergy::gui;

SettingsDialog::SettingsDialog(QWidget *parent, const synergy::gui::ServerConfig &serverConfig)
    : QDialog(parent),
      ui{std::make_unique<Ui::SettingsDialog>()},
      m_serverConfig(serverConfig)
{

  ui->setupUi(this);
  const auto bindToggle = [](QCheckBox *visible, QGroupBox *proxy) {
    connect(visible, &QCheckBox::toggled, proxy, &QGroupBox::setChecked);
    connect(proxy, &QGroupBox::toggled, visible, &QCheckBox::setChecked);
  };
  bindToggle(ui->cbLogToFile, ui->groupLogToFile);
  bindToggle(ui->cbTlsEnabled, ui->groupSecurity);
  bindToggle(ui->cbServiceEnabled, ui->groupService);

  // these are enabled by the control next to them
  ui->lineCommandEnter->setEnabled(false);
  ui->lineCommandExit->setEnabled(false);

  // 语言可即时预览，保存前保留原始设置，包括空值与旧版语言代码。
  m_originalLanguage = I18N::currentLanguage();
  m_originalLanguageSetting = Settings::value(Settings::Core::Language).toString();
  I18N::reDetectLanguages();
  for (const auto &language : I18N::detectedLanguages())
    ui->comboLanguage->addItem(language, I18N::nativeTo639Name(language));
  // 使用可测量的 Qt 列表弹层，避免 macOS 原生弹层按窄控件宽度截断语言名称。
  auto *languageView = new QListView(ui->comboLanguage);
  languageView->setTextElideMode(Qt::ElideNone);
  int popupWidth = 180;
  for (const auto &language : I18N::detectedLanguages())
    popupWidth = qMax(popupWidth, languageView->fontMetrics().horizontalAdvance(language) + 48);
  languageView->setMinimumWidth(popupWidth);
  ui->comboLanguage->setView(languageView);

  updateText();

  ui->comboTlsKeyLength->setItemIcon(0, appIcon(QStringLiteral("security-medium")));
  ui->comboTlsKeyLength->setItemIcon(1, appIcon(QStringLiteral("security-high")));

  ui->rbIconMono->setIcon(QIcon::fromTheme(QStringLiteral("%1-symbolic").arg(kRevFqdnName)));
  ui->rbIconColorful->setIcon(QIcon::fromTheme(kRevFqdnName));

  // force the first tab, since qt creator sets the active tab as the last one
  // the developer was looking at, and it's easy to accidentally save that.
  ui->tabWidget->setCurrentIndex(0);

  // The UI owns this control on the General page, independent of the window radio layout.
  m_checkForUpdates = ui->checkForUpdates;
  connect(m_checkForUpdates, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);

  // Populate the list of IP addresses
  const auto validAddresses = NetworkMonitor::validAddresses();
  for (const auto &address : validAddresses) {
    QString ipString = address;
    if (ui->comboInterface->findText(ipString) == -1) {
      ui->comboInterface->addItem(ipString, ipString);
    }
  }

  if (const auto interface = Settings::value(Settings::Core::Interface).toString();
      !interface.isEmpty() && (ui->comboInterface->findData(interface) == -1)) {
    ui->comboInterface->addItem(interface, interface);
  }

  connect(ui->comboTrayIcon, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
    const bool mono = ui->comboTrayIcon->itemData(index).toBool();
    ui->rbIconMono->setChecked(mono);
    ui->rbIconColorful->setChecked(!mono);
  });
  connect(ui->rbIconMono, &QRadioButton::toggled, this, [this](bool mono) {
    ui->comboTrayIcon->setCurrentIndex(ui->comboTrayIcon->findData(mono));
  });
  connect(ui->rbIconColorful, &QRadioButton::toggled, this, [this](bool colorful) {
    if (colorful)
      ui->comboTrayIcon->setCurrentIndex(ui->comboTrayIcon->findData(false));
  });
  const auto bindBehavior = [this](QComboBox *combo, QRadioButton *first, QRadioButton *second) {
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [combo, first, second](int index) {
      const bool value = combo->itemData(index).toBool();
      first->setChecked(!value);
      second->setChecked(value);
    });
    connect(first, &QRadioButton::toggled, combo, [combo](bool checked) {
      if (checked)
        combo->setCurrentIndex(combo->findData(false));
    });
    connect(second, &QRadioButton::toggled, combo, [combo](bool checked) {
      if (checked)
        combo->setCurrentIndex(combo->findData(true));
    });
  };
  bindBehavior(ui->comboStartBehavior, ui->rbShowOnStart, ui->rbAutoHide);
  bindBehavior(ui->comboCloseBehavior, ui->rbExitOnClose, ui->rbCloseToTray);
  loadFromConfig();
  ui->comboTrayIcon->setCurrentIndex(ui->comboTrayIcon->findData(ui->rbIconMono->isChecked()));
  logLevelChanged();

  adjustSize();

  setButtonBoxEnabledButtons();
  initConnections();

  // 设计稿：标签列固定 120px，控件紧贴标签，剩余空间放到最右列。
  for (auto *grid : findChildren<QGridLayout *>()) {
    if (grid->objectName() == QStringLiteral("logPathForm") ||
        grid->objectName() == QStringLiteral("certificateForm")) {
      grid->setColumnStretch(1, 1);
      continue;
    }
    if (grid->columnCount() != 2)
      continue;
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 0);
    for (int row = 0; row < grid->rowCount(); ++row) {
      if (grid->itemAtPosition(row, 2) == nullptr)
        grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 2);
    }
    grid->setColumnStretch(2, 1);
  }

#ifdef SYNERGY_EXTRA_HEADER
  synergy::hooks::onSettings(this);
#endif

  // Include hook-provided pages (Scope) without replacing their controls or connections.
  for (int index = 0; index < ui->tabWidget->count(); ++index) {
    auto *page = ui->tabWidget->widget(index);
    if (index >= 5) {
      if (auto *source = qobject_cast<QVBoxLayout *>(page->layout()); source != nullptr) {
        auto *card = new QGroupBox(page);
        card->setObjectName(QStringLiteral("scopeCard"));
        auto *contents = new QVBoxLayout(card);
        contents->setContentsMargins(12, 14, 12, 12);
        contents->setSpacing(8);
        while (auto *item = source->takeAt(0)) {
          if (item->spacerItem() != nullptr) {
            delete item;
          } else {
            contents->addItem(item);
            if (auto *widget = item->widget(); widget != nullptr) {
              widget->setParent(card);
              widget->show();
            }
          }
        }
        source->setContentsMargins(0, 0, 0, 0);
        source->addWidget(card);
      }
    }
    if (page->layout() != nullptr)
      page->layout()->setAlignment(Qt::AlignTop);
  }

  if (auto *scope = findChild<QComboBox *>(QStringLiteral("comboSettingsScope"))) {
    connect(
        scope, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::setButtonBoxEnabledButtons
    );
    scope->setFixedWidth(180);
    scope->setEnabled(Settings::isWritable());
    scope->setProperty("originalScopeIndex", scope->currentIndex());
  }
  const auto placeActions = [this] {
    auto *page = ui->tabWidget->currentWidget();
    QGroupBox *card = nullptr;
    for (auto *candidate : page->findChildren<QGroupBox *>()) {
      if (!candidate->isCheckable()) {
        card = candidate;
        break;
      }
    }
    if (!card)
      return;
    if (ui->buttonBox->parentWidget()->layout())
      ui->buttonBox->parentWidget()->layout()->removeWidget(ui->buttonBox);
    card->layout()->addWidget(ui->buttonBox);
    ui->buttonBox->show();
  };
  connect(ui->tabWidget, &QTabWidget::currentChanged, this, placeActions);
  placeActions();
  // Align independently nested form rows, and recompute when translated labels change.
  updateText();
}

void SettingsDialog::changeEvent(QEvent *e)
{
  QDialog::changeEvent(e);
  if (e->type() == QEvent::LanguageChange) {
    ui->retranslateUi(this);
    updateText();
  }
}

void SettingsDialog::initConnections()
{
  connect(this, &SettingsDialog::shown, this, &SettingsDialog::showReadOnlyMessage, Qt::QueuedConnection);

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui->buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &SettingsDialog::loadFromConfig);
  connect(
      ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
      &SettingsDialog::resetToDefault
  );

  connect(ui->cbRunEnterCommand, &QCheckBox::toggled, ui->lineCommandEnter, &QLineEdit::setEnabled);
  connect(ui->cbRunExitCommand, &QCheckBox::toggled, ui->lineCommandExit, &QLineEdit::setEnabled);

  connect(ui->groupSecurity, &QGroupBox::toggled, this, &SettingsDialog::updateTlsControlsEnabled);
  connect(ui->groupService, &QGroupBox::toggled, this, &SettingsDialog::updateControls);
  connect(ui->btnTlsRegenCert, &QPushButton::clicked, this, &SettingsDialog::regenCertificates);
  connect(ui->btnDismissTlsRegeneration, &QPushButton::clicked, this, [this] {
    ui->tlsRegenerationNotice->hide();
    ui->btnTlsRegenCert->setFocus();
  });
  connect(ui->btnViewFingerprint, &QPushButton::clicked, this, [this] {
    const auto savedPath = Settings::value(Settings::Security::Certificate).toString();
    const auto fingerprint = TlsUtility::certFingerprint(savedPath);
    if (fingerprint.size() != 32) {
      PageHost::present(
          this, new QMessageBox(
                    QMessageBox::Warning, tr("Certificate unavailable"),
                    tr("The saved certificate does not contain a valid SHA-256 fingerprint."), QMessageBox::Ok, this
                )
      );
      return;
    }
    PageHost::present(
        this, new FingerprintDialog(
                  this, {QCryptographicHash::Sha256, fingerprint}, FingerprintDialogMode::Local, {}, savedPath
              )
    );
  });
  connect(ui->btnTlsCertPath, &QPushButton::clicked, this, &SettingsDialog::browseCertificatePath);
  connect(ui->btnBrowseLog, &QPushButton::clicked, this, &SettingsDialog::browseLogPath);
  connect(ui->groupLogToFile, &QGroupBox::toggled, this, &SettingsDialog::setLogToFile);
  connect(
      ui->comboLogLevel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::logLevelChanged
  );
  const auto selectLanguage = [this](int row) {
    if (row < 0)
      return;
    setLanguageDraft(ui->comboLanguage->itemData(row).toString());
    setButtonBoxEnabledButtons();
  };
  connect(ui->comboLanguage, qOverload<int>(&QComboBox::currentIndexChanged), this, selectLanguage);
  // An explicit selection can pin the displayed system default without changing its index.
  connect(ui->comboLanguage, qOverload<int>(&QComboBox::activated), this, selectLanguage);

  // Connect modifiable controls
  connect(ui->rbIconMono, &QRadioButton::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->sbPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(
      ui->comboLogLevel, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      &SettingsDialog::setButtonBoxEnabledButtons
  );
  connect(
      ui->comboInterface, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      &SettingsDialog::setButtonBoxEnabledButtons
  );
  connect(
      ui->comboTlsKeyLength, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      &SettingsDialog::setButtonBoxEnabledButtons
  );
  connect(ui->rbAutoHide, &QRadioButton::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbPreventSleep, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->rbCloseToTray, &QRadioButton::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbElevateDaemon, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbGuiDebug, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbShowVersion, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbRequireClientCert, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupLogToFile, &QGroupBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupService, &QGroupBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupSecurity, &QGroupBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineLogFilename, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineTlsCertPath, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineTlsCertPath, &QLineEdit::textChanged, this, [this] { ui->lineTlsCertPath->setToolTip({}); });
  connect(ui->lineTlsCertPath, &QLineEdit::editingFinished, this, [this] {
    updateKeyLengthOnFile(ui->lineTlsCertPath->text());
  });
  connect(ui->cbRunEnterCommand, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbRunExitCommand, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineCommandEnter, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineCommandExit, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
}

void SettingsDialog::regenCertificates()
{
  ui->tlsRegenerationNotice->hide();
  const auto certificate = Settings::value(Settings::Security::Certificate).toString();
  const int keyLength = std::max(kMinimumRsaBits, ui->comboTlsKeyLength->currentText().toInt());
  if (TlsUtility::generateCertificateAt(certificate, keyLength)) {
    // Regeneration is an explicit file action. Commit only its matching metadata
    // so cancelling unrelated drafts cannot leave the saved certificate invalid.
    Settings::setValue(Settings::Security::KeySize, keyLength);
    if (ui->lineTlsCertPath->text() == certificate)
      updateKeyLengthOnFile(certificate);
    setButtonBoxEnabledButtons();
    ui->tlsRegenerationNotice->show();
    // Wait for PageHost to fit the resizable editor. On short screens its
    // outer viewport can scroll even when the Network tab itself fits.
    QTimer::singleShot(0, ui->tlsRegenerationNotice, [notice = ui->tlsRegenerationNotice] {
      if (!notice->isVisible())
        return;
      for (auto *parent = notice->parentWidget(); parent; parent = parent->parentWidget())
        if (auto *area = qobject_cast<QScrollArea *>(parent))
          area->ensureWidgetVisible(notice, 0, 0);
    });
  }
}

void SettingsDialog::browseCertificatePath()
{
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Select a TLS certificate to use..."), ui->lineTlsCertPath->text(), "Cert (*.pem)", nullptr,
      QFileDialog::DontConfirmOverwrite
  );

  if (!fileName.isEmpty()) {
    ui->lineTlsCertPath->setText(fileName);

    updateKeyLengthOnFile(fileName);
  }
}

void SettingsDialog::browseLogPath()
{
  QString fileName =
      QFileDialog::getSaveFileName(this, tr("Save log file to..."), ui->lineLogFilename->text(), "Logs (*.log *.txt)");

  if (!fileName.isEmpty()) {
    ui->lineLogFilename->setText(fileName);
  }
}

void SettingsDialog::setLogToFile(bool logToFile)
{
  ui->widgetLogFilename->setEnabled(logToFile);
}

void SettingsDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  Q_EMIT shown();
}

void SettingsDialog::showReadOnlyMessage()
{
  if (Settings::isWritable())
    return;
  if (property("readOnlyNoticeShown").toBool())
    return;
  setProperty("readOnlyNoticeShown", true);
  PageHost::present(
      this, new QMessageBox(
                QMessageBox::Information, tr("%1 Read-only settings").arg(kAppName),
                tr("<p>Settings are read-only because you only have read access to the file:</p><p>%1</p>")
                    .arg(QDir::toNativeSeparators(Settings::settingsFile()).toHtmlEscaped()),
                QMessageBox::Ok, this
            )
  );
}

void SettingsDialog::updateText()
{
  const auto logLevelNames = LogLevel::logLevelNames();
  const QStringList toolTips = {tr("Required messages"),        tr("Non-fatal errors"), tr("General warnings"),
                                tr("General events [Default]"), tr("Debug entries"),    tr("Verbose debug output")};
  if (ui->comboLogLevel->count() == 0) {
    const auto logLevelOptions = LogLevel::logLevelOptions();
    for (int i = 0; i < logLevelNames.count(); i++) {
      ui->comboLogLevel->addItem(logLevelNames.at(i));
      ui->comboLogLevel->setItemData(i, logLevelOptions.at(i), Qt::UserRole);
      ui->comboLogLevel->setItemData(i, toolTips.at(i), Qt::ToolTipRole);
    }
  } else {
    for (int i = 0; i < logLevelNames.count(); i++) {
      ui->comboLogLevel->setItemData(i, logLevelNames.at(i), Qt::DisplayRole);
      ui->comboLogLevel->setItemData(i, toolTips.at(i), Qt::ToolTipRole);
    }
  }
  ui->buttonBox->button(QDialogButtonBox::Save)->setToolTip(tr("Close and save changes"));
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setToolTip(tr("Close and forget changes"));
  ui->buttonBox->button(QDialogButtonBox::Reset)->setToolTip(tr("Reset to stored values"));
  ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setToolTip(tr("Reset to default values"));

  for (int index = 0; index < ui->tabWidget->count(); ++index) {
    auto *page = ui->tabWidget->widget(index);
    const auto forms = page->findChildren<QFormLayout *>();
    int fieldOffset = 0;
    const auto inset = [page](QFormLayout *form) {
      int result = 0;
      for (auto *parent = form->parentWidget(); parent != nullptr && parent != page; parent = parent->parentWidget()) {
        if (qobject_cast<QGroupBox *>(parent) != nullptr)
          result += parent->layout()->contentsMargins().left();
      }
      return result;
    };
    for (auto *form : forms) {
      for (int row = 0; row < form->rowCount(); ++row) {
        if (auto *item = form->itemAt(row, QFormLayout::LabelRole); item != nullptr && item->widget() != nullptr) {
          item->widget()->setMinimumWidth(0);
          fieldOffset = qMax(fieldOffset, inset(form) + item->widget()->sizeHint().width());
        }
      }
    }
    for (auto *form : forms) {
      for (int row = 0; row < form->rowCount(); ++row) {
        if (auto *item = form->itemAt(row, QFormLayout::LabelRole); item != nullptr && item->widget() != nullptr)
          item->widget()->setMinimumWidth(fieldOffset - inset(form));
      }
    }
  }
}

void SettingsDialog::accept()
{
  Settings::setValue(Settings::Core::Port, ui->sbPort->value());
  Settings::setValue(Settings::Core::Interface, ui->comboInterface->currentData());
  Settings::setValue(Settings::Log::Level, ui->comboLogLevel->currentData());
  Settings::setValue(Settings::Log::ToFile, ui->groupLogToFile->isChecked());
  Settings::setValue(Settings::Log::File, ui->lineLogFilename->text());
  Settings::setValue(Settings::Daemon::Elevate, ui->cbElevateDaemon->isChecked());
  Settings::setValue(Settings::Gui::Autohide, ui->rbAutoHide->isChecked());
  Settings::setValue(Settings::Core::PreventSleep, ui->cbPreventSleep->isChecked());
  Settings::setValue(Settings::Security::Certificate, ui->lineTlsCertPath->text());
  Settings::setValue(Settings::Security::KeySize, ui->comboTlsKeyLength->currentText().toInt());
  Settings::setValue(Settings::Security::TlsEnabled, ui->groupSecurity->isChecked());
  Settings::setValue(Settings::Gui::CloseToTray, ui->rbCloseToTray->isChecked());
  Settings::setValue(Settings::Gui::SymbolicTrayIcon, ui->rbIconMono->isChecked());
  Settings::setValue(Settings::Security::CheckPeers, ui->cbRequireClientCert->isChecked());
  Settings::setValue(Settings::Core::Language, m_languageDraft);
  Settings::setValue(Settings::Log::GuiDebug, ui->cbGuiDebug->isChecked());
  Settings::setValue(Settings::Gui::ShowVersionInTitle, ui->cbShowVersion->isChecked());
  if (m_checkForUpdates != nullptr) {
    Settings::setValue(Settings::Gui::CheckForUpdates, m_checkForUpdates->isChecked());
  }
  Settings::setValue(Settings::Core::EnableEnterCommand, ui->cbRunEnterCommand->isChecked());
  Settings::setValue(Settings::Core::EnableExitCommand, ui->cbRunExitCommand->isChecked());
  Settings::setValue(Settings::Core::ScreenEnterCommand, ui->lineCommandEnter->text());
  Settings::setValue(Settings::Core::ScreenExitCommand, ui->lineCommandExit->text());

  Settings::ProcessMode mode;
  if (ui->groupService->isChecked())
    mode = Settings::ProcessMode::Service;
  else
    mode = Settings::ProcessMode::Desktop;
  Settings::setValue(Settings::Core::ProcessMode, mode);

  QDialog::accept();
}

void SettingsDialog::loadFromConfig()
{
  setLanguageDraft(m_originalLanguageSetting);
  if (auto *scope = findChild<QComboBox *>(QStringLiteral("comboSettingsScope")))
    scope->setCurrentIndex(scope->property("originalScopeIndex").toInt());
  ui->sbPort->setValue(Settings::value(Settings::Core::Port).toInt());
  ui->comboLogLevel->setCurrentIndex(
      ui->comboLogLevel->findData(Settings::logLevelText(), Qt::UserRole, Qt::MatchFixedString)
  );
  ui->groupLogToFile->setChecked(Settings::value(Settings::Log::ToFile).toBool());
  ui->lineLogFilename->setText(Settings::value(Settings::Log::File).toString());
  ui->cbPreventSleep->setChecked(Settings::value(Settings::Core::PreventSleep).toBool());
  ui->cbElevateDaemon->setChecked(Settings::value(Settings::Daemon::Elevate).toBool());
  ui->cbGuiDebug->setChecked(Settings::value(Settings::Log::GuiDebug).toBool());
  ui->cbShowVersion->setChecked(Settings::value(Settings::Gui::ShowVersionInTitle).toBool());
  if (m_checkForUpdates != nullptr) {
    m_checkForUpdates->setChecked(Settings::value(Settings::Gui::CheckForUpdates).toBool());
  }
  ui->cbRunEnterCommand->setChecked(Settings::value(Settings::Core::EnableEnterCommand).toBool());
  ui->cbRunExitCommand->setChecked(Settings::value(Settings::Core::EnableExitCommand).toBool());
  ui->lineCommandEnter->setText(Settings::value(Settings::Core::ScreenEnterCommand).toString());
  ui->lineCommandExit->setText(Settings::value(Settings::Core::ScreenExitCommand).toString());

  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  ui->groupService->setChecked(processMode == Settings::ProcessMode::Service);

  if (!synergy::platform::isWindows())
    ui->groupService->setVisible(false);

  if (Settings::value(Settings::Gui::SymbolicTrayIcon).toBool())
    ui->rbIconMono->setChecked(true);
  else
    ui->rbIconColorful->setChecked(true);

  const auto autoHide = Settings::value(Settings::Gui::Autohide).toBool();
  ui->rbAutoHide->setChecked(autoHide);
  ui->rbShowOnStart->setChecked(!autoHide);

  const auto closeToTray = Settings::value(Settings::Gui::CloseToTray).toBool();
  ui->rbCloseToTray->setChecked(closeToTray);
  ui->rbExitOnClose->setChecked(!closeToTray);

  ui->lblDebugWarning->show();

  ui->comboInterface->setCurrentText(Settings::value(Settings::Core::Interface).toString());
  if (ui->comboInterface->currentIndex() <= 0) {
    ui->comboInterface->setCurrentIndex(0);
    m_interfaceSetOnLoad = false;
  } else {
    m_interfaceSetOnLoad = true;
  }

  qDebug() << "load from config done";

  updateTlsControls();
  updateControls();
  setButtonBoxEnabledButtons();
}

void SettingsDialog::updateTlsControls()
{
  const auto certificate = Settings::value(Settings::Security::Certificate).toString();
  ui->lineTlsCertPath->setText(certificate);
  updateKeyLengthOnFile(certificate);

  ui->comboTlsKeyLength->setCurrentText(Settings::value(Settings::Security::KeySize).toString());

  ui->cbRequireClientCert->setChecked(Settings::value(Settings::Security::CheckPeers).toBool());
  ui->groupSecurity->setChecked(TlsUtility::isEnabled());

  ui->groupSecurity->setEnabled(Settings::isWritable());

  updateTlsControlsEnabled();
}

void SettingsDialog::updateTlsControlsEnabled()
{
  const auto writable = Settings::isWritable();
  const auto tlsChecked = ui->groupSecurity->isChecked();

  auto enabled = writable && tlsChecked;
  ui->lblTlsKeyLength->setEnabled(enabled);
  ui->comboTlsKeyLength->setEnabled(enabled);
  ui->lblTlsCert->setEnabled(enabled);
  ui->widgetTlsCert->setEnabled(enabled);
  ui->btnTlsRegenCert->setEnabled(enabled);
  ui->cbRequireClientCert->setEnabled(enabled && !isClientMode());
  ui->cbTlsEnabled->setEnabled(writable);
}

bool SettingsDialog::isClientMode() const
{
  return Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Client;
}

void SettingsDialog::updateKeyLengthOnFile(const QString &path)
{
  const int length = QFile::exists(path) ? TlsUtility::getCertKeyLength(path) : 0;
  ui->lineTlsCertPath->setToolTip(length > 0 ? tr("Key length: %1 bits").arg(length) : QString());
}

void SettingsDialog::updateControls()
{
  const bool writable = Settings::isWritable();
  const bool serviceChecked = ui->groupService->isChecked();
  const bool logToFile = ui->groupLogToFile->isChecked();
  ui->cbLogToFile->setEnabled(writable);
  ui->cbServiceEnabled->setVisible(synergy::platform::isWindows() && !Settings::isPortableMode());
  ui->cbElevateDaemon->setVisible(synergy::platform::isWindows() && !Settings::isPortableMode());
  ui->cbServiceEnabled->setEnabled(writable);

  ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(writable);

  ui->sbPort->setEnabled(writable);
  ui->comboInterface->setEnabled(writable);
  ui->comboLogLevel->setEnabled(writable);
  ui->groupLogToFile->setEnabled(writable);
  ui->comboStartBehavior->setEnabled(writable);
  ui->comboCloseBehavior->setEnabled(writable);
  ui->rbAutoHide->setEnabled(writable);
  ui->rbShowOnStart->setEnabled(writable);
  ui->cbPreventSleep->setEnabled(writable);
  m_checkForUpdates->setEnabled(writable);
  ui->comboLanguage->setEnabled(writable);
  ui->comboTrayIcon->setEnabled(writable);
  ui->rbIconMono->setEnabled(writable);
  ui->rbIconColorful->setEnabled(writable);
  ui->cbShowVersion->setEnabled(writable);
  ui->cbGuiDebug->setEnabled(writable);
  ui->lineTlsCertPath->setEnabled(writable);
  ui->comboTlsKeyLength->setEnabled(writable);
  ui->rbCloseToTray->setEnabled(writable);
  ui->rbExitOnClose->setEnabled(writable);
  ui->cbRunEnterCommand->setEnabled(writable);
  ui->cbRunExitCommand->setEnabled(writable);
  ui->lineCommandEnter->setEnabled(writable && ui->cbRunEnterCommand->isChecked());
  ui->lineCommandExit->setEnabled(writable && ui->cbRunExitCommand->isChecked());

  // Portable mode only ever applies to Windows.
  // Daemon options should only be available on Windows when *not* in portable mode.
  if (!Settings::isPortableMode()) {
    ui->groupService->setEnabled(writable);
    ui->cbElevateDaemon->setEnabled(writable && serviceChecked);
  } else if (ui->groupService->isVisibleTo(ui->tabAdvanced)) {
    ui->groupService->setVisible(false);
  }

  const bool managedLogs = !synergy::runtime::isIsolatedRuntime() && synergy::usesManagedRuntimeLogs();
  ui->cbLogToFile->setEnabled(writable && !managedLogs);
  ui->lineLogFilename->setReadOnly(managedLogs);
  ui->btnBrowseLog->setVisible(!managedLogs);
  ui->widgetLogFilename->setEnabled(logToFile && (writable || managedLogs));

  updateTlsControlsEnabled();
}

void SettingsDialog::logLevelChanged()
{
  ui->lblDebugWarning->show();
}

bool SettingsDialog::isModified() const
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  const bool ignoreInterface = !m_interfaceSetOnLoad && (ui->comboInterface->currentIndex() == 0);

  bool modified =
      (ui->sbPort->value() != Settings::value(Settings::Core::Port).toInt()) ||
      (ui->comboLogLevel->currentData().toString().compare(Settings::logLevelText(), Qt::CaseInsensitive) != 0) ||
      (ui->groupLogToFile->isChecked() != Settings::value(Settings::Log::ToFile).toBool()) ||
      (ui->lineLogFilename->text() != Settings::value(Settings::Log::File).toString()) ||
      (ui->rbAutoHide->isChecked() != Settings::value(Settings::Gui::Autohide).toBool()) ||
      (ui->cbPreventSleep->isChecked() != Settings::value(Settings::Core::PreventSleep).toBool()) ||
      (ui->rbCloseToTray->isChecked() != Settings::value(Settings::Gui::CloseToTray).toBool()) ||
      (ui->cbElevateDaemon->isChecked() != Settings::value(Settings::Daemon::Elevate).toBool()) ||
      (ui->cbGuiDebug->isChecked() != Settings::value(Settings::Log::GuiDebug).toBool()) ||
      (ui->cbShowVersion->isChecked() != Settings::value(Settings::Gui::ShowVersionInTitle).toBool()) ||
      (ui->rbIconMono->isChecked() != Settings::value(Settings::Gui::SymbolicTrayIcon).toBool()) ||
      (ui->groupService->isChecked() != (processMode == Settings::ProcessMode::Service)) ||
      (ui->lineTlsCertPath->text() != Settings::value(Settings::Security::Certificate).toString()) ||
      (ui->comboTlsKeyLength->currentText() != Settings::value(Settings::Security::KeySize).toString()) ||
      (ui->groupSecurity->isChecked() != Settings::value(Settings::Security::TlsEnabled).toBool()) ||
      (ui->cbRequireClientCert->isChecked() != Settings::value(Settings::Security::CheckPeers).toBool()) ||
      (ui->cbRunEnterCommand->isChecked() != Settings::value(Settings::Core::EnableEnterCommand).toBool()) ||
      (ui->cbRunExitCommand->isChecked() != Settings::value(Settings::Core::EnableExitCommand).toBool()) ||
      (ui->lineCommandEnter->text() != Settings::value(Settings::Core::ScreenEnterCommand).toString()) ||
      (ui->lineCommandExit->text() != Settings::value(Settings::Core::ScreenExitCommand).toString()) ||
      (m_languageDraft != m_originalLanguageSetting);

  modified = modified || (m_checkForUpdates->isChecked() != Settings::value(Settings::Gui::CheckForUpdates).toBool());
  if (auto *scope = findChild<QComboBox *>(QStringLiteral("comboSettingsScope")))
    modified = modified || scope->currentIndex() != scope->property("originalScopeIndex").toInt();

  if (!ignoreInterface)
    modified = modified || ui->comboInterface->currentText() != Settings::value(Settings::Core::Interface).toString();
  return modified;
}

bool SettingsDialog::isDefault() const
{
  const auto processMode = Settings::defaultValue(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  const auto logLevelIndex =
      static_cast<int>(LogLevel::fromOption(Settings::defaultValue(Settings::Log::Level).toString()));

  return (
      (ui->sbPort->value() == Settings::defaultValue(Settings::Core::Port).toInt()) &&
      (ui->comboLogLevel->currentIndex() == logLevelIndex) &&
      (ui->groupLogToFile->isChecked() == Settings::defaultValue(Settings::Log::ToFile).toBool()) &&
      (ui->lineLogFilename->text() == Settings::defaultValue(Settings::Log::File).toString()) &&
      (ui->rbAutoHide->isChecked() == Settings::defaultValue(Settings::Gui::Autohide).toBool()) &&
      (ui->cbPreventSleep->isChecked() == Settings::defaultValue(Settings::Core::PreventSleep).toBool()) &&
      (ui->rbCloseToTray->isChecked() == Settings::defaultValue(Settings::Gui::CloseToTray).toBool()) &&
      (ui->cbElevateDaemon->isChecked() == Settings::defaultValue(Settings::Daemon::Elevate).toBool()) &&
      (ui->cbGuiDebug->isChecked() == Settings::defaultValue(Settings::Log::GuiDebug).toBool()) &&
      (ui->cbShowVersion->isChecked() == Settings::defaultValue(Settings::Gui::ShowVersionInTitle).toBool()) &&
      (m_checkForUpdates->isChecked() == Settings::defaultValue(Settings::Gui::CheckForUpdates).toBool()) &&
      (ui->rbIconMono->isChecked() == Settings::defaultValue(Settings::Gui::SymbolicTrayIcon).toBool()) &&
      (ui->groupService->isChecked() == (processMode == Settings::ProcessMode::Service)) &&
      (ui->comboInterface->currentIndex() == 0) &&
      (ui->lineTlsCertPath->text() == Settings::defaultValue(Settings::Security::Certificate).toString()) &&
      (ui->comboTlsKeyLength->currentText() == Settings::defaultValue(Settings::Security::KeySize).toString()) &&
      (ui->groupSecurity->isChecked() == Settings::defaultValue(Settings::Security::TlsEnabled).toBool()) &&
      (ui->cbRequireClientCert->isChecked() == Settings::defaultValue(Settings::Security::CheckPeers).toBool()) &&
      (ui->lineCommandEnter->text() == Settings::defaultValue(Settings::Core::ScreenEnterCommand).toString()) &&
      (ui->lineCommandExit->text() == Settings::defaultValue(Settings::Core::ScreenExitCommand).toString()) &&
      (ui->cbRunEnterCommand->isChecked() == Settings::defaultValue(Settings::Core::EnableEnterCommand).toBool()) &&
      (ui->cbRunExitCommand->isChecked() == Settings::defaultValue(Settings::Core::EnableExitCommand).toBool()) &&
      (m_languageDraft == Settings::defaultValue(Settings::Core::Language).toString())
  );
}

void SettingsDialog::resetToDefault()
{
  setLanguageDraft(Settings::defaultValue(Settings::Core::Language).toString());
  ui->sbPort->setValue(Settings::defaultValue(Settings::Core::Port).toInt());
  ui->comboLogLevel->setCurrentIndex(
      static_cast<int>(LogLevel::fromOption(Settings::defaultValue(Settings::Log::Level).toString()))
  );
  ui->groupLogToFile->setChecked(Settings::defaultValue(Settings::Log::ToFile).toBool());
  ui->lineLogFilename->setText(Settings::defaultValue(Settings::Log::File).toString());
  ui->cbPreventSleep->setChecked(Settings::defaultValue(Settings::Core::PreventSleep).toBool());
  ui->cbElevateDaemon->setChecked(Settings::defaultValue(Settings::Daemon::Elevate).toBool());
  ui->cbGuiDebug->setChecked(Settings::defaultValue(Settings::Log::GuiDebug).toBool());
  ui->cbShowVersion->setChecked(Settings::defaultValue(Settings::Gui::ShowVersionInTitle).toBool());
  m_checkForUpdates->setChecked(Settings::defaultValue(Settings::Gui::CheckForUpdates).toBool());
  ui->cbRunEnterCommand->setChecked(Settings::defaultValue(Settings::Core::EnableEnterCommand).toBool());
  ui->cbRunExitCommand->setChecked(Settings::defaultValue(Settings::Core::EnableExitCommand).toBool());
  ui->lineCommandEnter->setText(Settings::defaultValue(Settings::Core::ScreenEnterCommand).toString());
  ui->lineCommandExit->setText(Settings::defaultValue(Settings::Core::ScreenExitCommand).toString());

  const auto autoHide = Settings::defaultValue(Settings::Gui::Autohide).toBool();
  ui->rbAutoHide->setChecked(autoHide);
  ui->rbShowOnStart->setChecked(!autoHide);

  const auto closeToTray = Settings::defaultValue(Settings::Gui::CloseToTray).toBool();
  ui->rbCloseToTray->setChecked(closeToTray);
  ui->rbExitOnClose->setChecked(!closeToTray);

  const auto processMode = Settings::defaultValue(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  ui->groupService->setChecked(processMode == Settings::ProcessMode::Service);

  if (!synergy::platform::isWindows())
    ui->groupService->setVisible(false);

  if (Settings::defaultValue(Settings::Gui::SymbolicTrayIcon).toBool())
    ui->rbIconMono->setChecked(true);
  else
    ui->rbIconColorful->setChecked(true);

  ui->lblDebugWarning->show();

  ui->lineTlsCertPath->setText(Settings::defaultValue(Settings::Security::Certificate).toString());
  ui->comboTlsKeyLength->setCurrentText(Settings::defaultValue(Settings::Security::KeySize).toString());
  ui->groupSecurity->setChecked(Settings::defaultValue(Settings::Security::TlsEnabled).toBool());
  ui->cbRequireClientCert->setChecked(Settings::defaultValue(Settings::Security::CheckPeers).toBool());
  updateKeyLengthOnFile(ui->lineTlsCertPath->text());

  ui->comboInterface->setCurrentIndex(0);

  qDebug() << "reset to default values";
  updateControls();
  setButtonBoxEnabledButtons();
}

void SettingsDialog::setButtonBoxEnabledButtons() const
{
  const bool writable = Settings::isWritable();
  const bool modified = isModified();
  ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(writable && modified);
  ui->buttonBox->button(QDialogButtonBox::Reset)->setEnabled(writable && modified);
  ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setEnabled(writable && !isDefault());
}

void SettingsDialog::setLanguageDraft(const QString &setting)
{
  const auto language = I18N::resolveLanguage(setting);
  m_languageDraft = setting;
  const QSignalBlocker blocker(ui->comboLanguage);
  ui->comboLanguage->setCurrentIndex(ui->comboLanguage->findData(language));
  I18N::setLanguage(language, false);
}

void SettingsDialog::reject()
{
  I18N::setLanguage(m_originalLanguage, false);
  Settings::setValue(Settings::Core::Language, m_originalLanguageSetting);
  QDialog::reject();
}

SettingsDialog::~SettingsDialog() = default;
