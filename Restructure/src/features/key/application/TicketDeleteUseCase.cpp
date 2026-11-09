#include "features/key/application/TicketDeleteUseCase.h"

TicketDeleteStartDecision TicketDeleteUseCase::evaluateStart(
        const TicketDeleteStartRequest &request)
{
    TicketDeleteStartDecision decision;
    const bool supportedOrigin =
            request.origin == TicketDeleteOrigin::ManualDelete
            || request.origin == TicketDeleteOrigin::ManualCaptureDelete
            || request.origin
               == TicketDeleteOrigin::AdminClearUnregisteredTask;
    if (!supportedOrigin) {
        return decision;
    }
    if (!request.administratorAuthorized) {
        decision.reason = TicketDeleteStartReason::AdministratorRequired;
        decision.message = QStringLiteral(
                    "\u5F53\u524D\u7528\u6237\u6CA1\u6709\u7BA1\u7406\u5458\u5220\u9664\u6743\u9650");
        return decision;
    }
    if (!request.serialConnected) {
        decision.reason = TicketDeleteStartReason::SerialDisconnected;
        decision.message = QStringLiteral(
                    "\u4E32\u53E3\u672A\u8FDE\u63A5\uFF0C\u65E0\u6CD5\u5220\u9664\u94A5\u5319\u4EFB\u52A1");
        return decision;
    }
    if (!request.taskSelected) {
        decision.reason = TicketDeleteStartReason::NoTaskSelected;
        decision.message = QStringLiteral(
                    "\u672A\u9009\u4E2D\u94A5\u5319\u4EFB\u52A1\uFF0C"
                    "\u8BF7\u5148\u8BFB\u53D6\u94A5\u5319\u7968\u5217\u8868\u5E76\u9009\u4E2D\u4E00\u6761\u4EFB\u52A1");
        return decision;
    }
    if (request.origin
            == TicketDeleteOrigin::AdminClearUnregisteredTask
            && request.ticketExists) {
        decision.reason = TicketDeleteStartReason::RegisteredTask;
        decision.message = QStringLiteral(
                    "\u9009\u4E2D\u4EFB\u52A1\u5DF2\u6709\u5BF9\u5E94\u7CFB\u7EDF\u7968\uFF0C"
                    "\u4E0D\u5141\u8BB8\u4F7F\u7528\u672A\u767B\u8BB0\u4EFB\u52A1\u6E05\u9664");
        return decision;
    }
    if (!request.keyReady) {
        decision.reason = TicketDeleteStartReason::KeyNotReady;
        decision.message = QStringLiteral(
                    "\u94A5\u5319\u672A\u5728\u4F4D\u3001\u672A\u7A33\u5B9A\u6216\u901A\u8BAF\u4F1A\u8BDD\u672A\u5C31\u7EEA\uFF0C\u65E0\u6CD5\u5220\u9664");
        return decision;
    }
    if (!request.freshTaskObservation) {
        decision.reason = TicketDeleteStartReason::TaskObservationStale;
        decision.message = QStringLiteral(
                    "\u5F53\u524D\u9009\u4E2D\u4EFB\u52A1\u6CA1\u6709\u672C\u6B21\u94A5\u5319\u4F1A\u8BDD\u7684 Q_TASK \u5728\u94A5\u8BC1\u660E\uFF0C"
                    "\u8BF7\u91CD\u65B0\u8BFB\u53D6\u94A5\u5319\u7968\u5217\u5E76\u9009\u4E2D\u540E\u518D\u5220\u9664");
        return decision;
    }
    if (request.commandBusy) {
        decision.reason = TicketDeleteStartReason::CommandBusy;
        decision.message = QStringLiteral(
                    "\u5F53\u524D\u6709\u94A5\u5319\u901A\u8BAF\u547D\u4EE4\u5728\u9014\uFF0C\u8BF7\u7A0D\u540E\u518D\u8BD5");
        return decision;
    }
    if (request.deleteBusy) {
        decision.reason = TicketDeleteStartReason::DeleteBusy;
        decision.message = QStringLiteral(
                    "\u5F53\u524D\u5DF2\u6709\u5220\u9664\u64CD\u4F5C\u5728\u9014\uFF0C\u8BF7\u7A0D\u540E\u518D\u8BD5");
        return decision;
    }
    if (request.returnBusy) {
        decision.reason = TicketDeleteStartReason::ReturnBusy;
        decision.message = QStringLiteral(
                    "\u5F53\u524D\u6709\u56DE\u4F20\u64CD\u4F5C\u5728\u9014\uFF0C\u65E0\u6CD5\u540C\u65F6\u5220\u9664\u94A5\u5319\u4EFB\u52A1");
        return decision;
    }
    if (request.transferBusy) {
        decision.reason = TicketDeleteStartReason::TransferBusy;
        decision.message = QStringLiteral(
                    "\u5F53\u524D\u6709\u4F20\u7968\u64CD\u4F5C\u5728\u9014\uFF0C\u65E0\u6CD5\u540C\u65F6\u5220\u9664\u94A5\u5319\u4EFB\u52A1");
        return decision;
    }
    if (request.cancelBusy) {
        decision.reason = TicketDeleteStartReason::CancelBusy;
        decision.message = QStringLiteral(
                    "\u5F53\u524D\u6709\u7EC8\u6B62\u64CD\u4F5C\u5728\u9014\uFF0C\u65E0\u6CD5\u540C\u65F6\u5220\u9664\u94A5\u5319\u4EFB\u52A1");
        return decision;
    }

    decision.reason = TicketDeleteStartReason::Ready;
    decision.allowed = true;
    return decision;
}
TicketDeleteRetryDecision TicketDeleteUseCase::evaluateRetry(
        const TicketDeleteRetryRequest &request)
{
    TicketDeleteRetryDecision decision;
    decision.nextRetryCount = request.retryCount;
    decision.nextInFlightStallCount = request.inFlightStallCount;
    if (!request.hasDeviceIdentity
            || request.origin == TicketDeleteOrigin::None) {
        return decision;
    }
    if (!request.keyReady) {
        decision.action = TicketDeleteRetryAction::WaitForKey;
        return decision;
    }
    if (request.commandBusy) {
        decision.action = TicketDeleteRetryAction::WaitForCommand;
        decision.nextInFlightStallCount =
                request.inFlightStallCount + 1;
        decision.forceRetryLimit =
                decision.nextInFlightStallCount >= 10;
        return decision;
    }
    if (request.retryCount >= 3) {
        switch (request.origin) {
        case TicketDeleteOrigin::ManualDelete:
        case TicketDeleteOrigin::ManualCaptureDelete:
            decision.action =
                    TicketDeleteRetryAction::ExhaustedManualDelete;
            break;
        case TicketDeleteOrigin::AdminClearUnregisteredTask:
            decision.action =
                    TicketDeleteRetryAction::ExhaustedUnregisteredClear;
            break;
        case TicketDeleteOrigin::AutoReturnCleanup:
        case TicketDeleteOrigin::CaptureCleanup:
            decision.action =
                    TicketDeleteRetryAction::ExhaustedReturnCleanup;
            break;
        case TicketDeleteOrigin::None:
            break;
        }
        return decision;
    }

    decision.action = TicketDeleteRetryAction::SendRetry;
    decision.nextRetryCount = request.retryCount + 1;
    return decision;
}

