#include "features/key/application/TicketStore.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {
QString buildTicketNo(const QString &taskName)
{
    const QStringList parts = taskName.split('-', Qt::KeepEmptyParts);
    if (parts.size() >= 5) {
        return parts.mid(0, 5).join('-');
    }
    return taskName;
}
}

TicketStore::TicketStore(QObject *parent)
    : QObject(parent)
{
}

bool TicketStore::buildDto(const QByteArray &jsonBytes,
                           const QString &jsonPath,
                           SystemTicketDto &dto,
                           QString *errorMsg)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &pe);
    if (doc.isNull() || !doc.isObject()) {
        if (errorMsg)
            *errorMsg = QString::fromUtf8("JSON 解析失败: %1").arg(pe.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonObject ticketObj = (root.contains("data") && root.value("data").isObject())
            ? root.value("data").toObject()
            : root;

    dto.taskId = ticketObj.value("taskId").toString();
    dto.taskName = ticketObj.value("taskName").toString();
    dto.taskType = ticketObj.value("taskType").toInt(0);
    dto.stepNum = ticketObj.value("stepNum").toInt(0);
    dto.createTime = ticketObj.value("createTime").toString();
    dto.planTime = ticketObj.value("planTime").toString();
    dto.ticketNo = buildTicketNo(dto.taskName);
    dto.source = QStringLiteral("workbench-http");
    dto.transferState = QStringLiteral("received");
    dto.lastError.clear();
    dto.returnState = QStringLiteral("idle");
    dto.returnError.clear();
    dto.jsonPath = jsonPath;
    dto.receivedAt = QDateTime::currentDateTime();
    dto.valid = !dto.taskId.trimmed().isEmpty();

    if (!dto.valid) {
        if (errorMsg)
            *errorMsg = QStringLiteral("taskId 为空，无法入系统票池");
        return false;
    }

    return true;
}

bool TicketStore::ingestJson(const QByteArray &jsonBytes,
                             const QString &jsonPath,
                             QString *errorMsg)
{
    SystemTicketDto dto;
    if (!buildDto(jsonBytes, jsonPath, dto, errorMsg))
        return false;

    bool replaced = false;
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == dto.taskId) {
            // 保留已有的发送状态，避免同一 taskId 被新的 HTTP JSON 覆盖成 received。
            dto.transferState = m_tickets[i].transferState;
            dto.lastError = m_tickets[i].lastError;
            dto.returnState = m_tickets[i].returnState;
            dto.returnError = m_tickets[i].returnError;
            m_tickets[i] = dto;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        m_tickets.prepend(dto);

    m_rawJsonByTaskId.insert(dto.taskId, jsonBytes);
    emit ticketsChanged(m_tickets);
    return true;
}

bool TicketStore::updateTransferState(const QString &taskId,
                                      const QString &state,
                                      const QString &lastError)
{
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            m_tickets[i].transferState = state;
            m_tickets[i].lastError = lastError;
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}

bool TicketStore::markKeyTaskDeleted(const QString &taskId)
{
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            m_tickets[i].transferState = QStringLiteral("key-cleared");
            m_tickets[i].lastError.clear();
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}

bool TicketStore::updateReturnState(const QString &taskId,
                                    const QString &state,
                                    const QString &returnError)
{
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            m_tickets[i].returnState = state;
            m_tickets[i].returnError = returnError;
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}

QList<SystemTicketDto> TicketStore::tickets() const
{
    return m_tickets;
}

SystemTicketDto TicketStore::ticketById(const QString &taskId) const
{
    for (const SystemTicketDto &dto : m_tickets) {
        if (dto.taskId == taskId)
            return dto;
    }
    return SystemTicketDto{};
}

QByteArray TicketStore::rawJsonById(const QString &taskId) const
{
    return m_rawJsonByTaskId.value(taskId);
}
