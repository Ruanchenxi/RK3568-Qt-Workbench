#ifndef CAPTURETICKETOPERATIONCONTEXT_H
#define CAPTURETICKETOPERATIONCONTEXT_H

#include <QByteArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QVariantMap>

enum class CaptureTicketPhase
{
    Idle,
    CheckingKeyCompatibility,
    Claiming,
    Delivering,
    AwaitingDeliveryProof,
    WaitingForCapture,
    ReadingRfid,
    Uploading,
    AwaitingDelete
};

// One process-local capture operation. It is never serialized or exposed to L1.
struct CaptureTicketOperationContext
{
    CaptureTicketPhase phase = CaptureTicketPhase::Idle;
    int stationNo = 0;
    int keySessionId = 0;
    int operationId = -1;
    QString taskId;
    QByteArray taskIdRaw;
    QByteArray payload;
    QJsonObject claimData;
    QSet<int> expectedInnerCodes;
    QVariantMap recordsByIdentity;
    bool deliveryUnknownAfterDisconnect = false;
    bool deliveryConfirmed = false;
    int lastReadSessionId = 0;

    bool isActive() const { return phase != CaptureTicketPhase::Idle; }
    bool hasClaim() const { return !taskId.isEmpty() && !payload.isEmpty(); }
    void clear() { *this = CaptureTicketOperationContext(); }
};

#endif // CAPTURETICKETOPERATIONCONTEXT_H