TicketDeleteDecision TicketDeleteUseCase::evaluate(
        const TicketDeleteObservation &observation)
{
    TicketDeleteDecision decision;
    if (observation.origin == TicketDeleteOrigin::None) {
        return decision;
    }

    if (!observation.sameKeySession) {
        decision.action = TicketDeleteAction::AbortSessionChanged;
        return decision;
    }

    if (observation.taskStillExists) {
        decision.action = observation.origin == TicketDeleteOrigin::AdminClearUnregisteredTask
                ? TicketDeleteAction::AwaitUserRetry
                : TicketDeleteAction::RetryDelete;
        return decision;
    }

    switch (observation.origin) {
    case TicketDeleteOrigin::ManualDelete:
        // The verified key task is sufficient authority for Admin deletion.
        // Process-memory ticket data may be absent after an application restart.
        decision.action = TicketDeleteAction::NotifyBackendTermination;
        break;
    case TicketDeleteOrigin::ManualCaptureDelete:
        // Capture tickets do not exist in the normal outage task table. A
        // same-session Q_TASK absence closes manual deletion locally.
        decision.action = TicketDeleteAction::ConfirmedAbsent;
        break;
    case TicketDeleteOrigin::AdminClearUnregisteredTask:
        decision.action = TicketDeleteAction::NotifyBackendTermination;
        break;
    case TicketDeleteOrigin::AutoReturnCleanup:
        decision.refreshWorkbench = true;
        decision.continueReturnSweep = true;
        if (observation.allowsRetransfer) {
            decision.action = TicketDeleteAction::MarkKeyCleared;
        } else if (observation.ticketExists && observation.returnUploadCompleted) {
            decision.action = TicketDeleteAction::RemoveTicket;
        } else {
            decision.action = TicketDeleteAction::ConfirmedAbsent;
        }
        break;
    case TicketDeleteOrigin::CaptureCleanup:
        decision.action = TicketDeleteAction::ConfirmedAbsent;
        break;
    case TicketDeleteOrigin::None:
        break;
    }
    return decision;
}
