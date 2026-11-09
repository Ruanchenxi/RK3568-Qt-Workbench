#include "features/key/application/TicketTransferWorkflowService.h"

#include "features/key/application/TicketStore.h"
#include "features/key/application/VerifiedKeyTaskRemoval.h"

namespace
{
bool taskExists(const QList<KeyTaskDto> &tasks, const QString &taskId)
{
    const QString wanted = taskId.trimmed();
    for (const KeyTaskDto &task : tasks) {
        if (VerifiedKeyTaskRemoval::decimalTaskId(task.taskId) == wanted) {
            return true;
        }
    }
    return false;
}
}

TicketTransferWorkflowService::TicketTransferWorkflowService(
        TicketStore *store,
        QObject *parent)
    : QObject(parent)
    , m_store(store)
{
}
 
// 对工作台请求执行接入和启动检查，通过后登记等待握手的传票操作。
// 返回受理结果及下一步动作，由 Controller 执行设备通信；受理不等于设备传票成功。
TicketTransferWorkbenchOutcome
TicketTransferWorkflowService::admitWorkbench(
        const TicketTransferWorkbenchRequest &request)
{
    
    TicketTransferWorkbenchOutcome outcome;
    outcome.taskId = request.taskId.trimmed();

    // 第一关：整理准入判断真正需要的事实，检查任务标识是否非空、钥匙是否就绪，
    // 以及是否存在重复传票、取消/退回/删除冲突或其他繁忙操作。
    const SystemTicketDto oldTicket = m_store->ticketById(outcome.taskId);
    WorkbenchTransferIngressRequest ingress;
    ingress.taskId = outcome.taskId;
    ingress.trackedCancelState = request.trackedCancelState;
    ingress.session = request.ingressSession;
    ingress.httpTransactionActive = m_workbenchTransaction.isActive();
    ingress.existingTicket = oldTicket;
    ingress.hasKeyTaskData = request.hasKeyTaskData;
    ingress.taskStillInKey = request.taskStillInKey;
    ingress.returnOrDeleteBusy = request.returnOrDeleteBusy;
    ingress.autoQueryBusy = request.autoQueryBusy;
    outcome.ingressDecision =
            TicketTransferUseCase::evaluateWorkbenchIngress(ingress);
    outcome.includeTaskId = outcome.ingressDecision.includeTaskId;
    if (!outcome.ingressDecision.accepted) {
        // 第一关未通过：把拒绝原因交回 Controller，到这里就结束，不启动设备通信。
        outcome.message = outcome.ingressDecision.message;
        return outcome;
    }

    // 第一关通过后，修正可能存在的旧成功记录，并登记本次 HTTP 传票事务。
    if (outcome.ingressDecision.resetStaleSuccess) {
        m_store->updateTransferState(
                    outcome.taskId, TicketTransferState::Received);
    }

    const int expectedSessionId = request.currentKeySessionId;
    const int expectedBridgeEpoch = request.ingressSession.bridgeEpoch;
    // 记录本次工作台事务的任务和会话归属，供占用检查及后续结果关联使用。
    // 此处不保存 HTTP socket；原连接的延迟响应由 TicketIngressService 管理。
    m_workbenchTransaction.begin(
                outcome.taskId,
                expectedSessionId,
                expectedBridgeEpoch);

    // 将票据及原始 JSON 保存到进程内票池，不落盘；入池失败须释放刚登记的事务。
    QString ingestError;
    if (!m_store->ingestJson(request.jsonBytes, &ingestError)) {
        m_workbenchTransaction.clear();
        outcome.message = QStringLiteral(
                    "\u4F20\u7968\u8BF7\u6C42\u5165\u6C60\u5931\u8D25\uFF1A%1")
                .arg(ingestError);
        return outcome;
    }

    // 第二关：用 Controller 传入的启动快照、当前流程占用和入池记录检查能否启动。
    // 此处不重新采样设备状态；检查会话归属、命令在途及恢复窗口等启动限制。
    TicketTransferStartRequest start;
    start.taskId = outcome.taskId;
    start.jsonBytes = request.jsonBytes;
    start.ticket = m_store->ticketById(outcome.taskId);
    start.session = request.startSession;
    start.trigger = TicketTransferTrigger::Workbench;
    start.anotherTransferActive = m_operation.isActive();
    start.peerWorkflowBusy = request.returnOrDeleteBusy;
    start.currentSessionValid = expectedSessionId > 0;
    start.bridgeEpochMatches = expectedBridgeEpoch > 0
            && request.startSession.bridgeEpoch == expectedBridgeEpoch;
    outcome.startDecision = TicketTransferUseCase::evaluateStart(start);
    if (!outcome.startDecision.allowed) {
        m_store->updateTransferState(
                    outcome.taskId,
                    TicketTransferState::Failed,
                    outcome.startDecision.message);
        m_workbenchTransaction.clear();
        outcome.message = outcome.startDecision.message;
        return outcome;
    }

    // 两关通过后保存 taskId、会话及待发送 JSON，使本函数返回后仍能继续本次操作。
    // begin() 同时登记待握手任务；acceptHandshakeAck() 随后取出 JSON，生成 SendTicket 动作。
    m_operation.begin(
                outcome.taskId,
                request.jsonBytes,
                request.debugFrameChunkSize,
                expectedSessionId);
    m_store->updateTransferTriggerSource(
                outcome.taskId, QStringLiteral("manual"));

    // 业务已受理，但 HTTP 最终结果须延后；从重新握手开始，尚未确认设备已有任务。
    outcome.accepted = true;
    outcome.deferred = true;
    outcome.message = QStringLiteral(
                "\u4F20\u7968\u8BF7\u6C42\u5DF2\u5F00\u59CB\u53D1\u9001");
    // 只返回握手动作说明；实际 SET_COM 由 Controller::applyTransferEffect() 发起。
    outcome.effect = effect(
                TicketTransferWorkflowAction::SendSetCom,
                outcome.taskId);
    outcome.effect.handled = true;
    outcome.effect.startSafetyTimer = true;
    outcome.effect.statusMessage = QStringLiteral(
                "\u4F20\u7968\u524D\u91CD\u65B0\u63E1\u624B\uFF1A%1")
            .arg(outcome.taskId);
    return outcome;
}

