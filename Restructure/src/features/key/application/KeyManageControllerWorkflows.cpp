/**
 * @file KeyManageControllerWorkflows.cpp
 * @brief L1 execution for transfer, ordinary return, and progressive return.
 *
 * The independent L2 workflow services own lifecycle state and decisions.
 * This file only gathers adapter observations and executes their explicit
 * serial, HTTP, scheduling, logging, and presentation effects.
 */
#include "features/key/application/KeyManageController.h"

#include <QDebug>
#include <QJsonObject>
#include <QTimer>

#include "core/ConfigManager.h"
#include "features/key/application/KeyManageObservabilityService.h"
#include "features/key/application/KeyTaskObservationService.h"
#include "features/key/application/TicketCancelWorkflowService.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/TicketReturnUploadService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketTransferWorkflowService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketCancelUseCase.h"
#include "features/key/domain/TicketCancelState.h"
#include "features/key/domain/TicketReturnState.h"
#include "features/key/domain/TicketTransferState.h"

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
bool isReturnInFlightState(const QString &state)
{
    return isTicketReturnInFlight(ticketReturnStateFromStorage(state));
}

bool shouldSkipAutoReturnForState(const QString &state)
{
    return blocksAutomaticReturn(ticketReturnStateFromStorage(state));
}

} // namespace
bool KeyManageController::progressiveStepReturnEnabled() const
{
    return ConfigManager::instance()
            ->value(QStringLiteral("ticket/progressiveStepReturnEnabled"), true)
            .toBool();
}

bool KeyManageController::isProgressiveStepTicket(const SystemTicketDto &ticket) const
{
    return ticket.valid && ticket.stepNum > 1;
}

// Builds a same-session candidate list only after the return policy grants
// ownership of the key workflow.
void KeyManageController::tryStartProgressiveStepReturnSweep(const QList<KeyTaskDto> &tasks)
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    ProgressiveReturnSweepStartRequest request;
    request.enabled = progressiveStepReturnEnabled();
    request.progressiveTaskActive = m_ticketReturnWorkflow->progressiveActive();
    request.progressiveHttpUploadActive =
            m_ticketReturnWorkflow->progressiveUploadActive();
    request.candidatesPending =
            m_ticketReturnWorkflow->progressiveCandidatesPending();
    request.returnChainActive = !activeReturnChainTaskId().isEmpty();
    request.transferActive = m_ticketTransferWorkflow->hasActiveOperation();
    request.cancelActive = m_ticketCancelWorkflow->hasActiveDelete();
    request.cancellationWorkPending =
            m_ticketCancelWorkflow->hasPendingWork();
    request.uploadConfigured = m_returnUploadService->isConfigured();
    request.sessionReady = isSessionReadySnapshot(snapshot);
    request.commandInFlight = snapshot.commandInFlight;
    request.recoveryWindowActive = snapshot.recoveryWindowActive;
    const ProgressiveReturnSweepStartDecision decision =
            m_ticketReturnWorkflow->evaluateProgressiveSweepStart(request);
    if (decision.action
            != ProgressiveReturnSweepStartAction::CollectCandidates) {
        return;
    }

    m_ticketReturnWorkflow->beginProgressiveSweep(m_keySessionId);
    for (const KeyTaskDto &task : tasks) {
        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        ProgressiveReturnCandidateRequest candidateRequest;
        candidateRequest.keyTaskStatus = task.status;
        candidateRequest.progressiveTicket =
                isProgressiveStepTicket(ticket);
        candidateRequest.keyPresenceFresh =
                isKeyPresenceFresh(ticket);
        candidateRequest.cancelActive =
                hasTicketCancelIntent(ticketCancelStateFromStorage(ticket.cancelState));
        candidateRequest.cleanupActive =
                isReturnCleanupState(ticket.returnState);
        const ProgressiveReturnCandidateDecision candidate =
                m_ticketReturnWorkflow->evaluateProgressiveCandidate(
                    candidateRequest);
        if (candidate.eligible) {
            m_ticketReturnWorkflow->addProgressiveCandidate(task.taskId);
        }
    }

    if (!m_ticketReturnWorkflow->progressiveCandidatesPending()) {
        m_ticketReturnWorkflow->clearProgressiveSweep();
        return;
    }
    startNextProgressiveStepReturn();
}

