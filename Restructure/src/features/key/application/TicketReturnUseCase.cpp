#include "features/key/application/TicketReturnUseCase.h"

namespace
{
TicketReturnStartResult rejected(TicketReturnStartReason reason, const QString &message)
{
    TicketReturnStartResult result;
    result.reason = reason;
    result.message = message;
    return result;
}
}

TicketReturnStartResult TicketReturnUseCase::evaluateStart(
        const TicketReturnStartRequest &request)
{
    if (request.taskId.trimmed().isEmpty()) {
        return rejected(TicketReturnStartReason::EmptyTaskId,
                        QStringLiteral("\u56de\u4f20\u5931\u8d25\uff1ataskId \u4e3a\u7a7a"));
    }
    if (!request.ticketExists) {
        return rejected(TicketReturnStartReason::TicketMissing,
                        QStringLiteral("\u56de\u4f20\u5931\u8d25\uff1a\u7cfb\u7edf\u7968\u4e0d\u5b58\u5728"));
    }
    if (!request.keyPresenceConfirmed) {
        return rejected(TicketReturnStartReason::KeyPresenceNotConfirmed,
                        QStringLiteral("\u56de\u4f20\u5931\u8d25\uff1a\u8be5\u7cfb\u7edf\u7968\u5c1a\u672a\u83b7\u5f97\u94a5\u5319\u7aef\u786e\u8ba4"));
    }
    if (request.cleanupActive) {
        return rejected(TicketReturnStartReason::CleanupActive,
                        QStringLiteral("\u8be5\u7cfb\u7edf\u7968\u5df2\u8fdb\u5165\u56de\u4f20\u6210\u529f\u540e\u7684\u6e05\u7406\u9636\u6bb5\uff0c\u672c\u6b21\u4e0d\u91cd\u590d\u4e0a\u4f20"));
    }
    if (!request.clientConfigured) {
        return rejected(TicketReturnStartReason::ClientNotConfigured,
                        QStringLiteral("\u56de\u4f20\u63a5\u53e3\u672a\u914d\u7f6e\uff0c\u6682\u4e0d\u53d1\u8d77\u56de\u4f20"));
    }
    if (request.peerWorkflowBusy) {
        return rejected(TicketReturnStartReason::PeerWorkflowBusy,
                        QStringLiteral("\u5f53\u524d\u6b63\u5728\u6267\u884c\u4f20\u7968\u6216\u64a4\u9500\uff0c\u8bf7\u7b49\u5f85\u5f53\u524d\u94a5\u5319\u64cd\u4f5c\u5b8c\u6210\u540e\u518d\u56de\u4f20"));
    }
    if (!request.busyTaskId.isEmpty() && request.busyTaskId != request.taskId) {
        return rejected(TicketReturnStartReason::AnotherReturnActive,
                        QStringLiteral("\u5f53\u524d\u4efb\u52a1 %1 \u6b63\u5728\u56de\u4f20\u6216\u6e05\u7406\uff0c\u8bf7\u7b49\u5f85\u5f53\u524d\u94fe\u8def\u5b8c\u6210\u540e\u518d\u8bd5")
                            .arg(request.busyTaskId));
    }
    if (request.busyTaskId == request.taskId
            && (request.returnInFlight || request.cleanupActive)) {
        return rejected(TicketReturnStartReason::SameReturnActive,
                        QStringLiteral("\u8be5\u7cfb\u7edf\u7968\u5f53\u524d\u56de\u4f20\u94fe\u4ecd\u5728\u6267\u884c\u4e2d\uff0c\u8bf7\u7a0d\u540e\u518d\u8bd5"));
    }
    if (request.keyTaskIdRaw.isEmpty()) {
        return rejected(TicketReturnStartReason::TaskMissingFromKey,
                        QStringLiteral("\u94a5\u5319\u5185\u672a\u627e\u5230\u8be5\u7cfb\u7edf\u7968\uff0c\u8bf7\u5148\u8bfb\u53d6\u94a5\u5319\u7968\u5217\u8868"));
    }
    if (request.keyTaskStatus != 0x02) {
        return rejected(TicketReturnStartReason::TaskNotCompleted,
                        QStringLiteral("\u8be5\u7cfb\u7edf\u7968\u5728\u94a5\u5319\u4e2d\u7684\u72b6\u6001\u672a\u5b8c\u6210\uff08hwStatus=0x%1\uff09\uff0c\u5f53\u524d\u4efb\u52a1\u5b8c\u6210\u540e\u624d\u5141\u8bb8\u56de\u4f20")
                            .arg(QString::number(request.keyTaskStatus, 16)
                                 .toUpper().rightJustified(2, QLatin1Char('0'))));
    }

    TicketReturnStartResult result;
    result.reason = TicketReturnStartReason::Ready;
    result.allowed = true;
    return result;
}

