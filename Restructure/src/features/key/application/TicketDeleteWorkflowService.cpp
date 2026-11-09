#include "features/key/application/TicketDeleteWorkflowService.h"

#include "features/key/application/TicketStore.h"
#include "features/key/application/VerifiedKeyTaskRemoval.h"
#include "features/key/domain/TicketReturnState.h"

TicketDeleteWorkflowService::TicketDeleteWorkflowService(
        TicketStore *store,
        QObject *parent)
    : QObject(parent)
    , m_store(store)
{
}

TicketDeleteStartOutcome TicketDeleteWorkflowService::start(
        const TicketDeleteStartRequest &request,
        const QString &taskId,
        const QByteArray &taskIdRaw,
        int keySessionId,
        bool allowsRetransfer)
{
    TicketDeleteStartOutcome outcome;
    outcome.decision = TicketDeleteUseCase::evaluateStart(request);
    if (!outcome.decision.allowed) {
        return outcome;
    }

    m_operation.begin(taskId,
                      taskIdRaw,
                      request.origin,
                      keySessionId,
                      allowsRetransfer);
    outcome.effect = effect(TicketDeleteWorkflowAction::SendDelete);
    return outcome;
}

TicketDeleteWorkflowEffect
TicketDeleteWorkflowService::beginAutoReturnCleanup(
        const QString &taskId,
        const QByteArray &taskIdRaw,
        int keySessionId,
        bool allowsRetransfer)
{
    if (taskId.trimmed().isEmpty() || taskIdRaw.isEmpty()) {
        return TicketDeleteWorkflowEffect();
    }

    m_store->updateReturnState(taskId, TicketReturnState::Cleanup);
    m_operation.begin(taskId,
                      taskIdRaw,
                      TicketDeleteOrigin::AutoReturnCleanup,
                      keySessionId,
                      allowsRetransfer);
    return effect(TicketDeleteWorkflowAction::SendDelete);
}

TicketDeleteWorkflowEffect
TicketDeleteWorkflowService::beginCaptureCleanup(
        const QString &taskId,
        const QByteArray &taskIdRaw,
        int keySessionId)
{
    if (taskId.trimmed().isEmpty() || taskIdRaw.size() != 16) {
        return TicketDeleteWorkflowEffect();
    }
    m_operation.begin(taskId, taskIdRaw,
                      TicketDeleteOrigin::CaptureCleanup,
                      keySessionId, false);
    return effect(TicketDeleteWorkflowAction::SendDelete);
}

bool TicketDeleteWorkflowService::hasActiveOperation() const
{
    return m_operation.isActive();
}

bool TicketDeleteWorkflowService::hasDeviceIdentity() const
{
    return m_operation.hasDeviceIdentity();
}

QString TicketDeleteWorkflowService::activeTaskId() const
{
    return m_operation.taskId;
}

TicketDeleteOrigin TicketDeleteWorkflowService::activeOrigin() const
{
    return m_operation.origin;
}

void TicketDeleteWorkflowService::recordDeleteDispatched()
{
    if (m_operation.isActive()) {
        m_operation.deleteCommandDispatched = true;
    }
}

void TicketDeleteWorkflowService::acceptDeleteAck()
{
    if (m_operation.isActive()) {
        m_operation.deleteCommandDispatched = true;
        m_operation.deleteCommandAcknowledged = true;
    }
}

TicketDeleteWorkflowEffect TicketDeleteWorkflowService::interrupt(
        TicketDeleteInterruptionCause cause)
{
    if (!m_operation.isActive()) {
        return TicketDeleteWorkflowEffect();
    }

    if (m_operation.origin
            == TicketDeleteOrigin::AdminClearUnregisteredTask) {
        QString message;
        switch (cause) {
        case TicketDeleteInterruptionCause::Timeout:
            message = QStringLiteral(
                        "未登记任务清理未完成（DEL 超时），请确认钥匙状态后再次点击清除");
            break;
        case TicketDeleteInterruptionCause::Nak:
            message = QStringLiteral(
                        "未登记任务清理未完成（DEL 被拒绝），请确认钥匙状态后再次点击清除");
            break;
        case TicketDeleteInterruptionCause::KeyRemoved:
            message = QStringLiteral(
                        "未登记任务清理中断（钥匙已移除），请重新放回钥匙后再次点击清除");
            break;
        }
        TicketDeleteWorkflowEffect result = effect(
                    TicketDeleteWorkflowAction::AwaitUserRetry, message);
        m_operation.clear();
        return result;
    }

    QString detail;
    TicketDeleteWorkflowAction action =
            TicketDeleteWorkflowAction::WaitForCommand;
    switch (cause) {
    case TicketDeleteInterruptionCause::Timeout:
        detail = QStringLiteral(
                    "DEL 超时，等待后续 Q_TASK 对账确认钥匙任务是否已删除");
        break;
    case TicketDeleteInterruptionCause::Nak:
        detail = QStringLiteral(
                    "DEL 收到 NAK，等待后续 Q_TASK 对账确认钥匙任务状态");
        break;
    case TicketDeleteInterruptionCause::KeyRemoved:
        detail = QStringLiteral(
                    "钥匙已移除，删除验证已中断，等待重新放回后继续对账");
        action = TicketDeleteWorkflowAction::WaitForKey;
        break;
    }
    if (m_operation.origin == TicketDeleteOrigin::AutoReturnCleanup) {
        m_store->updateReturnState(
                    m_operation.taskId, TicketReturnState::Cleanup, detail);
    }
    // A command timeout or NAK does not replace the physical key. Keep the
    // session correlation so a fresh Q_TASK can still prove DEL outcome.
    if (cause == TicketDeleteInterruptionCause::KeyRemoved) {
        m_operation.invalidateKeySession();
    }
    return effect(action, detail);
}

