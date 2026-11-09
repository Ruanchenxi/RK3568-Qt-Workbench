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
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QTimer>
#include <QVariantList>

#include "shared/contracts/IKeySessionService.h"
#include "shared/contracts/KeyTaskDto.h"
#include "shared/contracts/KeyTaskDisplayDto.h"
#include "shared/contracts/SystemTicketDto.h"
#include "features/key/application/SerialLogManager.h"
#include "features/key/application/TicketIngressService.h"
#include "features/key/application/TicketDeleteUseCase.h"
#include "features/key/application/KeyManualActionUseCase.h"
#include "features/key/application/KeyManageEventUseCase.h"

class TicketStore;
class TicketReturnUploadService;
class AdminTicketTerminationService;
class KeyProvisioningService;
class TicketCancelWorkflowService;
class TicketDeleteWorkflowService;
class TicketTransferWorkflowService;
class TicketReturnWorkflowService;
class CaptureTicketWorkflowService;
class CaptureTicketHttpClient;
struct CaptureTicketWorkflowEffect;
struct TicketTransferWorkflowEffect;
struct TicketReturnWorkflowEffect;
struct TicketReturnStartRequest;
class KeyMaintenanceService;
class KeyPortRuntimeCoordinator;
class KeyTaskObservationService;
class KeyManageObservabilityService;
class KeyManageSelectionService;

// Lists every service required by the key-management application module.
// Only the application composition root may assemble concrete implementations.
struct KeyManageDependencies
{
    IKeySessionService *session = nullptr;
    SerialLogManager *logManager = nullptr;
    TicketStore *ticketStore = nullptr;
    TicketTransferWorkflowService *ticketTransferWorkflow = nullptr;
    TicketReturnWorkflowService *ticketReturnWorkflow = nullptr;
    TicketIngressService *ticketIngress = nullptr;
    TicketReturnUploadService *returnUploadService = nullptr;
    KeyProvisioningService *keyProvisioning = nullptr;
    TicketCancelWorkflowService *ticketCancelWorkflow = nullptr;
    TicketDeleteWorkflowService *ticketDeleteWorkflow = nullptr;
    AdminTicketTerminationService *adminTerminationService = nullptr;
    KeyMaintenanceService *maintenanceService = nullptr;
    KeyPortRuntimeCoordinator *keyPortRuntime = nullptr;
    KeyTaskObservationService *keyTaskObservation = nullptr;
    KeyManageObservabilityService *observability = nullptr;
    KeyManageSelectionService *selection = nullptr;
    CaptureTicketWorkflowService *captureWorkflow = nullptr;
    CaptureTicketHttpClient *captureHttpClient = nullptr;

    bool isComplete() const;
};

class KeyManageController : public QObject
{
    Q_OBJECT
public:
    explicit KeyManageController(const KeyManageDependencies &dependencies,
                                 QObject *parent = nullptr);
    ~KeyManageController() override = default;

    void start();

    void onInitClicked();
    void onDownloadRfidClicked();
    void onQueryBatteryClicked();
    void onSyncDeviceTimeClicked();
    void onQueryTasksClicked();
    void onCaptureTicketClicked(int stationNo);
    void onDeleteClicked();
    void onTransferSelectedTicket();
    void onReturnSelectedTicket();
    void onSystemTicketSelected(int row);
    void onKeyTaskSelected(int row);

    void onExpertModeChanged(bool enabled);
    void onShowHexChanged(bool enabled);
    void onClearLogsClicked();
    void onClearHttpClientLogsClicked();
    void onClearHttpServerLogsClicked();
    // Captures bounded log snapshots immediately and writes them off the GUI thread.
    bool exportLogsAsync(const QString &path);

