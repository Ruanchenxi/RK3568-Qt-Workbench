#ifndef TICKETTRANSFEROPERATIONCONTEXT_H
#define TICKETTRANSFEROPERATIONCONTEXT_H

#include <QByteArray>
#include <QString>

// Correlates one transfer with its physical-key session and SET_COM handshake.
// This context is process-memory-only and never persists ticket lifecycle data.
struct TicketTransferOperationContext
{
    QString taskId;
    int keySessionId = 0;
    QString handshakeTaskId;
    QByteArray handshakeJsonBytes;
    int handshakeDebugChunk = 0;
    bool awaitingDeviceConfirmation = false;

    bool isActive() const
    {
        return !taskId.isEmpty();
    }

    bool hasPendingHandshake() const
    {
        return !handshakeTaskId.isEmpty();
    }

    bool isAwaitingDeviceConfirmation() const
    {
        return isActive() && awaitingDeviceConfirmation;
    }

    // Starts one transfer only after the use case has accepted the request.
    void begin(const QString &newTaskId,
               const QByteArray &jsonBytes,
               int debugChunk,
               int sessionId)
    {
        taskId = newTaskId;
        keySessionId = sessionId;
        handshakeTaskId = newTaskId;
        handshakeJsonBytes = jsonBytes;
        handshakeDebugChunk = debugChunk;
        awaitingDeviceConfirmation = false;
    }

    // Completes only the SET_COM handshake while retaining transfer identity.
    void clearHandshake()
    {
        handshakeTaskId.clear();
        handshakeJsonBytes.clear();
        handshakeDebugChunk = 0;
    }

    // Retains transfer identity after TICKET ACK until Q_TASK supplies the
    // authoritative device-presence proof.
    void waitForDeviceConfirmation()
    {
        clearHandshake();
        awaitingDeviceConfirmation = true;
    }

    // Ends the active transfer only after Q_TASK confirmation or failure.
    void clearActive()
    {
        taskId.clear();
        keySessionId = 0;
        awaitingDeviceConfirmation = false;
    }

    // Aborts the complete transfer chain, including a pending handshake.
    void clear()
    {
        clearHandshake();
        clearActive();
    }
};

#endif // TICKETTRANSFEROPERATIONCONTEXT_H