TicketReturnLogDecision TicketReturnUseCase::evaluateTaskLog(
        const TicketReturnLogRequest &request)
{
    TicketReturnLogDecision decision;

    if (request.progressiveReturnActive) {
        decision.action = TicketReturnLogAction::HandleProgressive;
        return decision;
    }
    if (!request.returnActive || !request.sameKeySession) {
        decision.action = TicketReturnLogAction::RejectNoActiveReturn;
        decision.message = QStringLiteral(
                "\u6536\u5230\u4EFB\u52A1\u56DE\u4F20\u65E5\u5FD7\uFF0C"
                "\u4F46\u5F53\u524D\u65E0\u53EF\u5339\u914D\u7684\u6D3B\u52A8\u56DE\u4F20\u4EFB\u52A1");
        return decision;
    }

    decision.action = TicketReturnLogAction::UploadStandard;
    return decision;
}

ProgressiveReturnUploadSuccessDecision
TicketReturnUseCase::evaluateProgressiveUploadSuccess(
        const ProgressiveReturnUploadSuccessRequest &request)
{
    ProgressiveReturnUploadSuccessDecision decision;
    if (request.httpTaskId.isEmpty()
            || request.responseTaskId != request.httpTaskId) {
        return decision;
    }

    decision.recordUploadedStep =
            request.serialNumber >= 0
            && request.serialNumber < request.totalSteps;
    if (!decision.recordUploadedStep) {
        decision.action =
                ProgressiveReturnUploadSuccessAction::StopProgressiveReturn;
        decision.statusMessage = QStringLiteral(
                    "\u4EFB\u52A1 %1 \u56DE\u4F20\u6210\u529F\u54CD\u5E94\u7684"
                    "\u6B65\u9AA4\u5E8F\u53F7\u65E0\u6548\uFF0C"
                    "\u5DF2\u505C\u6B62\u9010\u6B65\u56DE\u4F20")
                .arg(request.responseTaskId);
        return decision;
    }
    decision.statusMessage = QStringLiteral(
                "\u4EFB\u52A1 %1 \u7B2C %2/%3 \u6B65\u56DE\u4F20\u6210\u529F")
            .arg(request.responseTaskId)
            .arg(request.serialNumber + 1)
            .arg(request.totalSteps);

    const bool mayContinue =
            request.contextActive
            && request.activeTaskId == request.responseTaskId
            && request.sameKeySession;
    decision.action = mayContinue
            ? ProgressiveReturnUploadSuccessAction::ContinueCompletedStepUploads
            : ProgressiveReturnUploadSuccessAction::StopProgressiveReturn;
    return decision;
}

ProgressiveReturnUploadFailureDecision
TicketReturnUseCase::evaluateProgressiveUploadFailure(
        const ProgressiveReturnUploadFailureRequest &request)
{
    ProgressiveReturnUploadFailureDecision decision;
    if (request.httpTaskId.isEmpty()
            || request.responseTaskId != request.httpTaskId) {
        return decision;
    }

    decision.action =
            ProgressiveReturnUploadFailureAction::StopAndKeepParentTicket;
    decision.ticketState = TicketReturnState::Idle;
    decision.ticketOutcome = TicketReturnOutcome::RetryableFailure;
    decision.ticketError = request.reason;
    decision.statusMessage = QStringLiteral(
                "\u4EFB\u52A1 %1 \u5DF2\u5B8C\u6210\u6B65\u9AA4\u56DE\u4F20\u5931\u8D25\uFF0C"
                "\u7236\u7968\u4FDD\u7559\uFF1A%2")
            .arg(request.responseTaskId, request.reason);
    return decision;
}

