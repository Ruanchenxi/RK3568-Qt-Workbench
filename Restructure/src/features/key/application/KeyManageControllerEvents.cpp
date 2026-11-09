/**
 * @file KeyManageControllerEvents.cpp
 * @brief Session-event execution for the key-management L1 controller.
 *
 * This file keeps protocol/session event routing separate from UI commands and
 * workflow start paths. Business decisions remain in the independent L2/L3
 * workflow services and KeyManageEventUseCase; these methods only execute the
 * selected serial, workflow, logging, and presentation effects.
 */
#include "features/key/application/KeyManageController.h"

#include <QDebug>
#include <QJsonArray>

#include "features/key/application/KeyMaintenanceService.h"
#include "features/key/application/KeyManageSelectionService.h"
#include "features/key/application/KeyPortRuntimeCoordinator.h"
#include "features/key/application/KeyManageObservabilityService.h"
#include "features/key/application/KeyTaskObservationService.h"
#include "features/key/application/KeyTaskUpdateUseCase.h"
#include "features/key/application/TicketCancelWorkflowService.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketTransferWorkflowService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/CaptureTicketWorkflowService.h"
#include "features/key/domain/TicketCancelState.h"
#include "features/key/domain/TicketReturnState.h"
#include "features/key/protocol/KeyProtocolDefs.h"

