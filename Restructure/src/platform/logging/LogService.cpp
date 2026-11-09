/**
 * @file LogService.cpp
 * @brief 日志服务实现
 */

#include "platform/logging/LogService.h"
#include "core/AppContext.h"
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QCoreApplication>
#include <QDebug>

namespace
{
const int kCurrentRunLineLimit = 10000;
const qint64 kNormalArchiveLimit = 100LL * 1024 * 1024;
const qint64 kIncidentArchiveLimit = 20LL * 1024 * 1024;

// Retention is enforced only at startup/rotation so logging never adds a polling loop.
void cleanupLogBackups(const QString &logFilePath)
{
    const QFileInfo logInfo(logFilePath);
    QDir dir = logInfo.dir();
    if (!dir.exists()) return;
    const bool incident = logInfo.fileName() == QStringLiteral("incident.log");
    const int retentionDays = incident ? 90 : 30;
    const qint64 totalLimit = incident ? kIncidentArchiveLimit : kNormalArchiveLimit;
    const QString pattern = logInfo.fileName() + QStringLiteral(".*");
    QFileInfoList backups = dir.entryInfoList(QStringList() << pattern, QDir::Files, QDir::Time);
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-retentionDays);
    qint64 total = 0;
    for (const QFileInfo &backup : backups) {
        if (backup.lastModified() < cutoff) QFile::remove(backup.absoluteFilePath());
        else total += backup.size();
    }
    backups = dir.entryInfoList(QStringList() << pattern, QDir::Files, QDir::Time);
    for (int i = backups.size() - 1; i >= 0 && total > totalLimit; --i) {
        total -= backups.at(i).size();
        QFile::remove(backups.at(i).absoluteFilePath());
    }
}
}
LogService *LogService::s_instance = nullptr;

LogService::LogService(QObject *parent)
    : QObject(parent), m_minLevel(LogLevel::Info), m_consoleOutput(true), m_fileOutput(true), m_logFile(nullptr), m_logStream(nullptr), m_maxFileSize(10 * 1024 * 1024), m_logDate(QDate::currentDate()) // 10MB
{
    // 默认日志路径
    QString appDir = QCoreApplication::applicationDirPath();
    m_logFilePath = appDir + "/logs/app.log";
    const QFileInfo existingLog(m_logFilePath);
    if (existingLog.exists()) {
        m_logDate = existingLog.lastModified().date();
    }

    // 确保日志目录存在
    QDir dir(appDir);
    if (!dir.exists("logs"))
    {
        dir.mkdir("logs");
    }

    // 打开日志文件
    if (m_fileOutput)
    {
        cleanupLogBackups(m_logFilePath);
        cleanupLogBackups(QFileInfo(m_logFilePath).dir().filePath(QStringLiteral("incident.log")));

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

void LogService::incident(const QString &module, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = LogLevel::Error;
    entry.module = module;
    entry.message = message;
    entry.userId = AppContext::instance()->currentUser().userId;

    const QString normalPath = m_logFilePath;
    const QString incidentPath = QFileInfo(normalPath).dir().filePath(QStringLiteral("incident.log"));
    QFileInfo incidentInfo(incidentPath);
    if (incidentInfo.exists()
            && (incidentInfo.size() >= m_maxFileSize
                || incidentInfo.lastModified().date() != entry.timestamp.date())) {
        const QString archive = incidentPath + QLatin1Char('.')
                + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
        QFile::rename(incidentPath, archive);
        cleanupLogBackups(incidentPath);
    }
    QFile file(incidentPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << QStringLiteral("[%1] [INCIDENT] [%2] [%3] %4\n")
                     .arg(entry.timestamp.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")))
                     .arg(module)
                     .arg(entry.userId.isEmpty() ? QStringLiteral("-") : entry.userId)
                     .arg(message);
        stream.flush();
    }
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

    QString exportLine = QStringLiteral("[%1] [%2] [%3] [%4] %5")
            .arg(entry.timestamp.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")))
            .arg(levelToString(entry.level))
            .arg(entry.module)
            .arg(entry.userId.isEmpty() ? QStringLiteral("-") : entry.userId)
            .arg(entry.message);
    if (m_currentRunLines.size() >= kCurrentRunLineLimit)
        m_currentRunLines.removeFirst();
    m_currentRunLines.append(exportLine);

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
    if (m_logFile && (m_logFile->size() > m_maxFileSize || m_logDate != entry.timestamp.date()))
    {
        rotateLogFile();
        if (!m_logStream)
        {
            return;
        }
    }

    QString line = QString("[%1] [%2] [%3] [%4] %5\n")
                       .arg(entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                       .arg(levelToString(entry.level))
                       .arg(entry.module)
                       .arg(entry.userId.isEmpty() ? "-" : entry.userId)
                       .arg(entry.message);

    m_logDate = entry.timestamp.date();
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
    if (!m_logFile || !m_logStream)
        return;

    // 关闭当前文件
    m_logStream->flush();
    delete m_logStream;
    m_logStream = nullptr;
    m_logFile->close();

    // 重命名为带时间戳的备份文件
    QString backupPath = m_logFilePath + "." +
                         QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    if (!QFile::rename(m_logFilePath, backupPath))
    {
        qWarning() << "Failed to rotate log file:" << m_logFilePath;
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            m_logStream = new QTextStream(m_logFile);
            m_logStream->setCodec("UTF-8");
        }
        return;
    }

    cleanupLogBackups(m_logFilePath);

    // 重新打开新文件
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        m_logStream = new QTextStream(m_logFile);
        m_logStream->setCodec("UTF-8");
    }
    else
    {
        qWarning() << "Failed to reopen log file:" << m_logFilePath;
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

    cleanupLogBackups(m_logFilePath);

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

QString LogService::currentRunLogText() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentRunLines.join(QLatin1Char('\n'));
}

QStringList LogService::currentRunLogLines() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentRunLines;
}
