#include "features/key/application/TicketTransferUseCase.h"

#include "features/key/domain/TicketReturnState.h"

namespace
{
TicketTransferStartResult rejected(TicketTransferStartReason reason,
                                   const QString &message,
                                   bool emitMessage)
{
    TicketTransferStartResult result;
    result.reason = reason;
    result.message = message;
    result.emitMessage = emitMessage;
    return result;
}

bool isAutomatic(const TicketTransferStartRequest &request)
{
    return request.trigger == TicketTransferTrigger::Automatic;
}

// Applies the shared manual/automatic policy when the physical key is not
// currently ready. Automatic requests remain queued; manual requests do not.
TicketTransferStartResult unavailable(const TicketTransferStartRequest &request,
                                      TicketTransferStartReason reason,
                                      const QString &automaticMessage,
                                      const QString &manualMessage)
{
    const bool automatic = isAutomatic(request);
    TicketTransferStartResult result = rejected(reason,
                                                automatic ? automaticMessage : manualMessage,
                                                true);
    if (automatic) {
        result.updateTicketState = true;
        result.ticketState = TicketTransferState::AutoPending;
    } else if (request.failInsteadOfDeferring) {
        result.updateTicketState = true;
        result.ticketState = TicketTransferState::Failed;
        result.ticketError = result.message;
    }
    return result;
}
}

// 纯判断工作台传票能否进入流程：不修改票池、不保存流程状态，也不发送命令。
// 条件按顺序检查，首个不满足项立即返回；只有走到函数末尾才标记 accepted。
WorkbenchTransferIngressDecision TicketTransferUseCase::evaluateWorkbenchIngress(
        const WorkbenchTransferIngressRequest &request)
{
    WorkbenchTransferIngressDecision decision;

    if (request.taskId.trimmed().isEmpty()) {
        decision.reason = WorkbenchTransferIngressReason::EmptyTaskId;
        decision.message = QStringLiteral("传票请求中的 taskId 为空，无法处理");
        decision.includeTaskId = false;
        return decision;
    }

    if (isTicketCancelActive(request.trackedCancelState)) {
        decision.reason = WorkbenchTransferIngressReason::CancelActive;
        decision.message = QStringLiteral("该任务已受理撤销，当前不再接受重复传票");
        return decision;
    }

    // 连接、钥匙在位、状态稳定和握手就绪必须同时满足，缺少任一项均不能传票。
    if (!request.session.connected
            || !request.session.keyPresent
            || !request.session.keyStable
            || !request.session.sessionReady) {
        decision.reason = WorkbenchTransferIngressReason::KeyNotReady;
        decision.message = QStringLiteral("未检测到钥匙在位并就绪，请确认");
        decision.includeTaskId = false;
        return decision;
    }

    if (request.httpTransactionActive) {
        decision.reason = WorkbenchTransferIngressReason::TransactionBusy;
        decision.message = QStringLiteral("当前已有工作台传票确认事务正在执行，请稍后重试");
        return decision;
    }

    // 已存在的同 taskId 记录按传票状态阻止重复提交。
    // 仅当缓存任务列表明确显示旧“成功”任务已不在钥匙中时，允许重置旧成功状态。
    if (request.existingTicket.valid) {
        if (ticketTransferStateFromStorage(request.existingTicket.transferState) == TicketTransferState::Success) {
            if (request.hasKeyTaskData && !request.taskStillInKey) {
                decision.resetStaleSuccess = true;
            } else {
                decision.reason = WorkbenchTransferIngressReason::AlreadyTransferred;
                decision.message = QStringLiteral("该任务已经传票到钥匙，请勿重复操作");
                return decision;
            }
        }
        if (ticketTransferStateFromStorage(request.existingTicket.transferState) == TicketTransferState::Sending) {
            decision.reason = WorkbenchTransferIngressReason::AlreadySending;
            decision.message = QStringLiteral("该任务正在传票中，请勿重复操作");
            return decision;
        }
        if (ticketTransferStateFromStorage(request.existingTicket.transferState) == TicketTransferState::Received
                || ticketTransferStateFromStorage(request.existingTicket.transferState) == TicketTransferState::AutoPending) {
            decision.reason = WorkbenchTransferIngressReason::AlreadyPending;
            decision.message = QStringLiteral("该任务已有待处理的传票请求，请勿重复操作");
            return decision;
        }
    }

    if (request.returnOrDeleteBusy) {
        decision.reason = WorkbenchTransferIngressReason::ReturnOrDeleteBusy;
        decision.message = QStringLiteral("当前钥匙仍有回传或删除确认业务，请稍后重试");
        return decision;
    }

    if (request.autoQueryBusy) {
        decision.reason = WorkbenchTransferIngressReason::AutoQueryBusy;
        decision.message = QStringLiteral("当前正在确认钥匙通信，请稍后重试");
        return decision;
    }

    // 所有拒绝条件均未命中，本次请求才获得接入许可。
    decision.reason = WorkbenchTransferIngressReason::Ready;
    decision.accepted = true;
    return decision;
}

