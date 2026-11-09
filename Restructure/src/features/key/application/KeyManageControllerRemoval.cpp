/**
 * @file KeyManageControllerRemoval.cpp
 * @brief L1 execution for return cleanup, workbench cancellation, and admin removal.
 *
 * TicketReturnWorkflowService, TicketCancelWorkflowService, and
 * TicketDeleteWorkflowService remain independent L2 owners. This file only
 * coordinates their explicit effects around the shared physical proof:
 * fresh Q_TASK presence, DEL acknowledgement, then same-session Q_TASK absence.
 */
#include "features/key/application/KeyManageController.h"

#include <QDebug>
#include <QTimer>

#include "core/ConfigManager.h"
#include "features/key/application/TicketTransferWorkflowService.h"
#include "features/key/application/AdminTicketTerminationService.h"
#include "features/key/application/KeyManageObservabilityService.h"
#include "features/key/application/KeyTaskObservationService.h"
#include "features/key/application/TicketCancelUseCase.h"
#include "features/key/application/TicketCancelWorkflowService.h"
#include "features/key/application/TicketDeleteUseCase.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/CaptureTicketWorkflowService.h"
#include "features/key/application/VerifiedKeyTaskRemoval.h"
#include "features/key/domain/TicketCancelState.h"
#include "features/key/domain/TicketReturnState.h"

namespace {

bool isSessionReadySnapshot(const KeySessionSnapshot &snapshot)
{
    return snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
}

bool isReturnCleanupState(const QString &state)
{
    return isTicketReturnCleanup(ticketReturnStateFromStorage(state));
}

} // namespace
void KeyManageController::failActiveReturnHandshake(const QString &reason)
{
    applyReturnEffect(
                m_ticketReturnWorkflow->interrupt(reason, m_keySessionId));
}


void KeyManageController::markReturnDeletePending(
        const QString &taskId,
        const QByteArray &taskIdRaw,
        int keySessionId)
{
    m_ticketDeleteWorkflow->beginAutoReturnCleanup(
                taskId,
                taskIdRaw,
                keySessionId);
}

