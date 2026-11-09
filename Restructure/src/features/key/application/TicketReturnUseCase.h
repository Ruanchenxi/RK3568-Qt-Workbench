#ifndef TICKETRETURNUSECASE_H
#define TICKETRETURNUSECASE_H

#include <QByteArray>
#include <QString>
#include <QSet>

#include "features/key/domain/TicketReturnState.h"

enum class TicketReturnTrigger
{
    Manual,
    Automatic
};

enum class TicketReturnStartReason
{
    Ready,
    EmptyTaskId,
    TicketMissing,
    KeyPresenceNotConfirmed,
    CleanupActive,
    ClientNotConfigured,
    PeerWorkflowBusy,
    AnotherReturnActive,
    SameReturnActive,
    TaskMissingFromKey,
    TaskNotCompleted
};

struct TicketReturnStartRequest
{
    QString taskId;
    QString busyTaskId;
    QByteArray keyTaskIdRaw;
    quint8 keyTaskStatus = 0xFF;
    TicketReturnTrigger trigger = TicketReturnTrigger::Manual;
    bool ticketExists = false;
    bool keyPresenceConfirmed = false;
    bool cleanupActive = false;
    bool returnInFlight = false;
    bool clientConfigured = false;
    bool peerWorkflowBusy = false;
};

enum class TicketReturnLogAction
{
    HandleProgressive,
    RejectNoActiveReturn,
    UploadStandard
};

struct TicketReturnLogRequest
{
    bool progressiveReturnActive = false;
    bool returnActive = false;
    bool sameKeySession = false;
};

struct TicketReturnLogDecision
{
    TicketReturnLogAction action = TicketReturnLogAction::RejectNoActiveReturn;
    QString message;
};

enum class ProgressiveReturnLogReason
{
    Ready,
    ContextInactive,
    KeySessionChanged,
    TaskIdMismatch,
    RawTaskIdMismatch,
    UnsupportedTaskAttribute,
    StepCountMismatch
};

struct ProgressiveReturnLogRequest
{
    bool contextActive = false;
    bool sameKeySession = false;
    QString expectedTaskId;
    QString eventTaskId;
    QByteArray expectedTaskIdRaw;
    QByteArray eventTaskIdRaw;
    int taskAttribute = -1;
    int expectedSteps = 0;
    int eventSteps = -1;
};

struct ProgressiveReturnLogDecision
{
    ProgressiveReturnLogReason reason = ProgressiveReturnLogReason::ContextInactive;
    QString message;
    bool accepted = false;
};

enum class ProgressiveReturnSweepStartAction
{
    NoAction,
    CollectCandidates
};

struct ProgressiveReturnSweepStartRequest
{
    bool enabled = false;
    bool progressiveTaskActive = false;
    bool progressiveHttpUploadActive = false;
    bool candidatesPending = false;
    bool returnChainActive = false;
    bool transferActive = false;
    bool cancelActive = false;
    bool cancellationWorkPending = false;
    bool uploadConfigured = false;
    bool sessionReady = false;
    bool commandInFlight = false;
    bool recoveryWindowActive = false;
};

struct ProgressiveReturnSweepStartDecision
{
    ProgressiveReturnSweepStartAction action =
            ProgressiveReturnSweepStartAction::NoAction;
};

enum class ProgressiveReturnSweepNextAction
{
    NoAction,
    StopSweep,
    RetryAfterCommand,
    SelectCandidate
};

struct ProgressiveReturnSweepNextRequest
{
    bool progressiveTaskActive = false;
    bool progressiveHttpUploadActive = false;
    bool sameKeySession = false;
    bool blockingWorkflowActive = false;
    bool sessionReady = false;
    bool commandInFlight = false;
    bool recoveryWindowActive = false;
};

struct ProgressiveReturnSweepNextDecision
{
    ProgressiveReturnSweepNextAction action =
            ProgressiveReturnSweepNextAction::NoAction;
};

enum class ProgressiveReturnCandidateReason
{
    Eligible,
    UnsupportedKeyStatus,
    KeyTaskIdentityChanged,
    NotProgressiveTicket,
    KeyPresenceNotFresh,
    CancelActive,
    CleanupActive
};

