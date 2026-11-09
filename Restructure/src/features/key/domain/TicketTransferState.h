#ifndef TICKETTRANSFERSTATE_H
#define TICKETTRANSFERSTATE_H

#include <QString>

// Typed business meaning for the in-memory transfer projection. The codec is
// the only place that knows the temporary DTO strings.
enum class TicketTransferState
{
    Received,
    AutoPending,
    Sending,
    Success,
    KeyCleared,
    Failed,
    Unknown
};

inline TicketTransferState ticketTransferStateFromStorage(const QString &state)
{
    if (state == QLatin1String("received"))
        return TicketTransferState::Received;
    if (state == QLatin1String("auto-pending"))
        return TicketTransferState::AutoPending;
    if (state == QLatin1String("sending"))
        return TicketTransferState::Sending;
    if (state == QLatin1String("success"))
        return TicketTransferState::Success;
    if (state == QLatin1String("key-cleared"))
        return TicketTransferState::KeyCleared;
    if (state == QLatin1String("failed"))
        return TicketTransferState::Failed;
    return TicketTransferState::Unknown;
}

inline QString ticketTransferStateToStorage(TicketTransferState state)
{
    switch (state) {
    case TicketTransferState::Received:
        return QStringLiteral("received");
    case TicketTransferState::AutoPending:
        return QStringLiteral("auto-pending");
    case TicketTransferState::Sending:
        return QStringLiteral("sending");
    case TicketTransferState::Success:
        return QStringLiteral("success");
    case TicketTransferState::KeyCleared:
        return QStringLiteral("key-cleared");
    case TicketTransferState::Failed:
        return QStringLiteral("failed");
    case TicketTransferState::Unknown:
        return QString();
    }
    return QString();
}

#endif // TICKETTRANSFERSTATE_H
