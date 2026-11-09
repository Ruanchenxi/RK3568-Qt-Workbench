#ifndef TICKETCANCELUSECASE_H
#define TICKETCANCELUSECASE_H

#include <QString>

#include "shared/contracts/SystemTicketDto.h"
#include "features/key/domain/TicketCancelState.h"
#include "features/key/application/VerifiedKeyTaskRemoval.h"

enum class TicketCancelExecutionReason
{
    Ready,
    CancelBusy,
    TransferBusy,
    ReturnOrDeleteBusy,
    KeyNotReady,
    CommandBusy,
    RecoveryActive,
    AutoQueryBusy
};

struct TicketCancelExecutionRequest
{
    bool cancelBusy = false;
    bool transferBusy = false;
    bool returnOrDeleteBusy = false;
    bool keyReady = false;
    bool commandBusy = false;
    bool recoveryActive = false;
    bool autoQueryBusy = false;
};

struct TicketCancelExecutionDecision
{
    TicketCancelExecutionReason reason =
            TicketCancelExecutionReason::KeyNotReady;
    QString message;
    bool allowed = false;
};

enum class TicketCancelIngressReason
{
    EmptyTaskId,
    AlreadyCanceled,
    DuplicateActive,
    TicketMissing,
    ExistingIntent,
    TaskCompleted,
    ReturnCleanupActive,
    KeyTaskNotConfirmed,
    RemovePendingTransfer,
    AcceptedNow,
    AcceptedDeferred
};

enum class TicketCancelIngressAction
{
    Reject,
    ReturnDone,
    ReturnAccepted,
    RecordDone,
    RemoveTicket,
    AcceptCancel
};

struct TicketCancelIngressRequest
{
    QString taskId;
    TicketCancelState trackedCancelState = TicketCancelState::None;
    SystemTicketDto ticket;
    bool trackedRecordExists = false;
    bool keyReady = false;
    bool hasTaskList = false;
    bool taskInKey = false;
    bool canProcessNow = false;
};

struct TicketCancelIngressDecision
{
    TicketCancelIngressReason reason = TicketCancelIngressReason::TicketMissing;
    TicketCancelIngressAction action = TicketCancelIngressAction::Reject;
    QString message;
    TicketCancelState nextCancelState = TicketCancelState::None;
    bool accepted = false;
    bool deferred = false;
};

enum class TicketCancelTaskAction
{
    MarkDone,
    RejectMissingKeyTask,
    StartDelete
};

enum class TicketCancelTaskReason
{
    TaskCompleted,
    ReturnCleanupActive,
    TaskMissing,
    ReadyToDelete
};

struct TicketCancelTaskObservationRequest
{
    SystemTicketDto ticket;
    KeyTaskRemovalProof removalProof =
            KeyTaskRemovalProof::TaskMissingBeforeDelete;
};

struct TicketCancelTaskObservationDecision
{
    TicketCancelTaskAction action = TicketCancelTaskAction::RejectMissingKeyTask;
    TicketCancelTaskReason reason = TicketCancelTaskReason::TaskMissing;
    QString message;
};

enum class TicketCancelDeleteVerificationAction
{
    RetryAfterSessionChange,
    CompleteCancel,
    RetryTaskStillPresent
};

struct TicketCancelDeleteVerificationDecision
{
    TicketCancelDeleteVerificationAction action =
            TicketCancelDeleteVerificationAction::RetryAfterSessionChange;
    QString message;
};
class TicketCancelUseCase
{
public:
    // Grants a same-process execution window before Q_TASK/DEL effects;
    // it does not decide HTTP cancellation semantics.
    static TicketCancelExecutionDecision evaluateExecution(
            const TicketCancelExecutionRequest &request);

    static TicketCancelIngressDecision evaluateIngress(
            const TicketCancelIngressRequest &request);

    // Maps one fresh pre-DEL Q_TASK observation to a business action.
    static TicketCancelTaskObservationDecision evaluateTaskObservation(
            const TicketCancelTaskObservationRequest &request);

    // Maps verified post-DEL proof to the next cancellation action.
    static TicketCancelDeleteVerificationDecision evaluateDeleteVerification(
            KeyTaskRemovalProof proof);
};

#endif // TICKETCANCELUSECASE_H
