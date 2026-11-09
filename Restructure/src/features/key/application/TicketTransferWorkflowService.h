#ifndef TICKETTRANSFERWORKFLOWSERVICE_H
#define TICKETTRANSFERWORKFLOWSERVICE_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "features/key/application/HttpTransferTransactionContext.h"
#include "features/key/application/TicketTransferOperationContext.h"
#include "features/key/application/TicketTransferUseCase.h"
#include "shared/contracts/KeyTaskDto.h"

class TicketStore;

enum class TicketTransferWorkflowAction
{
    None,
    SendSetCom,
    SendTicket,
    QueryTasks,
    CompleteSuccess,
    CompleteFailure,
    RetryAutomatically
};

struct TicketTransferWorkflowEffect
{
    TicketTransferWorkflowAction action =
            TicketTransferWorkflowAction::None;
    QString taskId;
    QByteArray jsonBytes;
    int debugFrameChunkSize = 0;
    int operationSessionId = 0;
    QString statusMessage;
    bool handled = false;
    bool completeDeferredResponse = false;
    bool deferredSuccess = false;
    QString deferredMessage;
    bool refreshWorkbench = false;
    bool scheduleCancelReconcile = false;
    bool startSafetyTimer = false;
    bool stopSafetyTimer = false;
    bool tryNextAutomatic = false;
    QString automaticRetryReason;
};

// Controller 汇集的单次工作台传票输入：原始票据、钥匙状态和业务占用事实。
// 由 admitWorkbench() 用于准入及启动检查；此结构本身不作判断，也不发送设备命令。
struct TicketTransferWorkbenchRequest
{
    QString taskId;                         // 票据身份，用于查重、撤销冲突检查及后续流程关联。
    QByteArray jsonBytes;                   // 完整原始 JSON，受理后交给票池并用于后续编码发送。
    TicketCancelState trackedCancelState = TicketCancelState::None; // 该任务已记录的撤销状态。
    KeySessionSnapshot ingressSession;      // 进入业务入口时读取的钥匙状态，供接入准入判断。
    KeySessionSnapshot startSession;        // 再次读取的状态，供启动检查及桥接身份比较。
    bool hasKeyTaskData = false;            // 已保存的设备任务列表是否非空；不表示查询刚完成。
    bool taskStillInKey = false;            // 已保存列表是否包含 taskId；不是本次新查询的确认结果。
    bool returnOrDeleteBusy = false;        // 退票、删除、撤销或采集等冲突业务是否占用设备。
    bool autoQueryBusy = false;             // 当前钥匙会话是否有自动任务查询正在进行。
    int currentKeySessionId = 0;            // 本机钥匙会话编号，用于关联后续流程；不是 taskId。
    int debugFrameChunkSize = 0;            // 后续传票分块的调试配置；0 保留 builder 默认大小。
};

struct TicketTransferWorkbenchOutcome
{
    WorkbenchTransferIngressDecision ingressDecision;
    TicketTransferStartResult startDecision;
    bool accepted = false;
    bool deferred = false;
    bool includeTaskId = true;
    QString taskId;
    QString message;
    TicketTransferWorkflowEffect effect;
};

struct TicketTransferStoredStartRequest
{
    QString taskId;
    bool automatic = false;
    bool failInsteadOfDeferring = false;
    bool peerWorkflowBusy = false;
    KeySessionSnapshot session;
    int currentKeySessionId = 0;
    int debugFrameChunkSize = 0;
};

struct TicketTransferStoredStartOutcome
{
    TicketTransferStartResult decision;
    TicketTransferWorkflowEffect effect;
};

// Owns the complete process-local transfer workflow from admission through
// SET_COM/TICKET and authoritative Q_TASK presence proof. It returns typed
// effects; L1 sends commands/HTTP replies and L4 owns protocol timing.
class TicketTransferWorkflowService : public QObject
{
public:
    // Uses the process-local ticket store; no lifecycle state is persisted.
    explicit TicketTransferWorkflowService(TicketStore *store,
                                           QObject *parent = nullptr);

    // Accepts one workbench request only after both ingress and immediate
    // pre-command snapshots pass, then acquires its deferred transaction.
    TicketTransferWorkbenchOutcome admitWorkbench(
            const TicketTransferWorkbenchRequest &request);