struct ProgressiveReturnCandidateRequest
{
    int keyTaskStatus = -1;
    bool sameKeyTaskIdentity = true;
    bool progressiveTicket = false;
    bool keyPresenceFresh = false;
    bool cancelActive = false;
    bool cleanupActive = false;
};

struct ProgressiveReturnCandidateDecision
{
    ProgressiveReturnCandidateReason reason =
            ProgressiveReturnCandidateReason::UnsupportedKeyStatus;
    bool eligible = false;
};
enum class TicketReturnAutomaticCandidateReason
{
    Eligible,
    KeyTaskNotCompleted,
    TicketMissing,
    ProgressiveTicket,
    CancelActive,
    KeyPresenceNotFresh,
    ReturnStateBlocked
};

struct TicketReturnAutomaticCandidateRequest
{
    bool keyTaskCompleted = false;
    bool ticketExists = false;
    bool progressiveTicket = false;
    bool cancelActive = false;
    bool keyPresenceFresh = false;
    bool returnStateBlocked = false;
};

struct TicketReturnAutomaticCandidateDecision
{
    TicketReturnAutomaticCandidateReason reason =
            TicketReturnAutomaticCandidateReason::KeyTaskNotCompleted;
    QString reasonCode;
    bool eligible = false;
};

enum class ProgressiveReturnNextAction
{
    NoAction,
    UploadNextCompletedStep,
    WaitForRemainingSteps,
    StartVerifiedCleanup
};

struct ProgressiveReturnNextRequest
{
    QString taskId;
    bool contextActive = false;
    bool httpUploadActive = false;
    int pendingUploadCount = 0;
    int uploadedStepCount = 0;
    int totalSteps = 0;
    bool cleanupVerified = false;
};

struct ProgressiveReturnNextDecision
{
    ProgressiveReturnNextAction action = ProgressiveReturnNextAction::NoAction;
    TicketReturnState ticketState = TicketReturnState::Unknown;
    TicketReturnOutcome ticketOutcome = TicketReturnOutcome::None;
    QString ticketError;
    QString statusMessage;
};

enum class ProgressiveReturnCleanupReason
{
    Ready,
    ContextInactive,
    InvalidStepCount,
    KeySessionChanged,
    KeyStepsIncomplete,
    StepUploadsIncomplete,
    KeyTaskIdentityChanged,
    KeyTaskNotCompleted
};

struct ProgressiveReturnCleanupRequest
{
    bool contextActive = false;
    int totalSteps = 0;
    bool sameKeySession = false;
    QSet<int> observedCompletedSteps;
    QSet<int> uploadedSteps;
    bool sameKeyTaskIdentity = false;
    bool keyTaskCompleted = false;
};

struct ProgressiveReturnCleanupDecision
{
    ProgressiveReturnCleanupReason reason =
            ProgressiveReturnCleanupReason::ContextInactive;
    QString message;
    bool allowed = false;
};

enum class ProgressiveReturnUploadSuccessAction
{
    NotProgressiveUpload,
    ContinueCompletedStepUploads,
    StopProgressiveReturn
};

struct ProgressiveReturnUploadSuccessRequest
{
    QString responseTaskId;
    QString httpTaskId;
    int serialNumber = -1;
    int totalSteps = 0;
    bool contextActive = false;
    QString activeTaskId;
    bool sameKeySession = false;
};

struct ProgressiveReturnUploadSuccessDecision
{
    ProgressiveReturnUploadSuccessAction action =
            ProgressiveReturnUploadSuccessAction::NotProgressiveUpload;
    QString statusMessage;
    bool recordUploadedStep = false;
};

enum class ProgressiveReturnUploadFailureAction
{
    NotProgressiveUpload,
    StopAndKeepParentTicket
};

struct ProgressiveReturnUploadFailureRequest
{
    QString responseTaskId;
    QString httpTaskId;
    QString reason;
};

struct ProgressiveReturnUploadFailureDecision
{
    ProgressiveReturnUploadFailureAction action =
            ProgressiveReturnUploadFailureAction::NotProgressiveUpload;
    TicketReturnState ticketState = TicketReturnState::Unknown;
    TicketReturnOutcome ticketOutcome = TicketReturnOutcome::None;
    QString ticketError;
    QString statusMessage;
};

enum class TicketReturnInterruptionAction
{
    ClearStandardReturn,
    WaitForStandardUpload,
    ContinueProgressiveSweep,
    StopProgressiveReturn,
    WaitForProgressiveUpload
};

