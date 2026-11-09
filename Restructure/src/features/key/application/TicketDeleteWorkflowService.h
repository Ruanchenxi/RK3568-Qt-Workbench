#ifndef TICKETDELETEWORKFLOWSERVICE_H
#define TICKETDELETEWORKFLOWSERVICE_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>

#include "features/key/application/TicketDeleteOperationContext.h"
#include "shared/contracts/KeyTaskDto.h"

class TicketStore;

enum class TicketDeleteWorkflowAction
{
    None,
    SendDelete,
    WaitForKey,
    WaitForCommand,
    AwaitUserRetry,
    ExhaustedManualDelete,
    ExhaustedUnregisteredClear,
    ExhaustedReturnCleanup,
    AbortSessionChanged,
    NotifyBackendTermination,
    RemoveTicket,
    MarkKeyCleared,
    ConfirmedAbsent
};

struct TicketDeleteWorkflowEffect
{
    TicketDeleteWorkflowAction action = TicketDeleteWorkflowAction::None;
    TicketDeleteOrigin origin = TicketDeleteOrigin::None;
    QString taskId;
    QByteArray taskIdRaw;
    QString message;
    int retryCount = 0;
    bool refreshWorkbench = false;
    bool continueReturnSweep = false;
    bool sameKeySession = false;
};

struct TicketDeleteStartOutcome
{
    TicketDeleteStartDecision decision;
    TicketDeleteWorkflowEffect effect;
};

struct TicketDeleteRuntimeObservation
{
    int currentKeySessionId = 0;
    bool keyReady = false;
    bool commandBusy = false;
};

enum class TicketDeleteInterruptionCause
{
    Timeout,
    Nak,
    KeyRemoved
};

// Owns the shared process-local DEL/Q_TASK proof lifecycle used by return
// cleanup and administrator cleanup. It returns typed effects and never sends
// serial commands, calls a peer workflow, or owns ACK/retry timers.
class TicketDeleteWorkflowService : public QObject
{
public:
    explicit TicketDeleteWorkflowService(TicketStore *store,
                                         QObject *parent = nullptr);

    // Applies manual/unregistered admission and creates a correlated DEL
    // context only when the pure L3 decision allows it.
    TicketDeleteStartOutcome start(const TicketDeleteStartRequest &request,
                                   const QString &taskId,
                                   const QByteArray &taskIdRaw,
                                   int keySessionId,
                                   bool allowsRetransfer = false);

    // Starts the cleanup phase after a return upload has already succeeded.
    TicketDeleteWorkflowEffect beginAutoReturnCleanup(
            const QString &taskId,
            const QByteArray &taskIdRaw,
            int keySessionId,
            bool allowsRetransfer = false);

    // Reuses DEL/Q_TASK proof for a capture whose HTTP upload already succeeded.
    TicketDeleteWorkflowEffect beginCaptureCleanup(
            const QString &taskId,
            const QByteArray &taskIdRaw,
            int keySessionId);

    bool hasActiveOperation() const;
    bool hasDeviceIdentity() const;
    QString activeTaskId() const;
    TicketDeleteOrigin activeOrigin() const;

    // Records that DEL for this exact operation was handed to the serial
    // session. If the ACK is lost during removal, a fresh Q_TASK may still
    // prove whether the command took effect.
    void recordDeleteDispatched();

    // Retains the short-lived fact that the key accepted DEL. A reconnect may
    // then use a fresh Q_TASK to finish this exact manual deletion.
    void acceptDeleteAck();

    // Converts timeout, NAK, or key removal into one retryable typed effect.
    TicketDeleteWorkflowEffect interrupt(
            TicketDeleteInterruptionCause cause);

    // Applies a fresh post-DEL Q_TASK observation and returns the next effect.
    TicketDeleteWorkflowEffect verify(
            const QList<KeyTaskDto> &tasks,
            const TicketDeleteRuntimeObservation &runtime);

    // Clears operation-local proof after an external business owner closes it.
    void clear();

    // Drops retry counters when the physical-key session is invalidated while
    // retaining the task correlation for the normal interruption path.
    void resetRetryEvidence();

private:
    TicketDeleteWorkflowEffect retry(
            const QString &reason,
            const TicketDeleteRuntimeObservation &runtime);
    TicketDeleteWorkflowEffect effect(
            TicketDeleteWorkflowAction action,
            const QString &message = QString()) const;

    TicketStore *m_store;
    TicketDeleteOperationContext m_operation;
};

#endif // TICKETDELETEWORKFLOWSERVICE_H