    // Starts one already-ingested manual or automatic ticket without exposing
    // the operation context to the event router.
    TicketTransferStoredStartOutcome startStoredTicket(
            const TicketTransferStoredStartRequest &request);

    // Reports whether one physical-key transfer currently owns the workflow.
    bool hasActiveOperation() const;
    // Reports whether SET_COM confirmation is still required before TICKET.
    bool hasPendingHandshake() const;
    // Reports whether TICKET frames are awaiting authoritative Q_TASK proof.
    bool isAwaitingDeviceConfirmation() const;
    // Exposes the active task only for L1 logging and event correlation.
    QString activeTaskId() const;
    // Exposes the owning physical-key session for stale-event rejection.
    int activeSessionId() const;

    // Reports whether one deferred workbench HTTP request is owned.
    bool hasActiveWorkbenchTransaction() const;
    // Exposes deferred task identity for diagnostics only.
    QString activeWorkbenchTaskId() const;
    // Exposes deferred physical-key session for correlation logs.
    int activeWorkbenchSessionId() const;
    // Exposes deferred bridge epoch for correlation logs.
    int activeWorkbenchBridgeEpoch() const;
    // Accepts events only from the deferred request's original session/epoch.
    bool matchesWorkbenchTransaction(int sessionId, int bridgeEpoch) const;

    // Converts the matching SET_COM ACK into the TICKET-send effect.
    TicketTransferWorkflowEffect acceptHandshakeAck(
            int currentKeySessionId);

    // Closes a failed SET_COM handshake and any matching deferred request.
    TicketTransferWorkflowEffect failHandshake(const QString &reason);

    // Records frame progress only for the current physical-key session.
    TicketTransferWorkflowEffect recordFrameProgress(
            int currentKeySessionId);

    // Keeps transfer identity alive after TICKET ACK and asks L1 for Q_TASK.
    TicketTransferWorkflowEffect finishFrames(
            int currentKeySessionId,
            int eventBridgeEpoch);

    // Finalizes transfer only from a fresh Q_TASK observation.
    TicketTransferWorkflowEffect verifyDevicePresence(
            const QList<KeyTaskDto> &tasks,
            int currentKeySessionId,
            int eventBridgeEpoch,
            qint64 observedAtMs);

    // Closes a Q_TASK timeout/NAK while preserving the success invariant.
    TicketTransferWorkflowEffect failDeviceConfirmation(
            const QString &reason);

    // Applies a lower-level TICKET failure and classifies automatic retry.
    TicketTransferWorkflowEffect failTransfer(
            const QString &reason,
            int currentKeySessionId,
            int eventBridgeEpoch);

    // Fails the active transfer when its physical-key link is lost.
    TicketTransferWorkflowEffect interrupt(const QString &reason);

    // Releases a deferred workbench transaction that exceeded the L1 timer.
    TicketTransferWorkflowEffect expireWorkbenchTransaction(
            const QString &reason);

    // Reconciles all registered tickets observed in one fresh Q_TASK result.
    QList<TicketTransferWorkflowEffect> reconcileObservedTasks(
            const QList<KeyTaskDto> &tasks,
            qint64 observedAtMs);

    // Requeues only automatic failures after a fresh empty-key observation.
    QStringList recoverAutomaticWhenKeyEmpty(
            const QList<KeyTaskDto> &tasks,
            bool automaticEnabled,
            bool returnActive);

    // Selects one automatic candidate while treating peer workflows as facts.
    TicketAutoTransferSelectionDecision selectAutomaticCandidate(
            bool automaticEnabled,
            bool peerWorkflowBusy,
            const QList<SystemTicketDto> &tickets) const;

private:
    // Creates one typed effect without performing adapter side effects.
    TicketTransferWorkflowEffect effect(
            TicketTransferWorkflowAction action,
            const QString &taskId = QString()) const;
    // Releases deferred correlation and optionally its matching transfer.
    void clearWorkbenchTransaction(
            TicketTransferWorkflowEffect *effect,
            bool clearActiveTransfer);

    TicketStore *m_store;
    TicketTransferOperationContext m_operation;
    HttpTransferTransactionContext m_workbenchTransaction;
};

#endif // TICKETTRANSFERWORKFLOWSERVICE_H