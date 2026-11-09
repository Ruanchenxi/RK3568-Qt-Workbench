#ifndef TICKETCANCELOPERATIONCONTEXT_H
#define TICKETCANCELOPERATIONCONTEXT_H

#include <QByteArray>
#include <QString>

// Keeps the short-lived device proof for one H5 termination request together.
// This is operation-local memory, not a persisted ticket lifecycle.
struct TicketCancelOperationContext
{
    QString taskId;
    QByteArray taskIdRaw;
    int keySessionId = 0;

    bool isActive() const
    {
        return !taskId.isEmpty();
    }

    // Starts the DEL proof only after a fresh Q_TASK has found this exact task.
    void begin(const QString &newTaskId,
               const QByteArray &newTaskIdRaw,
               int newKeySessionId)
    {
        taskId = newTaskId;
        taskIdRaw = newTaskIdRaw;
        keySessionId = newKeySessionId;
    }

    // Invalidates the physical-key proof but keeps the H5 intent retryable.
    void invalidateKeySession()
    {
        keySessionId = 0;
    }

    // Clears every correlated field together so stale task bytes cannot leak
    // into a later termination request.
    void clear()
    {
        taskId.clear();
        taskIdRaw.clear();
        keySessionId = 0;
    }
};

#endif // TICKETCANCELOPERATIONCONTEXT_H
