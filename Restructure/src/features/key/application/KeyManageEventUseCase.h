#ifndef KEYMANAGEEVENTUSECASE_H
#define KEYMANAGEEVENTUSECASE_H

#include <QString>
#include <QtGlobal>

#include "shared/contracts/IKeySessionService.h"

enum class KeyManageSessionEventRoute
{
    IgnoreStale,
    Notice,
    Timeout,
    Ack,
    Nak,
    TasksUpdated,
    TaskLogReady,
    TaskLogFailed,
    CaptureRfidReady,
    CaptureRfidFailed,
    KeyStateChanged,
    TransferProgress,
    TransferFinished,
    TransferFailed,
    NoAction
};

struct KeyCommandEventContext
{
    bool transferAwaitingConfirmation = false;
    bool deleteActive = false;
    bool cancelDeleteActive = false;
    bool transferHandshakePending = false;
    bool returnActive = false;
    bool returnHandshakePending = false;
};

struct KeyCommandEventPlan
{
    bool releaseAutomaticQuery = false;
    bool failTransferConfirmation = false;
    bool interruptDelete = false;
    bool interruptCancelDelete = false;
    bool failTransferHandshake = false;
    bool failReturnHandshake = false;
    bool rejectAmbiguousSetComAck = false;
    bool acceptTransferHandshake = false;
    bool acceptReturnSetCom = false;
    bool acceptReturnTaskLog = false;
    bool reconcileCancelDelete = false;
};

// Pure event-routing rules. L1 executes the plan; this class never sends a
// serial command, starts a timer, mutates a workflow, or emits UI state.
class KeyManageEventUseCase
{
public:
    static KeyManageSessionEventRoute routeSessionEvent(
            KeySessionEvent::Kind kind,
            bool bridgeEventAccepted);
    static KeyCommandEventPlan planTimeout(
            quint8 command,
            const KeyCommandEventContext &context);
    static KeyCommandEventPlan planAck(
            quint8 ackedCmd,
            const KeyCommandEventContext &context);
    static KeyCommandEventPlan planNak(
            quint8 originalCmd,
            const KeyCommandEventContext &context);
};

#endif // KEYMANAGEEVENTUSECASE_H
