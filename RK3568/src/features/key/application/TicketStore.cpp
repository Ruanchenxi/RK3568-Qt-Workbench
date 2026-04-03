#include "features/key/application/TicketStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

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

// ── 持久化 ──

QString TicketStore::storePath()
{
    const QString logsDir = QCoreApplication::applicationDirPath() + QStringLiteral("/logs");
    QDir().mkpath(logsDir);
    return logsDir + QStringLiteral("/ticket_store.json");
}

static QJsonObject dtoToJson(const SystemTicketDto &dto)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("taskId"), dto.taskId);
    obj.insert(QStringLiteral("ticketNo"), dto.ticketNo);
    obj.insert(QStringLiteral("taskName"), dto.taskName);
    obj.insert(QStringLiteral("taskType"), dto.taskType);
    obj.insert(QStringLiteral("stepNum"), dto.stepNum);
    obj.insert(QStringLiteral("createTime"), dto.createTime);
    obj.insert(QStringLiteral("planTime"), dto.planTime);
    obj.insert(QStringLiteral("source"), dto.source);
    obj.insert(QStringLiteral("transferState"), dto.transferState);
    obj.insert(QStringLiteral("lastError"), dto.lastError);
    obj.insert(QStringLiteral("returnState"), dto.returnState);
    obj.insert(QStringLiteral("returnError"), dto.returnError);
    obj.insert(QStringLiteral("cancelState"), dto.cancelState);
    obj.insert(QStringLiteral("residentSlotId"), dto.residentSlotId);
    obj.insert(QStringLiteral("residentKeyId"), dto.residentKeyId);
    obj.insert(QStringLiteral("cancelRequestedAt"),
               dto.cancelRequestedAt.isValid()
                   ? dto.cancelRequestedAt.toString(Qt::ISODateWithMs)
                   : QString());
    obj.insert(QStringLiteral("cancelSource"), dto.cancelSource);
    obj.insert(QStringLiteral("wasInKey"), dto.wasInKey);
    obj.insert(QStringLiteral("stateDetail"), dto.stateDetail);
    obj.insert(QStringLiteral("jsonPath"), dto.jsonPath);
    obj.insert(QStringLiteral("receivedAt"),
               dto.receivedAt.isValid()
                   ? dto.receivedAt.toString(Qt::ISODateWithMs)
                   : QString());
    return obj;
}

static SystemTicketDto dtoFromJson(const QJsonObject &obj)
{
    SystemTicketDto dto;
    dto.taskId = obj.value(QStringLiteral("taskId")).toString().trimmed();
    dto.ticketNo = obj.value(QStringLiteral("ticketNo")).toString().trimmed();
    dto.taskName = obj.value(QStringLiteral("taskName")).toString().trimmed();
    dto.taskType = obj.value(QStringLiteral("taskType")).toInt(0);
    dto.stepNum = obj.value(QStringLiteral("stepNum")).toInt(0);
    dto.createTime = obj.value(QStringLiteral("createTime")).toString().trimmed();
    dto.planTime = obj.value(QStringLiteral("planTime")).toString().trimmed();
    dto.source = obj.value(QStringLiteral("source")).toString().trimmed();
    dto.transferState = obj.value(QStringLiteral("transferState")).toString().trimmed();
    dto.lastError = obj.value(QStringLiteral("lastError")).toString().trimmed();
    dto.returnState = obj.value(QStringLiteral("returnState")).toString().trimmed();
    dto.returnError = obj.value(QStringLiteral("returnError")).toString().trimmed();
    dto.cancelState = obj.value(QStringLiteral("cancelState")).toString().trimmed();
    dto.residentSlotId = obj.value(QStringLiteral("residentSlotId")).toString().trimmed();
    dto.residentKeyId = obj.value(QStringLiteral("residentKeyId")).toString().trimmed();
    const QString tsStr = obj.value(QStringLiteral("cancelRequestedAt")).toString().trimmed();
    dto.cancelRequestedAt = tsStr.isEmpty() ? QDateTime() : QDateTime::fromString(tsStr, Qt::ISODateWithMs);
    dto.cancelSource = obj.value(QStringLiteral("cancelSource")).toString().trimmed();
    dto.wasInKey = obj.value(QStringLiteral("wasInKey")).toBool(false);
    dto.stateDetail = obj.value(QStringLiteral("stateDetail")).toString().trimmed();
    dto.jsonPath = obj.value(QStringLiteral("jsonPath")).toString().trimmed();
    const QString rcvStr = obj.value(QStringLiteral("receivedAt")).toString().trimmed();
    dto.receivedAt = rcvStr.isEmpty() ? QDateTime() : QDateTime::fromString(rcvStr, Qt::ISODateWithMs);
    dto.valid = !dto.taskId.isEmpty();
    return dto;
}