TicketDeleteWorkflowEffect TicketDeleteWorkflowService::verify(
        const QList<KeyTaskDto> &tasks,
        const TicketDeleteRuntimeObservation &runtime)
{
    if (!m_operation.hasDeviceIdentity()) {
        return TicketDeleteWorkflowEffect();
    }

    const QString taskId = m_operation.taskId;
    const TicketDeleteOrigin origin = m_operation.origin;
    const SystemTicketDto ticket = m_store->ticketById(taskId);
    bool sameSession = m_operation.keySessionId > 0
            && runtime.currentKeySessionId == m_operation.keySessionId;

    const bool recoverableCleanup = origin == TicketDeleteOrigin::AutoReturnCleanup
            || origin == TicketDeleteOrigin::CaptureCleanup;
    const bool dispatchedManualDelete =
            (origin == TicketDeleteOrigin::ManualDelete
             || origin == TicketDeleteOrigin::ManualCaptureDelete)
            && m_operation.deleteCommandDispatched;
    // A reconnect creates a new serial session. For cleanup whose backend
    // upload already succeeded, the first fresh ready Q_TASK becomes the new
    // authority: presence retries DEL and absence completes cleanup.
    if (!sameSession && (recoverableCleanup || dispatchedManualDelete)
            && runtime.keyReady
            && runtime.currentKeySessionId > 0) {
        m_operation.keySessionId = runtime.currentKeySessionId;
        sameSession = true;
    }

    KeyTaskRemovalObservation removalObservation;
    removalObservation.phase = KeyTaskRemovalPhase::AfterDelete;
    removalObservation.sameKeySession = sameSession;
    removalObservation.taskPresent = VerifiedKeyTaskRemoval::containsRawTask(
                tasks, m_operation.taskIdRaw);
    const KeyTaskRemovalProof proof =
            VerifiedKeyTaskRemoval::evaluate(removalObservation);

    TicketDeleteObservation observation;
    observation.origin = origin;
    observation.sameKeySession =
            proof != KeyTaskRemovalProof::SessionChanged;
    observation.taskStillExists =
            proof == KeyTaskRemovalProof::TaskStillPresent;
    observation.ticketExists = ticket.valid;
    observation.returnUploadCompleted = ticket.valid
            && isTicketReturnCleanup(
                ticketReturnStateFromStorage(ticket.returnState));
    observation.allowsRetransfer = m_operation.allowsRetransfer;
    const TicketDeleteDecision decision =
            TicketDeleteUseCase::evaluate(observation);

    if (decision.action == TicketDeleteAction::RetryDelete) {
        return retry(
                    QStringLiteral(
                        "删除后验证发现钥匙任务仍存在，重新发送 DEL 继续清理"),
                    runtime);
    }

    TicketDeleteWorkflowAction action = TicketDeleteWorkflowAction::None;
    QString message;
    switch (decision.action) {
    case TicketDeleteAction::Ignore:
    case TicketDeleteAction::RetryDelete:
        break;
    case TicketDeleteAction::AbortSessionChanged:
        action = TicketDeleteWorkflowAction::AbortSessionChanged;
        message = QStringLiteral("钥匙会话已变化，本次删除确认已结束");
        if (origin == TicketDeleteOrigin::AutoReturnCleanup && ticket.valid) {
            m_store->updateReturnState(
                        taskId,
                        TicketReturnState::DeleteFailed,
                        QStringLiteral("删除确认期间钥匙会话发生变化，请重新执行回传"));
        }
        break;
    case TicketDeleteAction::AwaitUserRetry:
        action = TicketDeleteWorkflowAction::AwaitUserRetry;
        message = QStringLiteral("未登记任务仍在钥匙中，请再次点击清除");
        break;
    case TicketDeleteAction::NotifyBackendTermination:
        action = TicketDeleteWorkflowAction::NotifyBackendTermination;
        m_store->markKeyTaskDeleted(taskId);
        break;
    case TicketDeleteAction::RemoveTicket:
        action = TicketDeleteWorkflowAction::RemoveTicket;
        m_store->removeTicket(taskId);
        break;
    case TicketDeleteAction::MarkKeyCleared:
        action = TicketDeleteWorkflowAction::MarkKeyCleared;
        m_store->markKeyTaskDeleted(taskId);
        break;
    case TicketDeleteAction::ConfirmedAbsent:
        action = TicketDeleteWorkflowAction::ConfirmedAbsent;
        break;
    }

    TicketDeleteWorkflowEffect result = effect(action, message);
    result.refreshWorkbench = decision.refreshWorkbench;
    result.continueReturnSweep = decision.continueReturnSweep;
    result.sameKeySession = sameSession;
    m_operation.clear();
    return result;
}

