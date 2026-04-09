/**
 * @file KeyManageController.h
 * @brief 钥匙管理页控制器（业务编排层）
 *
 * 职责边界：
 * 1. 接收 UI 动作并驱动会话服务。
 * 2. 管理串口日志数据层（SerialLogManager）。
 * 3. 输出可直接渲染的状态给页面，避免页面直接依赖协议细节。
 */
#ifndef KEYMANAGECONTROLLER_H
#define KEYMANAGECONTROLLER_H

#include <QObject>
#include <QList>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>

#include "shared/contracts/IKeySessionService.h"
#include "shared/contracts/KeyTaskDto.h"
#include "shared/contracts/SystemTicketDto.h"
#include "features/key/application/SerialLogManager.h"

class QNetworkAccessManager;
class TicketStore;
class TicketIngressService;
class TicketReturnHttpClient;
class KeyProvisioningService;
class TicketCancelCoordinator;

class KeyManageController : public QObject
{
    Q_OBJECT
public:
    explicit KeyManageController(IKeySessionService *session = nullptr,
                                 SerialLogManager *logManager = nullptr,
                                 QObject *parent = nullptr);
    ~KeyManageController() override = default;

    void start();

    void onInitClicked();
    void onDownloadRfidClicked();
    void onQueryBatteryClicked();
    void onSyncDeviceTimeClicked();
    void onQueryTasksClicked();
    void onDeleteClicked();
    void onTransferSelectedTicket();
    void onReturnSelectedTicket();
    void onGetSystemTicketListClicked();
    void onSystemTicketSelected(int row);
    void onKeyTaskSelected(int row);

    void onExpertModeChanged(bool enabled);
    void onShowHexChanged(bool enabled);
    void onClearLogsClicked();
    void onClearHttpClientLogsClicked();
    void onClearHttpServerLogsClicked();
    void requestApplyConfiguredKeyPort();
    bool exportLogs(const QString &path, QString *error) const;

    QList<LogItem> visibleLogs() const;
    bool shouldDisplay(const LogItem &item) const;
    KeySessionSnapshot currentSnapshot() const;
    QList<SystemTicketDto> systemTickets() const;
    QString httpServerLogText() const;
    QString httpClientLogText() const;
    QVariantMap keySerialApplyStatus() const;
    bool canStartManualReturn(const QString &taskId, QString *blockedReason = nullptr) const;

signals:
    void statusMessage(const QString &message);
    void sessionSnapshotChanged(const KeySessionSnapshot &snapshot);
    void tasksUpdated(const QList<KeyTaskDto> &tasks);
    void systemTicketsUpdated(const QList<SystemTicketDto> &tickets);
    void selectedSystemTicketChanged(const SystemTicketDto &ticket);
    void httpServerLogAppended(const QString &text);
    void httpClientLogAppended(const QString &text);
    void keySerialApplyStatusChanged(const QVariantMap &status);
    void ackReceived(quint8 ackedCmd);
    void nakReceived(quint8 origCmd, quint8 errCode);
    void timeoutOccurred(const QString &what);
    void logRowAppended(const LogItem &item);
    void logTableRefreshRequested();
    void logsCleared();
    void workbenchRefreshRequested();

private slots:
    void handleSessionEvent(const KeySessionEvent &event);
    void handleLogItem(const LogItem &item);
    void handleSystemTicketsChanged(const QList<SystemTicketDto> &tickets);
    void handleJsonReceived(const QByteArray &jsonBytes, const QString &savedPath);
    void handleHttpServerLog(const QString &text);
    void handleHttpClientLog(const QString &text);
    void handleInitPayloadReady(const QByteArray &payload);
    void handleRfidPayloadReady(const QByteArray &payload);
    void handleProvisionRequestFailed(const QString &reason);
    void handleReturnUploadSucceeded(const QString &taskId);
    void handleReturnUploadFailed(const QString &taskId, const QString &reason);

private:
    bool isCurrentKeySession(int sessionId) const;
    void openNewKeySession();
    void invalidateCurrentKeySession(const QString &reason = QString());
    void scheduleAutoTransferRetry(const QString &reason = QString());
    bool ingestWorkbenchJson(const QByteArray &jsonBytes,
                             const QString &savedPath,
                             QString *taskId = nullptr,
                             QString *resultMessage = nullptr,
                             bool scheduleAutoTransfer = true);
    void tryAutoConnectKeyPort();
    bool autoTransferEnabled() const;
    void tryAutoRefreshKeyTasksOnReady(const KeySessionSnapshot &snapshot);
    void reconcileSystemTicketsFromKeyTasks(const QList<KeyTaskDto> &tasks);
    void recoverAutoTransferWhenKeyEmpty(const QList<KeyTaskDto> &tasks);
    void tryAutoCleanupReturnedTicket(const QList<KeyTaskDto> &tasks);
    bool tryStartTicketTransfer(const QString &taskId,
                                bool automatic,
                                bool failInsteadOfDeferring = false,
                                QString *blockedReason = nullptr);
    void tryAutoTransferPendingTicket();
    void tryStartTicketReturn(const QString &taskId, bool automatic);
    void tryAutoReturnCompletedTicket(const QList<KeyTaskDto> &tasks);
    bool isKeyPortApplyBusy(QString *reason = nullptr) const;
    void refreshKeySerialApplyStatus(const QString &overrideState = QString(),
                                     const QString &overrideMessage = QString());
    bool hasPendingApplySensitiveBusiness() const;
    void failActiveReturnHandshake(const QString &reason);
    void clearActiveReturnContext(bool clearHandshakeOnly = false);
    void markReturnDeletePending(const QString &taskId, const QByteArray &taskIdRaw);
    void finalizePendingReturnDelete(const QList<KeyTaskDto> &tasks);
    bool retryPendingReturnDelete(const QString &reason);
    void callTermination(const QString &taskId);
    bool canProcessCancelNow(QString *blockedReason = nullptr) const;
    void schedulePendingCancelReconcile(const QString &reason = QString(), int delayMs = 300);
    void tryProcessPendingCancelTasks(const QList<KeyTaskDto> &tasks);
    void finalizePendingCancelDelete(const QList<KeyTaskDto> &tasks);
    bool canStartTicketReturn(const QString &taskId,
                              bool automatic,
                              QByteArray *taskIdRaw = nullptr,
                              QString *blockedReason = nullptr) const;
    QString activeReturnChainTaskId() const;
    QByteArray findKeyTaskIdRaw(const QString &taskId, quint8 *status = nullptr) const;
    static QString taskIdFromRaw(const QByteArray &taskIdRaw);
    bool taskExistsInList(const QList<KeyTaskDto> &tasks, const QString &taskId) const;
    bool hasBlockingIncompleteKeyTask(const QList<KeyTaskDto> &tasks,
                                      QString *blockingTaskId = nullptr,
                                      const QByteArray &excludeTaskIdRaw = QByteArray()) const;
    bool startDirectWorkbenchTransfer(const QString &taskId,
                                      const QByteArray &jsonBytes,
                                      int expectedSessionId,
                                      int expectedBridgeEpoch,
                                      QString *blockedReason = nullptr);
    // 设置 HTTP 传票互斥锁，同时启动 60 秒安全超时防止死锁
    void setHttpTransferTransaction(const QString &taskId, int sessionId, int bridgeEpoch);
    // 释放 HTTP 传票互斥锁并停止安全计时器（clearActiveTransfer=false 表示不联动清除 m_activeTransferTaskId）
    void clearHttpTransferTransaction(bool clearActiveTransfer = true);
    bool isActiveHttpTransferTransactionFor(int sessionId, int bridgeEpoch) const;
    int nextOpId();
    QList<KeyTaskDto> toTaskDtos(const QVariantList &taskList) const;