namespace {

bool isSessionReadySnapshot(const KeySessionSnapshot &snapshot)
{
    return snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
}

} // namespace
void KeyManageController::handleTimeoutEvent(quint8 command,
                                             const QString &what)
{
    if ((command == KeyProtocol::CmdIRfid
         || command == KeyProtocol::CmdUpRfid)
            && m_captureWorkflow->hasActiveOperation()) {
        applyCaptureEffect(m_captureWorkflow->failRfid(
                               m_captureWorkflow->activeTaskId(), what));
    }
    const KeyCommandEventPlan plan =
            KeyManageEventUseCase::planTimeout(
                command, commandEventContext());
    if (plan.releaseAutomaticQuery) {
        m_keyTaskObservation->releaseAutomaticQuery(m_keySessionId);
    }
    if (plan.failTransferConfirmation) {
        failActiveTransferConfirmation(QStringLiteral(
            "\u4F20\u7968\u5931\u8D25\uFF1AQ_TASK "
            "\u786E\u8BA4\u8D85\u65F6\uFF0C\u8BF7\u91CD\u65B0\u4F20\u7968"));
    }
    if (plan.interruptDelete) {
        const TicketDeleteWorkflowEffect effect =
                m_ticketDeleteWorkflow->interrupt(
                    TicketDeleteInterruptionCause::Timeout);
        if (effect.action == TicketDeleteWorkflowAction::AwaitUserRetry) {
            emit statusMessage(QStringLiteral("%1\uFF1A%2")
                               .arg(effect.message, effect.taskId));
        } else {
            emit statusMessage(QStringLiteral(
                "DEL \u8D85\u65F6\uFF0C\u5DF2\u4FDD\u7559\u5220\u9664\u9A8C\u8BC1\u610F\u56FE\uFF0C"
                "\u7B49\u5F85\u540E\u7EED Q_TASK \u7EE7\u7EED\u6838\u5BF9"));
        }
    }
    if (plan.interruptCancelDelete) {
        m_ticketCancelWorkflow->interruptActiveDelete(QStringLiteral(
            "DEL \u8D85\u65F6\uFF0C\u7B49\u5F85\u540E\u7EED Q_TASK "
            "\u6838\u5BF9\u786E\u8BA4\u4EFB\u52A1\u662F\u5426\u5DF2\u5220\u9664"));
        schedulePendingCancelReconcile(QStringLiteral("cancel-del-timeout"), 800);
    }
    if (plan.failTransferHandshake) {
        applyTransferEffect(m_ticketTransferWorkflow->failHandshake(
                    QStringLiteral(
                        "\u4F20\u7968\u5931\u8D25\uFF1ASET_COM "
                        "\u63E1\u624B\u8D85\u65F6\uFF0C\u8BF7\u91CD\u8BD5")));
    }
    if (plan.failReturnHandshake) {
        failActiveReturnHandshake(what);
    }
    emit timeoutOccurred(what);
    if (command == KeyProtocol::CmdQTask) {
        emit taskQueryTimedOut();
    }
    emit statusMessage(what);
}
void KeyManageController::handleAckEvent(quint8 ackedCmd)
{
    if (ackedCmd == KeyProtocol::CmdDel
            && m_ticketDeleteWorkflow->hasActiveOperation()) {
        m_ticketDeleteWorkflow->acceptDeleteAck();
    }
    const KeyCommandEventPlan plan =
            KeyManageEventUseCase::planAck(ackedCmd, commandEventContext());
    if (plan.rejectAmbiguousSetComAck) {
        const QString reason = QStringLiteral(
                    "\u68c0\u6d4b\u5230\u4f20\u7968\u4e0e\u56de\u4f20\u540c\u65f6\u7b49\u5f85 SET_COM\uff0c\u5df2\u4e2d\u6b62\u4e24\u4e2a\u6d41\u7a0b\uff0c\u8bf7\u91cd\u65b0\u64cd\u4f5c");
        applyTransferEffect(m_ticketTransferWorkflow->failHandshake(reason));
        failActiveReturnHandshake(reason);
        qCritical().noquote()
                << QStringLiteral("[KeyManageController] ambiguous SET_COM owner rejected");
        emit statusMessage(reason);
    }
    if (plan.acceptTransferHandshake) {
        applyTransferEffect(
                    m_ticketTransferWorkflow->acceptHandshakeAck(
                        m_keySessionId));
    }
    if (plan.acceptReturnSetCom) {
        applyReturnEffect(
                    m_ticketReturnWorkflow->acceptSetComAck(m_keySessionId));
    }
    if (plan.acceptReturnTaskLog) {
        applyReturnEffect(
                    m_ticketReturnWorkflow->acceptTaskLogAck(m_keySessionId));
    }
    if (plan.reconcileCancelDelete) {
        schedulePendingCancelReconcile(QStringLiteral("cancel-del-ack"), 300);
    }
    emit ackReceived(ackedCmd);
}
void KeyManageController::handleNakEvent(quint8 eventOrigCmd, quint8 eventErrCode)
{
    const KeyCommandEventPlan plan =
            KeyManageEventUseCase::planNak(
                eventOrigCmd, commandEventContext());
    if (plan.releaseAutomaticQuery) {
        m_keyTaskObservation->releaseAutomaticQuery(m_keySessionId);
    }
    if (plan.failTransferConfirmation) {
        failActiveTransferConfirmation(QStringLiteral(
            "\u4F20\u7968\u5931\u8D25\uFF1AQ_TASK "
            "\u786E\u8BA4\u88AB\u94A5\u5319\u62D2\u7EDD\uFF0C\u8BF7\u91CD\u65B0\u4F20\u7968"));
    }
    if (plan.interruptDelete) {
        const TicketDeleteWorkflowEffect effect =
                m_ticketDeleteWorkflow->interrupt(
                    TicketDeleteInterruptionCause::Nak);
        if (effect.action == TicketDeleteWorkflowAction::AwaitUserRetry) {
            emit statusMessage(QStringLiteral("%1\uFF1A%2")
                               .arg(effect.message, effect.taskId));
        }
    }
    if (plan.interruptCancelDelete) {
        m_ticketCancelWorkflow->interruptActiveDelete(QStringLiteral(
            "DEL \u6536\u5230 NAK\uFF0C\u7B49\u5F85\u540E\u7EED Q_TASK "
            "\u6838\u5BF9\u94A5\u5319\u4EFB\u52A1\u72B6\u6001"));
        schedulePendingCancelReconcile(QStringLiteral("cancel-del-nak"), 800);
    }
    if (plan.failTransferHandshake) {
        applyTransferEffect(m_ticketTransferWorkflow->failHandshake(
                    QStringLiteral(
                        "\u4F20\u7968\u5931\u8D25\uFF1ASET_COM "
                        "\u88AB\u94A5\u5319\u62D2\u7EDD\uFF0C\u8BF7\u91CD\u8BD5")));
    }
    if (plan.failReturnHandshake) {
        failActiveReturnHandshake(
                    QStringLiteral("\u6536\u5230 NAK\uFF0CorigCmd=0x%1")
                    .arg(eventOrigCmd, 2, 16, QLatin1Char('0'))
                    .toUpper());
    }
    emit nakReceived(eventOrigCmd, eventErrCode);
}
void KeyManageController::handleTasksUpdatedEvent(const QVariantList &taskData,
                                                   int eventBridgeEpoch)
{

    const QList<KeyTaskDto> tasks = toTaskDtos(taskData);
    const KeySessionSnapshot snapshot = m_session->snapshot();
    const bool ready = isSessionReadySnapshot(snapshot);
    const bool httpTransactionControlledEvent =
            m_ticketTransferWorkflow->matchesWorkbenchTransaction(
                m_ticketTransferWorkflow->activeWorkbenchSessionId(), eventBridgeEpoch);
    // This same-session Q_TASK result is the only admission proof used by
    // administrator deletion and other device-presence decisions.
    m_keyTaskObservation->acceptObservation(
                tasks,
                m_keySessionId,
                ready,
                QDateTime::currentMSecsSinceEpoch());

    const bool captureControlledEvent =
            m_captureWorkflow->hasActiveOperation();
    const bool sharedCaptureDeleteProofActive =
            m_captureWorkflow->isAwaitingDelete()
            && m_ticketDeleteWorkflow->hasActiveOperation()
            && m_ticketDeleteWorkflow->activeOrigin()
               == TicketDeleteOrigin::CaptureCleanup;
    bool captureCleanupRestartedFromObservation = false;
    if (ready && captureControlledEvent && !sharedCaptureDeleteProofActive) {
        const CaptureTicketWorkflowEffect captureEffect =
                m_captureWorkflow->observeTasks(tasks, m_keySessionId);
        captureCleanupRestartedFromObservation =
                captureEffect.action
                    == CaptureTicketWorkflowAction::BeginVerifiedDelete;
        applyCaptureEffect(captureEffect);
    }

    const bool transferConfirmationHandled = ready
            && finalizeActiveTransferConfirmation(tasks, eventBridgeEpoch);
    const bool suppressSessionActions =
            (httpTransactionControlledEvent && !transferConfirmationHandled)
            || captureControlledEvent;
    KeyTaskUpdateContext updateContext;
    updateContext.sessionReady = ready;
    updateContext.suppressSessionActions = suppressSessionActions;
    updateContext.transferConfirmationHandled =
            transferConfirmationHandled;
    // The Q_TASK that decided to restart DEL is pre-delete evidence and must
    // not be reused immediately as post-delete verification.
    updateContext.deleteActive =
            m_ticketDeleteWorkflow->hasActiveOperation()
            && !captureCleanupRestartedFromObservation;
    const TicketDeleteOrigin activeDeleteOrigin =
            m_ticketDeleteWorkflow->activeOrigin();
    updateContext.allowDeleteVerification =
            updateContext.deleteActive
            && (activeDeleteOrigin == TicketDeleteOrigin::ManualCaptureDelete
                || activeDeleteOrigin == TicketDeleteOrigin::CaptureCleanup);
    updateContext.cancelDeleteActive =
            m_ticketCancelWorkflow->hasActiveDelete();
    updateContext.systemTicketSelected =
            !m_selection->selectedSystemTicketId().isEmpty();
    updateContext.taskListEmpty = tasks.isEmpty();
    updateContext.transferActive =
            m_ticketTransferWorkflow->hasActiveOperation()
            || m_captureWorkflow->hasActiveOperation();
    const KeyTaskUpdatePlan updatePlan =
            KeyTaskUpdateUseCase::plan(updateContext);

    if (suppressSessionActions) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] TasksUpdated captured for active http transfer transaction taskId=%1 session=%2 epoch=%3; suppress auto side-effects")
                      .arg(m_ticketTransferWorkflow->activeWorkbenchTaskId().isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_ticketTransferWorkflow->activeWorkbenchTaskId(),
                           QString::number(m_ticketTransferWorkflow->activeWorkbenchSessionId()),
                           QString::number(m_ticketTransferWorkflow->activeWorkbenchBridgeEpoch()));
    } else if (ready) {
        if (updatePlan.reconcileSystemTickets) {
            reconcileSystemTicketsFromKeyTasks(tasks);
        }
        if (updatePlan.recoverAutomaticTransfer) {
            recoverAutoTransferWhenKeyEmpty(tasks);
        }
        if (updatePlan.tryReturnedTicketCleanup) {
            tryAutoCleanupReturnedTicket(tasks);
        }
    } else {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] TasksUpdated arrived while session not ready, skip auto actions currentSession=%1")
                      .arg(QString::number(m_keySessionId));
    }
    m_selection->reconcileKeyTasks(tasks);
    emit tasksUpdated(tasks);
    if (!transferConfirmationHandled) {
        emit statusMessage(QStringLiteral("Q_TASK 完成，任务数=%1").arg(tasks.size()));
    }
    if (updatePlan.finalizeDelete) {
        finalizePendingReturnDelete(tasks);
    }
    if (updatePlan.finalizeCancelDelete) {
        finalizePendingCancelDelete(tasks);
    }
    if (updatePlan.processPendingCancel) {
        tryProcessPendingCancelTasks(tasks);
    }
    if (updatePlan.tryProgressiveReturn) {
        tryStartProgressiveStepReturnSweep(tasks);
    }
    if (updatePlan.tryAutomaticReturn) {
        tryAutoReturnCompletedTicket(tasks);
    }
    if (updatePlan.refreshSelectedTicket) {
        emit selectedSystemTicketChanged(
                    m_ticketStore->ticketById(m_selection->selectedSystemTicketId()));
    }
    if (updatePlan.tryAutomaticTransfer) {
        tryAutoTransferPendingTicket();
    }
    // A座所有业务处理完成后，若无命令在途，触发 A座自动维护链（电量+校时）
    if (updatePlan.runMaintenance) {
        m_maintenanceService->afterTasksUpdated(snapshot);
    }
}
void KeyManageController::handleTaskLogReadyEvent(const KeySessionEvent &event)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("stationNo"),
                   event.data.value(QStringLiteral("stationNo")).toInt());
    payload.insert(QStringLiteral("steps"),
                   event.data.value(QStringLiteral("steps")).toInt());
    payload.insert(QStringLiteral("taskId"),
                   event.data.value(QStringLiteral("taskId")).toString());

    QJsonArray items;
    const QVariantList rawItems =
            event.data.value(QStringLiteral("items")).toList();
    for (const QVariant &itemVar : rawItems) {
        items.append(QJsonObject::fromVariantMap(itemVar.toMap()));
    }
    payload.insert(QStringLiteral("taskLogItems"), items);

    const TicketReturnWorkflowEffect effect =
            m_ticketReturnWorkflow->acceptTaskLog(payload, m_keySessionId);
    if (effect.action == TicketReturnWorkflowAction::HandleProgressiveLog) {
        handleProgressiveTaskLog(event);
        return;
    }
    applyReturnEffect(effect);
}