TicketTransferStoredStartOutcome
TicketTransferWorkflowService::startStoredTicket(
        const TicketTransferStoredStartRequest &request)
{
    TicketTransferStoredStartOutcome outcome;
    const SystemTicketDto ticket = m_store->ticketById(request.taskId);
    const QByteArray jsonBytes = m_store->rawJsonById(request.taskId);

    TicketTransferStartRequest start;
    start.taskId = request.taskId;
    start.jsonBytes = jsonBytes;
    start.ticket = ticket;
    start.session = request.session;
    start.trigger = request.automatic
            ? TicketTransferTrigger::Automatic
            : TicketTransferTrigger::Manual;
    start.anotherTransferActive = m_operation.isActive();
    start.peerWorkflowBusy = request.peerWorkflowBusy;
    start.failInsteadOfDeferring = request.failInsteadOfDeferring;
    outcome.decision = TicketTransferUseCase::evaluateStart(start);
    if (!outcome.decision.allowed) {
        if (outcome.decision.updateTicketState) {
            m_store->updateTransferState(
                        request.taskId,
                        outcome.decision.ticketState,
                        outcome.decision.ticketError);
        }
        return outcome;
    }

    m_operation.begin(
                request.taskId,
                jsonBytes,
                request.debugFrameChunkSize,
                request.currentKeySessionId);
    m_store->updateTransferTriggerSource(
                request.taskId,
                request.automatic
                    ? QStringLiteral("automatic")
                    : QStringLiteral("manual"));
    m_store->updateTransferState(
                request.taskId, TicketTransferState::Sending);

    outcome.effect = effect(
                TicketTransferWorkflowAction::SendSetCom,
                request.taskId);
    outcome.effect.handled = true;
    outcome.effect.statusMessage = QStringLiteral(
                "%1\u4F20\u7968\u524D\u91CD\u65B0\u63E1\u624B\uFF1A%2")
            .arg(request.automatic
                    ? QStringLiteral("\u81EA\u52A8")
                    : QStringLiteral("\u6267\u884C"))
            .arg(request.taskId);
    return outcome;
}

