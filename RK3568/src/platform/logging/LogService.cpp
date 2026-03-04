/**
 * @file LogService.cpp
 * @brief 日志服务实现
 */

#include "platform/logging/LogService.h"
#include "core/AppContext.h"
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

LogService *LogService::s_instance = nullptr;

LogService::LogService(QObject *parent)
    : QObject(parent), m_minLevel(LogLevel::Info), m_consoleOutput(true), m_fileOutput(true), m_logFile(nullptr), m_logStream(nullptr), m_maxFileSize(10 * 1024 * 1024) // 10MB
{
    // 默认日志路径
    QString appDir = QCoreApplication::applicationDirPath();
    m_logFilePath = appDir + "/logs/app.log";

    // 确保日志目录存在
    QDir dir(appDir);
    if (!dir.exists("logs"))
    {
        dir.mkdir("logs");
    }

    // 打开日志文件
    if (m_fileOutput)
    {
        m_logFile = new QFile(m_logFilePath);
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            m_logStream = new QTextStream(m_logFile);
            m_logStream->setCodec("UTF-8");
        }
    }
}

LogService::~LogService()
{
    if (m_logStream)
    {
        m_logStream->flush();
        delete m_logStream;
    }
    if (m_logFile)
    {
        m_logFile->close();
        delete m_logFile;
    }
}

LogService *LogService::instance()
{
    if (!s_instance)
    {
        s_instance = new LogService();
    }
    return s_instance;
}

void LogService::debug(const QString &module, const QString &message)
{
    log(LogLevel::Debug, module, message);
}

void LogService::info(const QString &module, const QString &message)
{
    log(LogLevel::Info, module, message);
}

void LogService::warning(const QString &module, const QString &message)
{
    log(LogLevel::Warning, module, message);
}

void LogService::error(const QString &module, const QString &message)
{
    log(LogLevel::Error, module, message);
}

void LogService::fatal(const QString &module, const QString &message)
{
    log(LogLevel::Fatal, module, message);
}

void LogService::audit(const QString &operation, const QString &target,
                       bool result, const QString &details)
{
    QString userId = AppContext::instance()->currentUser().userId;
    QString username = AppContext::instance()->currentUser().username;

    QString message = QString("操作=%1, 目标=%2, 结果=%3, 用户=%4(%5)")
                          .arg(operation)
                          .arg(target)
                          .arg(result ? "成功" : "失败")
                          .arg(username)
                          .arg(userId);

    if (!details.isEmpty())
    {
        message += ", 详情=" + details;
    }

    log(LogLevel::Info, "AUDIT", message);
}

void LogService::log(LogLevel level, const QString &module, const QString &message)
{
    // 检查日志级别
    if (level < m_minLevel)
    {
        return;
    }

    QMutexLocker locker(&m_mutex);

    // 创建日志条目
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.module = module;
    entry.message = message;
    entry.userId = AppContext::instance()->currentUser().userId;

    // 格式化日志
    QString formattedLog = QString("[%1] [%2] [%3] %4")
                               .arg(entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                               .arg(levelToString(level))
                               .arg(module)
                               .arg(message);

    // 控制台输出
    if (m_consoleOutput)
    {
        switch (level)
        {
        case LogLevel::Debug:
            qDebug().noquote() << formattedLog;
            break;
        case LogLevel::Info:
            qInfo().noquote() << formattedLog;
            break;
        case LogLevel::Warning:
            qWarning().noquote() << formattedLog;
            break;
        case LogLevel::Error:
        case LogLevel::Fatal:
            qCritical().noquote() << formattedLog;
            break;
        }
    }

    // 文件输出
    if (m_fileOutput && m_logStream)
    {
        writeToFile(entry);
    }

    // 发送信号（用于UI实时显示）
    emit logAdded(entry);
}

void LogService::writeToFile(const LogEntry &entry)
{
    if (!m_logStream)
        return;

    // 检查文件大小，需要时轮转
    if (m_logFile && m_logFile->size() > m_maxFileSize)
    {
        rotateLogFile();
    }

    QString line = QString("[%1] [%2] [%3] [%4] %5\n")
                       .arg(entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                       .arg(levelToString(entry.level))
                       .arg(entry.module)
                       .arg(entry.userId.isEmpty() ? "-" : entry.userId)
                       .arg(entry.message);

    *m_logStream << line;
    m_logStream->flush();
}

QString LogService::levelToString(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO ";
    case LogLevel::Warning:
        return "WARN ";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    default:
        return "UNKN ";
    }
}

void LogService::rotateLogFile()
{
    if (!m_logFile)
        return;

    // 关闭当前文件
    m_logStream->flush();
    delete m_logStream;
    m_logFile->close();

    // 重命名为带时间戳的备份文件
    QString backupPath = m_logFilePath + "." +
                         QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QFile::rename(m_logFilePath, backupPath);

    // 重新打开新文件
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        m_logStream = new QTextStream(m_logFile);
        m_logStream->setCodec("UTF-8");
    }
}

void LogService::setMinLevel(LogLevel level)
{
    m_minLevel = level;
}

void LogService::setLogFilePath(const QString &path)
{
    QMutexLocker locker(&m_mutex);

    // 关闭旧文件
    if (m_logStream)
    {
        m_logStream->flush();
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFile)
    {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }

    // 打开新文件
    m_logFilePath = path;

    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    m_logFile = new QFile(m_logFilePath);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        m_logStream = new QTextStream(m_logFile);
        m_logStream->setCodec("UTF-8");
    }
}

void LogService::setConsoleOutput(bool enabled)
{
    m_consoleOutput = enabled;
}

void LogService::setFileOutput(bool enabled)
{
    m_fileOutput = enabled;
}