// Revalidates each queued task immediately before starting its SET_COM
// handshake so stale key or ticket observations cannot begin a return.
void KeyManageController::startNextProgressiveStepReturn()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    ProgressiveReturnSweepNextRequest request;
    request.progressiveTaskActive = m_ticketReturnWorkflow->progressiveActive();
    request.progressiveHttpUploadActive =
            m_ticketReturnWorkflow->progressiveUploadActive();
    request.sameKeySession =
            isCurrentKeySession(m_ticketReturnWorkflow->progressiveSweepSessionId());
    request.blockingWorkflowActive =
            m_ticketDeleteWorkflow->hasActiveOperation()
            || m_ticketCancelWorkflow->hasActiveDelete()
            || m_ticketTransferWorkflow->hasActiveOperation()
            || m_ticketCancelWorkflow->hasPendingWork();
    request.sessionReady = isSessionReadySnapshot(snapshot);
    request.commandInFlight = snapshot.commandInFlight;
    request.recoveryWindowActive = snapshot.recoveryWindowActive;
    const ProgressiveReturnSweepNextDecision decision =
            m_ticketReturnWorkflow->evaluateProgressiveSweepNext(request);
    switch (decision.action) {
    case ProgressiveReturnSweepNextAction::NoAction:
        return;
    case ProgressiveReturnSweepNextAction::StopSweep:
        clearProgressiveStepReturnContext(true);
        return;
    case ProgressiveReturnSweepNextAction::RetryAfterCommand:
        QTimer::singleShot(
                    150, this,
                    [this]() { startNextProgressiveStepReturn(); });
        return;
    case ProgressiveReturnSweepNextAction::SelectCandidate:
        break;
    }

    while (m_ticketReturnWorkflow->progressiveCandidatesPending()) {
        const QByteArray taskIdRaw =
                m_ticketReturnWorkflow->takeNextProgressiveCandidate();
        const QString taskId = taskIdFromRaw(taskIdRaw);
        quint8 taskStatus = 0xFF;
        const QByteArray currentRaw =
                m_keyTaskObservation->findTaskIdRaw(taskId, &taskStatus);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        ProgressiveReturnCandidateRequest candidateRequest;
        candidateRequest.keyTaskStatus = taskStatus;
        candidateRequest.sameKeyTaskIdentity =
                currentRaw == taskIdRaw;
        candidateRequest.progressiveTicket =
                isProgressiveStepTicket(ticket);
        candidateRequest.keyPresenceFresh =
                isKeyPresenceFresh(ticket);
        candidateRequest.cancelActive =
                hasTicketCancelIntent(ticketCancelStateFromStorage(ticket.cancelState));
        candidateRequest.cleanupActive =
                isReturnCleanupState(ticket.returnState);
        const ProgressiveReturnCandidateDecision candidate =
                m_ticketReturnWorkflow->evaluateProgressiveCandidate(
                    candidateRequest);
        if (!candidate.eligible) {
            continue;
        }

        applyReturnEffect(
                    m_ticketReturnWorkflow->startProgressiveReturn(
                        taskId, taskIdRaw, ticket.stepNum, m_keySessionId));
        return;
    }

    const int completedSweepSessionId =
            m_ticketReturnWorkflow->progressiveSweepSessionId();
    m_ticketReturnWorkflow->clearProgressiveSweep();
    QTimer::singleShot(0, this, [this, completedSweepSessionId]() {
        if (isCurrentKeySession(completedSweepSessionId)) {
            tryAutoReturnCompletedTicket(m_keyTaskObservation->tasks());
        }
    });
}
void KeyManageController::handleProgressiveTaskLog(const KeySessionEvent &event)
{
    ProgressiveReturnLogRequest request;
    request.contextActive = m_ticketReturnWorkflow->progressiveActive();
    request.sameKeySession =
            isCurrentKeySession(m_ticketReturnWorkflow->activeSessionId());
    request.expectedTaskId = m_ticketReturnWorkflow->activeTaskId();
    request.eventTaskId =
            event.data.value(QStringLiteral("taskId")).toString();
    request.expectedTaskIdRaw = m_ticketReturnWorkflow->activeTaskIdRaw();
    request.eventTaskIdRaw =
            event.data.value(QStringLiteral("taskIdRaw")).toByteArray();
    request.taskAttribute =
            event.data.value(QStringLiteral("taskAttr"), -1).toInt();
    request.expectedSteps = m_ticketReturnWorkflow->progressiveTotalSteps();
    request.eventSteps =
            event.data.value(QStringLiteral("steps"), -1).toInt();

    QList<QJsonObject> parsedItems;
    const QVariantList rawItems =
            event.data.value(QStringLiteral("items")).toList();
    for (const QVariant &itemVar : rawItems) {
        parsedItems.append(QJsonObject::fromVariantMap(itemVar.toMap()));
    }
    const TicketReturnWorkflowEffect effect =
            m_ticketReturnWorkflow->acceptProgressiveTaskLog(
                request,
                event.data.value(QStringLiteral("stationNo")).toInt(),
                parsedItems);
    if (effect.action
            == TicketReturnWorkflowAction::ContinueProgressiveUploads) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] progressive log taskId=%1 completed=%2/%3 pendingUpload=%4")
                      .arg(m_ticketReturnWorkflow->activeTaskId())
                      .arg(m_ticketReturnWorkflow->progressiveObservedStepCount())
                      .arg(m_ticketReturnWorkflow->progressiveTotalSteps())
                      .arg(m_ticketReturnWorkflow->progressivePendingUploadCount());
    }
    applyReturnEffect(effect);
}