TicketKeyObservationDecision TicketTransferUseCase::evaluateKeyObservation(
        const SystemTicketDto &ticket)
{
    TicketKeyObservationDecision decision;
    if (!ticket.valid) {
        decision.reason = TicketKeyObservationReason::UnknownTicket;
        return decision;
    }
    if (hasTicketCancelIntent(ticketCancelStateFromStorage(ticket.cancelState))) {
        decision.reason = TicketKeyObservationReason::CancelActive;
        return decision;
    }

    decision.confirmed = true;
    const bool needsReconcile =
            ticketTransferStateFromStorage(ticket.transferState) == TicketTransferState::Received
            || ticketTransferStateFromStorage(ticket.transferState) == TicketTransferState::AutoPending
            || ticketTransferStateFromStorage(ticket.transferState) == TicketTransferState::Sending
            || ticketTransferStateFromStorage(ticket.transferState) == TicketTransferState::Failed;
    decision.reconcileTransferSuccess = needsReconcile;
    decision.reason = needsReconcile
            ? TicketKeyObservationReason::ConfirmAndReconcile
            : TicketKeyObservationReason::ConfirmOnly;
    return decision;
}

TicketAutoTransferSelectionDecision TicketTransferUseCase::selectAutomaticCandidate(
        const TicketAutoTransferSelectionRequest &request)
{
    TicketAutoTransferSelectionDecision decision;

    if (request.httpTransactionActive) {
        decision.reason = TicketAutoTransferSelectionReason::HttpTransactionActive;
        return decision;
    }
    if (!request.enabled) {
        decision.reason = TicketAutoTransferSelectionReason::Disabled;
        return decision;
    }
    if (request.operationBusy) {
        decision.reason = TicketAutoTransferSelectionReason::OperationBusy;
        return decision;
    }

    for (const SystemTicketDto &ticket : request.tickets) {
        const bool cancelActive = hasTicketCancelIntent(ticketCancelStateFromStorage(ticket.cancelState));
        if (cancelActive) {
            continue;
        }
        if (ticketTransferStateFromStorage(ticket.transferState) == TicketTransferState::Received
                || ticketTransferStateFromStorage(ticket.transferState) == TicketTransferState::AutoPending) {
            decision.reason = TicketAutoTransferSelectionReason::Selected;
            decision.taskId = ticket.taskId;
            decision.selected = true;
            return decision;
        }
    }
    decision.reason = TicketAutoTransferSelectionReason::NoCandidate;
    return decision;
}

