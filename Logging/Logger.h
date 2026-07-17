#pragma once

#include <QString>
#include <QtMessageHandler>

class Logger final
{
public:
    static Logger &instance();

    bool initialize();
    void shutdown();
    QString logFilePath() const;

    void write(QtMsgType type, const char *category, const QString &message,
               const char *file, const char *function, int line);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    static void messageHandler(QtMsgType type, const QMessageLogContext &context,
                               const QString &message);
    void rotateIfNeeded(qint64 incomingBytes);
};

#define PA_LOG_DEBUG(category, message) \
    Logger::instance().write(QtDebugMsg, category, (message), __FILE__, Q_FUNC_INFO, __LINE__)
#define PA_LOG_INFO(category, message) \
    Logger::instance().write(QtInfoMsg, category, (message), __FILE__, Q_FUNC_INFO, __LINE__)
#define PA_LOG_WARNING(category, message) \
    Logger::instance().write(QtWarningMsg, category, (message), __FILE__, Q_FUNC_INFO, __LINE__)
#define PA_LOG_ERROR(category, message) \
    Logger::instance().write(QtCriticalMsg, category, (message), __FILE__, Q_FUNC_INFO, __LINE__)