void KeyManageController::uploadNextCompletedProgressiveStep()
{
    bool cleanupVerified = false;
    if (m_ticketReturnWorkflow->progressiveActive()
            && !m_ticketReturnWorkflow->progressiveUploadActive()
            && m_ticketReturnWorkflow->progressivePendingUploadCount() == 0) {
        cleanupVerified = canDeleteProgressiveStepTicket();
    }
    applyReturnEffect(
                m_ticketReturnWorkflow->nextProgressiveUpload(
                    cleanupVerified));
}

bool KeyManageController::canDeleteProgressiveStepTicket() const
{
    quint8 currentStatus = 0xFF;
    const QByteArray currentRaw =
            m_keyTaskObservation->findTaskIdRaw(
                m_ticketReturnWorkflow->activeTaskId(), &currentStatus);
    return m_ticketReturnWorkflow->canStartProgressiveCleanup(
                currentRaw, currentStatus, m_keySessionId);
}
void KeyManageController::finishProgressiveStepReturn(bool continueSweep)
{
    const int sweepSessionId = m_ticketReturnWorkflow->progressiveSweepSessionId();
    clearProgressiveStepReturnContext(!continueSweep);
    if (!continueSweep || !isCurrentKeySession(sweepSessionId)) {
        m_ticketReturnWorkflow->clearProgressiveSweep();
        return;
    }

    QTimer::singleShot(0, this, [this, sweepSessionId]() {
        if (isCurrentKeySession(sweepSessionId)) {
            startNextProgressiveStepReturn();
        } else {
            clearProgressiveStepReturnContext(true);
        }
    });
}

void KeyManageController::clearProgressiveStepReturnContext(bool clearCandidates)
{
    m_ticketReturnWorkflow->clearProgressiveTask(clearCandidates);
    schedulePendingCancelReconcile(QStringLiteral("return-context-cleared"), 300);
}

void KeyManageController::tryStartTicketReturn(const QString &taskId, bool automatic)
{
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    if (progressiveStepReturnEnabled() && isProgressiveStepTicket(ticket)) {
        tryStartProgressiveStepReturnSweep(m_keyTaskObservation->tasks());
        if (!automatic) {
            emit statusMessage(QStringLiteral("该多步骤票已启用逐步回传，已按当前钥匙日志尝试处理"));
        }
        return;
    }

    const TicketReturnStartRequest request =
            buildReturnStartRequest(taskId, automatic);
    const TicketReturnWorkflowStartOutcome outcome =
            m_ticketReturnWorkflow->startReturn(request, m_keySessionId);
    if (!outcome.decision.allowed) {
        if (!automatic && !outcome.decision.message.trimmed().isEmpty()) {
            emit statusMessage(outcome.decision.message);
        }
        return;
    }
    applyReturnEffect(outcome.effect);
}

