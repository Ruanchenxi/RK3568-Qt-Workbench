/**
 * @file TicketCancelCoordinator.h
 * @brief 工作台撤销票协调器（撤销意图、幂等、最小恢复记录）
 */
#ifndef TICKETCANCELCOORDINATOR_H
#define TICKETCANCELCOORDINATOR_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QStringList>

#include "shared/contracts/SystemTicketDto.h"

class TicketCancelCoordinator : public QObject
{
    Q_OBJECT
public:
    struct CancelRecord
    {
        QString taskId;
        QString cancelState;
        QString residentSlotId;
        QString residentKeyId;
        QDateTime cancelRequestedAt;
        QString cancelSource;
        bool wasInKey = false;
        QString lastKnownTransferState;
        QString lastKnownReturnState;
        QString lastMessage;
        bool valid = false;
    };

    explicit TicketCancelCoordinator(QObject *parent = nullptr);

    void start();

    CancelRecord recordFor(const QString &taskId) const;
    bool hasActiveRecord(const QString &taskId) const;
    bool hasPendingWork() const;
    QStringList activeTaskIds() const;

    void recordAccepted(const QString &taskId,
                        const SystemTicketDto &ticket,
                        const QString &state,
                        const QString &message);
    void recordExecuting(const QString &taskId,
                         const SystemTicketDto &ticket,
                         const QString &message);
    void recordDone(const QString &taskId,
                    const SystemTicketDto &ticket,
                    const QString &message);
    void recordDoneWithoutTicket(const QString &taskId,
                                 const QString &message,
                                 const QString &residentSlotId = QStringLiteral("slot-1"));
    void recordFailed(const QString &taskId,
                      const SystemTicketDto &ticket,
                      const QString &message);

private:
    static bool isActiveState(const QString &state);
    QString journalPath() const;
    void loadRecords();
    void saveRecords() const;
    void upsertRecord(const CancelRecord &record);
    CancelRecord buildRecord(const QString &taskId,
                             const SystemTicketDto &ticket,
                             const QString &state,
                             const QString &message) const;

    bool m_started = false;
    QHash<QString, CancelRecord> m_records;
};

#endif // TICKETCANCELCOORDINATOR_H