void KeyManageController::handleTaskLogFailedEvent(const QString &reason)
{
    const QString message = reason.trimmed().isEmpty()
            ? QStringLiteral("\u56DE\u4F20\u65E5\u5FD7\u63A5\u6536\u5931\u8D25\uFF0C\u8BF7\u91CD\u8BD5")
            : reason.trimmed();
    if (m_ticketReturnWorkflow->hasReturnWork()) {
        failActiveReturnHandshake(message);
    }
    emit statusMessage(message);
}

void KeyManageController::handleTicketTransferProgressEvent(int frameIndex,
                                                            int totalFrames)
{
    if (m_captureWorkflow->ownsTicketTransfer()) {
        emit statusMessage(
                    QStringLiteral("\u6b63\u5728\u4e0b\u53d1\u91c7\u7801\u7968\uff1a\u7b2c %1/%2 \u5e27")
                        .arg(frameIndex).arg(totalFrames));
        return;
    }
    const TicketTransferWorkflowEffect effect =
            m_ticketTransferWorkflow->recordFrameProgress(m_keySessionId);
    if (!effect.handled) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] ignore stale TicketTransferProgress activeTransfer=%1 activeTransferSession=%2 currentSession=%3")
                      .arg(m_ticketTransferWorkflow->activeTaskId().isEmpty()
                               ? QStringLiteral("<none>")
                               : m_ticketTransferWorkflow->activeTaskId(),
                           QString::number(m_ticketTransferWorkflow->activeSessionId()),
                           QString::number(m_keySessionId));
        return;
    }
    emit statusMessage(
                QStringLiteral("\u4F20\u7968\u53D1\u9001\u4E2D\uFF1A\u7B2C %1/%2 \u5E27")
                .arg(frameIndex)
                .arg(totalFrames));
}