struct TicketReturnInterruptionRequest
{
    QString reason;
    bool progressiveActive = false;
    bool standardHttpUploadActive = false;

    bool progressiveHttpUploadActive = false;
    bool sameKeySession = false;
};

struct TicketReturnInterruptionDecision
{
    TicketReturnInterruptionAction action =
            TicketReturnInterruptionAction::ClearStandardReturn;
    TicketReturnState ticketState = TicketReturnState::Unknown;
    TicketReturnOutcome ticketOutcome = TicketReturnOutcome::None;
    QString ticketError;
    QString statusMessage;
};
enum class TicketReturnUploadSuccessAction
{
    StartVerifiedKeyCleanup,
    RequireManualKeyCleanup
};

struct TicketReturnUploadSuccessRequest
{
    bool sameKeySession = false;
    bool hasRawTaskId = false;
};

struct TicketReturnUploadSuccessDecision
{
    TicketReturnUploadSuccessAction action =
            TicketReturnUploadSuccessAction::RequireManualKeyCleanup;
    QString message;
};

struct TicketReturnStartResult
{
    TicketReturnStartReason reason = TicketReturnStartReason::Ready;
    QString message;
    bool allowed = false;
};

class TicketReturnUseCase
{
public:
    static TicketReturnStartResult evaluateStart(const TicketReturnStartRequest &request);

    // Routes a parsed key task-log event before payload construction or HTTP
    // upload. The controller remains responsible for those side effects.
    static TicketReturnLogDecision evaluateTaskLog(
            const TicketReturnLogRequest &request);

    // Classifies one completed-step HTTP result. The controller applies the
    // returned plan to the in-memory context and ticket projection.
    static ProgressiveReturnUploadSuccessDecision
    evaluateProgressiveUploadSuccess(
            const ProgressiveReturnUploadSuccessRequest &request);
    static ProgressiveReturnUploadFailureDecision
    evaluateProgressiveUploadFailure(
            const ProgressiveReturnUploadFailureRequest &request);

    // Requires complete key evidence and complete upload evidence before the
    // progressive parent ticket may enter verified physical-key cleanup.
    static ProgressiveReturnCleanupDecision evaluateProgressiveCleanup(
            const ProgressiveReturnCleanupRequest &request);

    // Opens a progressive sweep only when no competing key workflow owns the
    // current session and the return adapter is ready.
    static ProgressiveReturnSweepStartDecision evaluateProgressiveSweepStart(
            const ProgressiveReturnSweepStartRequest &request);

    // Controls sweep continuation without carrying work across a changed
    // physical-key session.
    static ProgressiveReturnSweepNextDecision evaluateProgressiveSweepNext(
            const ProgressiveReturnSweepNextRequest &request);

    // Applies the same ticket eligibility rules during initial collection and
    // immediately before a candidate starts its serial handshake.
    static ProgressiveReturnCandidateDecision evaluateProgressiveCandidate(
            const ProgressiveReturnCandidateRequest &request);
    // Selects one ordinary automatic-return candidate without owning serial
    // or HTTP effects; progressive tickets remain in their independent sweep.
    static TicketReturnAutomaticCandidateDecision
    evaluateAutomaticCandidate(
            const TicketReturnAutomaticCandidateRequest &request);

    // Chooses the next progressive-return action after completed key steps
    // have been compared with the in-memory uploaded-step evidence.
    static ProgressiveReturnNextDecision evaluateProgressiveNext(
            const ProgressiveReturnNextRequest &request);

    // Validates that a progressive task-log belongs to the active physical-key
    // operation before completed steps are collected for upload.
    static ProgressiveReturnLogDecision evaluateProgressiveTaskLog(
            const ProgressiveReturnLogRequest &request);

    // Converts a return handshake or protocol interruption into one explicit
    // cleanup action without allowing work to continue on another key session.
    static TicketReturnInterruptionDecision evaluateInterruption(
            const TicketReturnInterruptionRequest &request);

    // Decides whether a successful backend upload may proceed to physical-key
    // cleanup. Upload success alone is not authority to delete from another key.
    static TicketReturnUploadSuccessDecision evaluateUploadSuccess(
            const TicketReturnUploadSuccessRequest &request);
};

#endif // TICKETRETURNUSECASE_H
