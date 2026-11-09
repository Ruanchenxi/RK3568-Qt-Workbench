#ifndef TICKETRETURNOPERATIONCONTEXT_H
#define TICKETRETURNOPERATIONCONTEXT_H

#include <QByteArray>
#include <QString>

// Correlates one return handshake and upload with the exact physical-key
// session that produced the task log. It is operation-local and never persisted.
struct TicketReturnOperationContext
{
    QString taskId;
    QByteArray taskIdRaw;
    QString handshakeTaskId;
    QByteArray handshakeTaskIdRaw;
    int keySessionId = 0;
    bool httpUploadActive = false;

    bool isActive() const
    {
        return !taskId.isEmpty();
    }

    bool hasPendingHandshake() const
    {
        return !handshakeTaskId.isEmpty();
    }

    // Starts a return only after the task has been matched to the current key.
    void begin(const QString &newTaskId,
               const QByteArray &newTaskIdRaw,
               int newKeySessionId)
    {
        taskId = newTaskId;
        taskIdRaw = newTaskIdRaw;
        handshakeTaskId = newTaskId;
        handshakeTaskIdRaw = newTaskIdRaw;
        keySessionId = newKeySessionId;
        httpUploadActive = false;
    }

    // The handshake is one phase of the same operation and can finish before
    // the task log upload and verified device cleanup.
    void clearHandshake()
    {
        handshakeTaskId.clear();
        handshakeTaskIdRaw.clear();
    }

    // Keeps the return chain busy while the backend owns the parsed key log.
    void beginHttpUpload()
    {
        httpUploadActive = true;
    }

    bool hasHttpUpload() const
    {
        return httpUploadActive;
    }

    void clearHttpUpload()
    {
        httpUploadActive = false;
    }

    // Clears all correlated return data after completion or a terminal abort.
    void clear()
    {
        taskId.clear();
        taskIdRaw.clear();
        clearHandshake();
        keySessionId = 0;
        clearHttpUpload();
    }

    QString chainTaskId() const
    {
        return isActive() ? taskId : handshakeTaskId;
    }
};

#endif // TICKETRETURNOPERATIONCONTEXT_H
