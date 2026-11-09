#include "features/key/application/KeyManageSelectionService.h"

KeyManageSelectionService::KeyManageSelectionService(QObject *parent)
    : QObject(parent)
{
}

SystemTicketDto KeyManageSelectionService::selectSystemTicket(
        int row,
        const QList<SystemTicketDto> &tickets)
{
    if (row < 0 || row >= tickets.size()) {
        m_selectedSystemTicketId.clear();
        return SystemTicketDto();
    }

    const SystemTicketDto ticket = tickets.at(row);
    m_selectedSystemTicketId = ticket.taskId;
    return ticket;
}

void KeyManageSelectionService::selectKeyTask(
        int row,
        const QList<KeyTaskDto> &tasks)
{
    if (row < 0 || row >= tasks.size()) {
        m_selectedKeyTaskRaw.clear();
        return;
    }
    m_selectedKeyTaskRaw = tasks.at(row).taskId;
}

QString KeyManageSelectionService::selectedSystemTicketId() const
{
    return m_selectedSystemTicketId;
}

QByteArray KeyManageSelectionService::selectedKeyTaskRaw() const
{
    return m_selectedKeyTaskRaw;
}

SystemTicketDto KeyManageSelectionService::selectedSystemTicket(
        const QList<SystemTicketDto> &tickets) const
{
    for (const SystemTicketDto &ticket : tickets) {
        if (ticket.taskId == m_selectedSystemTicketId) {
            return ticket;
        }
    }
    return SystemTicketDto();
}

SystemTicketDto KeyManageSelectionService::reconcileSystemTickets(
        const QList<SystemTicketDto> &tickets)
{
    const SystemTicketDto ticket = selectedSystemTicket(tickets);
    if (!ticket.valid) {
        m_selectedSystemTicketId.clear();
    }
    return ticket;
}
void KeyManageSelectionService::reconcileKeyTasks(
        const QList<KeyTaskDto> &tasks)
{
    if (m_selectedKeyTaskRaw.isEmpty()) {
        return;
    }
    for (const KeyTaskDto &task : tasks) {
        if (task.taskId == m_selectedKeyTaskRaw) {
            return;
        }
    }
    m_selectedKeyTaskRaw.clear();
}
