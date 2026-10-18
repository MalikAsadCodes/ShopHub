#pragma once

#include <QString>

namespace shophub {

// Routes qDebug/qInfo/qWarning/qCritical to a rotating log file and stderr.
// Never log passwords, password hashes, or full addresses.
class Logger
{
public:
    // Creates logDirectory if needed and writes to logDirectory/shophub.log.
    static void install(const QString &logDirectory);
    static QString logFilePath();
};

} // namespace shophub
