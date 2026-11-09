#include "features/key/application/TicketCancelUseCase.h"

#include "features/key/domain/TicketTransferState.h"

#include "features/key/domain/TicketReturnState.h"

namespace
{
TicketCancelIngressDecision decision(TicketCancelIngressReason reason,
                                     TicketCancelIngressAction action,
                                     const QString &message,
                                     bool accepted = false,
                                     bool deferred = false)
{
    TicketCancelIngressDecision result;
    result.reason = reason;
    result.action = action;
    result.message = message;
    result.accepted = accepted;
    result.deferred = deferred;
    return result;
}
}

TicketCancelExecutionDecision
TicketCancelUseCase::evaluateExecution(
        const TicketCancelExecutionRequest &request)
{
    TicketCancelExecutionDecision decision;
    if (request.cancelBusy) {
        decision.reason = TicketCancelExecutionReason::CancelBusy;
        decision.message = QStringLiteral(
                "\u5F53\u524D\u5DF2\u6709\u64A4\u9500\u5220\u9664\u94FE\u5728\u6267\u884C");
        return decision;
    }
    if (request.transferBusy) {
        decision.reason = TicketCancelExecutionReason::TransferBusy;
        decision.message = QStringLiteral(
                "\u5F53\u524D\u5B58\u5728\u4F20\u7968\u94FE\u5728\u9014");
        return decision;
    }
    if (request.returnOrDeleteBusy) {
        decision.reason =
                TicketCancelExecutionReason::ReturnOrDeleteBusy;
        decision.message = QStringLiteral(
                "\u5F53\u524D\u5B58\u5728\u56DE\u4F20\u6216\u5220\u9664\u9A8C\u8BC1\u94FE\u5728\u9014");
        return decision;
    }
    if (!request.keyReady) {
        decision.reason = TicketCancelExecutionReason::KeyNotReady;
        decision.message = QStringLiteral(
                "\u5F53\u524D\u94A5\u5319\u4F1A\u8BDD\u672A\u5C31\u7EEA");
        return decision;
    }
    if (request.commandBusy) {
        decision.reason = TicketCancelExecutionReason::CommandBusy;
        decision.message = QStringLiteral(
                "\u5F53\u524D\u5E95\u5C42\u4ECD\u6709\u534F\u8BAE\u547D\u4EE4\u5728\u9014");
        return decision;
    }
    if (request.recoveryActive) {
        decision.reason = TicketCancelExecutionReason::RecoveryActive;
        decision.message = QStringLiteral(
                "\u5F53\u524D\u5904\u4E8E\u81EA\u52A8\u6062\u590D\u7A97\u53E3");
        return decision;
    }
    if (request.autoQueryBusy) {
        decision.reason = TicketCancelExecutionReason::AutoQueryBusy;
        decision.message = QStringLiteral(
                "\u5F53\u524D\u6B63\u5728\u6267\u884C\u81EA\u52A8 Q_TASK \u5BF9\u8D26");
        return decision;
    }

    decision.reason = TicketCancelExecutionReason::Ready;
    decision.allowed = true;
    return decision;
}