void KeyManageController::handleTicketTransferFinishedEvent(
        int eventBridgeEpoch)
{
    if (m_captureWorkflow->ownsTicketTransfer()) {
        applyCaptureEffect(
                    m_captureWorkflow->finishDelivery(m_keySessionId));
        return;
    }
    const QString activeTaskId = m_ticketTransferWorkflow->activeTaskId();
    const int activeSessionId = m_ticketTransferWorkflow->activeSessionId();
    const TicketTransferWorkflowEffect effect =
            m_ticketTransferWorkflow->finishFrames(
                m_keySessionId, eventBridgeEpoch);
    if (!effect.handled) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] ignore stale TicketTransferFinished activeTransfer=%1 activeTransferSession=%2 currentSession=%3")
                      .arg(activeTaskId.isEmpty()
                               ? QStringLiteral("<none>") : activeTaskId,
                           QString::number(activeSessionId),
                           QString::number(m_keySessionId));
        return;
    }

    if (effect.action == TicketTransferWorkflowAction::QueryTasks) {
        m_observability->writeBusinessEvent(
                    QStringLiteral("transfer"),
                    QStringLiteral("ticket-ack"),
                    QStringLiteral("awaiting-q-task"),
                    effect.taskId,
                    effect.operationSessionId);
    }
    applyTransferEffect(effect);
}

