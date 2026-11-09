/**
 * @file TicketTransferRequest.h
 * @brief In-memory ticket transfer request passed to the key-session port.
 */
#ifndef TICKETTRANSFERREQUEST_H
#define TICKETTRANSFERREQUEST_H

#include <QByteArray>

struct TicketTransferRequest
{
    QByteArray jsonBytes;               // Workbench JSON payload; never persisted.
    QByteArray encodedPayload;          // Capture payload already encoded in memory.
    quint8 stationId = 0x01;            // Outer ticket-frame station address.
    int opId = -1;                      // Correlation id for structured logs.
    int debugFrameChunkSize = 0;        // Zero uses the production frame size.
};

#endif // TICKETTRANSFERREQUEST_H