TicketReturnInterruptionDecision TicketReturnUseCase::evaluateInterruption(
        const TicketReturnInterruptionRequest &request)
{
    TicketReturnInterruptionDecision decision;
    decision.ticketState = TicketReturnState::Idle;
    decision.ticketOutcome = TicketReturnOutcome::RetryableFailure;
    decision.ticketError = request.reason;
    if (!request.progressiveActive) {
        if (request.standardHttpUploadActive) {
            // The key link is gone, but the original backend request still
            // owns this return chain until its correlated callback arrives.
            decision.ticketState = TicketReturnState::Uploading;
            decision.ticketOutcome = TicketReturnOutcome::None;
            decision.ticketError.clear();
            decision.action =
                    TicketReturnInterruptionAction::WaitForStandardUpload;
            decision.statusMessage = QStringLiteral(
                        "\u56DE\u4F20\u94A5\u5319\u4F1A\u8BDD\u5DF2\u4E2D\u65AD\uFF0C"
                        "\u7B49\u5F85\u5F53\u524D HTTP \u8BF7\u6C42\u5B8C\u6210\uFF1A%1")
                    .arg(request.reason);
        }
        return decision;
    }
    if (request.progressiveHttpUploadActive) {
        // The serial side was interrupted, but the HTTP request still owns
        // the return workflow until its callback selects the final outcome.
        decision.ticketState = TicketReturnState::Uploading;
        decision.ticketOutcome = TicketReturnOutcome::None;
        decision.ticketError.clear();
        decision.action =
                TicketReturnInterruptionAction::WaitForProgressiveUpload;
        decision.statusMessage = QStringLiteral(
                    "\u9010\u6B65\u56DE\u4F20\u4F1A\u8BDD\u5DF2\u4E2D\u65AD\uFF0C"
                    "\u7B49\u5F85\u5F53\u524D HTTP \u8BF7\u6C42\u5B8C\u6210\uFF1A%1")
                .arg(request.reason);
        return decision;
    }
    decision.action = request.sameKeySession
            ? TicketReturnInterruptionAction::ContinueProgressiveSweep
            : TicketReturnInterruptionAction::StopProgressiveReturn;
    return decision;
}

ProgressiveReturnCleanupDecision TicketReturnUseCase::evaluateProgressiveCleanup(
        const ProgressiveReturnCleanupRequest &request)
{
    ProgressiveReturnCleanupDecision decision;
    if (!request.contextActive) {
        decision.reason = ProgressiveReturnCleanupReason::ContextInactive;
        return decision;
    }
    if (request.totalSteps <= 0) {
        decision.reason = ProgressiveReturnCleanupReason::InvalidStepCount;
        return decision;
    }
    if (!request.sameKeySession) {
        decision.reason = ProgressiveReturnCleanupReason::KeySessionChanged;
        return decision;
    }

    for (int serialNumber = 0;
         serialNumber < request.totalSteps;
         ++serialNumber) {
        if (!request.observedCompletedSteps.contains(serialNumber)) {
            decision.reason = ProgressiveReturnCleanupReason::KeyStepsIncomplete;
            return decision;
        }
        if (!request.uploadedSteps.contains(serialNumber)) {
            decision.reason =
                    ProgressiveReturnCleanupReason::StepUploadsIncomplete;
            return decision;
        }
    }
    if (!request.sameKeyTaskIdentity) {
        decision.reason =
                ProgressiveReturnCleanupReason::KeyTaskIdentityChanged;
        return decision;
    }
    if (!request.keyTaskCompleted) {
        decision.reason = ProgressiveReturnCleanupReason::KeyTaskNotCompleted;
        return decision;
    }

    decision.reason = ProgressiveReturnCleanupReason::Ready;
    decision.allowed = true;
    return decision;
}

ProgressiveReturnSweepStartDecision
TicketReturnUseCase::evaluateProgressiveSweepStart(
        const ProgressiveReturnSweepStartRequest &request)
{
    ProgressiveReturnSweepStartDecision decision;
    const bool workflowIdle =
            !request.progressiveTaskActive
            && !request.progressiveHttpUploadActive
            && !request.candidatesPending
            && !request.returnChainActive
            && !request.transferActive
            && !request.cancelActive
            && !request.cancellationWorkPending;
    const bool sessionAvailable =
            request.sessionReady
            && !request.commandInFlight
            && !request.recoveryWindowActive;
    if (request.enabled
            && request.uploadConfigured
            && workflowIdle
            && sessionAvailable) {
        decision.action =
                ProgressiveReturnSweepStartAction::CollectCandidates;
    }
    return decision;
}

ProgressiveReturnSweepNextDecision
TicketReturnUseCase::evaluateProgressiveSweepNext(
        const ProgressiveReturnSweepNextRequest &request)
{
    ProgressiveReturnSweepNextDecision decision;
    if (request.progressiveTaskActive
            || request.progressiveHttpUploadActive) {
        return decision;
    }
    if (!request.sameKeySession
            || request.blockingWorkflowActive
            || !request.sessionReady
            || request.recoveryWindowActive) {
        decision.action = ProgressiveReturnSweepNextAction::StopSweep;
        return decision;
    }
    decision.action = request.commandInFlight
            ? ProgressiveReturnSweepNextAction::RetryAfterCommand
            : ProgressiveReturnSweepNextAction::SelectCandidate;
    return decision;
}

