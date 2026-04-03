#include "features/key/application/TicketCancelCoordinator.h"

#include <algorithm>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QJsonObject toJsonObject(const TicketCancelCoordinator::CancelRecord &record)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("taskId"), record.taskId);
    obj.insert(QStringLiteral("cancelState"), record.cancelState);
    obj.insert(QStringLiteral("residentSlotId"), record.residentSlotId);
    obj.insert(QStringLiteral("residentKeyId"), record.residentKeyId);
    obj.insert(QStringLiteral("cancelRequestedAt"), record.cancelRequestedAt.isValid()
            ? record.cancelRequestedAt.toString(Qt::ISODateWithMs)
            : QString());
    obj.insert(QStringLiteral("cancelSource"), record.cancelSource);
    obj.insert(QStringLiteral("wasInKey"), record.wasInKey);
    obj.insert(QStringLiteral("lastKnownTransferState"), record.lastKnownTransferState);
    obj.insert(QStringLiteral("lastKnownReturnState"), record.lastKnownReturnState);
    obj.insert(QStringLiteral("lastMessage"), record.lastMessage);
    return obj;
}

TicketCancelCoordinator::CancelRecord fromJsonObject(const QJsonObject &obj)
{
    TicketCancelCoordinator::CancelRecord record;
    record.taskId = obj.value(QStringLiteral("taskId")).toString().trimmed();
    record.cancelState = obj.value(QStringLiteral("cancelState")).toString().trimmed();
    record.residentSlotId = obj.value(QStringLiteral("residentSlotId")).toString().trimmed();
    record.residentKeyId = obj.value(QStringLiteral("residentKeyId")).toString().trimmed();
    record.cancelRequestedAt = QDateTime::fromString(
            obj.value(QStringLiteral("cancelRequestedAt")).toString().trimmed(),
            Qt::ISODateWithMs);
    record.cancelSource = obj.value(QStringLiteral("cancelSource")).toString().trimmed();
    record.wasInKey = obj.value(QStringLiteral("wasInKey")).toBool(false);
    record.lastKnownTransferState = obj.value(QStringLiteral("lastKnownTransferState")).toString().trimmed();
    record.lastKnownReturnState = obj.value(QStringLiteral("lastKnownReturnState")).toString().trimmed();
    record.lastMessage = obj.value(QStringLiteral("lastMessage")).toString().trimmed();
    record.valid = !record.taskId.isEmpty();
    return record;
}

} // namespace

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
    loadRecords();
}

TicketCancelCoordinator::CancelRecord TicketCancelCoordinator::recordFor(const QString &taskId) const
{
    return m_records.value(taskId.trimmed());
}

bool TicketCancelCoordinator::hasActiveRecord(const QString &taskId) const
{
    const CancelRecord record = recordFor(taskId);
    return record.valid && isActiveState(record.cancelState);
}

bool TicketCancelCoordinator::hasPendingWork() const
{
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        if (isActiveState(it.value().cancelState)) {
            return true;
        }
    }
    return false;
}

QStringList TicketCancelCoordinator::activeTaskIds() const
{
    QStringList taskIds;
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        if (isActiveState(it.value().cancelState)) {
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
                                             const QString &state,
                                             const QString &message)
{
    upsertRecord(buildRecord(taskId, ticket, state, message));
}

void TicketCancelCoordinator::recordExecuting(const QString &taskId,
                                              const SystemTicketDto &ticket,
                                              const QString &message)
{
    upsertRecord(buildRecord(taskId, ticket, QStringLiteral("cancel-executing"), message));
}

void TicketCancelCoordinator::recordDone(const QString &taskId,
                                         const SystemTicketDto &ticket,
                                         const QString &message)
{
    upsertRecord(buildRecord(taskId, ticket, QStringLiteral("cancel-done"), message));
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
    upsertRecord(buildRecord(taskId, ticket, QStringLiteral("cancel-done"), message));
}

void TicketCancelCoordinator::recordFailed(const QString &taskId,
                                           const SystemTicketDto &ticket,
                                           const QString &message)
{
    upsertRecord(buildRecord(taskId, ticket, QStringLiteral("cancel-failed"), message));
}

bool TicketCancelCoordinator::isActiveState(const QString &state)
{
    return state == QLatin1String("cancel-accepted")
            || state == QLatin1String("cancel-pending")
            || state == QLatin1String("cancel-executing");
}

QString TicketCancelCoordinator::journalPath() const
{
    const QString logsDir = QCoreApplication::applicationDirPath() + QStringLiteral("/logs");
    QDir().mkpath(logsDir);
    return logsDir + QStringLiteral("/cancel_ticket_journal.json");
}

void TicketCancelCoordinator::loadRecords()
{
    m_records.clear();

    QFile file(journalPath());
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isArray()) {
        return;
    }

    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        const CancelRecord record = fromJsonObject(value.toObject());
        if (record.valid) {
            m_records.insert(record.taskId, record);
        }
    }
}

void TicketCancelCoordinator::saveRecords() const
{
    QJsonArray array;
    for (const CancelRecord &record : m_records) {
        if (!record.valid) {
            continue;
        }
        array.append(toJsonObject(record));
    }

    QSaveFile file(journalPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    file.commit();
}

void TicketCancelCoordinator::upsertRecord(const CancelRecord &record)
{
    if (!record.valid) {
        return;
    }
    m_records.insert(record.taskId, record);
    saveRecords();
}

TicketCancelCoordinator::CancelRecord TicketCancelCoordinator::buildRecord(const QString &taskId,
                                                                           const SystemTicketDto &ticket,
                                                                           const QString &state,
                                                                           const QString &message) const
{
    const CancelRecord existing = m_records.value(taskId.trimmed());

    CancelRecord record;
    record.taskId = taskId.trimmed();
    record.cancelState = state.trimmed();
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