// Chooses at most one ordinary completed ticket after the pure return
// policy excludes progressive, cancelling, stale, or busy candidates.
void KeyManageController::tryAutoReturnCompletedTicket(const QList<KeyTaskDto> &tasks)
{
    const QString busyChain = activeReturnChainTaskId();
    if (!busyChain.isEmpty()) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryAutoReturnCompletedTicket SKIPPED busyChain=%1 activeReturn=%2 pendingHandshake=%3 pendingDelete=%4")
                      .arg(busyChain,
                           !m_ticketReturnWorkflow->hasActiveReturn() ? QStringLiteral("<none>") : m_ticketReturnWorkflow->activeTaskId(),
                           !m_ticketReturnWorkflow->hasPendingHandshake() ? QStringLiteral("<none>") : m_ticketReturnWorkflow->handshakeTaskId(),
                           !m_ticketDeleteWorkflow->hasActiveOperation() ? QStringLiteral("<none>") : m_ticketDeleteWorkflow->activeTaskId());
        return;
    }
    if (!m_returnUploadService->isConfigured()) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryAutoReturnCompletedTicket SKIPPED returnClient not configured");
        return;
    }

    qInfo().noquote()
            << QStringLiteral("[KeyManageController] tryAutoReturnCompletedTicket ENTER taskCount=%1 observedAtMs=%2")
                  .arg(QString::number(tasks.size()), QString::number(m_keyTaskObservation->observedAtMs()));

    for (const KeyTaskDto &task : tasks) {
        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket =
                m_ticketStore->ticketById(taskId);
        TicketReturnAutomaticCandidateRequest request;
        request.keyTaskCompleted = task.status == 0x02;
        request.ticketExists = ticket.valid;
        request.progressiveTicket =
                progressiveStepReturnEnabled()
                && isProgressiveStepTicket(ticket);
        request.cancelActive =
                ticket.valid
                && hasTicketCancelIntent(
                    ticketCancelStateFromStorage(ticket.cancelState));
        request.keyPresenceFresh =
                ticket.valid && isKeyPresenceFresh(ticket);
        request.returnStateBlocked =
                ticket.valid
                && shouldSkipAutoReturnForState(ticket.returnState);
        const TicketReturnAutomaticCandidateDecision decision =
                m_ticketReturnWorkflow->evaluateAutomaticCandidate(request);
        if (!decision.eligible) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] tryAutoReturnCompletedTicket REJECT taskId=%1 reason=%2 hwStatus=0x%3 transferState=%4 returnState=%5 cancelState=%6")
                          .arg(taskId,
                               decision.reasonCode,
                               QString::number(task.status, 16)
                                   .toUpper().rightJustified(
                                       2, QLatin1Char('0')),
                               ticket.transferState,
                               ticket.returnState,
                               ticket.cancelState);
            continue;
        }

        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryAutoReturnCompletedTicket selected taskId=%1 returnState=%2 transferState=%3")
                      .arg(taskId,
                           ticket.returnState.isEmpty() ? QStringLiteral("idle") : ticket.returnState,
                           ticket.transferState);
        tryStartTicketReturn(taskId, true);
        return;
    }
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] tryAutoReturnCompletedTicket NO_CANDIDATE taskCount=%1")
                  .arg(QString::number(tasks.size()));
}

TicketReturnStartRequest KeyManageController::buildReturnStartRequest(
        const QString &taskId,
        bool automatic) const
{
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    quint8 keyTaskStatus = 0xFF;
    const QByteArray taskIdRaw = m_keyTaskObservation->findTaskIdRaw(taskId, &keyTaskStatus);

    TicketReturnStartRequest request;
    request.taskId = taskId;
    request.busyTaskId = activeReturnChainTaskId();
    request.keyTaskIdRaw = taskIdRaw;
    request.keyTaskStatus = keyTaskStatus;
    request.trigger = automatic
            ? TicketReturnTrigger::Automatic
            : TicketReturnTrigger::Manual;
    request.ticketExists = ticket.valid;
    request.keyPresenceConfirmed = ticket.valid && isKeyPresenceFresh(ticket);
    request.cleanupActive = isReturnCleanupState(ticket.returnState);
    request.returnInFlight = isReturnInFlightState(ticket.returnState);
    request.clientConfigured = m_returnUploadService->isConfigured();
    request.peerWorkflowBusy =
            m_ticketTransferWorkflow->hasActiveOperation()
            || m_ticketDeleteWorkflow->hasActiveOperation()
            || m_ticketCancelWorkflow->hasPendingWork()
            || m_ticketCancelWorkflow->hasActiveDelete();
    return request;
}

