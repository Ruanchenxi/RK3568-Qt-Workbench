#ifndef TICKETTRANSFERUSECASE_H
#define TICKETTRANSFERUSECASE_H

#include <QByteArray>
#include <QList>
#include <QString>

#include "shared/contracts/IKeySessionService.h"
#include "shared/contracts/SystemTicketDto.h"
#include "features/key/domain/TicketTransferState.h"
#include "features/key/domain/TicketCancelState.h"

enum class TicketTransferTrigger
{
    Manual,
    Automatic,
    Workbench
};

enum class TicketTransferStartReason
{
    Ready,
    EmptyTaskId,
    TransferBusy,
    PeerWorkflowBusy,
    SessionChanged,
    BridgeChanged,
    CancelPending,
    AlreadyTransferred,
    AlreadySending,
    SerialDisconnected,
    KeyNotReady,
    SessionNotReady,
    CommandInFlight,
    RecoveryInProgress,
    MissingPayload
};

enum class WorkbenchTransferIngressReason
{
    Ready,
    EmptyTaskId,
    CancelActive,
    KeyNotReady,
    TransactionBusy,
    AlreadyTransferred,
    AlreadySending,
    AlreadyPending,
    ReturnOrDeleteBusy,
    AutoQueryBusy
};

struct WorkbenchTransferIngressRequest
{
    QString taskId;
    TicketCancelState trackedCancelState = TicketCancelState::None;
    KeySessionSnapshot session;
    bool httpTransactionActive = false;
    SystemTicketDto existingTicket;
    bool hasKeyTaskData = false;
    bool taskStillInKey = false;
    bool returnOrDeleteBusy = false;
    bool autoQueryBusy = false;
};

struct WorkbenchTransferIngressDecision
{
    WorkbenchTransferIngressReason reason = WorkbenchTransferIngressReason::Ready;
    QString message;
    bool accepted = false;
    bool resetStaleSuccess = false;
    bool includeTaskId = true;
};

enum class TicketKeyObservationReason
{
    UnknownTicket,
    CancelActive,
    ConfirmOnly,
    ConfirmAndReconcile
};

struct TicketKeyObservationDecision
{
    TicketKeyObservationReason reason = TicketKeyObservationReason::UnknownTicket;
    bool confirmed = false;
    bool reconcileTransferSuccess = false;
};

enum class TicketAutoTransferSelectionReason
{
    Selected,
    HttpTransactionActive,
    Disabled,
    OperationBusy,
    NoCandidate
};

struct TicketAutoTransferSelectionRequest
{
    bool httpTransactionActive = false;
    bool enabled = false;
    bool operationBusy = false;
    QList<SystemTicketDto> tickets;
};

struct TicketAutoTransferSelectionDecision
{
    TicketAutoTransferSelectionReason reason =
            TicketAutoTransferSelectionReason::NoCandidate;
    QString taskId;
    bool selected = false;
};

enum class TicketAutoRecoveryReason
{
    Requeue,
    CancelActive,
    StateNotRecoverable,
    NotAutomaticSource,
    ReturnBusy
};

struct TicketAutoRecoveryDecision
{
    TicketAutoRecoveryReason reason = TicketAutoRecoveryReason::StateNotRecoverable;
    bool requeue = false;
};

enum class TicketTransferResultReason
{
    Handle,
    StaleOperation
};

enum class TicketTransferFailureAction
{
    Ignore,
    QueueAutomaticRetry,
    FailManualBusy,
    Fail
};

struct TicketTransferResultRequest
{
    QString taskId;
    int operationSessionId = 0;
    int currentSessionId = 0;
};

struct TicketTransferFinishedDecision
{
    TicketTransferResultReason reason = TicketTransferResultReason::Handle;
    bool handle = false;
};

enum class TicketTransferConfirmationAction
{
    Ignore,
    ConfirmSuccess,
    FailTaskMissing,
    FailSessionChanged
};

struct TicketTransferConfirmationRequest
{
    bool awaitingDeviceConfirmation = false;
    bool sameKeySession = false;
    bool taskPresent = false;
};

struct TicketTransferConfirmationDecision
{
    TicketTransferConfirmationAction action =
            TicketTransferConfirmationAction::Ignore;
    TicketTransferState ticketState = TicketTransferState::Unknown;
    QString message;
};

struct TicketTransferFailureRequest : TicketTransferResultRequest
{
    QString failureMessage;
    SystemTicketDto ticket;
};

struct TicketTransferFailureDecision
{
    TicketTransferResultReason reason = TicketTransferResultReason::Handle;
    TicketTransferFailureAction action = TicketTransferFailureAction::Ignore;
    TicketTransferState ticketState = TicketTransferState::Unknown;
    QString ticketError;
    QString deferredMessage;
    QString statusMessage;
    QString automaticRetryReason;
    bool completeDeferredResponse = false;
};

struct TicketTransferStartRequest
{
    QString taskId;
    QByteArray jsonBytes;
    SystemTicketDto ticket;
    KeySessionSnapshot session;
    TicketTransferTrigger trigger = TicketTransferTrigger::Manual;
    bool anotherTransferActive = false;
    bool peerWorkflowBusy = false;
    bool failInsteadOfDeferring = false;
    bool currentSessionValid = true;
    bool bridgeEpochMatches = true;
};

struct TicketTransferStartResult
{
    TicketTransferStartReason reason = TicketTransferStartReason::Ready;
    QString message;
    bool allowed = false;
    bool emitMessage = false;
    bool updateTicketState = false;
    TicketTransferState ticketState = TicketTransferState::Unknown;
    QString ticketError;
};

class TicketTransferUseCase
{
public:
    // Validates a workbench HTTP request before any store or serial side
    // effects occur.
    static WorkbenchTransferIngressDecision evaluateWorkbenchIngress(
            const WorkbenchTransferIngressRequest &request);

    // Classifies a fresh Q_TASK observation before the controller completes
    // deferred HTTP or reconciles the in-memory transfer projection.
    static TicketKeyObservationDecision evaluateKeyObservation(
            const SystemTicketDto &ticket);

    // Selects one automatic candidate and classifies empty-key recovery
    // without starting serial work or mutating the ticket store.
    static TicketAutoTransferSelectionDecision selectAutomaticCandidate(
            const TicketAutoTransferSelectionRequest &request);
    static TicketAutoRecoveryDecision evaluateAutomaticRecovery(
            const SystemTicketDto &ticket);

    // Classifies completion and failure events before the controller performs
    // store, serial-query, retry, or deferred-HTTP side effects.
    static TicketTransferFinishedDecision evaluateFinished(
            const TicketTransferResultRequest &request);
    /**
     * Turns one fresh Q_TASK observation into the final transfer result.
     * TICKET ACK alone is deliberately insufficient for success.
     */
    static TicketTransferConfirmationDecision evaluateConfirmation(
            const TicketTransferConfirmationRequest &request);
    static TicketTransferFailureDecision evaluateFailure(
            const TicketTransferFailureRequest &request);

    static TicketTransferStartResult evaluateStart(const TicketTransferStartRequest &request);
};

#endif // TICKETTRANSFERUSECASE_H