    IKeySessionService *m_session;
    SerialLogManager *m_logManager;
    TicketStore *m_ticketStore;
    TicketIngressService *m_ticketIngress;
    TicketReturnHttpClient *m_ticketReturnClient;
    KeyProvisioningService *m_keyProvisioning;
    TicketCancelCoordinator *m_ticketCancelCoordinator;
    QNetworkAccessManager *m_terminationNetwork;
    int m_nextOpId;
    QString m_selectedSystemTicketId;
    QByteArray m_selectedKeyTaskRaw;
    QString m_activeTransferTaskId;
    QString m_pendingTransferHandshakeTaskId;
    QByteArray m_pendingTransferHandshakeJsonBytes;
    int m_pendingTransferHandshakeDebugChunk = 0;
    QString m_activeReturnTaskId;
    QByteArray m_activeReturnTaskIdRaw;
    QString m_pendingReturnHandshakeTaskId;
    QByteArray m_pendingReturnHandshakeTaskIdRaw;
    QString m_pendingDeletedSystemTicketId;
    QByteArray m_pendingDeletedKeyTaskRaw;
    bool m_pendingDeleteAllowsRetransfer = false;
    int m_pendingDeleteRetryCount = 0;
    QString m_pendingCancelSystemTicketId;
    QByteArray m_pendingCancelKeyTaskRaw;
    int m_keySessionId = 0;
    int m_keySessionSeed = 0;
    int m_activeTransferSessionId = 0;
    int m_activeReturnSessionId = 0;
    enum class DeleteOrigin { None, ManualDelete, AutoReturnCleanup };
    DeleteOrigin m_pendingDeleteOrigin = DeleteOrigin::None;
    int m_pendingDeleteSessionId = 0;
    int m_pendingCancelSessionId = 0;
    int m_autoQuerySessionId = 0;
    bool m_httpTransferTransactionActive = false;
    QString m_httpTransferTransactionTaskId;
    int m_httpTransferTransactionSessionId = 0;
    int m_httpTransferTransactionBridgeEpoch = 0;
    // 安全超时：防止互斥锁因极端异常永久未释放（60 秒强制清锁）
    QTimer *m_httpTransactionSafetyTimer = nullptr;
    bool m_autoTransferRetryScheduled = false;
    bool m_cancelReconcileScheduled = false;
    bool m_keyPortApplyInProgress = false;
    bool m_suppressSessionAutomation = false;
    int m_keyPortApplyEpoch = 0;
    int m_acceptedBridgeEpoch = 0;
    QString m_keySerialApplyState = QStringLiteral("applied");
    QString m_keySerialApplyMessage;
    QString m_keyPortApplyTargetPort;
    QList<KeyTaskDto> m_lastKeyTasks;
    bool m_lastReadyState = false;
    QStringList m_httpClientLogLines;
    QStringList m_httpServerLogLines;
};

#endif // KEYMANAGECONTROLLER_H