void KeyManageController::finalizePendingReturnDelete(
        const QList<KeyTaskDto> &tasks)
{
    if (!m_ticketDeleteWorkflow->hasDeviceIdentity()) {
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    TicketDeleteRuntimeObservation runtime;
    runtime.currentKeySessionId = m_keySessionId;
    runtime.keyReady = isSessionReadySnapshot(snapshot);
    runtime.commandBusy =
            snapshot.commandInFlight || snapshot.recoveryWindowActive;
    const TicketDeleteWorkflowEffect effect =
            m_ticketDeleteWorkflow->verify(tasks, runtime);

    if (effect.action == TicketDeleteWorkflowAction::SendDelete) {
        CommandRequest request;
        request.id = CommandId::DeleteTask;
        request.opId = nextOpId();
        request.payload = effect.taskIdRaw;
        m_ticketDeleteWorkflow->recordDeleteDispatched();
        if (effect.origin == TicketDeleteOrigin::AutoReturnCleanup) {
            m_observability->writeBusinessEvent(
                        QStringLiteral("return"),
                        QStringLiteral("del-retry"),
                        QStringLiteral("started"),
                        effect.taskId,
                        m_keySessionId,
                        request.opId,
                        QStringLiteral("retry-%1").arg(effect.retryCount),
                        BusinessLogSeverity::Warning);
        }
        m_session->execute(request);
        emit statusMessage(
                QStringLiteral("任务 %1 删除验证未收口，已自动重发 DEL（第 %2/3 次）")
                        .arg(effect.taskId)
                        .arg(effect.retryCount));
        return;
    }
    if (effect.action == TicketDeleteWorkflowAction::WaitForKey
            || effect.action == TicketDeleteWorkflowAction::WaitForCommand) {
        emit statusMessage(effect.message);
        return;
    }
    if (effect.action == TicketDeleteWorkflowAction::ExhaustedManualDelete
            || effect.action
               == TicketDeleteWorkflowAction::ExhaustedUnregisteredClear
            || effect.action
               == TicketDeleteWorkflowAction::ExhaustedReturnCleanup) {
        if (effect.origin == TicketDeleteOrigin::CaptureCleanup) {
            applyCaptureEffect(m_captureWorkflow->completeVerifiedDelete(
                                   effect.taskId, false,
                                   QStringLiteral("DEL \u91cd\u8bd5 3 \u6b21\u4ecd\u672a\u786e\u8ba4\u5220\u9664")));
            return;
        }
        if (effect.action
                == TicketDeleteWorkflowAction::ExhaustedUnregisteredClear) {
            qWarning().noquote()
                    << QStringLiteral("[KeyManageController] AdminClearUnregisteredTask: DEL retry limit reached, current unregistered task remains visible in Q_TASK results taskId=%1")
                          .arg(effect.taskId);
        }
        emit statusMessage(
                QStringLiteral("任务 %1 删除验证失败：DEL 已重试 3 次仍未收口，请人工处理")
                        .arg(effect.taskId));
        return;
    }

    if (effect.origin == TicketDeleteOrigin::AutoReturnCleanup) {
        const bool cleanupClosed =
                effect.action == TicketDeleteWorkflowAction::RemoveTicket
                || effect.action == TicketDeleteWorkflowAction::MarkKeyCleared
                || effect.action == TicketDeleteWorkflowAction::ConfirmedAbsent;
        if (cleanupClosed) {
            m_observability->writeBusinessEvent(
                        QStringLiteral("return"),
                        QStringLiteral("q-task-after"),
                        effect.sameKeySession
                            ? QStringLiteral("delete-confirmed")
                            : QStringLiteral("cleanup-unconfirmed-session-change"),
                        effect.taskId,
                        m_keySessionId,
                        -1,
                        effect.message,
                        effect.sameKeySession
                            ? BusinessLogSeverity::Info
                            : BusinessLogSeverity::Warning);
        }
    }
    if (effect.origin == TicketDeleteOrigin::CaptureCleanup) {
        const bool deleted =
                effect.action == TicketDeleteWorkflowAction::ConfirmedAbsent;
        applyCaptureEffect(m_captureWorkflow->completeVerifiedDelete(
                               effect.taskId, deleted, effect.message));
        return;
    }
    if (effect.origin == TicketDeleteOrigin::ManualCaptureDelete
            && effect.action == TicketDeleteWorkflowAction::ConfirmedAbsent) {
        m_captureWorkflow->forgetVerifiedDeletedTask(effect.taskId);
        emit statusMessage(
                QStringLiteral("采码票已从钥匙中删除，不通知普通票终止接口：%1")
                        .arg(effect.taskId));
        return;
    }
    switch (effect.action) {
    case TicketDeleteWorkflowAction::None:
    case TicketDeleteWorkflowAction::SendDelete:
    case TicketDeleteWorkflowAction::WaitForKey:
    case TicketDeleteWorkflowAction::WaitForCommand:
    case TicketDeleteWorkflowAction::ExhaustedManualDelete:
    case TicketDeleteWorkflowAction::ExhaustedUnregisteredClear:
    case TicketDeleteWorkflowAction::ExhaustedReturnCleanup:
        break;
    case TicketDeleteWorkflowAction::AwaitUserRetry:
        emit statusMessage(
                QStringLiteral("%1：%2").arg(effect.message, effect.taskId));
        break;
    case TicketDeleteWorkflowAction::AbortSessionChanged:
        emit statusMessage(
                QStringLiteral("%1：%2").arg(effect.message, effect.taskId));
        break;
    case TicketDeleteWorkflowAction::NotifyBackendTermination:
        m_adminTerminationService->terminate(effect.taskId, m_keySessionId);
        emit statusMessage(
                QStringLiteral("钥匙任务已删除，正在通知工作台终止系统票：%1")
                        .arg(effect.taskId));
        break;
    case TicketDeleteWorkflowAction::RemoveTicket:
        if (effect.origin == TicketDeleteOrigin::AutoReturnCleanup) {
            m_ticketReturnWorkflow->removeProgressiveUploadEvidence(effect.taskId);
        }
        if (!effect.sameKeySession
                && effect.origin == TicketDeleteOrigin::AutoReturnCleanup) {
            emit statusMessage(
                    QStringLiteral("钥匙会话已变化，但回传已上传，本地运行记录已清除：%1")
                            .arg(effect.taskId));
        } else if (effect.origin
                   == TicketDeleteOrigin::AdminClearUnregisteredTask) {
            emit statusMessage(
                    QStringLiteral("钥匙未登记任务已清除：%1")
                            .arg(effect.taskId));
        } else {
            emit statusMessage(
                    QStringLiteral("回传完成，本地运行记录已清除：%1")
                            .arg(effect.taskId));
        }
        break;
    case TicketDeleteWorkflowAction::MarkKeyCleared:
        emit statusMessage(QStringLiteral("钥匙任务已删除，可再次传票"));
        break;
    case TicketDeleteWorkflowAction::ConfirmedAbsent:
        emit statusMessage(
                QStringLiteral("钥匙任务已确认删除：%1").arg(effect.taskId));
        break;
    }

    if (effect.refreshWorkbench) {
        emit workbenchRefreshRequested();
    }
    if (effect.continueReturnSweep) {
        QTimer::singleShot(400, this, [this, tasks]() {
            if (activeReturnChainTaskId().isEmpty()) {
                tryStartProgressiveStepReturnSweep(tasks);
                tryAutoReturnCompletedTicket(tasks);
            }
        });
    }
}
void KeyManageController::handleCancelDeferredRequestExpired(
        const QString &taskId)
{
    const TicketCancelExpiryOutcome outcome =
            m_ticketCancelWorkflow->expireDeferredRequest(taskId);
    if (outcome.action == TicketCancelExpiryAction::Ignore) {
        return;
    }

    if (outcome.action
            == TicketCancelExpiryAction::ContinueDeviceVerification) {
        m_observability->writeBusinessEvent(
                QStringLiteral("termination"),
                QStringLiteral("client-disconnected"),
                QStringLiteral("verification-continues"),
                outcome.taskId,
                m_keySessionId,
                -1,
                outcome.message,
                BusinessLogSeverity::Warning);
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] cancel client disconnected after DEL; continue device verification taskId=%1")
                      .arg(outcome.taskId);
        emit statusMessage(
                QStringLiteral("终止请求连接已断开，但钥匙删除已开始；系统将继续确认删除结果，可稍后重新点击终止：%1")
                        .arg(outcome.taskId));
        return;
    }

    qInfo().noquote()
            << QStringLiteral("[KeyManageController] cancel deferred request expired, cancel intent cleared taskId=%1")
                  .arg(outcome.taskId);
    emit statusMessage(
            QStringLiteral("撤销确认超时，已取消本次撤销，请重新点击终止任务：%1")
                    .arg(outcome.taskId));
}
// Adapts live workflow and key-session observations to the pure
// cancellation execution-window policy.
bool KeyManageController::canProcessCancelNow(
        QString *blockedReason) const
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    TicketCancelExecutionRequest request;
    request.cancelBusy = m_ticketCancelWorkflow->hasActiveDelete();
    request.transferBusy = m_ticketTransferWorkflow->hasActiveOperation();
    request.returnOrDeleteBusy =
            m_ticketReturnWorkflow->hasReturnWork()
            || m_ticketDeleteWorkflow->hasActiveOperation();
    request.keyReady =
            snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
    request.commandBusy = snapshot.commandInFlight;
    request.recoveryActive = snapshot.recoveryWindowActive;
    request.autoQueryBusy =
            m_keyTaskObservation->automaticQueryActive(m_keySessionId);

    const TicketCancelExecutionDecision decision =
            TicketCancelUseCase::evaluateExecution(request);
    if (blockedReason) {
        *blockedReason = decision.message;
    }
    return decision.allowed;
}

