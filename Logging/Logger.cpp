#include "Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <cstdlib>

namespace {
QMutex g_mutex;
QFile g_file;
QString g_path;
QtMessageHandler g_previousHandler = nullptr;
constexpr qint64 MaxLogBytes = 10 * 1024 * 1024;
constexpr int MaxLogFiles = 5;

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "UNKNOWN";
}

QString clean(QString value)
{
    value.replace('\r', "\\r");
    value.replace('\n', "\\n");
    return value;
}

QString baseName(const char *file)
{
    return file ? QFileInfo(QString::fromUtf8(file)).fileName() : QStringLiteral("-");
}

QString className(const char *function)
{
    if (!function) return QStringLiteral("-");
    QString signature = QString::fromUtf8(function);
    const int paren = signature.indexOf('(');
    if (paren >= 0) signature.truncate(paren);
    const int scope = signature.lastIndexOf("::");
    if (scope < 0) return QStringLiteral("-");
    QString prefix = signature.left(scope).trimmed();
    const int space = prefix.lastIndexOf(' ');
    return space >= 0 ? prefix.mid(space + 1) : prefix;
}
}

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

bool Logger::initialize()
{
    QMutexLocker locker(&g_mutex);
    if (g_file.isOpen()) return true;

    const QString directory = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/logs");
    if (!QDir().mkpath(directory)) return false;

    g_path = directory + QStringLiteral("/PA.log");
    g_file.setFileName(g_path);
    if (!g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return false;
    g_previousHandler = qInstallMessageHandler(&Logger::messageHandler);
    return true;
}

void Logger::shutdown()
{
    qInstallMessageHandler(g_previousHandler);
    QMutexLocker locker(&g_mutex);
    if (g_file.isOpen()) {
        g_file.flush();
        g_file.close();
    }
}

QString Logger::logFilePath() const
{
    QMutexLocker locker(&g_mutex);
    return g_path;
}

void Logger::rotateIfNeeded(qint64 incomingBytes)
{
    if (!g_file.isOpen() || g_file.size() + incomingBytes <= MaxLogBytes) return;
    g_file.flush();
    g_file.close();
    QFile::remove(g_path + QStringLiteral(".%1").arg(MaxLogFiles));
    for (int i = MaxLogFiles - 1; i >= 1; --i) {
        const QString from = g_path + QStringLiteral(".%1").arg(i);
        const QString to = g_path + QStringLiteral(".%1").arg(i + 1);
        if (QFile::exists(from)) QFile::rename(from, to);
    }
    QFile::rename(g_path, g_path + QStringLiteral(".1"));
    g_file.setFileName(g_path);
    g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void Logger::write(QtMsgType type, const char *category, const QString &message,
                   const char *file, const char *function, int line)
{
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const quintptr threadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
    const QString record = QStringLiteral(
        "%1 | %-5s | pid=%2 tid=0x%3 | category=%4 | file=%5 class=%6 function=%7 line=%8 | %9\n")
        .arg(timestamp)
        .arg(QCoreApplication::applicationPid())
        .arg(threadId, 0, 16)
        .arg(QString::fromLatin1(category ? category : "default"))
        .arg(baseName(file))
        .arg(className(function))
        .arg(function ? QString::fromUtf8(function) : QStringLiteral("-"))
        .arg(line)
        .arg(clean(message));
    QByteArray bytes = record.toUtf8();
    bytes.replace("%-5s", QByteArray(levelName(type)).leftJustified(5, ' '));

    QMutexLocker locker(&g_mutex);
    if (!g_file.isOpen()) return;
    rotateIfNeeded(bytes.size());
    g_file.write(bytes);
    g_file.flush();
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &context,
                            const QString &message)
{
    instance().write(type, context.category, message,
                     context.file, context.function, context.line);
    if (type == QtFatalMsg) abort();
}