// 已完成的票保留 7 天后自动从持久化中清理
static bool isExpiredFinished(const SystemTicketDto &dto)
{
    if (dto.returnState != QLatin1String("return-delete-success")
            && dto.cancelState != QLatin1String("cancel-done")) {
        return false;
    }
    if (!dto.receivedAt.isValid()) {
        return false;
    }
    return dto.receivedAt.daysTo(QDateTime::currentDateTime()) > 7;
}

void TicketStore::loadFromDisk()
{
    const QString path = storePath();
    QFile file(path);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote()
                << QStringLiteral("[TicketStore] loadFromDisk 打开失败: %1").arg(file.errorString());
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isArray()) {
        qWarning().noquote()
                << QStringLiteral("[TicketStore] loadFromDisk JSON 格式不是数组，跳过加载");
        return;
    }

    int loaded = 0;
    int expired = 0;
    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        const SystemTicketDto dto = dtoFromJson(value.toObject());
        if (!dto.valid) {
            continue;
        }
        if (isExpiredFinished(dto)) {
            ++expired;
            continue;
        }
        m_tickets.append(dto);

        // 尝试恢复原始 JSON（落盘文件路径）
        if (!dto.jsonPath.isEmpty()) {
            const QString absPath = dto.jsonPath.startsWith('/')
                    ? dto.jsonPath
                    : QCoreApplication::applicationDirPath() + QStringLiteral("/") + dto.jsonPath;
            QFile rawFile(absPath);
            if (rawFile.open(QIODevice::ReadOnly)) {
                m_rawJsonByTaskId.insert(dto.taskId, rawFile.readAll());
                rawFile.close();
            }
        }
        ++loaded;
    }

    qInfo().noquote()
            << QStringLiteral("[TicketStore] loadFromDisk loaded=%1 expired=%2 path=%3")
                  .arg(QString::number(loaded), QString::number(expired), path);
    if (!m_tickets.isEmpty()) {
        emit ticketsChanged(m_tickets);
    }
    // 如果有过期清理，重新写盘
    if (expired > 0) {
        saveToDisk();
    }
}

