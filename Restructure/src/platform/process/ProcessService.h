/**
 * @file ProcessService.h
 * @brief 服务进程治理实现（启动脚本、端口检查、外部日志跟踪）
 */
#ifndef PROCESSSERVICE_H
#define PROCESSSERVICE_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "shared/contracts/IProcessService.h"

class QProcess;
class QTimer;

class ProcessService : public IProcessService
{
    Q_OBJECT
public:
    explicit ProcessService(QObject *parent = nullptr);
    ~ProcessService() override;

    void startAuto() override;
    void stopAll() override;

private slots:
    void autoStartServices();
    void onProcessStarted();
    void onProcessReadyReadOutput();
    void onProcessReadyReadError();
    void onProcessFinished(int exitCode, int exitStatus);
    void onProcessError(int error);
    void onStartRetryTimeout();
    void pollExternalLogFiles();

private:
    void appendLog(const QString &text);
    void startServices(bool forceRestart = false);
    bool checkPortInUse(int port) const;
    void killProcessByPort(int port);
    QString queryProcessNameByPid(const QString &pid) const;
    QStringList resolveKillProcessWhitelist() const;
    bool checkAndCleanPorts();
    QString resolveServiceWorkDir() const;
    QString resolveServiceScriptPath() const;
    QList<int> resolveServicePorts() const;
    QList<int> resolveRemoteRequiredPorts() const;
    bool isAutoCleanPortsEnabled() const;
    bool startScriptProcess(const QString &scriptPath);
    QString serviceStartMode() const;
    bool checkRemoteServiceReady(QString *detail) const;
    void startExternalLogTailing();
    void stopExternalLogTailing();
    QStringList resolveExternalLogFiles() const;
    void scheduleStartRetry(const QString &reason);
    QString serviceStartupLogPath() const;
    void appendServiceStartupLogLine(const QString &text) const;

    QProcess *m_process;
    QTimer *m_autoStartTimer;
    QTimer *m_startRetryTimer;
    QTimer *m_externalLogTimer;
    QStringList m_externalLogFiles;
    QHash<QString, qint64> m_externalLogOffsets;
    int m_startAttempt;
    int m_maxStartRetries;
    bool m_startInProgress;
    bool m_externalLogTailingStarted;
};

#endif // PROCESSSERVICE_H
