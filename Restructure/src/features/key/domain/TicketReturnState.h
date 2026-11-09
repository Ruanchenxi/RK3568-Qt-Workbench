#ifndef TICKETRETURNSTATE_H
#define TICKETRETURNSTATE_H

#include <QString>

// Typed view of the existing in-memory return-state field. Storage remains
// string-compatible while business rules and UI stop interpreting raw labels.
enum class TicketReturnState
{
    Idle,
    Handshake,
    LogRequesting,
    LogReceiving,
    Uploading,
    Cleanup,
    DeleteFailed,
    ManualRequired,
    Unknown
};

// A completed attempt result is separate from the current workflow state.
// This prevents historical failures or partial progress from acting like an
// active return operation.
enum class TicketReturnOutcome
{
    None,
    RetryableFailure,
    WaitingForRemainingSteps,
    Unknown
};

inline TicketReturnState ticketReturnStateFromStorage(const QString &state)
{
    if (state.isEmpty() || state == QLatin1String("idle"))
        return TicketReturnState::Idle;
    if (state == QLatin1String("return-handshake"))
        return TicketReturnState::Handshake;
    if (state == QLatin1String("return-log-requesting"))
        return TicketReturnState::LogRequesting;
    if (state == QLatin1String("return-log-receiving"))
        return TicketReturnState::LogReceiving;
    if (state == QLatin1String("return-uploading"))
        return TicketReturnState::Uploading;
    if (state == QLatin1String("return-cleanup"))
        return TicketReturnState::Cleanup;
    if (state == QLatin1String("return-delete-failed"))
        return TicketReturnState::DeleteFailed;
    if (state == QLatin1String("manual-required"))
        return TicketReturnState::ManualRequired;
    return TicketReturnState::Unknown;
}

inline QString ticketReturnStateToStorage(TicketReturnState state)
{
    switch (state) {
    case TicketReturnState::Idle:
        return QStringLiteral("idle");
    case TicketReturnState::Handshake:
        return QStringLiteral("return-handshake");
    case TicketReturnState::LogRequesting:
        return QStringLiteral("return-log-requesting");
    case TicketReturnState::LogReceiving:
        return QStringLiteral("return-log-receiving");
    case TicketReturnState::Uploading:
        return QStringLiteral("return-uploading");
    case TicketReturnState::Cleanup:
        return QStringLiteral("return-cleanup");
    case TicketReturnState::DeleteFailed:
        return QStringLiteral("return-delete-failed");
    case TicketReturnState::ManualRequired:
        return QStringLiteral("manual-required");
    case TicketReturnState::Unknown:
        return QString();
    }
    return QString();
}
inline TicketReturnOutcome ticketReturnOutcomeFromStorage(const QString &outcome)
{
    if (outcome.isEmpty() || outcome == QLatin1String("none"))
        return TicketReturnOutcome::None;
    if (outcome == QLatin1String("retryable-failure"))
        return TicketReturnOutcome::RetryableFailure;
    if (outcome == QLatin1String("waiting-remaining-steps"))
        return TicketReturnOutcome::WaitingForRemainingSteps;
    return TicketReturnOutcome::Unknown;
}

inline QString ticketReturnOutcomeToStorage(TicketReturnOutcome outcome)
{
    switch (outcome) {
    case TicketReturnOutcome::None:
        return QStringLiteral("none");
    case TicketReturnOutcome::RetryableFailure:
        return QStringLiteral("retryable-failure");
    case TicketReturnOutcome::WaitingForRemainingSteps:
        return QStringLiteral("waiting-remaining-steps");
    case TicketReturnOutcome::Unknown:
        return QString();
    }
    return QString();
}

inline bool isTicketReturnInFlight(TicketReturnState state)
{
    return state == TicketReturnState::Handshake
            || state == TicketReturnState::LogRequesting
            || state == TicketReturnState::LogReceiving
            || state == TicketReturnState::Uploading;
}

inline bool isTicketReturnCleanup(TicketReturnState state)
{
    return state == TicketReturnState::Cleanup;
}

inline bool blocksAutomaticReturn(TicketReturnState state)
{
    return state == TicketReturnState::DeleteFailed
            || isTicketReturnInFlight(state)
            || isTicketReturnCleanup(state)
            || state == TicketReturnState::ManualRequired;
}

#endif // TICKETRETURNSTATE_H
