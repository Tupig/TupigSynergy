/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QMap>
#include <QObject>

class QTranslator;
/**
 * @brief The I18N singleton class handles detection and loading of translation files
 */
class I18N : public QObject
{
  Q_OBJECT

public:
  static I18N *instance();
  /**
   * @brief detectedLanguages
   * @return The supported UI language names.
   */
  static QStringList detectedLanguages();

  /**
   * @brief nativeTo639Name Convert a native Language name into a 639 name
   * @param nativeName English or 简体中文.
   * @return The corresponding settings value, en or zh_CN.
   */
  static QString nativeTo639Name(QString nativeName);

  /**
   * @brief toNativeName Convert a 639 Name into a Native Language string
   * @param shortName en or zh_CN.
   * @return  native language string for the language
   */
  static QString toNativeName(QString shortName);

  /**
   * @brief currentLanguage
   * @return The current UI language, en or zh_CN.
   */
  static QString currentLanguage();

  /// Resolve the stored preference, using the system locale when it is empty.
  static QString resolveLanguage(const QString &configured);

  /// Apply en or zh_CN. A preview leaves the stored preference untouched.
  static void setLanguage(const QString &langName, bool savePreference = true);

  /**
   * @brief detectLanguages Detect new language files
   */
  static void reDetectLanguages();

Q_SIGNALS:
  /**
   * @brief languageChanged Emitted when the current language changes
   * @param language The current language, en or zh_CN.
   */
  void languageChanged(const QString language);
  /**
   * @brief languagesChanged Emitted when the detected languages changes
   * @param languages The languages with detected catalogue files.
   */
  void languagesChanged(const QStringList languages);

private:
  explicit I18N(QObject *parent = nullptr);

  I18N *operator=(I18N &other) = delete;
  I18N(const I18N &other) = delete;
  ~I18N() override = default;
  void applyLanguage(const QString &langName);
  void detectLanguages();

  QMap<QString, QStringList> m_translations;
  QList<QTranslator *> m_currentTranslations;
  QString m_currentLang = QStringLiteral("en");
  QString m_appTrPath;
  QString m_qtTrPath;
};