bool KeyManageController::finalizeActiveTransferConfirmation(
        const QList<KeyTaskDto> &tasks,
        int eventBridgeEpoch)
{
    const TicketTransferWorkflowEffect effect =
            m_ticketTransferWorkflow->verifyDevicePresence(
                tasks,
                m_keySessionId,
                eventBridgeEpoch,
                m_keyTaskObservation->observedAtMs());
    if (!effect.handled) {
        return false;
    }

    const bool confirmed = effect.action
            == TicketTransferWorkflowAction::CompleteSuccess;
    m_observability->writeBusinessEvent(
                QStringLiteral("transfer"),
                QStringLiteral("q-task-after"),
                confirmed ? QStringLiteral("task-confirmed")
                          : QStringLiteral("task-missing"),
                effect.taskId,
                effect.operationSessionId,
                -1,
                effect.deferredMessage,
                confirmed ? BusinessLogSeverity::Info
                          : BusinessLogSeverity::Warning);
    applyTransferEffect(effect);
    return true;
}

void KeyManageController::failActiveTransferConfirmation(
        const QString &reason)
{
    const TicketTransferWorkflowEffect effect =
            m_ticketTransferWorkflow->failDeviceConfirmation(reason);
    if (!effect.handled) {
        return;
    }
    m_observability->writeBusinessEvent(
                QStringLiteral("transfer"),
                QStringLiteral("q-task-after"),
                QStringLiteral("confirmation-failed"),
                effect.taskId,
                effect.operationSessionId,
                -1,
                reason,
                BusinessLogSeverity::Warning);
    applyTransferEffect(effect);
}

void KeyManageController::handleTicketTransferFailedEvent(
        const QString &what,
        int eventBridgeEpoch)
{
    if (m_captureWorkflow->ownsTicketTransfer()) {
        applyCaptureEffect(m_captureWorkflow->failDelivery(what));
        return;
    }
    const QString activeTaskId = m_ticketTransferWorkflow->activeTaskId();
    const int activeSessionId = m_ticketTransferWorkflow->activeSessionId();
    const TicketTransferWorkflowEffect effect =
            m_ticketTransferWorkflow->failTransfer(
                what, m_keySessionId, eventBridgeEpoch);
    if (!effect.handled) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] ignore stale TicketTransferFailed reason=%1 activeTransfer=%2 activeTransferSession=%3 currentSession=%4")
                      .arg(what,
                           activeTaskId.isEmpty()
                               ? QStringLiteral("<none>") : activeTaskId,
                           QString::number(activeSessionId),
                           QString::number(m_keySessionId));
        return;
    }
    applyTransferEffect(effect);
}

