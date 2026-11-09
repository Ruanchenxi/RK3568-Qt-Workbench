#ifndef KEYMANAGESELECTIONSERVICE_H
#define KEYMANAGESELECTIONSERVICE_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>

#include "shared/contracts/KeyTaskDto.h"
#include "shared/contracts/SystemTicketDto.h"

// Owns only the two selections shown by the key-management page. Selection is
// presentation state and must not become ticket lifecycle or device proof.
class KeyManageSelectionService : public QObject
{
public:
    explicit KeyManageSelectionService(QObject *parent = nullptr);

    SystemTicketDto selectSystemTicket(
            int row,
            const QList<SystemTicketDto> &tickets);
    void selectKeyTask(int row, const QList<KeyTaskDto> &tasks);

    QString selectedSystemTicketId() const;
    QByteArray selectedKeyTaskRaw() const;
    SystemTicketDto selectedSystemTicket(
            const QList<SystemTicketDto> &tickets) const;
    SystemTicketDto reconcileSystemTickets(
            const QList<SystemTicketDto> &tickets);

    // Clears a key selection as soon as a fresh Q_TASK list no longer has it.
    void reconcileKeyTasks(const QList<KeyTaskDto> &tasks);

private:
    QString m_selectedSystemTicketId;
    QByteArray m_selectedKeyTaskRaw;
};

#endif // KEYMANAGESELECTIONSERVICE_H