void TicketStore::saveToDisk() const
{
    QJsonArray array;
    for (const SystemTicketDto &dto : m_tickets) {
        if (!dto.valid) {
            continue;
        }
        array.append(dtoToJson(dto));
    }

    QSaveFile file(storePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning().noquote()
                << QStringLiteral("[TicketStore] saveToDisk 打开失败: %1").arg(file.errorString());
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    file.commit();
}

bool TicketStore::ingestOrphanTicket(const QString &taskId, const QString &residentSlotId)
{
    if (taskId.trimmed().isEmpty()) {
        return false;
    }
    // 已存在则不重复创建
    for (const SystemTicketDto &dto : m_tickets) {
        if (dto.taskId == taskId) {
            return true;
        }
    }

    SystemTicketDto dto;
    dto.taskId = taskId;
    dto.ticketNo = taskId;
    dto.taskName = QStringLiteral("orphan-%1").arg(taskId);
    dto.source = QStringLiteral("orphan-recovery");
    dto.transferState = QStringLiteral("success");
    dto.returnState = QStringLiteral("idle");
    dto.cancelState = QStringLiteral("none");
    dto.residentSlotId = residentSlotId;
    dto.wasInKey = true;
    dto.receivedAt = QDateTime::currentDateTime();
    dto.valid = true;

    m_tickets.prepend(dto);
    qInfo().noquote()
            << QStringLiteral("[TicketStore] ingestOrphanTicket taskId=%1 slot=%2 count=%3")
                  .arg(taskId, residentSlotId, QString::number(m_tickets.size()));
    saveToDisk();
    emit ticketsChanged(m_tickets);
    return true;
}

// ── 业务方法 ──

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
    dto.cancelState = QStringLiteral("none");
    dto.residentSlotId = QStringLiteral("slot-1");
    dto.residentKeyId.clear();
    dto.cancelRequestedAt = QDateTime();
    dto.cancelSource.clear();
    dto.wasInKey = false;
    dto.stateDetail.clear();
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
            // 已受理撤销的票不允许被覆盖
            if (!m_tickets[i].cancelState.isEmpty()
                    && m_tickets[i].cancelState != QLatin1String("none")) {
                if (errorMsg)
                    *errorMsg = QStringLiteral("该任务已受理撤销，无法重新提交");
                return false;
            }
            // 保留已有的发送状态，避免同一 taskId 被新的 HTTP JSON 覆盖成 received。
            dto.transferState = m_tickets[i].transferState;
            dto.lastError = m_tickets[i].lastError;
            dto.returnState = m_tickets[i].returnState;
            dto.returnError = m_tickets[i].returnError;
            dto.cancelState = m_tickets[i].cancelState;
            dto.residentSlotId = m_tickets[i].residentSlotId;
            dto.residentKeyId = m_tickets[i].residentKeyId;
            dto.cancelRequestedAt = m_tickets[i].cancelRequestedAt;
            dto.cancelSource = m_tickets[i].cancelSource;
            dto.wasInKey = m_tickets[i].wasInKey;
            dto.stateDetail = m_tickets[i].stateDetail;
            m_tickets[i] = dto;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        m_tickets.prepend(dto);

    m_rawJsonByTaskId.insert(dto.taskId, jsonBytes);
    qInfo().noquote()
            << QStringLiteral("[TicketStore] ingestJson taskId=%1 replaced=%2 transferState=%3 returnState=%4 count=%5 jsonPath=%6")
                  .arg(dto.taskId,
                       replaced ? QStringLiteral("true") : QStringLiteral("false"),
                       dto.transferState,
                       dto.returnState,
                       QString::number(m_tickets.size()),
                       dto.jsonPath);
    saveToDisk();
    emit ticketsChanged(m_tickets);
    return true;
}

bool TicketStore::updateTransferState(const QString &taskId,
                                      const QString &state,
                                      const QString &lastError)
{
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            const QString oldState = m_tickets[i].transferState;
            const QString oldError = m_tickets[i].lastError;
            if (oldState == state && oldError == lastError) {
                return true;
            }
            m_tickets[i].transferState = state;
            m_tickets[i].lastError = lastError;
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] updateTransferState taskId=%1 %2 -> %3 error=%4")
                          .arg(taskId,
                               oldState,
                               state,
                               lastError.isEmpty() ? QStringLiteral("<none>") : lastError);
            saveToDisk();
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
            saveToDisk();
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
            const QString oldState = m_tickets[i].returnState;
            const QString oldError = m_tickets[i].returnError;
            if (oldState == state && oldError == returnError) {
                return true;
            }
            m_tickets[i].returnState = state;
            m_tickets[i].returnError = returnError;
            qInfo().noquote()
                    << QStringLiteral("[TicketStore] updateReturnState taskId=%1 %2 -> %3 error=%4")
                          .arg(taskId,
                               oldState,
                               state,
                               returnError.isEmpty() ? QStringLiteral("<none>") : returnError);
            saveToDisk();
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

bool TicketStore::updateCancelState(const QString &taskId, const QString &state)
{
    return updateCancelTracking(taskId, state);
}

bool TicketStore::updateCancelTracking(const QString &taskId,
                                       const QString &state,
                                       const QString &cancelSource,
                                       const QString &residentSlotId,
                                       const QString &residentKeyId,
                                       const QDateTime &cancelRequestedAt,
                                       bool wasInKey,
                                       const QString &stateDetail)
{
    for (int i = 0; i < m_tickets.size(); ++i) {
        if (m_tickets[i].taskId == taskId) {
            const QString oldState = m_tickets[i].cancelState;
            const QString oldSlotId = m_tickets[i].residentSlotId;
            const QString oldKeyId = m_tickets[i].residentKeyId;
            const QString oldSource = m_tickets[i].cancelSource;
            const QDateTime oldRequestedAt = m_tickets[i].cancelRequestedAt;
            const bool oldWasInKey = m_tickets[i].wasInKey;
            const QString oldDetail = m_tickets[i].stateDetail;

            m_tickets[i].cancelState = state;
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
            saveToDisk();
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
            saveToDisk();
            emit ticketsChanged(m_tickets);
            return true;
        }
    }
    return false;
}
