#ifndef TICKETCANCELWORKFLOWSERVICE_H
#define TICKETCANCELWORKFLOWSERVICE_H

#include <QObject>
#include <QList>
#include <QString>

#include "features/key/application/TicketCancelUseCase.h"
#include "features/key/application/TicketCancelOperationContext.h"
#include "shared/contracts/KeyTaskDto.h"

class TicketCancelCoordinator;
class TicketCancelTrackingService;
class TicketStore;

enum class TicketCancelWorkflowAction
{
    None,
    MarkDone,
    Reject,
    SendDelete,
    Complete,
    RetryAfterSessionChange,
    RetryTaskStillPresent
};

struct TicketCancelWorkflowEffect
{
    TicketCancelWorkflowAction action = TicketCancelWorkflowAction::None;
    QString taskId;
    QByteArray taskIdRaw;
    QString message;
};

struct TicketCancelIngressObservation
{
    QString taskId;
    bool keyReady = false;
    bool hasTaskList = false;
    bool taskInKey = false;
    bool canProcessNow = false;
};

struct TicketCancelIngressOutcome
{
    TicketCancelIngressDecision decision;
    bool scheduleReconcile = false;
};

enum class TicketCancelExpiryAction
{
    Ignore,
    ContinueDeviceVerification,
    CancelIntent
};

struct TicketCancelExpiryOutcome
{
    TicketCancelExpiryAction action = TicketCancelExpiryAction::Ignore;
    QString taskId;
    QString message;
};

// Owns the independent L2 workbench-cancellation workflow. It updates only
// process-memory ticket state and returns typed effects; L1 executes HTTP/UI
// effects and L4 remains the sole owner of Q_TASK, DEL, ACK, and retry state.
class TicketCancelWorkflowService : public QObject
{
public:
    explicit TicketCancelWorkflowService(
            TicketStore *store,
            TicketCancelCoordinator *coordinator,
            TicketCancelTrackingService *tracking,
            QObject *parent = nullptr);

    // Initializes the in-memory cancellation record owner once per process.
    void start();

    // Returns the tracked cancellation state without exposing coordinator data.
    TicketCancelState trackedState(const QString &taskId,
                                   bool *recordExists = nullptr) const;

    // Applies one HTTP ingress decision and records its process-memory intent.
    TicketCancelIngressOutcome handleIngress(
            const TicketCancelIngressObservation &observation);

    bool hasPendingWork() const;
    bool hasActiveDelete() const;
    bool isActiveDeleteFor(const QString &taskId) const;
    QString activeDeleteTaskId() const;

    // Reconciles pending intents against one fresh pre-DEL Q_TASK result.
    QList<TicketCancelWorkflowEffect> beginPendingDeletes(
            const QList<KeyTaskDto> &tasks,
            int keySessionId);

    // Reconciles the active DEL against one fresh post-DEL Q_TASK result.
    TicketCancelWorkflowEffect verifyActiveDelete(
            const QList<KeyTaskDto> &tasks,
            int currentKeySessionId);

    // Invalidates only the physical-key proof while retaining a retryable
    // workbench cancellation intent in process memory.
    QString interruptActiveDelete(const QString &message);

    // Handles HTTP-client expiry without undoing a DEL already sent to a key.
    TicketCancelExpiryOutcome expireDeferredRequest(const QString &taskId);

private:
    SystemTicketDto ticketForIngress(const QString &taskId,
                                     bool taskInKey,
                                     TicketCancelState trackedState,
                                     bool trackedRecordExists) const;
    static SystemTicketDto transientTicket(const QString &taskId,
                                           bool wasInKey);

    TicketStore *m_store;
    TicketCancelCoordinator *m_coordinator;
    TicketCancelTrackingService *m_tracking;
    TicketCancelOperationContext m_deleteOperation;
};

#endif // TICKETCANCELWORKFLOWSERVICE_H