void KeyManageController::schedulePendingCancelReconcile(const QString &reason, int delayMs)
{
    if (m_cancelReconcileScheduled || !m_ticketCancelWorkflow->hasPendingWork()) {
        return;
    }

    m_cancelReconcileScheduled = true;
    const int effectiveDelayMs = delayMs > 0 ? delayMs : 300;
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] schedulePendingCancelReconcile reason=%1 delayMs=%2 currentSession=%3")
                  .arg(reason.isEmpty() ? QStringLiteral("<none>") : reason,
                       QString::number(effectiveDelayMs),
                       QString::number(m_keySessionId));

    QTimer::singleShot(effectiveDelayMs, this, [this]() {
        m_cancelReconcileScheduled = false;
        if (!m_ticketCancelWorkflow->hasPendingWork()) {
            return;
        }

        QString blockedReason;
        if (!canProcessCancelNow(&blockedReason)) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] pending cancel reconcile skipped reason=%1")
                          .arg(blockedReason.isEmpty() ? QStringLiteral("<none>") : blockedReason);
            return;
        }

        if (m_keySessionId <= 0) {
            return;
        }

        if (m_keyTaskObservation->beginAutomaticQuery(m_keySessionId)) {
            emit statusMessage(QStringLiteral("检测到待撤销任务，开始读取钥匙票列表进行对账"));
            onQueryTasksClicked();
        }
    });
}

