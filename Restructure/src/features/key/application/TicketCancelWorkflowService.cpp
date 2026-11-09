#include "features/key/application/TicketCancelWorkflowService.h"

#include <QDateTime>

#include "features/key/application/TicketCancelCoordinator.h"
#include "features/key/application/TicketCancelTrackingService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/VerifiedKeyTaskRemoval.h"

TicketCancelWorkflowService::TicketCancelWorkflowService(
        TicketStore *store,
        TicketCancelCoordinator *coordinator,
        TicketCancelTrackingService *tracking,
        QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_coordinator(coordinator)
    , m_tracking(tracking)
{
}

void TicketCancelWorkflowService::start()
{
    if (m_coordinator) {
        m_coordinator->start();
    }
}

TicketCancelState TicketCancelWorkflowService::trackedState(
        const QString &taskId,
        bool *recordExists) const
{
    const TicketCancelCoordinator::CancelRecord record =
            m_coordinator->recordFor(taskId);
    if (recordExists) {
        *recordExists = record.valid;
    }
    return record.valid ? record.cancelState : TicketCancelState::None;
}

TicketCancelIngressOutcome TicketCancelWorkflowService::handleIngress(
        const TicketCancelIngressObservation &observation)
{
    bool trackedRecordExists = false;
    const TicketCancelState currentTrackedState =
            trackedState(observation.taskId, &trackedRecordExists);
    const SystemTicketDto ticket = ticketForIngress(
                observation.taskId,
                observation.taskInKey,
                currentTrackedState,
                trackedRecordExists);

    TicketCancelIngressRequest request;
    request.taskId = observation.taskId;
    request.trackedRecordExists = trackedRecordExists;
    request.trackedCancelState = currentTrackedState;
    request.ticket = ticket;
    request.keyReady = observation.keyReady;
    request.hasTaskList = observation.hasTaskList;
    request.taskInKey = observation.taskInKey;
    request.canProcessNow = observation.canProcessNow;

    TicketCancelIngressOutcome outcome;
    outcome.decision = TicketCancelUseCase::evaluateIngress(request);
    const TicketCancelIngressDecision &decision = outcome.decision;

    switch (decision.action) {
    case TicketCancelIngressAction::Reject:
    case TicketCancelIngressAction::ReturnDone:
        break;
    case TicketCancelIngressAction::ReturnAccepted:
        if (decision.reason == TicketCancelIngressReason::ExistingIntent) {
            m_coordinator->recordAccepted(
                        observation.taskId,
                        ticket,
                        ticketCancelStateFromStorage(ticket.cancelState),
                        decision.message);
        }
        break;
    case TicketCancelIngressAction::RecordDone:
        m_coordinator->recordDone(
                    observation.taskId, ticket, decision.message);
        break;
    case TicketCancelIngressAction::RemoveTicket:
        m_coordinator->recordDone(
                    observation.taskId, ticket, decision.message);
        m_store->removeTicket(observation.taskId);
        break;
    case TicketCancelIngressAction::AcceptCancel:
        m_tracking->recordAccepted(
                    ticket,
                    decision.nextCancelState,
                    observation.taskInKey,
                    observation.canProcessNow
                        ? QStringLiteral("已受理撤销，等待 Q_TASK 对账后执行删除")
                        : QStringLiteral("已受理撤销，等待当前业务链收口"),
                    decision.message);
        outcome.scheduleReconcile = observation.canProcessNow;
        break;
    }

    return outcome;
}

bool TicketCancelWorkflowService::hasPendingWork() const
{
    return m_coordinator->hasPendingWork();
}

bool TicketCancelWorkflowService::hasActiveDelete() const
{
    return m_deleteOperation.isActive();
}

bool TicketCancelWorkflowService::isActiveDeleteFor(const QString &taskId) const
{
    return m_deleteOperation.taskId == taskId.trimmed();
}

QString TicketCancelWorkflowService::activeDeleteTaskId() const
{
    return m_deleteOperation.taskId;
}

QList<TicketCancelWorkflowEffect>
TicketCancelWorkflowService::beginPendingDeletes(
        const QList<KeyTaskDto> &tasks,
        int keySessionId)
{
    QList<TicketCancelWorkflowEffect> effects;
    if (!hasPendingWork() || hasActiveDelete()) {
        return effects;
    }

    const QStringList pendingTaskIds = m_coordinator->activeTaskIds();
    for (const QString &taskId : pendingTaskIds) {
        if (taskId.trimmed().isEmpty()) {
            continue;
        }

        const SystemTicketDto ticket = m_store->ticketById(taskId);
        const QByteArray keyTaskRaw =
                VerifiedKeyTaskRemoval::findRawTask(tasks, taskId);
        KeyTaskRemovalObservation removalObservation;
        removalObservation.phase = KeyTaskRemovalPhase::BeforeDelete;
        removalObservation.sameKeySession = keySessionId > 0;
        removalObservation.taskPresent = !keyTaskRaw.isEmpty();

        TicketCancelTaskObservationRequest request;
        request.ticket = ticket;
        request.removalProof = VerifiedKeyTaskRemoval::evaluate(
                    removalObservation);
        const TicketCancelTaskObservationDecision decision =
                TicketCancelUseCase::evaluateTaskObservation(request);

        TicketCancelWorkflowEffect effect;
        effect.taskId = taskId;
        effect.message = decision.message;
        if (decision.action == TicketCancelTaskAction::MarkDone) {
            m_coordinator->recordDone(taskId, ticket, decision.message);
            effect.action = TicketCancelWorkflowAction::MarkDone;
            effects.append(effect);
            continue;
        }

        if (decision.action == TicketCancelTaskAction::RejectMissingKeyTask) {
            if (ticket.valid) {
                m_tracking->recordFailureAndClear(
                            ticket, decision.message, decision.message);
            } else {
                m_coordinator->recordFailed(
                            taskId,
                            transientTicket(taskId, false),
                            decision.message);
            }
            effect.action = TicketCancelWorkflowAction::Reject;
            effects.append(effect);
            continue;
        }

        m_deleteOperation.begin(taskId, keyTaskRaw, keySessionId);
        if (ticket.valid) {
            m_tracking->recordExecuting(ticket, decision.message);
        } else {
            SystemTicketDto current = transientTicket(taskId, true);
            current.cancelState = ticketCancelStateToStorage(
                        TicketCancelState::Executing);
            m_coordinator->recordExecuting(
                        taskId, current, decision.message);
        }
        effect.action = TicketCancelWorkflowAction::SendDelete;
        effect.taskIdRaw = keyTaskRaw;
        effects.append(effect);
        break;
    }

    return effects;
}

TicketCancelWorkflowEffect TicketCancelWorkflowService::verifyActiveDelete(
        const QList<KeyTaskDto> &tasks,
        int currentKeySessionId)
{
    TicketCancelWorkflowEffect effect;
    if (!m_deleteOperation.isActive()) {
        return effect;
    }

    effect.taskId = m_deleteOperation.taskId;
    KeyTaskRemovalObservation removalObservation;
    removalObservation.phase = KeyTaskRemovalPhase::AfterDelete;
    removalObservation.sameKeySession = m_deleteOperation.keySessionId > 0
            && currentKeySessionId == m_deleteOperation.keySessionId;
    removalObservation.taskPresent = VerifiedKeyTaskRemoval::containsRawTask(
                tasks, m_deleteOperation.taskIdRaw);
    const TicketCancelDeleteVerificationDecision decision =
            TicketCancelUseCase::evaluateDeleteVerification(
                VerifiedKeyTaskRemoval::evaluate(removalObservation));
    effect.message = decision.message;

    const SystemTicketDto ticket = m_store->ticketById(effect.taskId);
    switch (decision.action) {
    case TicketCancelDeleteVerificationAction::RetryAfterSessionChange:
        effect.action = TicketCancelWorkflowAction::RetryAfterSessionChange;
        if (ticket.valid) {
            m_tracking->recordAccepted(
                        ticket,
                        TicketCancelState::Pending,
                        true,
                        decision.message,
                        decision.message);
        }
        break;
    case TicketCancelDeleteVerificationAction::CompleteCancel:
        effect.action = TicketCancelWorkflowAction::Complete;
        if (ticket.valid) {
            m_coordinator->recordDone(
                        effect.taskId, ticket, decision.message);
            m_store->removeTicket(effect.taskId);
        } else {
            m_coordinator->recordDoneWithoutTicket(
                        effect.taskId, decision.message);
        }
        break;
    case TicketCancelDeleteVerificationAction::RetryTaskStillPresent:
        effect.action = TicketCancelWorkflowAction::RetryTaskStillPresent;
        if (ticket.valid) {
            m_tracking->recordAccepted(
                        ticket,
                        TicketCancelState::Pending,
                        true,
                        decision.message,
                        decision.message);
        }
        break;
    }

    m_deleteOperation.clear();
    return effect;
}

QString TicketCancelWorkflowService::interruptActiveDelete(
        const QString &message)
{
    if (!m_deleteOperation.isActive()) {
        return QString();
    }

    const QString taskId = m_deleteOperation.taskId;
    const SystemTicketDto ticket = m_store->ticketById(taskId);
    if (ticket.valid) {
        m_tracking->recordAccepted(
                    ticket,
                    TicketCancelState::Pending,
                    true,
                    message,
                    message);
    }
    m_deleteOperation.invalidateKeySession();
    return taskId;
}

TicketCancelExpiryOutcome TicketCancelWorkflowService::expireDeferredRequest(
        const QString &taskId)
{
    TicketCancelExpiryOutcome outcome;
    outcome.taskId = taskId.trimmed();
    if (outcome.taskId.isEmpty()) {
        return outcome;
    }

    if (isActiveDeleteFor(outcome.taskId)) {
        outcome.action = TicketCancelExpiryAction::ContinueDeviceVerification;
        outcome.message = QStringLiteral(
                    "HTTP client disconnected after DEL; verification continues");
        return outcome;
    }

    const QString recordMessage =
            QStringLiteral("撤销确认超时，已取消本次撤销意图，等待用户重新发起终止");
    const QString trackingMessage =
            QStringLiteral("撤销确认超时，已取消本次撤销意图，需重新点击终止");
    const SystemTicketDto ticket = m_store->ticketById(outcome.taskId);
    if (ticket.valid) {
        m_tracking->recordFailureAndClear(
                    ticket, recordMessage, trackingMessage);
    } else {
        m_coordinator->recordFailed(
                    outcome.taskId,
                    transientTicket(outcome.taskId, false),
                    recordMessage);
    }
    outcome.action = TicketCancelExpiryAction::CancelIntent;
    outcome.message = trackingMessage;
    return outcome;
}

SystemTicketDto TicketCancelWorkflowService::ticketForIngress(
        const QString &taskId,
        bool taskInKey,
        TicketCancelState currentTrackedState,
        bool trackedRecordExists) const
{
    SystemTicketDto ticket = m_store->ticketById(taskId);
    const bool trackedStopsRequest = trackedRecordExists
            && (currentTrackedState == TicketCancelState::Done
                || isTicketCancelActive(currentTrackedState));
    if (!trackedStopsRequest && !ticket.valid
            && !taskId.trimmed().isEmpty() && taskInKey) {
        ticket = transientTicket(taskId, true);
        ticket.ticketNo = taskId;
        ticket.source = QStringLiteral("key-observation");
    }
    return ticket;
}

SystemTicketDto TicketCancelWorkflowService::transientTicket(
        const QString &taskId,
        bool wasInKey)
{
    SystemTicketDto ticket;
    ticket.taskId = taskId.trimmed();
    ticket.residentSlotId = QStringLiteral("slot-1");
    ticket.cancelSource = QStringLiteral("workbench-http");
    ticket.cancelRequestedAt = QDateTime::currentDateTime();
    ticket.wasInKey = wasInKey;
    ticket.valid = !ticket.taskId.isEmpty();
    return ticket;
}