bool TicketTransferWorkflowService::hasActiveOperation() const
{
    return m_operation.isActive();
}

bool TicketTransferWorkflowService::hasPendingHandshake() const
{
    return m_operation.hasPendingHandshake();
}

bool TicketTransferWorkflowService::isAwaitingDeviceConfirmation() const
{
    return m_operation.isAwaitingDeviceConfirmation();
}

QString TicketTransferWorkflowService::activeTaskId() const
{
    return m_operation.taskId;
}

int TicketTransferWorkflowService::activeSessionId() const
{
    return m_operation.keySessionId;
}

bool TicketTransferWorkflowService::hasActiveWorkbenchTransaction() const
{
    return m_workbenchTransaction.isActive();
}

QString TicketTransferWorkflowService::activeWorkbenchTaskId() const
{
    return m_workbenchTransaction.taskId;
}

int TicketTransferWorkflowService::activeWorkbenchSessionId() const
{
    return m_workbenchTransaction.keySessionId;
}

int TicketTransferWorkflowService::activeWorkbenchBridgeEpoch() const
{
    return m_workbenchTransaction.bridgeEpoch;
}
bool TicketTransferWorkflowService::matchesWorkbenchTransaction(
        int sessionId,
        int bridgeEpoch) const
{
    return m_workbenchTransaction.matches(sessionId, bridgeEpoch);
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::acceptHandshakeAck(int currentKeySessionId)
{
    if (!m_operation.hasPendingHandshake()
            || m_operation.handshakeJsonBytes.isEmpty()
            || m_operation.keySessionId <= 0
            || m_operation.keySessionId != currentKeySessionId) {
        return TicketTransferWorkflowEffect();
    }

    TicketTransferWorkflowEffect result = effect(
                TicketTransferWorkflowAction::SendTicket,
                m_operation.handshakeTaskId);
    result.handled = true;
    result.jsonBytes = m_operation.handshakeJsonBytes;
    result.debugFrameChunkSize = m_operation.handshakeDebugChunk;
    result.statusMessage = QStringLiteral(
                "\u4F20\u7968\u524D\u63E1\u624B\u5DF2\u786E\u8BA4\uFF0C"
                "\u5F00\u59CB\u53D1\u9001 TICKET \u5E27\uFF1A%1")
            .arg(result.taskId);
    m_operation.clearHandshake();
    return result;
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::failHandshake(const QString &reason)
{
    if (!m_operation.hasPendingHandshake()) {
        return TicketTransferWorkflowEffect();
    }

    const QString taskId = m_operation.handshakeTaskId;
    TicketTransferWorkflowEffect result = effect(
                TicketTransferWorkflowAction::CompleteFailure,
                taskId);
    result.handled = true;
    result.statusMessage = reason;
    m_store->updateTransferState(
                taskId, TicketTransferState::Failed, reason);
    m_operation.clear();
    if (m_workbenchTransaction.isActive()) {
        result.completeDeferredResponse = true;
        result.deferredMessage = reason;
        clearWorkbenchTransaction(&result, false);
    }
    return result;
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::recordFrameProgress(int currentKeySessionId)
{
    if (!m_operation.isActive()
            || m_operation.keySessionId <= 0
            || m_operation.keySessionId != currentKeySessionId) {
        return TicketTransferWorkflowEffect();
    }

    m_store->updateTransferState(
                m_operation.taskId, TicketTransferState::Sending);
    TicketTransferWorkflowEffect result = effect(
                TicketTransferWorkflowAction::None,
                m_operation.taskId);
    result.handled = true;
    return result;
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::finishFrames(
        int currentKeySessionId,
        int eventBridgeEpoch)
{
    TicketTransferResultRequest request;
    request.taskId = m_operation.taskId;
    request.operationSessionId = m_operation.keySessionId;
    request.currentSessionId = currentKeySessionId;
    const TicketTransferFinishedDecision decision =
            TicketTransferUseCase::evaluateFinished(request);
    if (!decision.handle) {
        return TicketTransferWorkflowEffect();
    }

    const QString taskId = m_operation.taskId;
    const int sessionId = m_operation.keySessionId;
    const bool trackedTicket = m_store->ticketById(taskId).valid;
    const bool matchingWorkbench =
            m_workbenchTransaction.matches(sessionId, eventBridgeEpoch);
    if (!trackedTicket) {
        const QString reason = QStringLiteral(
                    "\u4F20\u7968\u53D1\u9001\u5B8C\u6210\uFF0C"
                    "\u4F46\u8FD0\u884C\u8BB0\u5F55\u4E0D\u5B58\u5728\uFF0C"
                    "\u65E0\u6CD5\u6267\u884C Q_TASK \u786E\u8BA4");
        TicketTransferWorkflowEffect result = effect(
                    TicketTransferWorkflowAction::CompleteFailure,
                    taskId);
        result.handled = true;
        result.statusMessage = reason;
        result.completeDeferredResponse = true;
        result.deferredMessage = reason;
        result.tryNextAutomatic = true;
        m_operation.clear();
        if (matchingWorkbench) {
            clearWorkbenchTransaction(&result, false);
        }
        return result;
    }

    m_operation.waitForDeviceConfirmation();
    TicketTransferWorkflowEffect result = effect(
                TicketTransferWorkflowAction::QueryTasks,
                taskId);
    result.handled = true;
    result.statusMessage = QStringLiteral(
                "\u4F20\u7968\u5E27\u53D1\u9001\u5B8C\u6210\uFF0C"
                "\u6B63\u5728\u901A\u8FC7 Q_TASK \u786E\u8BA4"
                "\u94A5\u5319\u4EFB\u52A1\uFF1A%1")
            .arg(taskId);
    result.scheduleCancelReconcile = true;
    return result;
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::verifyDevicePresence(
        const QList<KeyTaskDto> &tasks,
        int currentKeySessionId,
        int eventBridgeEpoch,
        qint64 observedAtMs)
{
    const bool ownsWorkbench = m_workbenchTransaction.isActive()
            && m_workbenchTransaction.taskId == m_operation.taskId;
    TicketTransferConfirmationRequest request;
    request.awaitingDeviceConfirmation =
            m_operation.isAwaitingDeviceConfirmation();
    request.sameKeySession = m_operation.keySessionId > 0
            && m_operation.keySessionId == currentKeySessionId
            && (!ownsWorkbench
                || m_workbenchTransaction.matches(
                    m_operation.keySessionId, eventBridgeEpoch));
    request.taskPresent = taskExists(tasks, m_operation.taskId);
    const TicketTransferConfirmationDecision decision =
            TicketTransferUseCase::evaluateConfirmation(request);
    if (decision.action == TicketTransferConfirmationAction::Ignore) {
        return TicketTransferWorkflowEffect();
    }

    const QString taskId = m_operation.taskId;
    const bool confirmed = decision.action
            == TicketTransferConfirmationAction::ConfirmSuccess;

    m_store->updateTransferState(
                taskId,
                decision.ticketState,
                confirmed ? QString() : decision.message);
    if (confirmed) {
        m_store->refreshKeyConfirmedAt(
                    QList<QString>{taskId}, observedAtMs);
    }

    TicketTransferWorkflowEffect result = effect(
                confirmed
                    ? TicketTransferWorkflowAction::CompleteSuccess
                    : TicketTransferWorkflowAction::CompleteFailure,
                taskId);
    result.handled = true;
    result.statusMessage = decision.message;
    result.completeDeferredResponse = ownsWorkbench;
    result.deferredSuccess = confirmed;
    result.deferredMessage = decision.message;
    result.refreshWorkbench = true;
    result.scheduleCancelReconcile = true;
    m_operation.clearActive();
    if (ownsWorkbench) {
        clearWorkbenchTransaction(&result, false);
    }
    return result;
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::failDeviceConfirmation(
        const QString &reason)
{
    if (!m_operation.isAwaitingDeviceConfirmation()) {
        return TicketTransferWorkflowEffect();
    }

    const QString taskId = m_operation.taskId;
    const bool ownsWorkbench = m_workbenchTransaction.isActive()
            && m_workbenchTransaction.taskId == taskId;
    m_store->updateTransferState(
                taskId, TicketTransferState::Failed, reason);
    TicketTransferWorkflowEffect result = effect(
                TicketTransferWorkflowAction::CompleteFailure,
                taskId);
    result.handled = true;
    result.statusMessage = reason;
    result.completeDeferredResponse = true;
    result.deferredMessage = reason;
    result.refreshWorkbench = true;
    m_operation.clearActive();
    if (ownsWorkbench) {
        clearWorkbenchTransaction(&result, false);
    }
    return result;
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::failTransfer(
        const QString &reason,
        int currentKeySessionId,
        int eventBridgeEpoch)
{
    const QString taskId = m_operation.taskId;
    const int sessionId = m_operation.keySessionId;
    TicketTransferFailureRequest request;
    request.taskId = taskId;
    request.operationSessionId = sessionId;
    request.currentSessionId = currentKeySessionId;
    request.failureMessage = reason;
    request.ticket = m_store->ticketById(taskId);
    const TicketTransferFailureDecision decision =
            TicketTransferUseCase::evaluateFailure(request);
    if (decision.action == TicketTransferFailureAction::Ignore) {
        return TicketTransferWorkflowEffect();
    }

    TicketTransferWorkflowEffect result = effect(
                decision.action
                        == TicketTransferFailureAction::QueueAutomaticRetry
                    ? TicketTransferWorkflowAction::RetryAutomatically
                    : TicketTransferWorkflowAction::CompleteFailure,
                taskId);
    result.handled = true;
    result.statusMessage = decision.statusMessage;
    result.completeDeferredResponse = decision.completeDeferredResponse;
    result.deferredMessage = decision.deferredMessage;
    result.automaticRetryReason = decision.automaticRetryReason;
    if (decision.ticketState != TicketTransferState::Unknown) {
        m_store->updateTransferState(
                    taskId,
                    decision.ticketState,
                    decision.ticketError);
    }
    m_operation.clearActive();
    if (m_workbenchTransaction.matches(sessionId, eventBridgeEpoch)) {
        clearWorkbenchTransaction(&result, false);
    }
    return result;
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::interrupt(const QString &reason)
{
    if (!m_operation.isActive()) {
        return TicketTransferWorkflowEffect();
    }

    const QString taskId = m_operation.taskId;
    m_store->updateTransferState(
                taskId, TicketTransferState::Failed, reason);
    TicketTransferWorkflowEffect result = effect(
                TicketTransferWorkflowAction::CompleteFailure,
                taskId);
    result.handled = true;
    result.statusMessage = reason;
    m_operation.clear();
    if (m_workbenchTransaction.isActive()) {
        result.completeDeferredResponse = true;
        result.deferredMessage = reason;
        clearWorkbenchTransaction(&result, false);
    }
    return result;
}

TicketTransferWorkflowEffect
TicketTransferWorkflowService::expireWorkbenchTransaction(
        const QString &reason)
{
    if (!m_workbenchTransaction.isActive()) {
        return TicketTransferWorkflowEffect();
    }

    const QString taskId = m_workbenchTransaction.taskId;
    TicketTransferWorkflowEffect result = effect(
                TicketTransferWorkflowAction::CompleteFailure,
                taskId);
    result.handled = true;
    result.completeDeferredResponse = true;
    result.deferredMessage = reason;
    result.statusMessage = reason;
    m_store->updateTransferState(
                taskId, TicketTransferState::Failed, reason);
    clearWorkbenchTransaction(&result, true);
    return result;
}

QList<TicketTransferWorkflowEffect>
TicketTransferWorkflowService::reconcileObservedTasks(
        const QList<KeyTaskDto> &tasks,
        qint64 observedAtMs)
{
    QList<TicketTransferWorkflowEffect> effects;
    QList<QString> confirmedTaskIds;
    for (const KeyTaskDto &task : tasks) {
        const QString taskId =
                VerifiedKeyTaskRemoval::decimalTaskId(task.taskId);
        const SystemTicketDto ticket = m_store->ticketById(taskId);
        const TicketKeyObservationDecision decision =
                TicketTransferUseCase::evaluateKeyObservation(ticket);
        if (!decision.confirmed) {
            continue;
        }

        confirmedTaskIds.append(taskId);
        TicketTransferWorkflowEffect result = effect(
                    TicketTransferWorkflowAction::CompleteSuccess,
                    taskId);
        result.handled = true;
        result.completeDeferredResponse = true;
        result.deferredSuccess = true;
        result.deferredMessage = QStringLiteral(
                    "\u4F20\u7968\u6210\u529F\uFF0C"
                    "\u5DF2\u786E\u8BA4\u4EFB\u52A1\u5B58\u5728"
                    "\u4E8E\u94A5\u5319\u4E2D");
        if (decision.reconcileTransferSuccess) {
            m_store->updateTransferState(
                        taskId, TicketTransferState::Success);
            result.statusMessage = QStringLiteral(
                        "\u68C0\u6D4B\u5230\u94A5\u5319\u4E2D\u5DF2\u5B58\u5728"
                        "\u7CFB\u7EDF\u7968\uFF0C\u5DF2\u5C06\u5185\u5B58\u72B6\u6001"
                        "\u7EA0\u6B63\u4E3A\u201C\u5DF2\u4F20\u7968\u5230\u94A5\u5319\u201D\uFF1A%1")
                    .arg(taskId);
        }
        effects.append(result);
    }
    m_store->refreshKeyConfirmedAt(confirmedTaskIds, observedAtMs);
    return effects;
}

QStringList
TicketTransferWorkflowService::recoverAutomaticWhenKeyEmpty(
        const QList<KeyTaskDto> &tasks,
        bool automaticEnabled,
        bool returnActive)
{
    QStringList requeuedTaskIds;
    if (!automaticEnabled
            || !tasks.isEmpty()
            || m_operation.isActive()
            || returnActive) {
        return requeuedTaskIds;
    }

    const QList<SystemTicketDto> tickets = m_store->tickets();
    for (const SystemTicketDto &ticket : tickets) {
        const TicketAutoRecoveryDecision decision =
                TicketTransferUseCase::evaluateAutomaticRecovery(ticket);
        if (!decision.requeue) {
            continue;
        }
        m_store->updateTransferState(
                    ticket.taskId, TicketTransferState::AutoPending);
        requeuedTaskIds.append(ticket.taskId);
    }
    return requeuedTaskIds;
}

TicketAutoTransferSelectionDecision
TicketTransferWorkflowService::selectAutomaticCandidate(
        bool automaticEnabled,
        bool peerWorkflowBusy,
        const QList<SystemTicketDto> &tickets) const
{
    TicketAutoTransferSelectionRequest request;
    request.httpTransactionActive = m_workbenchTransaction.isActive();
    request.enabled = automaticEnabled;
    request.operationBusy = m_operation.isActive() || peerWorkflowBusy;
    request.tickets = tickets;
    return TicketTransferUseCase::selectAutomaticCandidate(request);
}

TicketTransferWorkflowEffect TicketTransferWorkflowService::effect(
        TicketTransferWorkflowAction action,
        const QString &taskId) const
{
    TicketTransferWorkflowEffect result;
    result.action = action;
    result.taskId = taskId;
    result.operationSessionId = m_operation.keySessionId;
    return result;
}

void TicketTransferWorkflowService::clearWorkbenchTransaction(
        TicketTransferWorkflowEffect *result,
        bool clearActiveTransfer)
{
    const QString taskId = m_workbenchTransaction.taskId;
    const int sessionId = m_workbenchTransaction.keySessionId;
    if (clearActiveTransfer
            && !taskId.isEmpty()
            && m_operation.taskId == taskId
            && m_operation.keySessionId == sessionId) {
        m_operation.clear();
    }
    m_workbenchTransaction.clear();
    if (result) {
        result->stopSafetyTimer = true;
    }
}