bool KeyManageController::canStartTicketReturn(const QString &taskId,
                                               bool automatic,
                                               QByteArray *taskIdRaw,
                                               QString *blockedReason) const
{
    const TicketReturnStartRequest request =
            buildReturnStartRequest(taskId, automatic);
    const TicketReturnStartResult result =
            m_ticketReturnWorkflow->evaluateStart(request);
    if (blockedReason) {
        *blockedReason = result.message;
    }
    if (taskIdRaw) {
        *taskIdRaw = result.allowed ? request.keyTaskIdRaw : QByteArray();
    }
    return result.allowed;
}
QString KeyManageController::activeReturnChainTaskId() const
{
    const QString returnTaskId = m_ticketReturnWorkflow->chainTaskId();
    if (!returnTaskId.isEmpty()) {
        return returnTaskId;
    }
    if (m_ticketDeleteWorkflow->hasActiveOperation()) {
        return m_ticketDeleteWorkflow->activeTaskId();
    }
    return QString();
}

QString KeyManageController::taskIdFromRaw(const QByteArray &taskIdRaw)
{
    return KeyTaskObservationService::taskIdFromRaw(taskIdRaw);
}
bool KeyManageController::tryStartTicketTransfer(
        const QString &taskId,
        bool automatic,
        bool failInsteadOfDeferring,
        QString *blockedReason)
{
    TicketTransferStoredStartRequest request;
    request.taskId = taskId;
    request.automatic = automatic;
    request.failInsteadOfDeferring = failInsteadOfDeferring;
    request.session = m_session->snapshot();
    request.currentKeySessionId = m_keySessionId;
    request.debugFrameChunkSize = ConfigManager::instance()
            ->value("ticket/debugFrameChunkSize", 0).toInt();
    request.peerWorkflowBusy =
            m_ticketReturnWorkflow->hasReturnWork()
            || m_ticketDeleteWorkflow->hasActiveOperation()
            || m_ticketCancelWorkflow->hasPendingWork()
            || m_ticketCancelWorkflow->hasActiveDelete();
    const TicketTransferStoredStartOutcome outcome =
            m_ticketTransferWorkflow->startStoredTicket(request);

    if (!outcome.decision.allowed) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] transfer start rejected taskId=%1 reason=%2 automatic=%3")
                      .arg(taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                           QString::number(static_cast<int>(
                               outcome.decision.reason)),
                           automatic ? QStringLiteral("true")
                                     : QStringLiteral("false"));
        if (outcome.decision.emitMessage) {
            emit statusMessage(outcome.decision.message);
        }
        if (blockedReason) {
            *blockedReason = outcome.decision.message;
        }
        return false;
    }

    applyTransferEffect(outcome.effect);
    if (blockedReason) {
        blockedReason->clear();
    }
    return true;
}

