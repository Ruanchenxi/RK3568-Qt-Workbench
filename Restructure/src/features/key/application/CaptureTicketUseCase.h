#ifndef CAPTURETICKETUSECASE_H
#define CAPTURETICKETUSECASE_H

#include <QList>
#include <QSet>
#include <QString>
#include <QVariantList>

#include "shared/contracts/IKeySessionService.h"
#include "shared/contracts/KeyTaskDto.h"

enum class CaptureTicketAdmissionReason
{
    Ready,
    InvalidStation,
    KeyNotReady,
    CommandBusy,
    PeerWorkflowBusy,
    CaptureBusy,
    DuplicateStation
};

struct CaptureTicketAdmissionRequest
{
    int stationNo = 0;
    KeySessionSnapshot session;
    bool peerWorkflowBusy = false;
    bool captureBusy = false;
    bool stationAlreadyPending = false;
};

struct CaptureTicketAdmissionDecision
{
    CaptureTicketAdmissionReason reason = CaptureTicketAdmissionReason::KeyNotReady;
    QString message;
    bool allowed = false;
};

struct CaptureTicketRecordDecision
{
    bool complete = false;
    QSet<int> expectedInnerCodes;
    QSet<int> capturedInnerCodes;
    QString message;
};

class CaptureTicketUseCase
{
public:
    // Applies capture admission without performing serial or HTTP work.
    static CaptureTicketAdmissionDecision evaluateAdmission(
            const CaptureTicketAdmissionRequest &request);

    // Capture task types are interpreted here rather than in UI or protocol code.
    static bool isCaptureTaskType(quint8 type);
    static const KeyTaskDto *findTask(const QList<KeyTaskDto> &tasks,
                                      const QByteArray &taskIdRaw);

    // Fails closed when the claimed ticket has no valid expected devices.
    static CaptureTicketRecordDecision evaluateRecords(
            const QSet<int> &expectedInnerCodes,
            const QVariantList &records);
};

#endif // CAPTURETICKETUSECASE_H