    QList<LogItem> visibleLogs() const;
    bool shouldDisplay(const LogItem &item) const;
    QString serialCommandText(quint8 cmd) const;
    KeySessionSnapshot currentSnapshot() const;
    QList<SystemTicketDto> systemTickets() const;
    // Supplies business display semantics without exposing protocol bytes to UI.
    QList<KeyTaskDisplayDto> keyTaskDisplayRows(
            const QList<KeyTaskDto> &tasks) const;
    QString httpServerLogText() const;
    QString httpClientLogText() const;
    bool canStartManualReturn(const QString &taskId, QString *blockedReason = nullptr) const;
    void onClearUnregisteredKeyTaskClicked();
    bool isSelectedKeyTaskUnregistered() const;
    // Exposes only the selected task's display category so UI can present the
    // stronger capture-delete confirmation without parsing protocol fields.
    bool isSelectedKeyTaskCapture() const;

signals:
    void statusMessage(const QString &message);
    void sessionSnapshotChanged(const KeySessionSnapshot &snapshot);
    void tasksUpdated(const QList<KeyTaskDto> &tasks);
    void systemTicketsUpdated(const QList<SystemTicketDto> &tickets);
    void selectedSystemTicketChanged(const SystemTicketDto &ticket);
    void httpServerLogAppended(const QString &text);
    void httpClientLogAppended(const QString &text);
    void ackReceived(quint8 ackedCmd);
    void nakReceived(quint8 origCmd, quint8 errCode);
    void timeoutOccurred(const QString &what);
    /// Reports a typed Q_TASK timeout so the page never parses protocol text.
    void taskQueryTimedOut();
    void logRowAppended(const LogItem &item);
    void logTableRefreshRequested();
    void logsCleared();
    void logExportFinished(bool success, const QString &result);
    void workbenchRefreshRequested();

private slots:
    void handleSessionEvent(const KeySessionEvent &event);
    void handleLogItem(const LogItem &item);
    void handleSystemTicketsChanged(const QList<SystemTicketDto> &tickets);
    void handleHttpServerLog(const QString &text);
    void handleHttpClientLog(const QString &text);
    void handleInitPayloadReady(const QByteArray &payload);
    void handleRfidPayloadReady(const QByteArray &payload);
    void handleProvisionRequestFailed(const QString &reason);
    void handleReturnUploadSucceeded(const QString &taskId,
                                     int keySessionId);
    void handleReturnUploadFailed(const QString &taskId,
                                  int keySessionId,
                                  const QString &reason);
    void handleCancelDeferredRequestExpired(const QString &taskId);
    void handleCaptureClaimSucceeded(int operationId, const QJsonObject &data);
    void handleCaptureClaimFailed(int operationId, const QString &reason);
    void handleCaptureFinishSucceeded(int operationId, const QString &taskId);
    void handleCaptureFinishFailed(int operationId, const QString &taskId,
                                   const QString &reason);

private:
    /// Handles one workbench transfer request after HTTP parsing.
    TicketIngressService::IngressResult handleTicketIngress(
            const QByteArray &jsonBytes);
    /// Handles one workbench termination request after HTTP parsing.
    TicketIngressService::IngressResult handleCancelIngress(
            const QString &taskId);
    /// Closes the affected workflow context after a protocol timeout.
    void handleTimeoutEvent(quint8 command, const QString &what);
    /// Routes an ACK only to the workflow waiting for that command.
    void handleAckEvent(quint8 ackedCmd);
    /// Preserves deletion intent when a protocol command is rejected.
    void handleNakEvent(quint8 origCmd, quint8 errCode);
    /// Applies one fresh Q_TASK observation in the established business order.
    void handleTasksUpdatedEvent(const QVariantList &taskData, int eventBridgeEpoch);
    /// Finalizes the active transfer only from its correlated Q_TASK observation.
    bool finalizeActiveTransferConfirmation(const QList<KeyTaskDto> &tasks,
                                            int eventBridgeEpoch);
    /// Closes a pending Q_TASK confirmation after timeout, NAK, or link loss.
    void failActiveTransferConfirmation(const QString &reason);
    /// Routes one task-log payload to progressive or whole-ticket return.
    void handleTaskLogReadyEvent(const KeySessionEvent &event);
    /// Converts a protocol task-log rejection into a retryable return failure.
    void handleTaskLogFailedEvent(const QString &reason);
    void handleCaptureRfidReadyEvent(const KeySessionEvent &event);
    void handleCaptureRfidFailedEvent(const KeySessionEvent &event);
    /// Projects transfer frame progress for the active transfer operation.
    void handleTicketTransferProgressEvent(int frameIndex, int totalFrames);
    /// Closes a successful transfer and starts its final Q_TASK confirmation.
    void handleTicketTransferFinishedEvent(int eventBridgeEpoch);
    /// Applies a typed transfer failure result and releases its operation context.
    void handleTicketTransferFailedEvent(const QString &what, int eventBridgeEpoch);
    /// Invalidates operation-local proofs when the key link is interrupted.
    void handleKeyLinkInterrupted(const KeySessionSnapshot &snapshot);
    /// Reconciles one key-presence snapshot with all operation-local contexts.
    void handleKeyStateChangedEvent(const KeySessionSnapshot &snapshot);
    bool isCurrentKeySession(int sessionId) const;
    void openNewKeySession();
    void invalidateCurrentKeySession(const QString &reason = QString());
    void scheduleAutoTransferRetry(const QString &reason = QString());
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
    bool progressiveStepReturnEnabled() const;
    bool isProgressiveStepTicket(const SystemTicketDto &ticket) const;
    void tryStartProgressiveStepReturnSweep(const QList<KeyTaskDto> &tasks);
    void startNextProgressiveStepReturn();
    void handleProgressiveTaskLog(const KeySessionEvent &event);
    void uploadNextCompletedProgressiveStep();
    bool canDeleteProgressiveStepTicket() const;
    void finishProgressiveStepReturn(bool continueSweep);
    void clearProgressiveStepReturnContext(bool clearCandidates);