ProgressiveReturnCandidateDecision
TicketReturnUseCase::evaluateProgressiveCandidate(
        const ProgressiveReturnCandidateRequest &request)
{
    ProgressiveReturnCandidateDecision decision;
    if (request.keyTaskStatus != 0x01
            && request.keyTaskStatus != 0x02) {
        return decision;
    }
    if (!request.sameKeyTaskIdentity) {
        decision.reason =
                ProgressiveReturnCandidateReason::KeyTaskIdentityChanged;
        return decision;
    }
    if (!request.progressiveTicket) {
        decision.reason =
                ProgressiveReturnCandidateReason::NotProgressiveTicket;
        return decision;
    }
    if (!request.keyPresenceFresh) {
        decision.reason =
                ProgressiveReturnCandidateReason::KeyPresenceNotFresh;
        return decision;
    }
    if (request.cancelActive) {
        decision.reason = ProgressiveReturnCandidateReason::CancelActive;
        return decision;
    }
    if (request.cleanupActive) {
        decision.reason = ProgressiveReturnCandidateReason::CleanupActive;
        return decision;
    }
    decision.reason = ProgressiveReturnCandidateReason::Eligible;
    decision.eligible = true;
    return decision;
}

TicketReturnAutomaticCandidateDecision
TicketReturnUseCase::evaluateAutomaticCandidate(
        const TicketReturnAutomaticCandidateRequest &request)
{
    TicketReturnAutomaticCandidateDecision decision;
    if (!request.keyTaskCompleted) {
        decision.reasonCode = QStringLiteral("key-task-not-completed");
        return decision;
    }
    if (!request.ticketExists) {
        decision.reason =
                TicketReturnAutomaticCandidateReason::TicketMissing;
        decision.reasonCode = QStringLiteral("ticket-missing");
        return decision;
    }
    if (request.progressiveTicket) {
        decision.reason =
                TicketReturnAutomaticCandidateReason::ProgressiveTicket;
        decision.reasonCode = QStringLiteral("progressive-ticket");
        return decision;
    }
    if (request.cancelActive) {
        decision.reason =
                TicketReturnAutomaticCandidateReason::CancelActive;
        decision.reasonCode = QStringLiteral("cancel-active");
        return decision;
    }
    if (!request.keyPresenceFresh) {
        decision.reason =
                TicketReturnAutomaticCandidateReason::KeyPresenceNotFresh;
        decision.reasonCode = QStringLiteral("key-presence-not-fresh");
        return decision;
    }
    if (request.returnStateBlocked) {
        decision.reason =
                TicketReturnAutomaticCandidateReason::ReturnStateBlocked;
        decision.reasonCode = QStringLiteral("return-state-blocked");
        return decision;
    }

    decision.reason = TicketReturnAutomaticCandidateReason::Eligible;
    decision.reasonCode = QStringLiteral("eligible");
    decision.eligible = true;
    return decision;
}

ProgressiveReturnNextDecision TicketReturnUseCase::evaluateProgressiveNext(
        const ProgressiveReturnNextRequest &request)
{
    ProgressiveReturnNextDecision decision;
    if (!request.contextActive || request.httpUploadActive) {
        decision.action = ProgressiveReturnNextAction::NoAction;
        return decision;
    }
    if (request.pendingUploadCount > 0) {
        decision.action = ProgressiveReturnNextAction::UploadNextCompletedStep;
        return decision;
    }
    if (request.cleanupVerified) {
        decision.action = ProgressiveReturnNextAction::StartVerifiedCleanup;
        return decision;
    }

    decision.action = ProgressiveReturnNextAction::WaitForRemainingSteps;
    decision.ticketState = TicketReturnState::Idle;
    decision.ticketOutcome = TicketReturnOutcome::WaitingForRemainingSteps;
    decision.ticketError = QStringLiteral(
                "\u5DF2\u9010\u6B65\u56DE\u4F20 %1/%2\uFF0C\u7B49\u5F85\u5269\u4F59\u6B65\u9AA4")
            .arg(request.uploadedStepCount)
            .arg(request.totalSteps);
    decision.statusMessage = QStringLiteral(
                "\u4EFB\u52A1 %1 \u5DF2\u56DE\u4F20 %2/%3 \u6B65\uFF0C\u7236\u7968\u7EE7\u7EED\u4FDD\u7559")
            .arg(request.taskId)
            .arg(request.uploadedStepCount)
            .arg(request.totalSteps);
    return decision;
}

