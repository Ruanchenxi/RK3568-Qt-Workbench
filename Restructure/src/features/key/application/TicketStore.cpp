#include "features/key/application/TicketStore.h"

#include <QDateTime>
#include <QDebug>
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
    dto.transferTriggerSource = QStringLiteral("manual");
    dto.transferState = QStringLiteral("received");
    dto.lastError.clear();
    dto.returnState = QStringLiteral("idle");
    dto.returnOutcome = QStringLiteral("none");
    dto.returnError.clear();
    dto.cancelState = QStringLiteral("none");
    dto.residentSlotId = QStringLiteral("slot-1");
    dto.residentKeyId.clear();
    dto.cancelRequestedAt = QDateTime();
    dto.cancelSource.clear();
    dto.wasInKey = false;
    dto.stateDetail.clear();
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
                             QString *errorMsg)
{
    SystemTicketDto dto;
    if (!buildDto(jsonBytes, dto, errorMsg))
        return false;

    bool replaced = false;
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == dto.taskId) {
            // 已受理撤销的票不允许被覆盖
            if (hasTicketCancelIntent(ticketCancelStateFromStorage(m_tickets[i].cancelState))) {
                if (errorMsg)
                    *errorMsg = QStringLiteral("该任务已受理撤销，无法重新提交");
                return false;
            }
            // 保留已有的发送状态，避免同一 taskId 被新的 HTTP JSON 覆盖成 received。
            dto.transferState = m_tickets[i].transferState;
            dto.transferTriggerSource = m_tickets[i].transferTriggerSource;
            dto.lastError = m_tickets[i].lastError;
            dto.returnState = m_tickets[i].returnState;
            dto.returnOutcome = m_tickets[i].returnOutcome;
            dto.returnError = m_tickets[i].returnError;
            dto.cancelState = m_tickets[i].cancelState;
            dto.residentSlotId = m_tickets[i].residentSlotId;
            dto.residentKeyId = m_tickets[i].residentKeyId;
            dto.cancelRequestedAt = m_tickets[i].cancelRequestedAt;
            dto.cancelSource = m_tickets[i].cancelSource;
            dto.wasInKey = m_tickets[i].wasInKey;
            dto.stateDetail = m_tickets[i].stateDetail;
            dto.adminDeleteStage = m_tickets[i].adminDeleteStage;
            dto.adminDeleteError = m_tickets[i].adminDeleteError;
            dto.keyConfirmedAtMs = m_tickets[i].keyConfirmedAtMs;
            m_tickets[i] = dto;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        m_tickets.prepend(dto);

    m_rawJsonByTaskId.insert(dto.taskId, jsonBytes);
    qInfo().noquote()
            << QStringLiteral("[TicketStore] ingestJson taskId=%1 replaced=%2 transferState=%3 returnState=%4 count=%5 storage=memory")
                  .arg(dto.taskId,
                       replaced ? QStringLiteral("true") : QStringLiteral("false"),
                       dto.transferState,
                       dto.returnState,
                       QString::number(m_tickets.size()));
    emit ticketsChanged(m_tickets);
    return true;
}

bool TicketStore::updateTransferState(const QString &taskId,
                                      TicketTransferState state,
                                      const QString &lastError)
{
    if (state == TicketTransferState::Unknown) {
        qWarning().noquote()
                << QStringLiteral("[TicketStore] reject unknown transfer state taskId=%1")
                      .arg(taskId);
        return false;
    }
    const QString storageState = ticketTransferStateToStorage(state);
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            const QString oldState = m_tickets[i].transferState;
            const QString oldError = m_tickets[i].lastError;
            if (oldState == storageState && oldError == lastError) {
                return true;
            }
            m_tickets[i].transferState = storageState;
            m_tickets[i].lastError = lastError;
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] updateTransferState taskId=%1 %2 -> %3 error=%4")
                          .arg(taskId,
                               oldState,
                               storageState,
                               lastError.isEmpty() ? QStringLiteral("<none>") : lastError);
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}
bool TicketStore::updateTransferTriggerSource(const QString &taskId, const QString &source)
{
    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            if (m_tickets[i].transferTriggerSource == normalized) {
                return true;
            }
            m_tickets[i].transferTriggerSource = normalized;
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] updateTransferTriggerSource taskId=%1 source=%2")
                          .arg(taskId, normalized);
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
            m_tickets[i].transferState = ticketTransferStateToStorage(TicketTransferState::KeyCleared);
            m_tickets[i].lastError.clear();
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}

bool TicketStore::updateReturnState(const QString &taskId,
                                    TicketReturnState state,
                                    const QString &returnError,
                                    TicketReturnOutcome outcome)
{
    if (state == TicketReturnState::Unknown
            || outcome == TicketReturnOutcome::Unknown) {
        qWarning().noquote()
                << QStringLiteral("[TicketStore] reject unknown return meaning taskId=%1")
                      .arg(taskId);
        return false;
    }
    const QString storageState = ticketReturnStateToStorage(state);
    const QString storageOutcome = ticketReturnOutcomeToStorage(outcome);
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            const QString oldState = m_tickets[i].returnState;
            const QString oldOutcome = m_tickets[i].returnOutcome;
            const QString oldError = m_tickets[i].returnError;
            if (oldState == storageState
                    && oldOutcome == storageOutcome
                    && oldError == returnError) {
                return true;
            }
            m_tickets[i].returnState = storageState;
            m_tickets[i].returnOutcome = storageOutcome;
            m_tickets[i].returnError = returnError;
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] updateReturnState taskId=%1 %2 -> %3 outcome=%4 error=%5")
                          .arg(taskId,
                               oldState,
                               storageState,
                               storageOutcome,
                               returnError.isEmpty() ? QStringLiteral("<none>") : returnError);
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

