#ifndef TICKETDELETEOPERATIONCONTEXT_H
#define TICKETDELETEOPERATIONCONTEXT_H

#include <QByteArray>
#include <QString>

#include "features/key/application/TicketDeleteUseCase.h"

// Correlates one physical-key deletion attempt across DEL retries and Q_TASK
// verification. It is discarded after the operation and is never persisted.
struct TicketDeleteOperationContext
{
    QString taskId;
    QByteArray taskIdRaw;
    bool allowsRetransfer = false;
    int retryCount = 0;
    int inFlightStallCount = 0;
    bool deleteCommandDispatched = false;
    bool deleteCommandAcknowledged = false;
    TicketDeleteOrigin origin = TicketDeleteOrigin::None;
    int keySessionId = 0;

    bool isActive() const
    {
        return !taskId.isEmpty();
    }

    bool hasDeviceIdentity() const
    {
        return isActive() && !taskIdRaw.isEmpty();
    }

    // Begins one deletion only after the caller has selected the exact key
    // task and captured the current physical-key session.
    void begin(const QString &newTaskId,
               const QByteArray &newTaskIdRaw,
               TicketDeleteOrigin newOrigin,
               int newKeySessionId,
               bool newAllowsRetransfer = false)
    {
        taskId = newTaskId;
        taskIdRaw = newTaskIdRaw;
        allowsRetransfer = newAllowsRetransfer;
        retryCount = 0;
        inFlightStallCount = 0;
        deleteCommandDispatched = false;
        deleteCommandAcknowledged = false;
        origin = newOrigin;
        keySessionId = newKeySessionId;
    }

    // Keeps the retryable operation but invalidates proof tied to a removed or
    // replaced physical key.
    void invalidateKeySession()
    {
        keySessionId = 0;
    }

    // Clears correlated deletion data atomically after completion or abort.
    void clear()
    {
        taskId.clear();
        taskIdRaw.clear();
        allowsRetransfer = false;
        retryCount = 0;
        inFlightStallCount = 0;
        deleteCommandDispatched = false;
        deleteCommandAcknowledged = false;
        origin = TicketDeleteOrigin::None;
        keySessionId = 0;
    }
};

#endif // TICKETDELETEOPERATIONCONTEXT_H