void KeyManageController::handleKeyLinkInterrupted(
        const KeySessionSnapshot &snapshot)
{
    const bool linkInterrupted = !snapshot.connected || !snapshot.keyPresent;
    if (linkInterrupted) {
        if (m_captureWorkflow->hasActiveOperation()) {
            applyCaptureEffect(m_captureWorkflow->interrupt(
                                   !snapshot.connected
                                   ? QStringLiteral("\u4e32\u53e3\u5df2\u65ad\u5f00")
                                   : QStringLiteral("\u94a5\u5319\u5df2\u79fb\u9664")));
        }
        invalidateCurrentKeySession(!snapshot.connected
                ? QStringLiteral("串口断开")
                : QStringLiteral("钥匙移除"));
        if (m_ticketTransferWorkflow->hasActiveOperation()) {
            const QString reason = !snapshot.connected
                    ? QStringLiteral(
                        "\u4E32\u53E3\u5DF2\u65AD\u5F00\uFF0C\u5F53\u524D\u4F20\u7968"
                        "\u5DF2\u4E2D\u65AD\uFF0C\u8BF7\u91CD\u65B0\u8FDE\u63A5\u540E\u91CD\u8BD5")
                    : QStringLiteral(
                        "\u94A5\u5319\u5DF2\u79FB\u9664\uFF0C\u5F53\u524D\u4F20\u7968"
                        "\u5DF2\u4E2D\u65AD\uFF0C\u8BF7\u91CD\u65B0\u653E\u56DE\u94A5\u5319\u540E\u91CD\u8BD5");
            applyTransferEffect(
                        m_ticketTransferWorkflow->interrupt(reason));
        }

        if (m_ticketReturnWorkflow->hasActiveReturn() || m_ticketReturnWorkflow->hasPendingHandshake()) {
            const QString reason = !snapshot.connected
                    ? QStringLiteral("串口已断开，当前回传已中断，请重新连接后重试")
                    : QStringLiteral("钥匙已移除，当前回传已中断，请重新放回钥匙后重试");
            failActiveReturnHandshake(reason);
            emit statusMessage(reason);
        }

        if (m_ticketDeleteWorkflow->hasActiveOperation()) {
            const TicketDeleteWorkflowEffect effect =
                    m_ticketDeleteWorkflow->interrupt(
                        TicketDeleteInterruptionCause::KeyRemoved);
            if (effect.action == TicketDeleteWorkflowAction::AwaitUserRetry) {
                emit statusMessage(
                        QStringLiteral("%1：%2").arg(effect.message, effect.taskId));
            } else {
                emit statusMessage(
                        QStringLiteral("钥匙已移除，任务 %1 的钥匙清理验证已中断；重新放回后将继续检查")
                                .arg(effect.taskId));
            }
        }
    }

    if (linkInterrupted && m_ticketCancelWorkflow->hasActiveDelete()) {
        const QString message =
                QStringLiteral("钥匙已移除，撤销删除验证已中断，等待重新放回后继续对账");
        const QString taskId =
                m_ticketCancelWorkflow->interruptActiveDelete(message);
        emit statusMessage(QStringLiteral("钥匙已移除，任务 %1 的撤销删除链已中断；重新放回后将继续对账").arg(taskId));
    }

}
void KeyManageController::handleKeyStateChangedEvent(
        const KeySessionSnapshot &snapshot)
{
    const bool ready = snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
    handleKeyLinkInterrupted(snapshot);

    if (ready && !m_keyTaskObservation->wasReady()) {
        openNewKeySession();
        applyCaptureEffect(
                    m_captureWorkflow->resumeAfterReconnect(m_keySessionId));
    }

    emit sessionSnapshotChanged(snapshot);
    if (!m_ticketTransferWorkflow->hasActiveWorkbenchTransaction()) {
        tryAutoRefreshKeyTasksOnReady(snapshot);
        schedulePendingCancelReconcile(QStringLiteral("key-state-ready"), 150);
        tryAutoTransferPendingTicket();
    } else if (m_ticketTransferWorkflow->hasActiveWorkbenchTransaction()) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] skip ready-edge automation during active http transfer transaction taskId=%1 session=%2 epoch=%3")
                      .arg(m_ticketTransferWorkflow->activeWorkbenchTaskId().isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_ticketTransferWorkflow->activeWorkbenchTaskId(),
                           QString::number(m_ticketTransferWorkflow->activeWorkbenchSessionId()),
                           QString::number(m_ticketTransferWorkflow->activeWorkbenchBridgeEpoch()));
    }
}
void KeyManageController::handleSessionEvent(const KeySessionEvent &event)
{
    const int eventBridgeEpoch =
            event.data.value(QStringLiteral("bridgeEpoch")).toInt();
    const KeyManageSessionEventRoute route =
            KeyManageEventUseCase::routeSessionEvent(
                event.kind,
                m_keyPortRuntime->acceptsBridgeEvent(eventBridgeEpoch));
    if (route == KeyManageSessionEventRoute::IgnoreStale) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] ignore stale bridge-epoch event kind=%1 eventEpoch=%2 acceptedEpoch=%3")
                      .arg(QString::number(static_cast<int>(event.kind)),
                           QString::number(eventBridgeEpoch),
                           QString::number(m_keyPortRuntime->acceptedBridgeEpoch()));
        return;
    }

    switch (route) {
    case KeyManageSessionEventRoute::IgnoreStale:
    case KeyManageSessionEventRoute::NoAction:
        break;
    case KeyManageSessionEventRoute::Notice:
        emit statusMessage(event.data.value(QStringLiteral("what")).toString());
        break;
    case KeyManageSessionEventRoute::Timeout:
        handleTimeoutEvent(
                    static_cast<quint8>(
                        event.data.value(QStringLiteral("command")).toInt()),
                    event.data.value(QStringLiteral("what")).toString());
        break;
    case KeyManageSessionEventRoute::Ack:
        handleAckEvent(static_cast<quint8>(
                           event.data.value(QStringLiteral("ackedCmd")).toInt()));
        break;
    case KeyManageSessionEventRoute::Nak:
        handleNakEvent(
                    static_cast<quint8>(
                        event.data.value(QStringLiteral("origCmd")).toInt()),
                    static_cast<quint8>(
                        event.data.value(QStringLiteral("errCode")).toInt()));
        break;
    case KeyManageSessionEventRoute::TasksUpdated:
        handleTasksUpdatedEvent(
                    event.data.value(QStringLiteral("tasks")).toList(),
                    eventBridgeEpoch);
        break;
    case KeyManageSessionEventRoute::TaskLogReady:
        handleTaskLogReadyEvent(event);
        break;
    case KeyManageSessionEventRoute::TaskLogFailed:
        handleTaskLogFailedEvent(
                    event.data.value(QStringLiteral("what")).toString());
        break;
    case KeyManageSessionEventRoute::CaptureRfidReady:
        handleCaptureRfidReadyEvent(event);
        break;
    case KeyManageSessionEventRoute::CaptureRfidFailed:
        handleCaptureRfidFailedEvent(event);
        break;
    case KeyManageSessionEventRoute::KeyStateChanged:
        handleKeyStateChangedEvent(m_session->snapshot());
        break;
    case KeyManageSessionEventRoute::TransferProgress:
        handleTicketTransferProgressEvent(
                    event.data.value(QStringLiteral("frameIndex")).toInt(),
                    event.data.value(QStringLiteral("totalFrames")).toInt());
        break;
    case KeyManageSessionEventRoute::TransferFinished:
        handleTicketTransferFinishedEvent(eventBridgeEpoch);
        break;
    case KeyManageSessionEventRoute::TransferFailed:
        handleTicketTransferFailedEvent(
                    event.data.value(QStringLiteral("what")).toString(),
                    eventBridgeEpoch);
        break;
    }
}
