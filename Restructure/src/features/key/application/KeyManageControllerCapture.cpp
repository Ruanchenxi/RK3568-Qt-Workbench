/**
 * L1 execution for the independent capture-ticket workflow.
 *
 * This file translates typed L2 effects into serial/HTTP calls. It owns no
 * capture phase flags and never decides whether partial RFID data is complete.
 */
#include "features/key/application/KeyManageController.h"

#include <QJsonArray>
#include <QSet>

#include "core/ConfigManager.h"
#include "features/key/application/CaptureTicketHttpClient.h"
#include "features/key/application/CaptureTicketWorkflowService.h"
#include "features/key/application/TicketCancelWorkflowService.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketTransferWorkflowService.h"
#include "features/key/protocol/CaptureTicketPayloadEncoder.h"

namespace {
QByteArray decimalTaskIdToRaw(const QString &taskId)
{
    bool ok = false;
    const quint64 value = taskId.toULongLong(&ok);
    if (!ok || value == 0) return QByteArray();
    QByteArray raw(16, '\0');
    for (int i = 0; i < 8; ++i) {
        raw[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
    }
    return raw;
}

QSet<int> expectedInnerCodes(const QJsonObject &claim)
{
    QSet<int> result;
    const QJsonArray steps = claim.value(QStringLiteral("steps")).toArray();
    for (const QJsonValue &value : steps) {
        const QJsonObject step = value.toObject();
        bool ok = false;
        int innerCode = -1;
        if (step.value(QStringLiteral("innerCode")).isDouble()) {
            innerCode = step.value(QStringLiteral("innerCode")).toInt(-1);
            ok = innerCode >= 0;
        } else {
            innerCode = step.value(QStringLiteral("innerCode"))
                    .toString().toInt(&ok);
        }
        if (!ok || innerCode < 0 || innerCode > 0xFFFF
                || result.contains(innerCode)) {
            return QSet<int>();
        }
        result.insert(innerCode);
    }
    return result;
}

quint8 configuredProtocolStation()
{
    bool ok = false;
    const int value = ConfigManager::instance()->stationId().toInt(&ok);
    return static_cast<quint8>(ok && value >= 1 && value <= 255 ? value : 1);
}
}

void KeyManageController::onCaptureTicketClicked(int stationNo)
{
    CaptureTicketAdmissionRequest request;
    request.stationNo = stationNo;
    request.session = m_session->snapshot();
    request.captureBusy = m_captureWorkflow->hasForegroundOperation();
    request.stationAlreadyPending =
            m_captureWorkflow->hasPendingStation(stationNo);
    request.peerWorkflowBusy =
            m_ticketTransferWorkflow->hasActiveOperation()
            || m_ticketReturnWorkflow->hasReturnWork()
            || m_ticketDeleteWorkflow->hasActiveOperation()
            || m_ticketCancelWorkflow->hasPendingWork()
            || m_ticketCancelWorkflow->hasActiveDelete();
    applyCaptureEffect(m_captureWorkflow->start(
                           request, m_keySessionId, nextOpId()));
}

void KeyManageController::applyCaptureEffect(
        const CaptureTicketWorkflowEffect &effect)
{
    if (!effect.handled) return;
    if (!effect.message.isEmpty()) emit statusMessage(effect.message);
    CommandRequest command;
    switch (effect.action) {
    case CaptureTicketWorkflowAction::None:
    case CaptureTicketWorkflowAction::ReportFailure:
        return;
    case CaptureTicketWorkflowAction::Complete:
        // A fresh Q_TASK absence closes any stale capture-cleanup command
        // context left by a removed key; the observed key state is decisive.
        if (m_ticketDeleteWorkflow->hasActiveOperation()
                && m_ticketDeleteWorkflow->activeOrigin()
                   == TicketDeleteOrigin::CaptureCleanup
                && m_ticketDeleteWorkflow->activeTaskId() == effect.taskId) {
            m_ticketDeleteWorkflow->clear();
        }
        return;
    case CaptureTicketWorkflowAction::QueryTasks:
        command.id = CommandId::QueryTasks;
        command.opId = effect.operationId;
        m_session->execute(command);
        return;
    case CaptureTicketWorkflowAction::ClaimTicket:
        m_captureHttpClient->claim(effect.stationNo, effect.operationId);
        return;
    case CaptureTicketWorkflowAction::SendTicket: {
        TicketTransferRequest transfer;
        transfer.encodedPayload = effect.payload;
        transfer.stationId = configuredProtocolStation();
        transfer.opId = effect.operationId;
        m_session->transferTicket(transfer);
        return;
    }
    case CaptureTicketWorkflowAction::ReadRfid:
        command.id = CommandId::QueryCaptureRfid;
        command.opId = effect.operationId;
        command.payload = effect.taskIdRaw;
        m_session->execute(command);
        return;
    case CaptureTicketWorkflowAction::UploadResults:
        m_captureHttpClient->finish(
                    effect.taskId, effect.taskLogItems, effect.operationId);
        return;
    case CaptureTicketWorkflowAction::BeginVerifiedDelete: {
        const KeySessionSnapshot snapshot = m_session->snapshot();
        if (!snapshot.connected || !snapshot.keyPresent
                || !snapshot.keyStable || !snapshot.sessionReady
                || snapshot.commandInFlight
                || snapshot.recoveryWindowActive) {
            emit statusMessage(
                        QStringLiteral("\u91c7\u7801\u7ed3\u679c\u5df2\u4e0a\u4f20\uff0c\u7b49\u5f85\u94a5\u5319\u901a\u8baf\u6062\u590d\u540e\u4ec5\u7ee7\u7eed\u5220\u7968"));
            return;
        }
        const TicketDeleteWorkflowEffect cleanup =
                m_ticketDeleteWorkflow->beginCaptureCleanup(
                    effect.taskId, effect.taskIdRaw, m_keySessionId);
        if (cleanup.action != TicketDeleteWorkflowAction::SendDelete) {
            applyCaptureEffect(m_captureWorkflow->completeVerifiedDelete(
                                   effect.taskId, false,
                                   QStringLiteral("\u65e0\u6cd5\u521b\u5efa\u5220\u7968\u6821\u9a8c\u4e0a\u4e0b\u6587")));
            return;
        }
        command.id = CommandId::DeleteTask;
        command.opId = effect.operationId;
        command.payload = cleanup.taskIdRaw;
        m_ticketDeleteWorkflow->recordDeleteDispatched();
        m_session->execute(command);
        return;
    }
    }
}

void KeyManageController::handleCaptureClaimSucceeded(
        int operationId, const QJsonObject &data)
{
    const CaptureTicketPayloadEncoder::EncodeResult encoded =
            CaptureTicketPayloadEncoder::encode(data);
    for (const QString &line : encoded.fieldLog) {
        handleHttpClientLog(line);
    }
    if (!encoded.ok) {
        applyCaptureEffect(m_captureWorkflow->rejectClaim(
                               operationId, encoded.errorMsg));
        return;
    }
    const KeySessionSnapshot snapshot = m_session->snapshot();
    const bool keyReady = snapshot.connected && snapshot.keyPresent
            && snapshot.keyStable && snapshot.sessionReady;
    applyCaptureEffect(m_captureWorkflow->acceptClaim(
                           data,
                           encoded.taskId,
                           decimalTaskIdToRaw(encoded.taskId),
                           encoded.payload,
                           expectedInnerCodes(data),
                           operationId,
                           m_keySessionId,
                           keyReady));
}

void KeyManageController::handleCaptureClaimFailed(
        int operationId, const QString &reason)
{
    applyCaptureEffect(
                m_captureWorkflow->rejectClaim(operationId, reason));
}

void KeyManageController::handleCaptureFinishSucceeded(
        int operationId, const QString &taskId)
{
    applyCaptureEffect(
                m_captureWorkflow->finishUpload(taskId, operationId));
}

void KeyManageController::handleCaptureFinishFailed(
        int operationId, const QString &taskId, const QString &reason)
{
    applyCaptureEffect(m_captureWorkflow->failUpload(
                           taskId, operationId, reason));
}

void KeyManageController::handleCaptureRfidReadyEvent(
        const KeySessionEvent &event)
{
    applyCaptureEffect(m_captureWorkflow->acceptRfid(
                           event.data.value(QStringLiteral("taskId")).toString(),
                           event.data.value(QStringLiteral("records")).toList(),
                           m_keySessionId));
}

void KeyManageController::handleCaptureRfidFailedEvent(
        const KeySessionEvent &event)
{
    QString taskId = event.data.value(QStringLiteral("taskId")).toString();
    if (taskId.isEmpty()) taskId = m_captureWorkflow->activeTaskId();
    applyCaptureEffect(m_captureWorkflow->failRfid(
                           taskId,
                           event.data.value(QStringLiteral("what")).toString()));
}
