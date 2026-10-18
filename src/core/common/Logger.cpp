#include "common/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdio>

namespace shophub {

namespace {

QString g_logPath;
QMutex g_logMutex;
constexpr qint64 kMaxLogBytes = 2 * 1024 * 1024; // rotate at 2 MB

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "INFO";
}

void rotateIfNeeded()
{
    QFileInfo info(g_logPath);
    if (info.exists() && info.size() > kMaxLogBytes) {
        const QString old = g_logPath + QStringLiteral(".1");
        QFile::remove(old);
        QFile::rename(g_logPath, old);
    }
}

void handler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const QString line = QStringLiteral("%1 [%2] %3\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QString::fromLatin1(levelName(type)), msg);

    QMutexLocker lock(&g_logMutex);
    std::fputs(line.toLocal8Bit().constData(), stderr);

    if (!g_logPath.isEmpty()) {
        rotateIfNeeded();
        QFile file(g_logPath);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << line;
        }
    }
}

} // namespace

void Logger::install(const QString &logDirectory)
{
    QDir().mkpath(logDirectory);
    {
        QMutexLocker lock(&g_logMutex);
        g_logPath = QDir(logDirectory).filePath(QStringLiteral("shophub.log"));
    }
    qInstallMessageHandler(handler);
}

QString Logger::logFilePath()
{
    QMutexLocker lock(&g_logMutex);
    return g_logPath;
}

} // namespace shophub
