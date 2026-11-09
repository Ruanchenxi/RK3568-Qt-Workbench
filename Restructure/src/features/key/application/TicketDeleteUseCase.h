#ifndef TICKETDELETEUSECASE_H
#define TICKETDELETEUSECASE_H

#include <QString>

enum class TicketDeleteOrigin
{
    None,
    ManualDelete,
    ManualCaptureDelete,
    AutoReturnCleanup,
    CaptureCleanup,
    AdminClearUnregisteredTask
};

enum class TicketDeleteAction
{
    Ignore,
    AbortSessionChanged,
    RetryDelete,
    AwaitUserRetry,
    NotifyBackendTermination,
    RemoveTicket,
    MarkKeyCleared,
    ConfirmedAbsent
};

enum class TicketDeleteStartReason
{
    Ready,
    UnsupportedOrigin,
    AdministratorRequired,
    SerialDisconnected,
    NoTaskSelected,
    RegisteredTask,
    KeyNotReady,
    TaskObservationStale,
    CommandBusy,
    DeleteBusy,
    ReturnBusy,
    TransferBusy,
    CancelBusy
};

struct TicketDeleteStartRequest
{
    TicketDeleteOrigin origin = TicketDeleteOrigin::None;
    bool administratorAuthorized = false;
    bool serialConnected = false;
    bool taskSelected = false;
    bool ticketExists = false;
    bool keyReady = false;
    bool freshTaskObservation = false;
    bool commandBusy = false;
    bool deleteBusy = false;
    bool returnBusy = false;
    bool transferBusy = false;
    bool cancelBusy = false;
};

struct TicketDeleteStartDecision
{
    TicketDeleteStartReason reason =
            TicketDeleteStartReason::UnsupportedOrigin;
    QString message;
    bool allowed = false;
};

enum class TicketDeleteRetryAction
{
    InvalidContext,
    WaitForKey,
    WaitForCommand,
    ExhaustedManualDelete,
    ExhaustedUnregisteredClear,
    ExhaustedReturnCleanup,
    SendRetry
};

struct TicketDeleteRetryRequest
{
    TicketDeleteOrigin origin = TicketDeleteOrigin::None;
    bool hasDeviceIdentity = false;
    bool keyReady = false;
    bool commandBusy = false;
    int retryCount = 0;
    int inFlightStallCount = 0;
};

struct TicketDeleteRetryDecision
{
    TicketDeleteRetryAction action =
            TicketDeleteRetryAction::InvalidContext;
    int nextRetryCount = 0;
    int nextInFlightStallCount = 0;
    bool forceRetryLimit = false;
};

struct TicketDeleteObservation
{
    TicketDeleteOrigin origin = TicketDeleteOrigin::None;
    bool sameKeySession = false;
    bool taskStillExists = false;
    bool ticketExists = false;
    bool returnUploadCompleted = false;
    bool allowsRetransfer = false;
};

struct TicketDeleteDecision
{
    TicketDeleteAction action = TicketDeleteAction::Ignore;
    bool refreshWorkbench = false;
    bool continueReturnSweep = false;
};

class TicketDeleteUseCase
{
public:
    // Validates manual and unregistered-task deletion before a DEL
    // context is created or any serial side effect occurs.
    static TicketDeleteStartDecision evaluateStart(
            const TicketDeleteStartRequest &request);

    // Selects the next shared DEL retry action without mutating the
    // operation context or performing serial/store side effects.
    static TicketDeleteRetryDecision evaluateRetry(
            const TicketDeleteRetryRequest &request);

    static TicketDeleteDecision evaluate(const TicketDeleteObservation &observation);
};

#endif // TICKETDELETEUSECASE_H