bool TicketStore::updateCancelState(const QString &taskId, TicketCancelState state)
{
    return updateCancelTracking(taskId, state);
}

bool TicketStore::updateCancelTracking(const QString &taskId,
                                       TicketCancelState state,
                                       const QString &cancelSource,
                                       const QString &residentSlotId,
                                       const QString &residentKeyId,
                                       const QDateTime &cancelRequestedAt,
                                       bool wasInKey,
                                       const QString &stateDetail)
{
    if (state == TicketCancelState::Unknown) {
        qWarning().noquote()
                << QStringLiteral("[TicketStore] reject unknown cancel state taskId=%1")
                      .arg(taskId);
        return false;
    }
    const QString storageState = ticketCancelStateToStorage(state);
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            const QString oldState = m_tickets[i].cancelState;
            const QString oldSlotId = m_tickets[i].residentSlotId;
            const QString oldKeyId = m_tickets[i].residentKeyId;
            const QString oldSource = m_tickets[i].cancelSource;
            const QDateTime oldRequestedAt = m_tickets[i].cancelRequestedAt;
            const bool oldWasInKey = m_tickets[i].wasInKey;
            const QString oldDetail = m_tickets[i].stateDetail;

            m_tickets[i].cancelState = storageState;
            if (!cancelSource.trimmed().isEmpty()) {
                m_tickets[i].cancelSource = cancelSource.trimmed();
            }
            if (!residentSlotId.trimmed().isEmpty()) {
                m_tickets[i].residentSlotId = residentSlotId.trimmed();
            }
            if (!residentKeyId.trimmed().isEmpty()) {
                m_tickets[i].residentKeyId = residentKeyId.trimmed();
            }
            if (cancelRequestedAt.isValid()) {
                m_tickets[i].cancelRequestedAt = cancelRequestedAt;
            }
            m_tickets[i].wasInKey = m_tickets[i].wasInKey || wasInKey;
            if (!stateDetail.trimmed().isEmpty()) {
                m_tickets[i].stateDetail = stateDetail.trimmed();
            }

            const bool unchanged = oldState == m_tickets[i].cancelState
                    && oldSlotId == m_tickets[i].residentSlotId
                    && oldKeyId == m_tickets[i].residentKeyId
                    && oldSource == m_tickets[i].cancelSource
                    && oldRequestedAt == m_tickets[i].cancelRequestedAt
                    && oldWasInKey == m_tickets[i].wasInKey
                    && oldDetail == m_tickets[i].stateDetail;
            if (unchanged) {
                return true;
            }
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] updateCancelTracking taskId=%1 %2 -> %3 slot=%4 key=%5 source=%6 wasInKey=%7 detail=%8")
                          .arg(taskId,
                               oldState.isEmpty() ? QStringLiteral("none") : oldState,
                               m_tickets[i].cancelState,
                               m_tickets[i].residentSlotId.isEmpty() ? QStringLiteral("<none>") : m_tickets[i].residentSlotId,
                               m_tickets[i].residentKeyId.isEmpty() ? QStringLiteral("<none>") : m_tickets[i].residentKeyId,
                               m_tickets[i].cancelSource.isEmpty() ? QStringLiteral("<none>") : m_tickets[i].cancelSource,
                               m_tickets[i].wasInKey ? QStringLiteral("true") : QStringLiteral("false"),
                               m_tickets[i].stateDetail.isEmpty() ? QStringLiteral("<none>") : m_tickets[i].stateDetail);
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}

bool TicketStore::removeTicket(const QString &taskId)
{
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            m_tickets.removeAt(i);
            m_rawJsonByTaskId.remove(taskId);
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] removeTicket taskId=%1 remainingCount=%2")
                          .arg(taskId, QString::number(m_tickets.size()));
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}

bool TicketStore::updateAdminDeleteStage(const QString &taskId,
                                         SystemTicketDto::AdminDeleteStage stage)
{
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            if (m_tickets[i].adminDeleteStage == stage) {
                return true;
            }
            m_tickets[i].adminDeleteStage = stage;
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] updateAdminDeleteStage taskId=%1 stage=%2")
                          .arg(taskId, QString::number(static_cast<int>(stage)));
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}

bool TicketStore::updateAdminDeleteError(const QString &taskId, const QString &errorText)
{
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            if (m_tickets[i].adminDeleteError == errorText) {
                return true;
            }
            m_tickets[i].adminDeleteError = errorText;
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] updateAdminDeleteError taskId=%1 error=%2")
                          .arg(taskId, errorText.left(80));
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}

bool TicketStore::refreshKeyConfirmedAt(const QList<QString> &taskIds, qint64 ms)
{
    if (taskIds.isEmpty()) {
        return false;
    }
    bool anyChanged = false;
    for (const QString &taskId : taskIds) {
        for (int i = 0; i < m_tickets.size(); ++i) {
            if (m_tickets[i].taskId == taskId) {
                if (m_tickets[i].keyConfirmedAtMs != ms) {
                    m_tickets[i].keyConfirmedAtMs = ms;
                    anyChanged = true;
                }
                break;
            }
        }
    }
    if (anyChanged) {
        emit ticketsChanged(m_tickets);
    }
    return anyChanged;
}