TicketAutoRecoveryDecision TicketTransferUseCase::evaluateAutomaticRecovery(
        const SystemTicketDto &ticket)
{
    TicketAutoRecoveryDecision decision;
    if (hasTicketCancelIntent(ticketCancelStateFromStorage(ticket.cancelState))) {
        decision.reason = TicketAutoRecoveryReason::CancelActive;
        return decision;
    }
    if (ticketTransferStateFromStorage(ticket.transferState) != TicketTransferState::Sending
            && ticketTransferStateFromStorage(ticket.transferState) != TicketTransferState::Failed) {
        decision.reason = TicketAutoRecoveryReason::StateNotRecoverable;
        return decision;
    }
    if (ticket.transferTriggerSource != QLatin1String("automatic")) {
        decision.reason = TicketAutoRecoveryReason::NotAutomaticSource;
        return decision;
    }
    const TicketReturnState returnState =
            ticketReturnStateFromStorage(ticket.returnState);
    if (isTicketReturnCleanup(returnState)
            || isTicketReturnInFlight(returnState)) {
        decision.reason = TicketAutoRecoveryReason::ReturnBusy;
        return decision;
    }

    decision.reason = TicketAutoRecoveryReason::Requeue;
    decision.requeue = true;
    return decision;
}

TicketTransferFinishedDecision TicketTransferUseCase::evaluateFinished(
        const TicketTransferResultRequest &request)
{
    TicketTransferFinishedDecision decision;

    if (request.taskId.isEmpty()
            || request.operationSessionId <= 0
            || request.operationSessionId != request.currentSessionId) {
        decision.reason = TicketTransferResultReason::StaleOperation;
        return decision;
    }

    decision.reason = TicketTransferResultReason::Handle;
    decision.handle = true;
    return decision;
}

TicketTransferConfirmationDecision TicketTransferUseCase::evaluateConfirmation(
        const TicketTransferConfirmationRequest &request)
{
    TicketTransferConfirmationDecision decision;
    if (!request.awaitingDeviceConfirmation) {
        return decision;
    }
    if (!request.sameKeySession) {
        decision.action =
                TicketTransferConfirmationAction::FailSessionChanged;
        decision.ticketState = TicketTransferState::Failed;
        decision.message = QStringLiteral(
                    "传票确认期间钥匙会话已变化，请重新传票");
        return decision;
    }
    if (!request.taskPresent) {
        decision.action =
                TicketTransferConfirmationAction::FailTaskMissing;
        decision.ticketState = TicketTransferState::Failed;
        decision.message = QStringLiteral(
                    "传票失败：Q_TASK 未查询到该任务，请重新传票");
        return decision;
    }

    decision.action = TicketTransferConfirmationAction::ConfirmSuccess;
    decision.ticketState = TicketTransferState::Success;
    decision.message = QStringLiteral(
                "传票成功，已确认任务存在于钥匙中");
    return decision;
}

