#include "features/key/application/TicketCancelTrackingService.h"

#include <QDateTime>

#include "features/key/application/TicketCancelCoordinator.h"
#include "features/key/application/TicketStore.h"

TicketCancelTrackingService::TicketCancelTrackingService(
        TicketStore *store,
        TicketCancelCoordinator *coordinator,
        QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_coordinator(coordinator)
{
}

void TicketCancelTrackingService::recordAccepted(
        const SystemTicketDto &ticket,
        TicketCancelState state,
        bool wasInKey,
        const QString &trackingDetail,
        const QString &recordMessage)
{
    if (!m_store || !m_coordinator || !ticket.valid
            || ticket.taskId.trimmed().isEmpty()) {
        return;
    }

    const SystemTicketDto normalized = normalizedTicket(ticket, wasInKey);
    m_store->updateCancelTracking(
                normalized.taskId,
                state,
                normalized.cancelSource,
                normalized.residentSlotId,
                normalized.residentKeyId,
                normalized.cancelRequestedAt,
                normalized.wasInKey,
                trackingDetail);

    const SystemTicketDto stored = m_store->ticketById(normalized.taskId);
    m_coordinator->recordAccepted(
                normalized.taskId,
                stored.valid ? stored : normalized,
                state,
                recordMessage.isEmpty() ? trackingDetail : recordMessage);
}

void TicketCancelTrackingService::recordExecuting(
        const SystemTicketDto &ticket,
        const QString &message)
{
    if (!m_store || !m_coordinator || !ticket.valid
            || ticket.taskId.trimmed().isEmpty()) {
        return;
    }

    const SystemTicketDto normalized = normalizedTicket(ticket, true);
    m_store->updateCancelTracking(
                normalized.taskId,
                TicketCancelState::Executing,
                normalized.cancelSource,
                normalized.residentSlotId,
                normalized.residentKeyId,
                normalized.cancelRequestedAt,
                true,
                message);

    const SystemTicketDto stored = m_store->ticketById(normalized.taskId);
    m_coordinator->recordExecuting(
                normalized.taskId,
                stored.valid ? stored : normalized,
                message);
}

void TicketCancelTrackingService::recordFailureAndClear(
        const SystemTicketDto &ticket,
        const QString &recordMessage,
        const QString &trackingDetail)
{
    if (!m_store || !m_coordinator || !ticket.valid
            || ticket.taskId.trimmed().isEmpty()) {
        return;
    }

    m_coordinator->recordFailed(ticket.taskId, ticket, recordMessage);
    m_store->updateCancelTracking(
                ticket.taskId,
                TicketCancelState::None,
                QString(),
                QString(),
                QString(),
                QDateTime(),
                false,
                trackingDetail);
}

SystemTicketDto TicketCancelTrackingService::normalizedTicket(
        const SystemTicketDto &ticket,
        bool wasInKey) const
{
    SystemTicketDto normalized = ticket;
    normalized.taskId = normalized.taskId.trimmed();
    if (normalized.residentSlotId.trimmed().isEmpty()) {
        normalized.residentSlotId = QStringLiteral("slot-1");
    }
    if (normalized.cancelSource.trimmed().isEmpty()) {
        normalized.cancelSource = QStringLiteral("workbench-http");
    }
    if (!normalized.cancelRequestedAt.isValid()) {
        normalized.cancelRequestedAt = QDateTime::currentDateTime();
    }
    normalized.wasInKey = normalized.wasInKey || wasInKey;
    return normalized;
}
