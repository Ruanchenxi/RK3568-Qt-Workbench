/**
 * @file TicketStore.h
 * @brief 系统票池（内存 + 磁盘持久化 + 原始 JSON 索引）
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

    // 持久化
    void loadFromDisk();

    bool ingestJson(const QByteArray &jsonBytes,
                    const QString &jsonPath,
                    QString *errorMsg = nullptr);
    bool ingestOrphanTicket(const QString &taskId,
                            const QString &residentSlotId = QStringLiteral("slot-1"));
    bool updateTransferState(const QString &taskId,
                             const QString &state,
                             const QString &lastError = QString());
    bool markKeyTaskDeleted(const QString &taskId);
    bool updateReturnState(const QString &taskId,
                           const QString &state,
                           const QString &returnError = QString());
    bool updateCancelState(const QString &taskId,
                           const QString &state);
    bool updateCancelTracking(const QString &taskId,
                              const QString &state,
                              const QString &cancelSource = QString(),
                              const QString &residentSlotId = QString(),
                              const QString &residentKeyId = QString(),
                              const QDateTime &cancelRequestedAt = QDateTime(),
                              bool wasInKey = false,
                              const QString &stateDetail = QString());
    bool removeTicket(const QString &taskId);

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
    void saveToDisk() const;
    static QString storePath();

    QList<SystemTicketDto> m_tickets;
    QHash<QString, QByteArray> m_rawJsonByTaskId;
};

#endif // TICKETSTORE_H