ProgressiveReturnLogDecision TicketReturnUseCase::evaluateProgressiveTaskLog(
        const ProgressiveReturnLogRequest &request)
{
    ProgressiveReturnLogDecision decision;
    if (!request.contextActive) {
        decision.reason = ProgressiveReturnLogReason::ContextInactive;
        decision.message = QStringLiteral(
                "\u9010\u6B65\u56DE\u4F20\u65E5\u5FD7\u65E0\u6CD5\u5904\u7406\uFF1A"
                "\u5F53\u524D\u65E0\u6D3B\u52A8\u7684\u9010\u6B65\u56DE\u4F20\u4EFB\u52A1");
        return decision;
    }
    if (!request.sameKeySession) {
        decision.reason = ProgressiveReturnLogReason::KeySessionChanged;
        decision.message = QStringLiteral(
                "\u9010\u6B65\u56DE\u4F20\u65E5\u5FD7\u6821\u9A8C\u5931\u8D25\uFF1A"
                "\u94A5\u5319\u4F1A\u8BDD\u5DF2\u53D8\u5316");
        return decision;
    }
    if (request.eventTaskId != request.expectedTaskId) {
        decision.reason = ProgressiveReturnLogReason::TaskIdMismatch;
        decision.message = QStringLiteral(
                "\u9010\u6B65\u56DE\u4F20\u65E5\u5FD7\u6821\u9A8C\u5931\u8D25\uFF1A"
                "taskId \u4E0E\u5F53\u524D\u4EFB\u52A1\u4E0D\u5339\u914D");
        return decision;
    }
    if (request.eventTaskIdRaw != request.expectedTaskIdRaw) {
        decision.reason = ProgressiveReturnLogReason::RawTaskIdMismatch;
        decision.message = QStringLiteral(
                "\u9010\u6B65\u56DE\u4F20\u65E5\u5FD7\u6821\u9A8C\u5931\u8D25\uFF1A"
                "\u94A5\u5319\u4EFB\u52A1\u6807\u8BC6\u4E0D\u5339\u914D");
        return decision;
    }
    if (request.taskAttribute != 0) {
        decision.reason = ProgressiveReturnLogReason::UnsupportedTaskAttribute;
        decision.message = QStringLiteral(
                "\u9010\u6B65\u56DE\u4F20\u65E5\u5FD7\u6821\u9A8C\u5931\u8D25\uFF1A"
                "\u4EFB\u52A1\u7C7B\u578B\u4E0D\u652F\u6301\u9010\u6B65\u56DE\u4F20");
        return decision;
    }
    if (request.eventSteps != request.expectedSteps) {
        decision.reason = ProgressiveReturnLogReason::StepCountMismatch;
        decision.message = QStringLiteral(
                "\u9010\u6B65\u56DE\u4F20\u65E5\u5FD7\u6821\u9A8C\u5931\u8D25\uFF1A"
                "\u603B\u6B65\u6570\u4E0D\u5339\u914D\uFF08\u9884\u671F %1\uFF0C\u5B9E\u9645 %2\uFF09")
                .arg(request.expectedSteps)
                .arg(request.eventSteps);
        return decision;
    }

    decision.reason = ProgressiveReturnLogReason::Ready;
    decision.accepted = true;
    return decision;
}

TicketReturnUploadSuccessDecision TicketReturnUseCase::evaluateUploadSuccess(
        const TicketReturnUploadSuccessRequest &request)
{
    TicketReturnUploadSuccessDecision decision;
    if (request.hasRawTaskId) {
        decision.action = TicketReturnUploadSuccessAction::StartVerifiedKeyCleanup;
        return decision;
    }

    decision.message = QStringLiteral(
            "\u56de\u4f20\u4e0a\u4f20\u6210\u529f\uff0c\u4f46\u5f53\u524d\u94a5\u5319\u4f1a\u8bdd"
            "\u6216\u4efb\u52a1\u8eab\u4efd\u5df2\u53d8\u5316\uff0c\u672a\u81ea\u52a8\u5220\u9664"
            "\u94a5\u5319\u4efb\u52a1\uff0c\u8bf7\u4eba\u5de5\u786e\u8ba4");
    return decision;
}
