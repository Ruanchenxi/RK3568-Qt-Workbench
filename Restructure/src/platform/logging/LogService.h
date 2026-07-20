/**
 * @file LogService.h
 * @brief 日志服务 - 统一的日志记录管理
 * @author RK3568项目组
 * @date 2026-02-01
 */

#ifndef LOGSERVICE_H
#define LOGSERVICE_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

/**
 * @brief 日志级别枚举
 */
enum class LogLevel
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4
};

/**
 * @brief 日志记录结构体
 */
struct LogEntry
{
    QDateTime timestamp;
    LogLevel level;
    QString module;
    QString message;
    QString userId;
    QString details;
};

/**
 * @class LogService
 * @brief 日志服务单例类
 *
 * 提供统一的日志记录功能：
 * - 多级别日志（Debug, Info, Warning, Error, Fatal）
 * - 文件日志
 * - 操作审计日志
 * - 日志查询
 */
class LogService : public QObject
{
    Q_OBJECT

private:
    explicit LogService(QObject *parent = nullptr);
    ~LogService();

    LogService(const LogService &) = delete;
    LogService &operator=(const LogService &) = delete;

public:
    static LogService *instance();

    // ========== 日志记录方法 ==========

    void debug(const QString &module, const QString &message);
    void info(const QString &module, const QString &message);
    void warning(const QString &module, const QString &message);
    void error(const QString &module, const QString &message);
    void fatal(const QString &module, const QString &message);

    /**
     * @brief 记录操作审计日志（用于安全审计）
     * @param operation 操作类型
     * @param target 操作目标
     * @param result 操作结果
     * @param details 详细信息
     */
    void audit(const QString &operation, const QString &target,
               bool result, const QString &details = QString());

    // ========== 配置方法 ==========

    /**
     * @brief 设置最低日志级别
     */
    void setMinLevel(LogLevel level);

    /**
     * @brief 设置日志文件路径
     */
    void setLogFilePath(const QString &path);

    /**
     * @brief 启用/禁用控制台输出
     */
    void setConsoleOutput(bool enabled);

    /**
     * @brief 启用/禁用文件输出
     */
    void setFileOutput(bool enabled);

signals:
    /**
     * @brief 新日志记录信号（用于界面实时显示）
     */
    void logAdded(const LogEntry &entry);

private:
    void log(LogLevel level, const QString &module, const QString &message);
    void writeToFile(const LogEntry &entry);
    QString levelToString(LogLevel level) const;
    void rotateLogFile();

    static LogService *s_instance;

    LogLevel m_minLevel;
    bool m_consoleOutput;
    bool m_fileOutput;
    QString m_logFilePath;
    QFile *m_logFile;
    QTextStream *m_logStream;
    QMutex m_mutex;
    qint64 m_maxFileSize; // 最大文件大小（字节）
};

// ========== 便捷宏定义 ==========
#define LOG_DEBUG(module, msg) LogService::instance()->debug(module, msg)
#define LOG_INFO(module, msg) LogService::instance()->info(module, msg)
#define LOG_WARNING(module, msg) LogService::instance()->warning(module, msg)
#define LOG_ERROR(module, msg) LogService::instance()->error(module, msg)
#define LOG_FATAL(module, msg) LogService::instance()->fatal(module, msg)
#define LOG_AUDIT(op, target, result, details) LogService::instance()->audit(op, target, result, details)

#endif // LOGSERVICE_H