void KeyManageController::tryProcessPendingCancelTasks(
        const QList<KeyTaskDto> &tasks)
{
    if (!m_ticketCancelWorkflow->hasPendingWork()
            || m_ticketCancelWorkflow->hasActiveDelete()) {
        return;
    }

    QString blockedReason;
    if (!canProcessCancelNow(&blockedReason)) {
        return;
    }

    const QList<TicketCancelWorkflowEffect> effects =
            m_ticketCancelWorkflow->beginPendingDeletes(
                tasks, m_keySessionId);
    for (const TicketCancelWorkflowEffect &effect : effects) {
        if (effect.action == TicketCancelWorkflowAction::MarkDone) {
            continue;
        }

        if (effect.action == TicketCancelWorkflowAction::Reject) {
            // Absence before DEL is not deletion proof, so H5 must not
            // terminate the backend task from this observation.
            m_observability->writeBusinessEvent(
                    QStringLiteral("termination"),
                    QStringLiteral("q-task-before"),
                    QStringLiteral("task-missing"),
                    effect.taskId,
                    m_keySessionId,
                    -1,
                    effect.message,
                    BusinessLogSeverity::Warning);
            m_ticketIngress->completeDeferredCancelResponse(
                        effect.taskId, false, effect.message);
            emit statusMessage(
                    QStringLiteral("%1：%2")
                            .arg(effect.message, effect.taskId));
            continue;
        }

        if (effect.action != TicketCancelWorkflowAction::SendDelete) {
            continue;
        }

        m_observability->writeBusinessEvent(
                QStringLiteral("termination"),
                QStringLiteral("q-task-before"),
                QStringLiteral("task-found"),
                effect.taskId,
                m_keySessionId);
        CommandRequest request;
        request.id = CommandId::DeleteTask;
        request.opId = nextOpId();
        request.payload = effect.taskIdRaw;
        m_observability->writeBusinessEvent(
                QStringLiteral("termination"),
                QStringLiteral("del-sent"),
                QStringLiteral("started"),
                effect.taskId,
                m_keySessionId,
                request.opId);
        m_session->execute(request);
        emit statusMessage(
                QStringLiteral("检测到待撤销任务仍在钥匙中，开始执行删除：%1")
                        .arg(effect.taskId));
        return;
    }
}

void KeyManageController::finalizePendingCancelDelete(
        const QList<KeyTaskDto> &tasks)
{
    if (!m_ticketCancelWorkflow->hasActiveDelete()) {
        return;
    }

    const TicketCancelWorkflowEffect effect =
            m_ticketCancelWorkflow->verifyActiveDelete(
                tasks, m_keySessionId);
    if (effect.action
            == TicketCancelWorkflowAction::RetryAfterSessionChange) {
        m_observability->writeBusinessEvent(
                QStringLiteral("termination"),
                QStringLiteral("q-task-after"),
                QStringLiteral("session-changed"),
                effect.taskId,
                m_keySessionId,
                -1,
                effect.message,
                BusinessLogSeverity::Warning);
        schedulePendingCancelReconcile(
                    QStringLiteral("stale-session-cleared"), 1500);
        return;
    }

    if (effect.action == TicketCancelWorkflowAction::Complete) {
        m_observability->writeBusinessEvent(
                QStringLiteral("termination"),
                QStringLiteral("q-task-after"),
                QStringLiteral("delete-confirmed"),
                effect.taskId,
                m_keySessionId);
        m_ticketIngress->completeDeferredCancelResponse(
                    effect.taskId, true, QStringLiteral("撤销票成功"));
        emit statusMessage(
                QStringLiteral("撤销任务已完成，钥匙侧任务已清理：%1")
                        .arg(effect.taskId));
        emit workbenchRefreshRequested();
        return;
    }

    if (effect.action != TicketCancelWorkflowAction::RetryTaskStillPresent) {
        return;
    }

    m_observability->writeBusinessEvent(
            QStringLiteral("termination"),
            QStringLiteral("q-task-after"),
            QStringLiteral("task-still-present"),
            effect.taskId,
            m_keySessionId,
            -1,
            effect.message,
            BusinessLogSeverity::Warning);
    emit statusMessage(
            QStringLiteral("撤销删除验证发现任务仍存在，等待后续继续清理：%1")
                    .arg(effect.taskId));
    schedulePendingCancelReconcile(
                QStringLiteral("cancel-verify-still-exists"), 1500);
}
bool KeyManageController::autoTransferEnabled() const
{
    return ConfigManager::instance()->value("ticket/autoTransferEnabled", false).toBool();
}