TicketTransferFailureDecision TicketTransferUseCase::evaluateFailure(
        const TicketTransferFailureRequest &request)
{
    TicketTransferFailureDecision decision;

    if (request.taskId.isEmpty()
            || request.operationSessionId <= 0
            || request.operationSessionId != request.currentSessionId) {
        decision.reason = TicketTransferResultReason::StaleOperation;
        return decision;
    }

    decision.reason = TicketTransferResultReason::Handle;
    const bool commandInFlight = request.failureMessage.contains(
                QStringLiteral("\u5F53\u524D\u6709\u547D\u4EE4\u5728\u9014"));
    const bool trackedTicket = request.ticket.valid;
    const bool automaticSource = trackedTicket
            && request.ticket.transferTriggerSource == QLatin1String("automatic");

    if (commandInFlight && automaticSource) {
        decision.action = TicketTransferFailureAction::QueueAutomaticRetry;
        decision.ticketState = TicketTransferState::AutoPending;
        decision.ticketError = QStringLiteral(
                    "\u5E95\u5C42\u4ECD\u6709\u547D\u4EE4\u5728\u9014\uFF0C\u7B49\u5F85\u5F53\u524D\u547D\u4EE4\u5B8C\u6210\u540E\u81EA\u52A8\u91CD\u8BD5");
        decision.statusMessage = QStringLiteral(
                    "\u4F20\u7968\u6682\u672A\u53D1\u9001\uFF1A\u5F53\u524D\u4ECD\u6709\u547D\u4EE4\u5728\u9014\uFF0C\u5DF2\u56DE\u5230\u81EA\u52A8\u7B49\u5F85\u961F\u5217\uFF1A%1")
                .arg(request.taskId);
        decision.automaticRetryReason = QStringLiteral(
                    "\u5E95\u5C42\u547D\u4EE4\u5728\u9014");
    } else if (commandInFlight && trackedTicket) {
        decision.action = TicketTransferFailureAction::FailManualBusy;
        decision.ticketState = TicketTransferState::Failed;
        decision.ticketError = QStringLiteral(
                    "\u672C\u6B21\u4E3A\u4EBA\u5DE5\u89E6\u53D1\uFF0C\u5E95\u5C42\u547D\u4EE4\u5728\u9014\uFF0C\u8BF7\u4EBA\u5DE5\u91CD\u8BD5");
        decision.deferredMessage = QStringLiteral(
                    "\u4F20\u7968\u5931\u8D25\uFF1A\u5E95\u5C42\u547D\u4EE4\u4ECD\u5728\u9014\uFF0C\u8BF7\u7A0D\u540E\u91CD\u8BD5");
        decision.statusMessage = QStringLiteral(
                    "\u4EBA\u5DE5\u4F20\u7968\u5931\u8D25\uFF1A\u5E95\u5C42\u547D\u4EE4\u4ECD\u5728\u9014\uFF0C\u7CFB\u7EDF\u4E0D\u4F1A\u81EA\u52A8\u8865\u53D1\uFF0C\u8BF7\u624B\u52A8\u91CD\u8BD5\uFF1A%1")
                .arg(request.taskId);
        decision.completeDeferredResponse = true;
    } else {
        const QString detail = request.failureMessage.isEmpty()
                ? QStringLiteral("\u4E32\u53E3\u4F20\u8F93\u9519\u8BEF")
                : request.failureMessage;
        decision.action = TicketTransferFailureAction::Fail;
        decision.ticketState = TicketTransferState::Failed;
        decision.ticketError = request.failureMessage;
        decision.deferredMessage =
                QStringLiteral("\u4F20\u7968\u5931\u8D25\uFF1A%1").arg(detail);
        decision.statusMessage =
                QStringLiteral("\u4F20\u7968\u53D1\u9001\u5931\u8D25\uFF1A%1")
                .arg(request.failureMessage);
        decision.completeDeferredResponse = true;
    }
    return decision;
}

