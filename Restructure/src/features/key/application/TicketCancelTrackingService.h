#ifndef TICKETCANCELTRACKINGSERVICE_H
#define TICKETCANCELTRACKINGSERVICE_H

#include <QObject>
#include <QString>

#include "features/key/domain/TicketCancelState.h"
#include "shared/contracts/SystemTicketDto.h"

class TicketCancelCoordinator;
class TicketStore;

// Keeps the process-memory ticket view and cancel coordinator record aligned.
// It does not decide whether cancellation is allowed or execute serial commands.
class TicketCancelTrackingService : public QObject
{
public:
    explicit TicketCancelTrackingService(
            TicketStore *store,
            TicketCancelCoordinator *coordinator,
            QObject *parent = nullptr);

    /// Records an accepted or pending cancel intent in both runtime views.
    void recordAccepted(const SystemTicketDto &ticket,
                        TicketCancelState state,
                        bool wasInKey,
                        const QString &trackingDetail,
                        const QString &recordMessage = QString());

    /// Records that the verified DEL operation has started.
    void recordExecuting(const SystemTicketDto &ticket,
                         const QString &message);

    /// Marks the coordinator attempt failed and clears the active store state.
    void recordFailureAndClear(const SystemTicketDto &ticket,
                               const QString &recordMessage,
                               const QString &trackingDetail);

private:
    SystemTicketDto normalizedTicket(const SystemTicketDto &ticket,
                                     bool wasInKey) const;

    TicketStore *m_store;
    TicketCancelCoordinator *m_coordinator;
};

#endif // TICKETCANCELTRACKINGSERVICE_H
