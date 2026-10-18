#pragma once

#include "common/Result.h"

#include <QString>

namespace shophub::validation {

// Lower-cases and trims an email address so lookups are consistent.
QString normalizeEmail(const QString &email);

bool isValidEmail(const QString &email);

// At least 8 characters with a letter and a digit, max 128.
Status checkPassword(const QString &password);

// Non-empty after trimming, at most maxLength characters.
Status checkRequired(const QString &value, const QString &fieldName, int maxLength);

// Accepts digits with optional +, spaces and dashes; 10-15 digits in total.
bool isValidPhone(const QString &phone);

// Upper-case letters, digits and dashes, 3-32 characters.
bool isValidSku(const QString &sku);

// Escapes %, _ and \ so user text is matched literally inside LIKE ... ESCAPE '\'.
QString escapeLike(const QString &text);

} // namespace shophub::validation
