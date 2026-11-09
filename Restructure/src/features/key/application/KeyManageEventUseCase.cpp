#include "features/key/application/KeyManageEventUseCase.h"

#include "features/key/protocol/KeyProtocolDefs.h"

namespace {

bool staleSensitive(KeySessionEvent::Kind kind)
{
    switch (kind) {
    case KeySessionEvent::TasksUpdated:
    case KeySessionEvent::TaskLogReady:
    case KeySessionEvent::TaskLogFailed:
    case KeySessionEvent::CaptureRfidReady:
    case KeySessionEvent::CaptureRfidFailed:
    case KeySessionEvent::Ack:
    case KeySessionEvent::Nak:
    case KeySessionEvent::Timeout:
    case KeySessionEvent::KeyStateChanged:
    case KeySessionEvent::TicketTransferProgress:
    case KeySessionEvent::TicketTransferFinished:
    case KeySessionEvent::TicketTransferFailed:
        return true;
    default:
        return false;
    }
}

}

KeyManageSessionEventRoute KeyManageEventUseCase::routeSessionEvent(
        KeySessionEvent::Kind kind,
        bool bridgeEventAccepted)
{
    if (!bridgeEventAccepted && staleSensitive(kind)) {
        return KeyManageSessionEventRoute::IgnoreStale;
    }

    switch (kind) {
    case KeySessionEvent::Notice:
        return KeyManageSessionEventRoute::Notice;
    case KeySessionEvent::Timeout:
        return KeyManageSessionEventRoute::Timeout;
    case KeySessionEvent::Ack:
        return KeyManageSessionEventRoute::Ack;
    case KeySessionEvent::Nak:
        return KeyManageSessionEventRoute::Nak;
    case KeySessionEvent::TasksUpdated:
        return KeyManageSessionEventRoute::TasksUpdated;
    case KeySessionEvent::TaskLogReady:
        return KeyManageSessionEventRoute::TaskLogReady;
    case KeySessionEvent::TaskLogFailed:
        return KeyManageSessionEventRoute::TaskLogFailed;
    case KeySessionEvent::CaptureRfidReady:
        return KeyManageSessionEventRoute::CaptureRfidReady;
    case KeySessionEvent::CaptureRfidFailed:
        return KeyManageSessionEventRoute::CaptureRfidFailed;
    case KeySessionEvent::KeyStateChanged:
        return KeyManageSessionEventRoute::KeyStateChanged;
    case KeySessionEvent::TicketTransferProgress:
        return KeyManageSessionEventRoute::TransferProgress;
    case KeySessionEvent::TicketTransferFinished:
        return KeyManageSessionEventRoute::TransferFinished;
    case KeySessionEvent::TicketTransferFailed:
        return KeyManageSessionEventRoute::TransferFailed;
    case KeySessionEvent::RawProtocol:
    case KeySessionEvent::AuxCommandFinished:
        return KeyManageSessionEventRoute::NoAction;
    }
    return KeyManageSessionEventRoute::NoAction;
}

KeyCommandEventPlan KeyManageEventUseCase::planTimeout(
        quint8 command,
        const KeyCommandEventContext &context)
{
    KeyCommandEventPlan plan;
    plan.releaseAutomaticQuery = true;
    const bool qTask = command == KeyProtocol::CmdQTask;
    const bool deleteTask = command == KeyProtocol::CmdDel;
    const bool setCom = command == KeyProtocol::CmdSetCom;
    const bool taskLog = command == KeyProtocol::CmdITaskLog
            || command == KeyProtocol::CmdUpTaskLog;

    plan.failTransferConfirmation =
            qTask && context.transferAwaitingConfirmation;
    plan.interruptDelete = deleteTask && context.deleteActive;
    plan.interruptCancelDelete = deleteTask && context.cancelDeleteActive;
    plan.failTransferHandshake = setCom && context.transferHandshakePending;
    plan.failReturnHandshake =
            (setCom || taskLog)
            && (context.returnActive || context.returnHandshakePending);
    return plan;
}

KeyCommandEventPlan KeyManageEventUseCase::planAck(
        quint8 ackedCmd,
        const KeyCommandEventContext &context)
{
    KeyCommandEventPlan plan;
    const bool ambiguousSetComOwner =
            ackedCmd == KeyProtocol::CmdSetCom
            && context.transferHandshakePending
            && context.returnHandshakePending;
    plan.rejectAmbiguousSetComAck = ambiguousSetComOwner;
    plan.acceptTransferHandshake =
            ackedCmd == KeyProtocol::CmdSetCom
            && context.transferHandshakePending
            && !ambiguousSetComOwner;
    plan.acceptReturnSetCom =
            ackedCmd == KeyProtocol::CmdSetCom
            && context.returnHandshakePending
            && !ambiguousSetComOwner;
    plan.acceptReturnTaskLog =
            ackedCmd == KeyProtocol::CmdITaskLog
            && context.returnActive;
    plan.reconcileCancelDelete =
            ackedCmd == KeyProtocol::CmdDel && context.cancelDeleteActive;
    return plan;
}

KeyCommandEventPlan KeyManageEventUseCase::planNak(
        quint8 originalCmd,
        const KeyCommandEventContext &context)
{
    KeyCommandEventPlan plan;
    plan.releaseAutomaticQuery = true;
    plan.failTransferConfirmation =
            originalCmd == KeyProtocol::CmdQTask
            && context.transferAwaitingConfirmation;
    plan.interruptDelete =
            originalCmd == KeyProtocol::CmdDel && context.deleteActive;
    plan.interruptCancelDelete =
            originalCmd == KeyProtocol::CmdDel && context.cancelDeleteActive;
    plan.failTransferHandshake =
            originalCmd == KeyProtocol::CmdSetCom
            && context.transferHandshakePending;
    plan.failReturnHandshake =
            (originalCmd == KeyProtocol::CmdSetCom
             || originalCmd == KeyProtocol::CmdITaskLog
             || originalCmd == KeyProtocol::CmdUpTaskLog)
            && (context.returnActive || context.returnHandshakePending);
    return plan;
}