TicketCancelIngressDecision TicketCancelUseCase::evaluateIngress(
        const TicketCancelIngressRequest &request)
{
    if (request.taskId.trimmed().isEmpty()) {
        return decision(TicketCancelIngressReason::EmptyTaskId,
                        TicketCancelIngressAction::Reject,
                        QStringLiteral("taskId \u4e3a\u7a7a"));
    }

    if (request.trackedRecordExists) {
        if (request.trackedCancelState == TicketCancelState::Done) {
            return decision(TicketCancelIngressReason::AlreadyCanceled,
                            TicketCancelIngressAction::ReturnDone,
                            QStringLiteral("\u4efb\u52a1\u5df2\u53d6\u6d88"), true);
        }
        if (isTicketCancelActive(request.trackedCancelState)) {
            return decision(TicketCancelIngressReason::DuplicateActive,
                            TicketCancelIngressAction::Reject,
                            QStringLiteral("\u4efb\u52a1\u64a4\u9500\u6b63\u5728\u5904\u7406\u4e2d\uff0c\u8bf7\u52ff\u91cd\u590d\u63d0\u4ea4"));
        }
    }

    // Missing process-memory data cannot prove that a removable key was queried.
    // Reject so H5 cannot continue to backend termination without device proof.
    if (!request.ticket.valid) {
        return decision(TicketCancelIngressReason::TicketMissing,
                        TicketCancelIngressAction::Reject,
                        QStringLiteral("\u672a\u627e\u5230\u5bf9\u5e94\u4efb\u52a1"));
    }

    if (hasTicketCancelIntent(ticketCancelStateFromStorage(request.ticket.cancelState))) {
        if (isTicketCancelActive(ticketCancelStateFromStorage(request.ticket.cancelState))) {
            return decision(TicketCancelIngressReason::DuplicateActive,
                            TicketCancelIngressAction::Reject,
                            QStringLiteral("\u4efb\u52a1\u64a4\u9500\u6b63\u5728\u5904\u7406\u4e2d\uff0c\u8bf7\u52ff\u91cd\u590d\u63d0\u4ea4"));
        }
        return decision(TicketCancelIngressReason::ExistingIntent,
                        TicketCancelIngressAction::ReturnAccepted,
                        QStringLiteral("\u4efb\u52a1\u53d6\u6d88\u5df2\u53d7\u7406"), true);
    }

    if (ticketTransferStateFromStorage(request.ticket.transferState) == TicketTransferState::KeyCleared) {
        return decision(TicketCancelIngressReason::TaskCompleted,
                        TicketCancelIngressAction::RecordDone,
                        QStringLiteral("\u4efb\u52a1\u5df2\u5b8c\u6210\uff0c\u65e0\u9700\u64a4\u9500"), true);
    }

    if (isTicketReturnCleanup(ticketReturnStateFromStorage(request.ticket.returnState))) {
        return decision(TicketCancelIngressReason::ReturnCleanupActive,
                        TicketCancelIngressAction::ReturnAccepted,
                        QStringLiteral("\u4efb\u52a1\u5df2\u5728\u6e05\u7406\u4e2d\uff0c\u65e0\u9700\u91cd\u590d\u64a4\u9500"), true);
    }

    if (ticketTransferStateFromStorage(request.ticket.transferState) == TicketTransferState::Success && !request.keyReady) {
        // Give the operator the physical recovery action instead of exposing
        // internal confirmation terminology from the termination safety gate.
        return decision(TicketCancelIngressReason::KeyTaskNotConfirmed,
                        TicketCancelIngressAction::Reject,
                        QStringLiteral("\u672a\u68c0\u6d4b\u5230\u94a5\u5319\u5728\u4f4d\u6216\u94a5\u5319\u901a\u8baf\u672a\u5c31\u7eea\uff0c\u65e0\u6cd5\u7ec8\u6b62\u4efb\u52a1\u3002\u8bf7\u5c06\u4f20\u7968\u65f6\u4f7f\u7528\u7684\u94a5\u5319\u653e\u56de\u94a5\u5319\u5ea7\uff0c\u786e\u8ba4\u8fde\u63a5\u540e\u91cd\u8bd5"));
    }

    if (ticketTransferStateFromStorage(request.ticket.transferState) == TicketTransferState::Success
            && (!request.hasTaskList || !request.taskInKey)) {
        return decision(TicketCancelIngressReason::KeyTaskNotConfirmed,
                        TicketCancelIngressAction::Reject,
                        QStringLiteral("\u5f53\u524d\u94a5\u5319\u4e2d\u672a\u627e\u5230\u8be5\u4efb\u52a1\uff0c\u65e0\u6cd5\u5b89\u5168\u7ec8\u6b62\u3002\u8bf7\u786e\u8ba4\u653e\u56de\u7684\u662f\u4f20\u7968\u65f6\u4f7f\u7528\u7684\u94a5\u5319\u540e\u91cd\u8bd5"));
    }

    if ((ticketTransferStateFromStorage(request.ticket.transferState) == TicketTransferState::Received
            || ticketTransferStateFromStorage(request.ticket.transferState) == TicketTransferState::AutoPending)
            && !request.taskInKey) {
        return decision(TicketCancelIngressReason::RemovePendingTransfer,
                        TicketCancelIngressAction::RemoveTicket,
                        QStringLiteral("\u4efb\u52a1\u5df2\u53d6\u6d88"), true);
    }

    TicketCancelIngressDecision result = decision(
                request.canProcessNow
                    ? TicketCancelIngressReason::AcceptedNow
                    : TicketCancelIngressReason::AcceptedDeferred,
                TicketCancelIngressAction::AcceptCancel,
                request.canProcessNow
                    ? QStringLiteral("\u4efb\u52a1\u53d6\u6d88\u5df2\u53d7\u7406\uff0c\u51c6\u5907\u5bf9\u8d26\u6267\u884c")
                    : QStringLiteral("\u4efb\u52a1\u53d6\u6d88\u5df2\u53d7\u7406\uff0c\u5f85\u5f53\u524d\u4efb\u52a1\u6536\u53e3\u540e\u6267\u884c"),
                true,
                true);
    result.nextCancelState = request.canProcessNow
            ? TicketCancelState::Pending
            : TicketCancelState::Accepted;
    return result;
}
TicketCancelTaskObservationDecision TicketCancelUseCase::evaluateTaskObservation(
        const TicketCancelTaskObservationRequest &request)
{
    TicketCancelTaskObservationDecision decision;
    if (request.ticket.valid
            && ticketTransferStateFromStorage(request.ticket.transferState)
               == TicketTransferState::KeyCleared) {
        decision.action = TicketCancelTaskAction::MarkDone;
        decision.reason = TicketCancelTaskReason::TaskCompleted;
        decision.message = QStringLiteral("任务已完成，无需撤销");
        return decision;
    }
    if (request.ticket.valid
            && isTicketReturnCleanup(
                    ticketReturnStateFromStorage(request.ticket.returnState))) {
        decision.action = TicketCancelTaskAction::MarkDone;
        decision.reason = TicketCancelTaskReason::ReturnCleanupActive;
        decision.message = QStringLiteral("任务已在清理中，无需重复撤销");
        return decision;
    }
    if (request.removalProof != KeyTaskRemovalProof::ReadyToDelete) {
        decision.action = TicketCancelTaskAction::RejectMissingKeyTask;
        decision.reason = TicketCancelTaskReason::TaskMissing;
        decision.message = QStringLiteral(
                "当前钥匙中未查询到该任务，无法确认安全删除，已拒绝终止");
        return decision;
    }

    decision.action = TicketCancelTaskAction::StartDelete;
    decision.reason = TicketCancelTaskReason::ReadyToDelete;
    decision.message = QStringLiteral("已确认钥匙中存在任务，开始执行 DEL");
    return decision;
}

TicketCancelDeleteVerificationDecision
TicketCancelUseCase::evaluateDeleteVerification(KeyTaskRemovalProof proof)
{
    TicketCancelDeleteVerificationDecision decision;
    switch (proof) {
    case KeyTaskRemovalProof::DeleteConfirmed:
        decision.action = TicketCancelDeleteVerificationAction::CompleteCancel;
        decision.message = QStringLiteral("任务已取消");
        return decision;
    case KeyTaskRemovalProof::TaskStillPresent:
        decision.action =
                TicketCancelDeleteVerificationAction::RetryTaskStillPresent;
        decision.message = QStringLiteral(
                "删除验证发现钥匙任务仍存在，等待下一次安全窗口继续清理");
        return decision;
    case KeyTaskRemovalProof::SessionChanged:
    case KeyTaskRemovalProof::TaskMissingBeforeDelete:
    case KeyTaskRemovalProof::ReadyToDelete:
        decision.action =
                TicketCancelDeleteVerificationAction::RetryAfterSessionChange;
        decision.message = QStringLiteral(
                "会话已变更，撤销删除验证已中断，等待下一次安全窗口继续清理");
        return decision;
    }

    return decision;
}