bool KeyManageController::isKeyPresenceFresh(const SystemTicketDto &ticket) const
{
    if (ticketTransferStateFromStorage(ticket.transferState) != TicketTransferState::Success) return false;
    return m_keyTaskObservation->isPresenceProofFresh(
                ticket.keyConfirmedAtMs, m_keySessionId);
}

void KeyManageController::reconcileSystemTicketsFromKeyTasks(
        const QList<KeyTaskDto> &tasks)
{
    const QList<TicketTransferWorkflowEffect> effects =
            m_ticketTransferWorkflow->reconcileObservedTasks(
                tasks, m_keyTaskObservation->observedAtMs());
    for (const TicketTransferWorkflowEffect &effect : effects) {
        m_observability->writeBusinessEvent(
                    QStringLiteral("transfer"),
                    QStringLiteral("q-task-after"),
                    QStringLiteral("task-confirmed"),
                    effect.taskId,
                    m_keySessionId);
        const bool completed = m_ticketIngress->completeDeferredResponse(
                    effect.taskId,
                    true,
                    effect.deferredMessage);
        if (completed) {
            emit workbenchRefreshRequested();
        }
        if (!effect.statusMessage.isEmpty()) {
            emit statusMessage(effect.statusMessage);
        }
    }
}

void KeyManageController::recoverAutoTransferWhenKeyEmpty(
        const QList<KeyTaskDto> &tasks)
{
    const QStringList requeued =
            m_ticketTransferWorkflow->recoverAutomaticWhenKeyEmpty(
                tasks,
                autoTransferEnabled(),
                m_ticketReturnWorkflow->hasReturnWork());
    for (const QString &taskId : requeued) {
        emit statusMessage(
                    QStringLiteral(
                        "\u94A5\u5319\u4E2D\u672A\u68C0\u6D4B\u5230\u7CFB\u7EDF\u7968\uFF0C"
                        "\u5DF2\u5C06\u4EFB\u52A1\u91CD\u65B0\u52A0\u5165"
                        "\u81EA\u52A8\u4F20\u7968\u961F\u5217\uFF1A%1")
                    .arg(taskId));
    }
    if (!requeued.isEmpty()) {
        tryAutoTransferPendingTicket();
    }
}

void KeyManageController::tryAutoCleanupReturnedTicket(const QList<KeyTaskDto> &tasks)
{
    if (m_ticketReturnWorkflow->hasReturnWork()) {
        return;
    }
    if (m_ticketDeleteWorkflow->hasActiveOperation()) {
        return;
    }
    if (m_ticketCancelWorkflow->hasActiveDelete()) {
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected || !snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        return;
    }

    for (const KeyTaskDto &task : tasks) {
        if (task.status != 0x02) {
            continue;
        }

        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (!ticket.valid) {
            continue;
        }
        if (hasTicketCancelIntent(ticketCancelStateFromStorage(ticket.cancelState))) {
            continue;
        }
        if (!isReturnCleanupState(ticket.returnState)) {
            continue;
        }

        const TicketDeleteWorkflowEffect effect =
                m_ticketDeleteWorkflow->beginAutoReturnCleanup(
                    taskId, task.taskId, m_keySessionId);

        CommandRequest req;
        req.id = CommandId::DeleteTask;
        req.opId = nextOpId();
        req.payload = effect.taskIdRaw;
        m_ticketDeleteWorkflow->recordDeleteDispatched();
        m_session->execute(req);
        emit statusMessage(QStringLiteral("检测到回传已成功但钥匙任务仍存在，自动继续清理钥匙任务：%1")
                               .arg(taskId));
        return;
    }
}
