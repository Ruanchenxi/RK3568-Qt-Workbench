/**
 * @file KeyManageControllerAdapters.cpp
 * @brief L1 projection and adapter callbacks for key management.
 *
 * These methods translate completed adapter operations into workflow effects
 * and presentation updates; they do not own ticket lifecycle decisions.
 */
#include "features/key/application/KeyManageController.h"

#include <QDebug>
#include <QTimer>
#include "features/key/application/KeyManageObservabilityService.h"
#include "features/key/application/KeyManageSelectionService.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/KeyProvisioningService.h"
#include "features/key/application/KeyTaskObservationService.h"
#include "features/key/application/TicketReturnUploadService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketTransferWorkflowService.h"
void KeyManageController::handleLogItem(const LogItem &item)
{
    m_logManager->append(item);
    if (m_logManager->shouldDisplay(item)) {
        emit logRowAppended(item);
    }
}

void KeyManageController::handleSystemTicketsChanged(const QList<SystemTicketDto> &tickets)
{
    const QString selectedTaskId = m_selection->selectedSystemTicketId();
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] handleSystemTicketsChanged count=%1 selectedTaskId=%2 activeTransfer=%3 activeReturn=%4 pendingDelete=%5")
                  .arg(QString::number(tickets.size()),
                       selectedTaskId.isEmpty() ? QStringLiteral("<none>") : selectedTaskId,
                       !m_ticketTransferWorkflow->hasActiveOperation()
                               ? QStringLiteral("<none>")
                               : m_ticketTransferWorkflow->activeTaskId(),
                       !m_ticketReturnWorkflow->hasActiveReturn()
                               ? QStringLiteral("<none>")
                               : m_ticketReturnWorkflow->activeTaskId(),
                       !m_ticketDeleteWorkflow->hasActiveOperation()
                               ? QStringLiteral("<none>")
                               : m_ticketDeleteWorkflow->activeTaskId());
    emit statusMessage(QStringLiteral(
                           "\u7CFB\u7EDF\u7968\u5217\u8868\u5DF2\u5237\u65B0\uFF0C"
                           "\u5F53\u524D\u7968\u6570\uFF1A%1")
                       .arg(tickets.size()));
    emit systemTicketsUpdated(tickets);

    const SystemTicketDto selected =
            m_selection->reconcileSystemTickets(tickets);
    if (selected.valid) {
        emit selectedSystemTicketChanged(selected);
    } else if (!selectedTaskId.isEmpty()) {
        emit selectedSystemTicketChanged(SystemTicketDto());
    }
}
void KeyManageController::handleHttpServerLog(const QString &text)
{
    m_observability->appendHttpServerLog(text);
    emit httpServerLogAppended(text);
}

void KeyManageController::handleHttpClientLog(const QString &text)
{
    m_observability->appendHttpClientLog(text);
    emit httpClientLogAppended(text);
}

void KeyManageController::handleInitPayloadReady(const QByteArray &payload)
{
    CommandRequest req;
    req.id = CommandId::InitKey;
    req.opId = nextOpId();
    req.payload = payload;
    m_session->execute(req);
    emit statusMessage(QStringLiteral(
                           "\u521D\u59CB\u5316\u6570\u636E\u51C6\u5907\u5B8C\u6210\uFF0C"
                           "\u5F00\u59CB\u4E0B\u53D1\u5230\u94A5\u5319"));
}
void KeyManageController::handleRfidPayloadReady(const QByteArray &payload)
{
    CommandRequest req;
    req.id = CommandId::DownloadRfid;
    req.opId = nextOpId();
    req.payload = payload;
    m_session->execute(req);
    emit statusMessage(QStringLiteral(
                           "RFID \u6570\u636E\u51C6\u5907\u5B8C\u6210\uFF0C"
                           "\u5F00\u59CB\u4E0B\u53D1\u5230\u94A5\u5319"));
}
void KeyManageController::handleProvisionRequestFailed(const QString &reason)
{
    emit statusMessage(reason);
}

void KeyManageController::handleReturnUploadSucceeded(
        const QString &taskId,
        int keySessionId)
{
    const TicketReturnWorkflowEffect progressiveEffect =
            m_ticketReturnWorkflow->completeProgressiveUploadSuccess(
                taskId, keySessionId, m_keySessionId);
    if (progressiveEffect.handled) {
        applyReturnEffect(progressiveEffect);
        return;
    }

    const TicketReturnWorkflowEffect effect =
            m_ticketReturnWorkflow->completeStandardUploadSuccess(
                taskId, keySessionId, m_keySessionId);
    if (!effect.handled) {
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] ignore stale standard return success taskId=%1 responseSession=%2 activeTaskId=%3 activeSession=%4")
                      .arg(taskId,
                           QString::number(keySessionId),
                           m_ticketReturnWorkflow->activeTaskId().isEmpty()
                               ? QStringLiteral("<none>")
                               : m_ticketReturnWorkflow->activeTaskId(),
                           QString::number(
                               m_ticketReturnWorkflow->activeSessionId()));
        return;
    }
    emit statusMessage(QStringLiteral("\u56DE\u4F20\u4E0A\u4F20\u6210\u529F\uFF1A%1")
                           .arg(taskId));
    applyReturnEffect(effect);
    schedulePendingCancelReconcile(
                QStringLiteral("return-context-cleared"), 300);
}

void KeyManageController::handleReturnUploadFailed(
        const QString &taskId,
        int keySessionId,
        const QString &reason)
{
    const TicketReturnWorkflowEffect progressiveEffect =
            m_ticketReturnWorkflow->completeProgressiveUploadFailure(
                taskId, keySessionId, m_keySessionId, reason);
    if (progressiveEffect.handled) {
        applyReturnEffect(progressiveEffect);
        return;
    }

    const TicketReturnWorkflowEffect effect =
            m_ticketReturnWorkflow->completeStandardUploadFailure(
                taskId, keySessionId, reason);
    if (!effect.handled) {
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] ignore stale standard return failure taskId=%1 responseSession=%2 activeTaskId=%3 activeSession=%4")
                      .arg(taskId,
                           QString::number(keySessionId),
                           m_ticketReturnWorkflow->activeTaskId().isEmpty()
                               ? QStringLiteral("<none>")
                               : m_ticketReturnWorkflow->activeTaskId(),
                           QString::number(
                               m_ticketReturnWorkflow->activeSessionId()));
        return;
    }
    applyReturnEffect(effect);
    schedulePendingCancelReconcile(
                QStringLiteral("return-context-cleared"), 300);
}
