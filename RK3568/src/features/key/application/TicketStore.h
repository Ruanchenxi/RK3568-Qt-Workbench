/**
 * @file TicketStore.h
 * @brief 系统票池（内存 + 原始 JSON 索引）
 */
#ifndef TICKETSTORE_H
#define TICKETSTORE_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QHash>

#include "shared/contracts/SystemTicketDto.h"

class TicketStore : public QObject
{
    Q_OBJECT
public:
    explicit TicketStore(QObject *parent = nullptr);

    bool ingestJson(const QByteArray &jsonBytes,
                    const QString &jsonPath,
                    QString *errorMsg = nullptr);
    bool updateTransferState(const QString &taskId,
                             const QString &state,
                             const QString &lastError = QString());

    QList<SystemTicketDto> tickets() const;
    SystemTicketDto ticketById(const QString &taskId) const;
    QByteArray rawJsonById(const QString &taskId) const;

signals:
    void ticketsChanged(const QList<SystemTicketDto> &tickets);

private:
    static bool buildDto(const QByteArray &jsonBytes,
                         const QString &jsonPath,
                         SystemTicketDto &dto,
                         QString *errorMsg);

    QList<SystemTicketDto> m_tickets;
    QHash<QString, QByteArray> m_rawJsonByTaskId;
};

#endif // TICKETSTORE_H