void KeyManageController::tryAutoTransferPendingTicket()
{
    const QList<SystemTicketDto> tickets = systemTickets();
    const bool peerWorkflowBusy =
            m_ticketReturnWorkflow->hasReturnWork()
            || m_ticketDeleteWorkflow->hasActiveOperation()
            || m_ticketCancelWorkflow->hasPendingWork()
            || m_ticketCancelWorkflow->hasActiveDelete();
    const TicketAutoTransferSelectionDecision decision =
            m_ticketTransferWorkflow->selectAutomaticCandidate(
                autoTransferEnabled(), peerWorkflowBusy, tickets);

    switch (decision.reason) {
    case TicketAutoTransferSelectionReason::HttpTransactionActive:
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] auto transfer skipped: workbench transaction taskId=%1 session=%2 epoch=%3")
                      .arg(m_ticketTransferWorkflow->activeWorkbenchTaskId().isEmpty()
                               ? QStringLiteral("<none>")
                               : m_ticketTransferWorkflow->activeWorkbenchTaskId(),
                           QString::number(m_ticketTransferWorkflow->activeWorkbenchSessionId()),
                           QString::number(m_ticketTransferWorkflow->activeWorkbenchBridgeEpoch()));
        return;
    case TicketAutoTransferSelectionReason::Disabled:
    case TicketAutoTransferSelectionReason::NoCandidate:
        return;
    case TicketAutoTransferSelectionReason::OperationBusy:
        qDebug().noquote()
                << QStringLiteral("[KeyManageController] auto transfer skipped: workflow busy transfer=%1 return=%2 delete=%3 cancel=%4")
                      .arg(m_ticketTransferWorkflow->activeTaskId().isEmpty()
                               ? QStringLiteral("<none>")
                               : m_ticketTransferWorkflow->activeTaskId(),
                           !m_ticketReturnWorkflow->hasActiveReturn()
                               ? QStringLiteral("<none>")
                               : m_ticketReturnWorkflow->activeTaskId(),
                           !m_ticketDeleteWorkflow->hasActiveOperation()
                               ? QStringLiteral("<none>")
                               : m_ticketDeleteWorkflow->activeTaskId(),
                           !m_ticketCancelWorkflow->hasActiveDelete()
                               ? QStringLiteral("<none>")
                               : m_ticketCancelWorkflow->activeDeleteTaskId());
        return;
    case TicketAutoTransferSelectionReason::Selected:
        break;
    }

    emit statusMessage(
                QStringLiteral(
                    "\u53D1\u73B0\u53EF\u81EA\u52A8\u4F20\u7968\u4EFB\u52A1\uFF1A%1")
                .arg(decision.taskId));
    tryStartTicketTransfer(decision.taskId, true);
}

