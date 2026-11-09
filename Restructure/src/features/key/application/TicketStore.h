/**
 * @file TicketStore.h
 * @brief 当前进程内的票据模型与原始 JSON 索引
 */
#ifndef TICKETSTORE_H
#define TICKETSTORE_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QHash>

#include "shared/contracts/SystemTicketDto.h"
#include "features/key/domain/TicketReturnState.h"
#include "features/key/domain/TicketTransferState.h"
#include "features/key/domain/TicketCancelState.h"

class TicketStore : public QObject
{
    Q_OBJECT
public:
    explicit TicketStore(QObject *parent = nullptr);

    bool ingestJson(const QByteArray &jsonBytes,
                    QString *errorMsg = nullptr);
    bool updateTransferState(const QString &taskId,
                             TicketTransferState state,
                             const QString &lastError = QString());
    bool updateTransferTriggerSource(const QString &taskId,
                                     const QString &source);
    bool markKeyTaskDeleted(const QString &taskId);
    // Stores only named return workflow meanings; raw labels are confined to
    // the domain codec for temporary DTO compatibility.
    bool updateReturnState(const QString &taskId,
                           TicketReturnState state,
                           const QString &returnError = QString(),
                           TicketReturnOutcome outcome = TicketReturnOutcome::None);
    bool updateCancelState(const QString &taskId,
                           TicketCancelState state);
    bool updateCancelTracking(const QString &taskId,
                              TicketCancelState state,
                              const QString &cancelSource = QString(),
                              const QString &residentSlotId = QString(),
                              const QString &residentKeyId = QString(),
                              const QDateTime &cancelRequestedAt = QDateTime(),
                              bool wasInKey = false,
                              const QString &stateDetail = QString());
    bool updateAdminDeleteStage(const QString &taskId,
                                SystemTicketDto::AdminDeleteStage stage);
    bool updateAdminDeleteError(const QString &taskId, const QString &errorText);
    bool refreshKeyConfirmedAt(const QList<QString> &taskIds, qint64 ms);
    bool removeTicket(const QString &taskId);

    QList<SystemTicketDto> tickets() const;
    SystemTicketDto ticketById(const QString &taskId) const;
    QByteArray rawJsonById(const QString &taskId) const;

signals:
    void ticketsChanged(const QList<SystemTicketDto> &tickets);

private:
    static bool buildDto(const QByteArray &jsonBytes,
                         SystemTicketDto &dto,
                         QString *errorMsg);

    QList<SystemTicketDto> m_tickets;
    QHash<QString, QByteArray> m_rawJsonByTaskId;
};

#endif // TICKETSTORE_H
