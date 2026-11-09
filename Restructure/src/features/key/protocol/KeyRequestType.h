#ifndef KEYREQUESTTYPE_H
#define KEYREQUESTTYPE_H

#include <QtGlobal>

/**
 * Identifies the single protocol request currently waiting for a response.
 * Values intentionally match the established INFLIGHT constants.
 */
enum class KeyRequestType : quint8 {
    None = 0,
    SetCom = 1,
    QTask = 2,
    DeleteTask = 3,
    Ticket = 4,
    TaskLog = 5,
    InitTransfer = 6,
    RfidTransfer = 7,
    QueryBattery = 8,
    SetTime = 9,
    CaptureRfid = 10
};

#endif // KEYREQUESTTYPE_H
