#include "features/key/application/TicketCancelCoordinator.h"

#include <algorithm>
#include <QDateTime>

TicketCancelCoordinator::TicketCancelCoordinator(QObject *parent)
    : QObject(parent)
{
}

void TicketCancelCoordinator::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
}

TicketCancelCoordinator::CancelRecord TicketCancelCoordinator::recordFor(const QString &taskId) const
{
    return m_records.value(taskId.trimmed());
}

bool TicketCancelCoordinator::hasActiveRecord(const QString &taskId) const
{
    const CancelRecord record = recordFor(taskId);
    return record.valid && isTicketCancelActive(record.cancelState);
}

bool TicketCancelCoordinator::hasPendingWork() const
{
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        if (isTicketCancelActive(it.value().cancelState)) {
            return true;
        }
    }
    return false;
}

QStringList TicketCancelCoordinator::activeTaskIds() const
{
    QStringList taskIds;
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        if (isTicketCancelActive(it.value().cancelState)) {
            taskIds.append(it.key());
        }
    }
    std::sort(taskIds.begin(), taskIds.end(), [this](const QString &lhs, const QString &rhs) {
        const CancelRecord left = m_records.value(lhs);
        const CancelRecord right = m_records.value(rhs);
        const bool leftValid = left.cancelRequestedAt.isValid();
        const bool rightValid = right.cancelRequestedAt.isValid();
        if (leftValid && rightValid) {
            return left.cancelRequestedAt < right.cancelRequestedAt;
        }
        if (leftValid != rightValid) {
            return leftValid;
        }
        return lhs < rhs;
    });
    return taskIds;
}

void TicketCancelCoordinator::recordAccepted(const QString &taskId,
                                             const SystemTicketDto &ticket,
                                             TicketCancelState state,
                                             const QString &message)
{
    upsertRecord(buildRecord(taskId, ticket, state, message));
}

void TicketCancelCoordinator::recordExecuting(const QString &taskId,
                                              const SystemTicketDto &ticket,
                                              const QString &message)
{
    upsertRecord(buildRecord(taskId, ticket, TicketCancelState::Executing, message));
}

void TicketCancelCoordinator::recordDone(const QString &taskId,
                                         const SystemTicketDto &ticket,
                                         const QString &message)
{
    upsertRecord(buildRecord(taskId, ticket, TicketCancelState::Done, message));
}

void TicketCancelCoordinator::recordDoneWithoutTicket(const QString &taskId,
                                                      const QString &message,
                                                      const QString &residentSlotId)
{
    SystemTicketDto ticket;
    ticket.taskId = taskId;
    ticket.residentSlotId = residentSlotId;
    ticket.cancelSource = QStringLiteral("workbench-http");
    ticket.cancelRequestedAt = QDateTime::currentDateTime();
    ticket.valid = !taskId.trimmed().isEmpty();
    upsertRecord(buildRecord(taskId, ticket, TicketCancelState::Done, message));
}

void TicketCancelCoordinator::recordFailed(const QString &taskId,
                                           const SystemTicketDto &ticket,
                                           const QString &message)
{
    upsertRecord(buildRecord(taskId, ticket, TicketCancelState::Failed, message));
}


void TicketCancelCoordinator::upsertRecord(const CancelRecord &record)
{
    if (!record.valid) {
        return;
    }
    m_records.insert(record.taskId, record);
}

TicketCancelCoordinator::CancelRecord TicketCancelCoordinator::buildRecord(const QString &taskId,
                                                                           const SystemTicketDto &ticket,
                                                                           TicketCancelState state,
                                                                           const QString &message) const
{
    const CancelRecord existing = m_records.value(taskId.trimmed());

    CancelRecord record;
    record.taskId = taskId.trimmed();
    record.cancelState = state;
    record.residentSlotId = !ticket.residentSlotId.trimmed().isEmpty()
            ? ticket.residentSlotId.trimmed()
            : (!existing.residentSlotId.trimmed().isEmpty()
               ? existing.residentSlotId.trimmed()
               : QStringLiteral("slot-1"));
    record.residentKeyId = !ticket.residentKeyId.trimmed().isEmpty()
            ? ticket.residentKeyId.trimmed()
            : existing.residentKeyId.trimmed();
    record.cancelRequestedAt = ticket.cancelRequestedAt.isValid()
            ? ticket.cancelRequestedAt
            : (existing.cancelRequestedAt.isValid()
               ? existing.cancelRequestedAt
               : QDateTime::currentDateTime());
    record.cancelSource = !ticket.cancelSource.trimmed().isEmpty()
            ? ticket.cancelSource.trimmed()
            : (!existing.cancelSource.trimmed().isEmpty()
               ? existing.cancelSource.trimmed()
               : QStringLiteral("workbench-http"));
    record.wasInKey = ticket.wasInKey || existing.wasInKey;
    record.lastKnownTransferState = !ticket.transferState.trimmed().isEmpty()
            ? ticket.transferState.trimmed()
            : existing.lastKnownTransferState.trimmed();
    record.lastKnownReturnState = !ticket.returnState.trimmed().isEmpty()
            ? ticket.returnState.trimmed()
            : existing.lastKnownReturnState.trimmed();
    record.lastMessage = message.trimmed().isEmpty()
            ? existing.lastMessage.trimmed()
            : message.trimmed();
    record.valid = !record.taskId.isEmpty();
    return record;
}
