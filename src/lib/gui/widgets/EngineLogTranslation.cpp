/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "EngineLogTranslation.h"
#include "EngineLogFormats.h"

#include <QCoreApplication>
#include <QStringList>
#include <QVector>

namespace synergy::gui {
namespace {
constexpr int kMaximumMessageLength = 16384;
constexpr int kMaximumFormatLength = 2048;
constexpr int kMaximumFields = 16;
constexpr int kMatchBudget = 4096;

enum class ValueKind
{
  String,
  Character,
  Signed,
  Unsigned,
  Hexadecimal,
  Octal,
  Decimal
};

struct Field
{
  ValueKind kind;
  int width = 0;
  int precision = -1;
};

struct Format
{
  const char *source = nullptr;
  QStringList literals;
  QStringList markers;
  QVector<Field> fields;
};

bool asciiDigit(QChar character)
{
  return character >= QLatin1Char('0') && character <= QLatin1Char('9');
}

bool parseFormat(const QString &text, Format &format)
{
  if (text.size() > kMaximumFormatLength)
    return false;
  QString literal;
  for (qsizetype index = 0; index < text.size();) {
    if (text.at(index) != QLatin1Char('%')) {
      literal += text.at(index++);
      continue;
    }
    const qsizetype start = index++;
    if (index == text.size())
      return false;
    if (text.at(index) == QLatin1Char('%')) {
      literal += QLatin1Char('%');
      format.markers.append(QStringLiteral("%%"));
      ++index;
      continue;
    }
    int width = 0;
    while (index < text.size() && asciiDigit(text.at(index))) {
      width = width * 10 + text.at(index++).digitValue();
      if (width > kMaximumMessageLength)
        return false;
    }
    int precision = -1;
    if (index < text.size() && text.at(index) == QLatin1Char('.')) {
      ++index;
      precision = 0;
      if (index == text.size() || !asciiDigit(text.at(index)))
        return false;
      while (index < text.size() && asciiDigit(text.at(index))) {
        precision = precision * 10 + text.at(index++).digitValue();
        if (precision > 16)
          return false;
      }
    }
    QString length;
    if (index < text.size() && QStringLiteral("lz").contains(text.at(index))) {
      length += text.at(index++);
      if (length == QStringLiteral("l") && index < text.size() && text.at(index) == QLatin1Char('l'))
        length += text.at(index++);
    }
    if (index == text.size())
      return false;
    const QChar conversion = text.at(index++);
    ValueKind kind;
    if (conversion == QLatin1Char('s') && (length.isEmpty() || length == QStringLiteral("l")))
      kind = ValueKind::String;
    else if (conversion == QLatin1Char('c') && length.isEmpty())
      kind = ValueKind::Character;
    else if (conversion == QLatin1Char('d') || conversion == QLatin1Char('i'))
      kind = ValueKind::Signed;
    else if (conversion == QLatin1Char('u'))
      kind = ValueKind::Unsigned;
    else if (conversion == QLatin1Char('x') || conversion == QLatin1Char('X'))
      kind = ValueKind::Hexadecimal;
    else if (conversion == QLatin1Char('o'))
      kind = ValueKind::Octal;
    else if (conversion == QLatin1Char('f') && length.isEmpty())
      kind = ValueKind::Decimal;
    else
      return false;
    if ((kind != ValueKind::Decimal && precision != -1) ||
        ((kind == ValueKind::String || kind == ValueKind::Character) && width != 0))
      return false;
    if (format.fields.size() == kMaximumFields)
      return false;
    format.literals.append(literal);
    literal.clear();
    const auto marker = text.mid(start, index - start);
    format.markers.append(marker);
    format.fields.append({kind, width, precision});
  }
  format.literals.append(literal);
  return true;
}

bool validValue(const QString &value, const Field &field)
{
  if (field.kind == ValueKind::String)
    return true;
  if (field.kind == ValueKind::Character)
    return value.size() == 1 && !value.at(0).isSurrogate();
  if (value.isEmpty() || value.size() < field.width)
    return false;
  qsizetype index = 0;
  if ((field.kind == ValueKind::Signed || field.kind == ValueKind::Decimal) && value.at(0) == QLatin1Char('-'))
    ++index;
  if (index == value.size())
    return false;
  if (field.kind == ValueKind::Decimal) {
    const qsizetype integerStart = index;
    while (index < value.size() && asciiDigit(value.at(index)))
      ++index;
    if (integerStart == index)
      return false;
    const int precision = field.precision < 0 ? 6 : field.precision;
    if (precision == 0)
      return index == value.size();
    if (index == value.size() || value.at(index++) != QLatin1Char('.'))
      return false;
    const qsizetype fractionStart = index;
    while (index < value.size() && asciiDigit(value.at(index)))
      ++index;
    return index == value.size() && index - fractionStart == precision;
  }
  for (; index < value.size(); ++index) {
    const auto character = value.at(index);
    if (asciiDigit(character)) {
      if (field.kind == ValueKind::Octal && character > QLatin1Char('7'))
        return false;
      continue;
    }
    if (field.kind != ValueKind::Hexadecimal || !((character >= QLatin1Char('a') && character <= QLatin1Char('f')) ||
                                                  (character >= QLatin1Char('A') && character <= QLatin1Char('F'))))
      return false;
  }
  return true;
}

// At most two complete partitions are needed to prove ambiguity. The shared
// budget bounds delimiter searches across the entire catalogue, without regex.
void captureValues(
    const Format &format, const QString &message, qsizetype fieldIndex, qsizetype position, QStringList &current,
    QStringList &values, int &solutions, int &budget
)
{
  if (solutions > 1 || budget <= 0)
    return;
  const auto &field = format.fields.at(fieldIndex);
  const auto &suffix = format.literals.at(fieldIndex + 1);
  const bool last = fieldIndex + 1 == format.fields.size();
  qsizetype end = position;
  while (end <= message.size() && budget > 0 && solutions < 2) {
    --budget;
    if (last) {
      end = message.size() - suffix.size();
      if (end < position || !message.endsWith(suffix))
        break;
    } else if (suffix.isEmpty()) {
      if (field.kind != ValueKind::Character || position == message.size())
        break;
      end = position + 1;
    } else {
      end = message.indexOf(suffix, end);
      if (end < 0)
        break;
    }
    const auto value = message.mid(position, end - position);
    if (validValue(value, field)) {
      current.append(value);
      if (last) {
        ++solutions;
        if (solutions == 1)
          values = current;
      } else {
        captureValues(format, message, fieldIndex + 1, end + suffix.size(), current, values, solutions, budget);
      }
      current.removeLast();
    }
    if (last || suffix.isEmpty())
      break;
    ++end;
  }
}

const QVector<Format> &sourceFormats()
{
  static const QVector<Format> formats = [] {
    QVector<Format> result;
    for (const auto *source : kEngineLogFormats) {
      Format format;
      format.source = source;
      // LogWidget already strips record-boundary whitespace from display text.
      if (!parseFormat(QString::fromUtf8(source).trimmed(), format) || format.fields.isEmpty())
        continue;
      // Bare values or punctuation-only wrappers cannot identify an engine log.
      bool hasContext = false;
      for (const auto &literal : format.literals)
        for (const auto character : literal)
          hasContext = hasContext || character.isLetter();
      if (!hasContext)
        continue;
      result.append(format);
    }
    return result;
  }();
  return formats;
}
} // namespace

QString translateEngineLogMessage(const QString &message)
{
  if (message.size() > kMaximumMessageLength || message.contains(QChar::Null))
    return message;
  const auto fixed = QCoreApplication::translate("EngineLogMessages", qUtf8Printable(message));
  if (fixed != message)
    return fixed;

  const Format *matched = nullptr;
  QStringList values;
  int budget = kMatchBudget;
  for (const auto &format : sourceFormats()) {
    const auto &prefix = format.literals.first();
    if (!message.startsWith(prefix) || !message.endsWith(format.literals.last()))
      continue;
    QStringList current;
    QStringList candidate;
    int solutions = 0;
    captureValues(format, message, 0, prefix.size(), current, candidate, solutions, budget);
    if (budget <= 0 || solutions > 1 || (solutions == 1 && matched))
      return message;
    if (solutions == 1) {
      matched = &format;
      values = candidate;
    }
  }
  if (!matched)
    return message;
  const auto translated = QCoreApplication::translate("EngineLogFormats", matched->source);
  if (translated == QString::fromUtf8(matched->source))
    return message;
  Format display;
  if (!parseFormat(translated.trimmed(), display) || display.markers != matched->markers)
    return message;
  QString result = display.literals.first();
  for (qsizetype index = 0; index < display.fields.size(); ++index) {
    result += values.at(index);
    result += display.literals.at(index + 1);
  }
  return result;
}
} // namespace synergy::gui
