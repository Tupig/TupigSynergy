/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "I18N.h"

#include "common/Constants.h"
#include "common/Settings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QLocale>
#include <QMap>
#include <QObject>
#include <QTranslator>

#include <utility>

namespace {
const auto kEnglish = QStringLiteral("en");
const auto kSimplifiedChinese = QStringLiteral("zh_CN");
const QMap<QString, QString> kLanguages{
    {kEnglish, QStringLiteral("English")}, {kSimplifiedChinese, QStringLiteral("简体中文")}
};
} // namespace

I18N *I18N::instance()
{
  static I18N m;
  return &m;
}

I18N::I18N(QObject *parent) : QObject{parent}
{
  const auto appDir = QCoreApplication::applicationDirPath();
  const auto homeDir = QDir::homePath();

  const QList<QDir> appTrDirs{
      {QStringLiteral("%1/%2").arg(appDir, QStringLiteral("translations"))},
      {QStringLiteral("%1/../translations").arg(appDir)},
      {QStringLiteral("%1/../Resources/translations").arg(appDir)},
      {QStringLiteral("%1/../share/%2/translations").arg(appDir, kAppId)},
      {QStringLiteral("%1/.local/share/%2/translations").arg(homeDir, kAppId)},
      {QStringLiteral("/usr/local/share/%1/translations").arg(kAppId)},
      {QStringLiteral("/usr/share/%1/translations").arg(kAppId)}
  };
  const QStringList appTrFilter{QStringLiteral("%1*.qm").arg(kUpstreamId)};

  for (const auto &dir : appTrDirs) {
    if (!dir.entryList(appTrFilter, QDir::Files, QDir::Name).isEmpty()) {
      m_appTrPath = dir.absolutePath();
      break;
    }
  }

  if (m_appTrPath.isEmpty()) {
    qInfo() << "no app translations found";
  }

  const auto qt = QStringLiteral("qt");
  const auto qt6 = QStringLiteral("qt6");

  const QList<QDir> qtTrDirs{
      {QStringLiteral("%1/%2").arg(appDir, QStringLiteral("translations"))},
      {QStringLiteral("%1/../Resources/translations").arg(appDir)},
      {QStringLiteral("%1/../qt-depends/translations").arg(appDir)},
      {QStringLiteral("%1/../share/%2/translations").arg(appDir, qt6)},
      {QStringLiteral("%1/../share/%2/translations").arg(appDir, qt)},
      {QStringLiteral("%1/.local/share/%2/translations").arg(homeDir, qt6)},
      {QStringLiteral("%1/.local/share/%2/translations").arg(homeDir, qt)},
      {QStringLiteral("/usr/local/share/%2/translations").arg(qt6)},
      {QStringLiteral("/usr/local/share/%2/translations").arg(qt)},
      {QStringLiteral("/usr/share/%2/translations").arg(qt6)},
      {QStringLiteral("/usr/share/%2/translations").arg(qt)}
  };
  const QStringList qtTrFilter{QStringLiteral("qt_*.qm")};

  for (const auto &dir : qtTrDirs) {
    if (!dir.entryList(qtTrFilter, QDir::Files, QDir::Name).isEmpty()) {
      m_qtTrPath = dir.absolutePath();
      break;
    }
  }

  if (m_qtTrPath.isEmpty()) {
    qInfo() << "no qt translations found";
  }

  detectLanguages();
  applyLanguage(resolveLanguage(Settings::value(Settings::Core::Language).toString()));
}

QStringList I18N::detectedLanguages()
{
  return kLanguages.values();
}

QString I18N::nativeTo639Name(QString nativeName)
{
  return kLanguages.key(nativeName);
}

QString I18N::toNativeName(QString shortName)
{
  return kLanguages.value(shortName);
}

QString I18N::currentLanguage()
{
  return instance()->m_currentLang;
}

QString I18N::resolveLanguage(const QString &configured)
{
  const auto locale = configured.isEmpty() ? QLocale() : QLocale(configured);
  return locale.language() == QLocale::Chinese ? kSimplifiedChinese : kEnglish;
}

void I18N::setLanguage(const QString &langName, bool savePreference)
{
  auto *localization = instance();
  if (!kLanguages.contains(langName))
    return;

  if (savePreference)
    Settings::setValue(Settings::Core::Language, langName);
  if (langName == localization->m_currentLang)
    return;

  localization->applyLanguage(langName);
  Q_EMIT localization->languageChanged(langName);
}

void I18N::applyLanguage(const QString &langName)
{
  m_currentLang = langName;
  for (const auto &translation : std::as_const(m_currentTranslations))
    QCoreApplication::removeTranslator(translation);
  qDeleteAll(m_currentTranslations);
  m_currentTranslations.clear();
  const auto translations = m_translations.value(langName);
  for (const auto &translation : translations) {
    auto translator = new QTranslator(this);
    if (translator->load(translation)) {
      m_currentTranslations.append(translator);
      QCoreApplication::installTranslator(translator);
    } else {
      delete translator;
    }
  }
}

void I18N::reDetectLanguages()
{
  instance()->detectLanguages();
}

void I18N::detectLanguages()
{
  const auto oldList = m_translations;
  m_translations.clear();

  for (const auto &lang : kLanguages.keys()) {
    QStringList translations;
    const auto addTranslation = [&translations, &lang](const QString &directory, const QString &prefix) {
      if (directory.isEmpty())
        return;
      const auto path = QDir(directory).filePath(QStringLiteral("%1_%2.qm").arg(prefix, lang));
      if (QFileInfo::exists(path))
        translations.append(path);
    };
    addTranslation(m_appTrPath, kUpstreamId);
    addTranslation(m_qtTrPath, QStringLiteral("qt"));
    if (!translations.isEmpty())
      m_translations.insert(lang, translations);
  }

  if (oldList != m_translations)
    Q_EMIT languagesChanged(m_translations.keys());
}
