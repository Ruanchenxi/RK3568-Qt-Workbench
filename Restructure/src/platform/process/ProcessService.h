#ifndef PROCESSSERVICE_H
#define PROCESSSERVICE_H

#include <QList>
#include <QString>
#include <QStringList>

#include "shared/contracts/IProcessService.h"

class QProcess;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class ProcessService : public IProcessService
{
    Q_OBJECT
public:
    explicit ProcessService(QObject *parent = nullptr);
    ~ProcessService() override;

    void startAuto() override;
    void stopAll() override;
    bool isServiceReady() const override;
    QString serviceReadyMessage() const override;
    bool usesLocalService() const override;

private slots:
    void autoStartServices();
    void onProcessStarted();
    void onProcessReadyReadOutput();
    void onProcessReadyReadError();
    void onProcessFinished(int exitCode, int exitStatus);
    void onProcessError(int error);
    void onStartRetryTimeout();
    void onServiceReadinessTimeout();

private:
    struct ProcessOwner
    {
        QString pid;
        QString parentPid;
        QString name;
        QString executablePath;
        QString commandLine;
    };

    // This service owns only outage lifecycle orchestration; ticket workflows never call it.
    void appendLog(const QString &text);
    void setServiceReady(bool ready, const QString &message);
    bool resolveLocalMode(bool *local, QString *error = nullptr) const;
    void startLocalServices();
    bool prepareLocalRuntime();
    bool stopLocalOutage();
    bool stopWindowsOutage(bool includeDetectedRoots);
    bool stopLinuxOutage();
    bool runStopScript(const QString &scriptPath);
    bool waitForPortsReleased(const QList<int> &ports, int timeoutMs) const;
    bool checkPortInUse(int port) const;
    QList<ProcessOwner> queryWindowsPortOwners(int port) const;
    ProcessOwner queryWindowsProcess(const QString &pid) const;
    QList<ProcessOwner> queryWindowsOutageProcesses(const QString &outageRoot) const;
    QStringList windowsOutageListeningRootProcessIds(
        const QString &outageRoot) const;
    QString windowsOutageRootForProcess(const ProcessOwner &owner,
                                        const QString &configuredWorkDir) const;
    bool isWindowsOutageRoot(const QString &outageRoot,
                             const QString &configuredWorkDir) const;
    QStringList windowsOutageRootProcessIds(
        const QList<ProcessOwner> &owners) const;
    bool terminateWindowsOutageRoot(const QString &outageRoot);
    QString describeProcessOwner(const ProcessOwner &owner) const;
    QString resolveServiceWorkDir() const;
    QString resolveServiceScriptPath() const;
    QString resolveStopScriptPath() const;
    QList<int> resolveServicePorts() const;
    int readinessPort() const;
    int readinessTimeoutSeconds() const;
    bool startScriptProcess(const QString &scriptPath);
    void scheduleStartRetry(const QString &reason);
    void startReadinessCheck();
    // A live startup process remains recoverable after the initial deadline; probes continue slowly.
    void scheduleReadinessRetry(const QString &pendingReason);
    // Probes HTTP readiness asynchronously so the login UI never blocks.
    void probeHealthEndpoint();
    void probeLoginEndpoint();
    void finishReadinessProbe(bool ready, const QString &message);
    void configureProbeTimeout(QNetworkReply *reply);
    QString serviceStartupLogPath() const;
    void resetServiceStartupLog();
    void appendServiceStartupLogLine(const QString &text);
    void flushServiceStartupLog();
    void disposeScriptProcess();

    QProcess *m_process;
    QTimer *m_autoStartTimer;
    QTimer *m_startRetryTimer;
    QTimer *m_readinessTimer;
    QTimer *m_logFileFlushTimer;
    QNetworkAccessManager *m_networkManager;
    int m_startAttempt;
    int m_maxStartRetries;
    int m_readinessAttempt;
    int m_maxReadinessAttempts;
    bool m_startInProgress;
    bool m_stopping;
    bool m_shutdownCompleted;
    bool m_localMode;
    bool m_modeResolved;
    bool m_serviceReady;
    bool m_probeInFlight;
    bool m_readinessSlowPolling;
    int m_readinessGeneration; // Invalidates HTTP callbacks from an older start/stop lifecycle.
    QString m_serviceReadyMessage;
    QString m_startedOutageWorkDir;
    QStringList m_pendingFileLogs;
};

#endif // PROCESSSERVICE_H
