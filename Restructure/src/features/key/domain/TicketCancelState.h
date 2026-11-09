#ifndef TICKETCANCELSTATE_H
#define TICKETCANCELSTATE_H

#include <QString>

// Cancellation has its own small vocabulary. It is intentionally independent
// from transfer and return so their combinations cannot form a global state.
enum class TicketCancelState
{
    None,
    Accepted,
    Pending,
    Executing,
    Done,
    Failed,
    Unknown
};

inline TicketCancelState ticketCancelStateFromStorage(const QString &state)
{
    if (state.isEmpty() || state == QLatin1String("none"))
        return TicketCancelState::None;
    if (state == QLatin1String("cancel-accepted"))
        return TicketCancelState::Accepted;
    if (state == QLatin1String("cancel-pending"))
        return TicketCancelState::Pending;
    if (state == QLatin1String("cancel-executing"))
        return TicketCancelState::Executing;
    if (state == QLatin1String("cancel-done"))
        return TicketCancelState::Done;
    if (state == QLatin1String("cancel-failed"))
        return TicketCancelState::Failed;
    return TicketCancelState::Unknown;
}

inline QString ticketCancelStateToStorage(TicketCancelState state)
{
    switch (state) {
    case TicketCancelState::None:
        return QStringLiteral("none");
    case TicketCancelState::Accepted:
        return QStringLiteral("cancel-accepted");
    case TicketCancelState::Pending:
        return QStringLiteral("cancel-pending");
    case TicketCancelState::Executing:
        return QStringLiteral("cancel-executing");
    case TicketCancelState::Done:
        return QStringLiteral("cancel-done");
    case TicketCancelState::Failed:
        return QStringLiteral("cancel-failed");
    case TicketCancelState::Unknown:
        return QString();
    }
    return QString();
}

inline bool isTicketCancelActive(TicketCancelState state)
{
    return state == TicketCancelState::Accepted
            || state == TicketCancelState::Pending
            || state == TicketCancelState::Executing;
}

inline bool hasTicketCancelIntent(TicketCancelState state)
{
    return state != TicketCancelState::None
            && state != TicketCancelState::Unknown;
}

#endif // TICKETCANCELSTATE_H