void TicketDeleteWorkflowService::clear()
{
    m_operation.clear();
}

void TicketDeleteWorkflowService::resetRetryEvidence()
{
    m_operation.retryCount = 0;
    m_operation.inFlightStallCount = 0;
}

TicketDeleteWorkflowEffect TicketDeleteWorkflowService::retry(
        const QString &reason,
        const TicketDeleteRuntimeObservation &runtime)
{
    TicketDeleteRetryRequest request;
    request.origin = m_operation.origin;
    request.hasDeviceIdentity = m_operation.hasDeviceIdentity();
    request.keyReady = runtime.keyReady;
    request.commandBusy = runtime.commandBusy;
    request.retryCount = m_operation.retryCount;
    request.inFlightStallCount = m_operation.inFlightStallCount;
    const TicketDeleteRetryDecision decision =
            TicketDeleteUseCase::evaluateRetry(request);

    const bool returnCleanup = m_operation.origin
            == TicketDeleteOrigin::AutoReturnCleanup;
    switch (decision.action) {
    case TicketDeleteRetryAction::InvalidContext:
        return TicketDeleteWorkflowEffect();
    case TicketDeleteRetryAction::WaitForKey:
        if (returnCleanup) {
            m_store->updateReturnState(
                        m_operation.taskId,
                        TicketReturnState::Cleanup,
                        reason.isEmpty()
                            ? QStringLiteral("等待钥匙重新就绪后继续删除验证")
                            : reason);
        }
        return effect(TicketDeleteWorkflowAction::WaitForKey, reason);
    case TicketDeleteRetryAction::WaitForCommand:
        m_operation.inFlightStallCount =
                decision.nextInFlightStallCount;
        if (decision.forceRetryLimit) {
            m_operation.retryCount = 3;
        }
        if (returnCleanup) {
            m_store->updateReturnState(
                        m_operation.taskId,
                        TicketReturnState::Cleanup,
                        reason.isEmpty()
                            ? QStringLiteral("等待当前协议窗口收口后继续删除验证")
                            : reason);
        }
        return effect(TicketDeleteWorkflowAction::WaitForCommand, reason);
    case TicketDeleteRetryAction::ExhaustedManualDelete:
        m_store->updateAdminDeleteStage(
                    m_operation.taskId,
                    SystemTicketDto::AdminDeleteStage::TerminationFailed);
        m_store->updateAdminDeleteError(
                    m_operation.taskId,
                    QStringLiteral(
                        "DEL 串口命令重试 3 次仍未确认删除，钥匙任务状态未知，请人工确认"));
        {
            TicketDeleteWorkflowEffect result = effect(
                        TicketDeleteWorkflowAction::ExhaustedManualDelete,
                        reason);
            m_operation.clear();
            return result;
        }
    case TicketDeleteRetryAction::ExhaustedUnregisteredClear:
        {
            TicketDeleteWorkflowEffect result = effect(
                        TicketDeleteWorkflowAction::ExhaustedUnregisteredClear,
                        reason);
            m_operation.clear();
            return result;
        }
    case TicketDeleteRetryAction::ExhaustedReturnCleanup:
        m_store->updateReturnState(
                    m_operation.taskId,
                    TicketReturnState::DeleteFailed,
                    QStringLiteral("DEL 已重试 3 次仍未确认删除，请人工处理"));
        {
            TicketDeleteWorkflowEffect result = effect(
                        TicketDeleteWorkflowAction::ExhaustedReturnCleanup,
                        reason);
            m_operation.clear();
            return result;
        }
    case TicketDeleteRetryAction::SendRetry:
        m_operation.retryCount = decision.nextRetryCount;
        m_operation.keySessionId = runtime.currentKeySessionId;
        if (returnCleanup) {
            m_store->updateReturnState(
                        m_operation.taskId,
                        TicketReturnState::Cleanup,
                        QStringLiteral("DEL 重试第 %1/3 次：%2")
                            .arg(m_operation.retryCount)
                            .arg(reason.isEmpty()
                                ? QStringLiteral("继续确认钥匙任务清理结果")
                                : reason));
        }
        return effect(TicketDeleteWorkflowAction::SendDelete, reason);
    }

    return TicketDeleteWorkflowEffect();
}

TicketDeleteWorkflowEffect TicketDeleteWorkflowService::effect(
        TicketDeleteWorkflowAction action,
        const QString &message) const
{
    TicketDeleteWorkflowEffect result;
    result.action = action;
    result.origin = m_operation.origin;
    result.taskId = m_operation.taskId;
    result.taskIdRaw = m_operation.taskIdRaw;
    result.message = message;
    result.retryCount = m_operation.retryCount;
    return result;
}
