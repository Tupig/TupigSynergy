/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AboutDialog.h"
#include "AboutDialogDesign.h"

#include "common/Constants.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"
#include "gui/StyleUtils.h"

#ifdef SYNERGY_EXTRA_HEADER
#include "synergy/hooks/gui_hook.h"
#endif

#include "widgets/PageHost.h"
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent), ui{std::make_unique<Ui::AboutDialog>()}
{
  ui->setupUi(this);

  setWindowTitle(windowTitle().arg(kAppName));
  ui->lblIcon->hide();
  ui->lblName->setText(QString::fromUtf8(kAppName));
  auto titleFont = ui->lblName->font();
  titleFont.setPixelSize(22);
  titleFont.setBold(true);
  ui->lblName->setFont(titleFont);

  const int px = (fontMetrics().height() * 6);
  const QSize pixmapSize(px, px);
  ui->lblIcon->setFixedSize(pixmapSize);

  ui->lblIcon->setPixmap(QPixmap(QIcon::fromTheme(kRevFqdnName).pixmap(QSize().scaled(pixmapSize, Qt::KeepAspectRatio)))
  );

  ui->btnCopyVersion->setIcon(synergy::gui::appIcon(QStringLiteral("edit-copy")));
  connect(ui->btnCopyVersion, &QPushButton::clicked, this, &AboutDialog::copyVersionText);

  ui->lblVersion->setText(kDisplayVersion);
  ui->lblDescription->setText(kAppDescription);
  ui->lblCopyright->setText(kCopyright);

  // Use non-breaking space in each awesome dev name so names are not split across lines.
  QStringList devsNbsp;
  for (const auto &dev : s_awesomeDevs) {
    QString withNbsp = dev;
    devsNbsp.append(withNbsp.replace(" ", QStringLiteral("&nbsp;")));
  }

  // 标题保留在 lblImportantDevs，贡献者名单放在下一行。
  ui->linkContributors->setTextFormat(Qt::RichText);
  ui->linkContributors->setText(devsNbsp.join(QStringLiteral(", ")));

  ui->btnOk->setDefault(true);
  // 嵌入页面宿主时 close() 不会发出 finished，必须使用 reject 才能返回上一页。
  connect(ui->btnOk, &QPushButton::clicked, this, &QDialog::reject);

#ifdef SYNERGY_EXTRA_HEADER
  synergy::hooks::onAbout(this);
#endif
  ui->program_name->setText(QStringLiteral("Synergy · Deskflow · TuPig Synergy"));
  ui->openExternalLinks->setText(QStringLiteral("<a href=\"%1\">%2</a> · <a href=\"%3\">%4</a>")
                                     .arg(
                                         QStringLiteral("https://github.com/Tupig/TupigSynergy"), tr("Project home"),
                                         QStringLiteral("https://github.com/Tupig/TupigSynergy/graphs/contributors"),
                                         tr("Contributors")
                                     ));
  // Keep every original label (including overlay-provided attribution and links)
  // as the authoritative notice content, but disclose the full text on demand.
  for (auto *widget : QList<QWidget *>{
           ui->lblDescription, ui->lblCopyright, ui->program_name, ui->openExternalLinks, ui->groupBox,
           ui->groupLicenses
       })
    widget->hide();
  auto *description = new QLabel(tr("Share one keyboard and mouse across computers."), ui->frameLogo);
  description->setProperty("muted", true);
  description->setWordWrap(true);
  static_cast<QVBoxLayout *>(ui->frameLogo->layout())->insertWidget(2, description);

  auto *projectRow = new QHBoxLayout;
  projectRow->setSpacing(8);
  ui->widget_2->layout()->removeWidget(ui->btnCopyVersion);
  ui->btnCopyVersion->setFlat(false);
  projectRow->addWidget(ui->btnCopyVersion);
  auto *project = new QPushButton(tr("Project home"), ui->frameLogo);
  project->setIcon(synergy::gui::appIcon(QStringLiteral("document-open")));
  auto *contributors = new QPushButton(tr("Contributors"), ui->frameLogo);
  contributors->setIcon(synergy::gui::appIcon(QStringLiteral("help-about")));
  projectRow->addWidget(project);
  projectRow->addWidget(contributors);
  projectRow->addStretch();
  static_cast<QVBoxLayout *>(ui->frameLogo->layout())->addLayout(projectRow);
  connect(project, &QPushButton::clicked, this, [] {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Tupig/TupigSynergy")));
  });
  connect(contributors, &QPushButton::clicked, this, [] {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Tupig/TupigSynergy/graphs/contributors")));
  });
  auto *noticesCard = new QGroupBox(ui->scrollAreaWidgetContents);
  noticesCard->setObjectName(QStringLiteral("aboutProjectCard"));
  noticesCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *noticesLayout = new QVBoxLayout(noticesCard);
  noticesLayout->setContentsMargins(12, 10, 12, 12);
  noticesLayout->setSpacing(8);
  auto *noticesHeading = new QLabel(tr("Third-party notices"), noticesCard);
  noticesHeading->setProperty("sectionHeading", true);
  noticesLayout->addWidget(noticesHeading);
  auto *licenseSummary = new QLabel(
      tr("GNU GPL Version 2 with the OpenSSL exception. Full copyright and third-party notices are available below."),
      noticesCard
  );
  licenseSummary->setProperty("muted", true);
  licenseSummary->setWordWrap(true);
  noticesLayout->addWidget(licenseSummary);
  auto *notices = new QPushButton(tr("Copyright and licenses"), noticesCard);
  notices->setObjectName(QStringLiteral("aboutNoticesButton"));
  notices->setIcon(synergy::gui::appIcon(QStringLiteral("help-about")));
  noticesLayout->addWidget(notices, 0, Qt::AlignLeft);
  ui->scrollAreaWidgetContents->layout()->addWidget(noticesCard);
  connect(notices, &QPushButton::clicked, this, [this] {
    auto *page = new QDialog(this);
    page->setWindowTitle(tr("Copyright and licenses"));
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(8);
    auto *text = new QTextBrowser(page);
    text->setObjectName(QStringLiteral("aboutNoticesText"));
    text->setOpenExternalLinks(true);
    text->setFrameShape(QFrame::NoFrame);
    text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text->document()->setDocumentMargin(12);
    const auto linkColor = synergy::gui::isDarkMode() ? QStringLiteral("#9bbfff") : QStringLiteral("#15549d");
    text->document()->setDefaultStyleSheet(QStringLiteral(R"(
          h2 { font-size: 15px; font-weight: 600; margin-top: 8px; margin-bottom: 4px; }
          p { font-size: 13px; font-weight: normal; line-height: 140%; margin-top: 0px; margin-bottom: 8px; }
          a { color: %1; }
        )")
                                               .arg(linkColor));
    const auto html = [](QLabel *label) {
      if (label->textFormat() != Qt::RichText)
        return label->text().toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
      // The overlay's fixed link colors belong to its labels. Let this
      // document use its own contrasting link color while keeping the URLs.
      static const QRegularExpression inlineColor(QStringLiteral(R"css( style="color: #[0-9a-fA-F]{6}")css"));
      return label->text().remove(inlineColor);
    };
    QString content;
    const auto addSection = [&content](const QString &heading, const QStringList &paragraphs) {
      content += QStringLiteral("<h2>%1</h2>").arg(heading.toHtmlEscaped());
      for (const auto &paragraph : paragraphs)
        content += QStringLiteral("<p>%1</p>").arg(paragraph);
    };
    addSection(
        tr("Project attribution"), {html(ui->lblDescription), html(ui->program_name), html(ui->openExternalLinks)}
    );
    addSection(tr("Copyright"), {html(ui->lblCopyright)});
    addSection(ui->lblImportantDevs->text(), {html(ui->linkContributors)});
    addSection(
        tr("Project license"), {tr("GNU GPL Version 2 with the OpenSSL exception.").toHtmlEscaped(), html(ui->linkGpl)}
    );
    QFile license(QStringLiteral(":/licenses/Lucide-Feather.txt"));
    if (license.open(QIODevice::ReadOnly | QIODevice::Text)) {
      auto paragraphs = QString::fromUtf8(license.readAll()).split(QStringLiteral("\n\n"), Qt::SkipEmptyParts);
      for (auto &paragraph : paragraphs)
        paragraph = paragraph.simplified().toHtmlEscaped();
      addSection(tr("Third-party notices"), paragraphs);
    }
    text->setHtml(content);
    layout->addWidget(text);
    auto *close = new QPushButton(tr("&Close"), page);
    close->setObjectName(QStringLiteral("aboutNoticesClose"));
    connect(close, &QPushButton::clicked, page, &QDialog::reject);
    layout->addWidget(close, 0, Qt::AlignRight);
    PageHost::present(this, page);
  });
}

void AboutDialog::copyVersionText() const
{
  // Divergence from upstream: kDisplayVersion already carries version + commit; don't append the git sha.
  QString infoString = QStringLiteral("%1: %2\nQt: %3\nSystem: %4")
                           .arg(kAppName, kDisplayVersion, qVersion(), QSysInfo::prettyProductName());
  if (Settings::isPortableMode()) {
    infoString.append(tr("\nPortable Mode"));
  }
#ifdef Q_OS_LINUX
  infoString.append(tr("\nSession: %1 (%2)")
                        .arg(qEnvironmentVariable("XDG_CURRENT_DESKTOP"), qEnvironmentVariable("XDG_SESSION_TYPE")));
#endif
  QGuiApplication::clipboard()->setText(infoString);
}

AboutDialog::~AboutDialog() = default;
