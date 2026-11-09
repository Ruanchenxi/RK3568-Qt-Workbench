#ifndef HTTPTRANSFERTRANSACTIONCONTEXT_H
#define HTTPTRANSFERTRANSACTIONCONTEXT_H

#include <QString>

// Correlates one deferred workbench HTTP request with the key session and
// serial bridge generation that accepted it. The context is never persisted.
struct HttpTransferTransactionContext
{
    bool active = false;
    QString taskId;
    int keySessionId = 0;
    int bridgeEpoch = 0;

    bool isActive() const
    {
        return active;
    }

    // Checks whether a serial event belongs to the deferred HTTP request.
    bool matches(int sessionId, int eventBridgeEpoch) const
    {
        return active
                && sessionId > 0
                && eventBridgeEpoch > 0
                && sessionId == keySessionId
                && eventBridgeEpoch == bridgeEpoch;
    }

    // Acquires the workbench transaction after ingress validation succeeds.
    void begin(const QString &newTaskId, int sessionId, int newBridgeEpoch)
    {
        active = true;
        taskId = newTaskId;
        keySessionId = sessionId;
        bridgeEpoch = newBridgeEpoch;
    }

    // Releases all correlation data after completion, rejection, or timeout.
    void clear()
    {
        active = false;
        taskId.clear();
        keySessionId = 0;
        bridgeEpoch = 0;
    }
};

#endif // HTTPTRANSFERTRANSACTIONCONTEXT_H