TicketTransferStartResult TicketTransferUseCase::evaluateStart(
        const TicketTransferStartRequest &request)
{
    const bool automatic = isAutomatic(request);
    const bool workbench = request.trigger == TicketTransferTrigger::Workbench;

    if (request.taskId.trimmed().isEmpty()) {
        return rejected(TicketTransferStartReason::EmptyTaskId,
                        QStringLiteral("传票发送失败：taskId 为空"),
                        true);
    }
    if (workbench && request.jsonBytes.isEmpty()) {
        return rejected(TicketTransferStartReason::MissingPayload,
                        QStringLiteral("传票发送失败：JSON 内容为空"),
                        false);
    }
    if (request.anotherTransferActive) {
        TicketTransferStartResult result = rejected(
                    TicketTransferStartReason::TransferBusy,
                    QStringLiteral("当前已有其他传票任务正在执行，请稍后重试"),
                    !automatic || request.failInsteadOfDeferring);
        if (request.failInsteadOfDeferring) {
            result.updateTicketState = true;
            result.ticketState = TicketTransferState::Failed;
            result.ticketError = result.message;
        }
        return result;
    }
    if (request.peerWorkflowBusy) {
        return rejected(
                    TicketTransferStartReason::PeerWorkflowBusy,
                    QStringLiteral("\u5f53\u524d\u6b63\u5728\u6267\u884c\u56de\u4f20\u3001\u5220\u9664\u6216\u64a4\u9500\uff0c\u8bf7\u7b49\u5f85\u5f53\u524d\u94a5\u5319\u64cd\u4f5c\u5b8c\u6210\u540e\u518d\u4f20\u7968"),
                    !automatic || request.failInsteadOfDeferring);
    }
    if (workbench && !request.currentSessionValid) {
        return rejected(TicketTransferStartReason::SessionChanged,
                        QStringLiteral("当前钥匙会话未建立，无法发送传票"),
                        false);
    }
    if (workbench && !request.bridgeEpochMatches) {
        return rejected(TicketTransferStartReason::BridgeChanged,
                        QStringLiteral("当前钥匙会话已变化，无法发送传票"),
                        false);
    }
    if (request.ticket.valid
            && hasTicketCancelIntent(ticketCancelStateFromStorage(request.ticket.cancelState))) {
        return rejected(TicketTransferStartReason::CancelPending,
                        QStringLiteral("该系统票已受理撤销，当前不再参与传票"),
                        true);
    }
    if (request.ticket.valid
            && ticketTransferStateFromStorage(request.ticket.transferState) == TicketTransferState::Success) {
        return rejected(TicketTransferStartReason::AlreadyTransferred,
                        QStringLiteral("该系统票已传票到钥匙，本次不重复下发"),
                        true);
    }
    if (request.ticket.valid
            && ticketTransferStateFromStorage(request.ticket.transferState) == TicketTransferState::Sending) {
        return rejected(TicketTransferStartReason::AlreadySending,
                        QStringLiteral("该系统票正在传票中，请稍后再试"),
                        true);
    }
    if (!request.session.connected) {
        return unavailable(request,
                           TicketTransferStartReason::SerialDisconnected,
                           QStringLiteral("系统票已接收，等待串口连接后自动传票"),
                           QStringLiteral("串口未连接，无法发送传票"));
    }
    if (!request.session.keyPresent || !request.session.keyStable) {
        return unavailable(request,
                           TicketTransferStartReason::KeyNotReady,
                           QStringLiteral("系统票已接收，等待钥匙就绪后自动传票"),
                           QStringLiteral("钥匙未就绪，无法发送传票"));
    }
    if (!request.session.sessionReady) {
        return unavailable(request,
                           TicketTransferStartReason::SessionNotReady,
                           QStringLiteral("系统票已接收，等待通信握手完成后自动传票"),
                           QStringLiteral("钥匙通信会话未就绪，无法发送传票"));
    }
    if (request.session.commandInFlight) {
        return unavailable(request,
                           TicketTransferStartReason::CommandInFlight,
                           QStringLiteral("系统票已接收，等待当前协议命令完成后自动传票"),
                           QStringLiteral("当前有协议命令正在执行，无法发送传票"));
    }
    if (request.session.recoveryWindowActive) {
        return unavailable(request,
                           TicketTransferStartReason::RecoveryInProgress,
                           QStringLiteral("系统票已接收，等待钥匙通信恢复后自动传票"),
                           QStringLiteral("钥匙通信正在恢复，无法发送传票"));
    }
    if (request.jsonBytes.isEmpty()) {
        TicketTransferStartResult result = rejected(
                    TicketTransferStartReason::MissingPayload,
                    QStringLiteral("选中的系统票缺少原始 JSON，无法发送传票"),
                    true);
        result.updateTicketState = true;
        result.ticketState = TicketTransferState::Failed;
        result.ticketError = QStringLiteral("raw json missing");
        return result;
    }

    TicketTransferStartResult result;
    result.reason = TicketTransferStartReason::Ready;
    result.allowed = true;
    return result;
}