// Executes typed transfer effects without reconstructing transfer decisions in
// L1. Serial commands, HTTP replies, UI output, and timers stay with owners.
// Executes return effects without giving L2 direct access to serial, HTTP, UI,
// timers, or the shared physical-key deletion workflow.
void KeyManageController::applyReturnEffect(
        const TicketReturnWorkflowEffect &effect)
{
    switch (effect.action) {
    case TicketReturnWorkflowAction::None:
    case TicketReturnWorkflowAction::WaitForUpload:
    case TicketReturnWorkflowAction::HandleProgressiveLog:
        break;
    case TicketReturnWorkflowAction::SendSetCom: {
        CommandRequest command;
        command.id = CommandId::SetCom;
        command.opId = nextOpId();
        m_session->execute(command);
        break;
    }
    case TicketReturnWorkflowAction::QueryTaskLog: {
        CommandRequest command;
        command.id = CommandId::QueryTaskLog;
        command.opId = nextOpId();
        command.payload = effect.taskIdRaw;
        m_session->execute(command);
        break;
    }
    case TicketReturnWorkflowAction::UploadStandardLog:
    case TicketReturnWorkflowAction::UploadProgressiveLog:
        m_returnUploadService->upload(effect.payload, effect.keySessionId);
        break;
    case TicketReturnWorkflowAction::ContinueProgressiveUploads:
        uploadNextCompletedProgressiveStep();
        break;
    case TicketReturnWorkflowAction::BeginVerifiedDelete: {
        // The return context may already be released; deletion must retain the
        // key session that produced the successful upload evidence.
        markReturnDeletePending(
                    effect.taskId, effect.taskIdRaw, effect.keySessionId);
        CommandRequest command;
        command.id = CommandId::DeleteTask;
        command.opId = nextOpId();
        command.payload = effect.taskIdRaw;
        m_ticketDeleteWorkflow->recordDeleteDispatched();
        m_observability->writeBusinessEvent(
                    QStringLiteral("return"),
                    QStringLiteral("del-sent"),
                    QStringLiteral("started"),
                    effect.taskId,
                    effect.keySessionId,
                    command.opId);
        m_session->execute(command);
        if (m_ticketReturnWorkflow->progressiveActive()) {
            clearProgressiveStepReturnContext(true);
        }
        break;
    }
    case TicketReturnWorkflowAction::AwaitVerifiedDelete:
        // HTTP success is retained, but a changed key session must first be
        // reconciled by a fresh Q_TASK before any DEL is sent.
        markReturnDeletePending(
                    effect.taskId, effect.taskIdRaw, 0);
        if (m_ticketReturnWorkflow->progressiveActive()) {
            clearProgressiveStepReturnContext(true);
        }
        // The reconnect's automatic Q_TASK may have completed before this
        // HTTP callback. Query again so cleanup never needs a manual refresh.
        QTimer::singleShot(0, this, [this]() {
            const KeySessionSnapshot snapshot = m_session->snapshot();
            if (!snapshot.connected || !snapshot.keyPresent
                    || !snapshot.keyStable || !snapshot.sessionReady
                    || snapshot.commandInFlight
                    || snapshot.recoveryWindowActive) {
                return;
            }
            m_keyTaskObservation->beginQuery();
            CommandRequest request;
            request.id = CommandId::QueryTasks;
            request.opId = nextOpId();
            m_session->execute(request);
        });
        break;
    case TicketReturnWorkflowAction::ContinueProgressiveSweep:
        finishProgressiveStepReturn(true);
        break;
    case TicketReturnWorkflowAction::StopProgressiveSweep:
        finishProgressiveStepReturn(false);
        break;
    case TicketReturnWorkflowAction::StopProgressiveSweepAndRequery:
        finishProgressiveStepReturn(false);
        // Reconcile a successful HTTP callback against the new key session.
        QTimer::singleShot(0, this, [this]() {
            const KeySessionSnapshot snapshot = m_session->snapshot();
            if (!snapshot.connected || !snapshot.keyPresent
                    || !snapshot.keyStable || !snapshot.sessionReady
                    || snapshot.commandInFlight
                    || snapshot.recoveryWindowActive) {
                return;
            }
            m_keyTaskObservation->beginQuery();
            CommandRequest request;
            request.id = CommandId::QueryTasks;
            request.opId = nextOpId();
            m_session->execute(request);
        });
        break;
    }
    if (!effect.statusMessage.trimmed().isEmpty()) {
        emit statusMessage(effect.statusMessage);
    }
}
void KeyManageController::applyTransferEffect(
        const TicketTransferWorkflowEffect &effect)
{
    if (!effect.handled) {
        return;
    }

    if (effect.stopSafetyTimer) {
        m_httpTransactionSafetyTimer->stop();
    }
    if (effect.startSafetyTimer) {
        m_httpTransactionSafetyTimer->start();
    }

    switch (effect.action) {
    case TicketTransferWorkflowAction::None:
    case TicketTransferWorkflowAction::CompleteSuccess:
    case TicketTransferWorkflowAction::CompleteFailure:
    case TicketTransferWorkflowAction::RetryAutomatically:
        break;
    case TicketTransferWorkflowAction::SendSetCom: {
        CommandRequest request;
        request.id = CommandId::SetCom;
        request.opId = nextOpId();
        m_session->execute(request);
        break;
    }
    case TicketTransferWorkflowAction::SendTicket: {
        TicketTransferRequest request;
        request.jsonBytes = effect.jsonBytes;
        request.opId = nextOpId();
        request.debugFrameChunkSize = effect.debugFrameChunkSize;
        m_session->transferTicket(request);
        break;
    }
    case TicketTransferWorkflowAction::QueryTasks: {
        // The transfer confirmation must not reuse an earlier Q_TASK proof.
        m_keyTaskObservation->beginQuery();
        CommandRequest request;
        request.id = CommandId::QueryTasks;
        request.opId = nextOpId();
        m_session->execute(request);
        break;
    }
    }

    if (effect.completeDeferredResponse) {
        m_ticketIngress->completeDeferredResponse(
                    effect.taskId,
                    effect.deferredSuccess,
                    effect.deferredMessage);
    }
    if (!effect.statusMessage.isEmpty()) {
        emit statusMessage(effect.statusMessage);
    }
    if (effect.refreshWorkbench) {
        emit workbenchRefreshRequested();
    }
    if (effect.scheduleCancelReconcile) {
        schedulePendingCancelReconcile(
                    effect.action
                            == TicketTransferWorkflowAction::QueryTasks
                        ? QStringLiteral("transfer-awaiting-q-task")
                        : QStringLiteral("transfer-confirmation-closed"),
                    150);
    }
    if (!effect.automaticRetryReason.isEmpty()) {
        scheduleAutoTransferRetry(effect.automaticRetryReason);
    }
    if (effect.tryNextAutomatic) {
        tryAutoTransferPendingTicket();
    }
}