    void failActiveReturnHandshake(const QString &reason);
    void markReturnDeletePending(const QString &taskId,
                                 const QByteArray &taskIdRaw,
                                 int keySessionId);
    void finalizePendingReturnDelete(const QList<KeyTaskDto> &tasks);
    bool isKeyPresenceFresh(const SystemTicketDto &ticket) const;
    KeyManualActionDecision evaluateManualAction(KeyManualAction action) const;
    KeyCommandEventContext commandEventContext() const;
    bool canProcessCancelNow(QString *blockedReason = nullptr) const;
    void schedulePendingCancelReconcile(const QString &reason = QString(), int delayMs = 300);
    void tryProcessPendingCancelTasks(const QList<KeyTaskDto> &tasks);
    void finalizePendingCancelDelete(const QList<KeyTaskDto> &tasks);
    /// Builds immutable L3 admission facts without acquiring the return workflow.
    TicketReturnStartRequest buildReturnStartRequest(
            const QString &taskId, bool automatic) const;
    bool canStartTicketReturn(const QString &taskId,
                              bool automatic,
                              QByteArray *taskIdRaw = nullptr,
                              QString *blockedReason = nullptr) const;
    QString activeReturnChainTaskId() const;
    static QString taskIdFromRaw(const QByteArray &taskIdRaw);
    /// Executes only the L1 serial/HTTP/UI effects selected by transfer L2.
    void applyTransferEffect(const TicketTransferWorkflowEffect &effect);
    /// Executes only the L1 serial, HTTP, UI, and scheduling effects selected by return L2.
    void applyReturnEffect(const TicketReturnWorkflowEffect &effect);
    void applyCaptureEffect(const CaptureTicketWorkflowEffect &effect);
    int nextOpId();
    QList<KeyTaskDto> toTaskDtos(const QVariantList &taskList) const;

    IKeySessionService *m_session;
    SerialLogManager *m_logManager;
    TicketStore *m_ticketStore;
    TicketTransferWorkflowService *m_ticketTransferWorkflow;
    TicketReturnWorkflowService *m_ticketReturnWorkflow;
    // 构造时从 dependencies.ticketIngress 保存地址；服务由 AppComposition 创建和拥有，
    // Controller 通过此指针注册业务回调、启动监听及完成 HTTP 回复。
    TicketIngressService *m_ticketIngress;
    TicketReturnUploadService *m_returnUploadService;
    KeyProvisioningService *m_keyProvisioning;
    TicketCancelWorkflowService *m_ticketCancelWorkflow;
    TicketDeleteWorkflowService *m_ticketDeleteWorkflow;
    AdminTicketTerminationService *m_adminTerminationService;
    KeyMaintenanceService *m_maintenanceService;
    KeyPortRuntimeCoordinator *m_keyPortRuntime;
    KeyTaskObservationService *m_keyTaskObservation;
    KeyManageObservabilityService *m_observability;
    KeyManageSelectionService *m_selection;
    CaptureTicketWorkflowService *m_captureWorkflow;
    CaptureTicketHttpClient *m_captureHttpClient;
    bool m_started = false;
    int m_nextOpId;
    int m_keySessionId = 0;
    int m_keySessionSeed = 0;

    // 安全超时：防止互斥锁因极端异常永久未释放（60 秒强制清锁）
    QTimer *m_httpTransactionSafetyTimer = nullptr;
    bool m_autoTransferRetryScheduled = false;
    bool m_cancelReconcileScheduled = false;
    bool m_logExportInProgress = false;
    // 运行时追踪正在途的 termination HTTP 请求，区别于磁盘持久化的 adminDeleteStage

};

#endif // KEYMANAGECONTROLLER_H
