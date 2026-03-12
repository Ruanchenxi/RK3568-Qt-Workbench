/**
 * @file KeyManageController.cpp
 * @brief 钥匙管理页控制器实现
 */
#include "features/key/application/KeyManageController.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include "core/ConfigManager.h"
#include "features/key/application/KeySessionService.h"
#include "features/key/application/KeyProvisioningService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketIngressService.h"
#include "features/key/application/TicketReturnHttpClient.h"
#include "features/key/protocol/KeyProtocolDefs.h"

namespace {

QString rawTaskIdToDecimalString(const QByteArray &taskIdRaw)
{
    if (taskIdRaw.size() < 8) {
        return QString();
    }

    qulonglong value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<qulonglong>(static_cast<quint8>(taskIdRaw[i])) << (i * 8));
    }
    return QString::number(value);
}

}

KeyManageController::KeyManageController(IKeySessionService *session,
                                         SerialLogManager *logManager,
                                         QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_logManager(logManager)
    , m_ticketStore(new TicketStore(this))
    , m_ticketIngress(new TicketIngressService(this))
    , m_ticketReturnClient(new TicketReturnHttpClient(this))
    , m_keyProvisioning(new KeyProvisioningService(nullptr, this))
    , m_nextOpId(1)
    , m_selectedSystemTicketId()
    , m_activeTransferTaskId()
    , m_activeReturnTaskId()
    , m_activeReturnTaskIdRaw()
{
    if (!m_session) {
        m_session = new KeySessionService(this);
    }
    if (!m_logManager) {
        m_logManager = new SerialLogManager(this);
    }

    connect(m_session, &IKeySessionService::eventOccurred,
            this, &KeyManageController::handleSessionEvent);
    connect(m_session, &IKeySessionService::logItemEmitted,
            this, &KeyManageController::handleLogItem);
    connect(m_ticketStore, &TicketStore::ticketsChanged,
            this, &KeyManageController::handleSystemTicketsChanged);
    connect(m_ticketIngress, &TicketIngressService::httpServerLogAppended,
            this, &KeyManageController::handleHttpServerLog);
    connect(m_ticketIngress, &TicketIngressService::jsonReceived,
            this, &KeyManageController::handleJsonReceived);
    connect(m_ticketReturnClient, &TicketReturnHttpClient::logAppended,
            this, &KeyManageController::handleHttpClientLog);
    connect(m_ticketReturnClient, &TicketReturnHttpClient::uploadSucceeded,
            this, &KeyManageController::handleReturnUploadSucceeded);
    connect(m_ticketReturnClient, &TicketReturnHttpClient::uploadFailed,
            this, &KeyManageController::handleReturnUploadFailed);
    connect(m_keyProvisioning, &KeyProvisioningService::logAppended,
            this, &KeyManageController::handleHttpClientLog);
    connect(m_keyProvisioning, &KeyProvisioningService::initPayloadReady,
            this, &KeyManageController::handleInitPayloadReady);
    connect(m_keyProvisioning, &KeyProvisioningService::rfidPayloadReady,
            this, &KeyManageController::handleRfidPayloadReady);
    connect(m_keyProvisioning, &KeyProvisioningService::requestFailed,
            this, &KeyManageController::handleProvisionRequestFailed);
    connect(m_logManager, &SerialLogManager::overflowDropped, this, [this]() {
        emit statusMessage(QStringLiteral("日志达到上限，已自动丢弃旧日志"));
    });
}

void KeyManageController::start()
{
    tryAutoConnectKeyPort();
    if (!m_ticketIngress->start()) {
        emit statusMessage(m_ticketIngress->lastError());
    }
    const KeySessionSnapshot snapshot = currentSnapshot();
    m_lastReadyState = snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
    emit sessionSnapshotChanged(snapshot);
    emit systemTicketsUpdated(systemTickets());
}

void KeyManageController::onInitClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法执行初始化"));
        return;
    }
    if (!snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        emit statusMessage(QStringLiteral("钥匙未就绪，无法执行初始化"));
        return;
    }
    if (m_keyProvisioning->isBusy()) {
        emit statusMessage(QStringLiteral("正在获取钥匙初始化数据，请稍后再试"));
        return;
    }

    const int stationNo = ConfigManager::instance()->value("key/backendStationNo", 0).toInt();
    emit statusMessage(QStringLiteral("初始化钥匙，正在获取初始化数据..."));
    m_keyProvisioning->requestInitPayload(stationNo);
}

void KeyManageController::onDownloadRfidClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法下载 RFID"));
        return;
    }
    if (!snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        emit statusMessage(QStringLiteral("钥匙未就绪，无法下载 RFID"));
        return;
    }
    if (m_keyProvisioning->isBusy()) {
        emit statusMessage(QStringLiteral("正在获取 RFID 数据，请稍后再试"));
        return;
    }

    const int stationNo = ConfigManager::instance()->value("key/backendStationNo", 0).toInt();
    emit statusMessage(QStringLiteral("下载 RFID，正在获取后端数据..."));
    m_keyProvisioning->requestRfidPayload(stationNo);
}

void KeyManageController::onQueryTasksClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法查询任务"));
        return;
    }

    CommandRequest req;
    req.id = CommandId::QueryTasks;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral("执行获取任务数 (Q_TASK)"));
}

void KeyManageController::onDeleteClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法删除票"));
        return;
    }

    m_pendingDeletedSystemTicketId.clear();
    m_pendingDeletedKeyTaskRaw.clear();
    m_pendingDeleteAllowsRetransfer = false;

    if (m_selectedKeyTaskRaw.isEmpty()) {
        emit statusMessage(QStringLiteral("未选中钥匙票，无法删除，请先读取钥匙票列表并选中一条任务"));
        return;
    }

    m_pendingDeletedKeyTaskRaw = m_selectedKeyTaskRaw;
    m_pendingDeletedSystemTicketId = taskIdFromRaw(m_pendingDeletedKeyTaskRaw);
    const SystemTicketDto mappedTicket = m_ticketStore->ticketById(m_pendingDeletedSystemTicketId);
    m_pendingDeleteAllowsRetransfer = mappedTicket.valid
            && (mappedTicket.returnState.isEmpty() || mappedTicket.returnState == QLatin1String("idle"));

    CommandRequest req;
    req.id = CommandId::DeleteTask;
    req.opId = nextOpId();
    req.payload = m_pendingDeletedKeyTaskRaw;
    m_session->execute(req);
    emit statusMessage(QStringLiteral("执行删除票 (DEL)"));
}

void KeyManageController::onTransferSelectedTicket()
{
    if (m_selectedSystemTicketId.trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("未选中系统票，无法发送传票"));
        return;
    }

    tryStartTicketTransfer(m_selectedSystemTicketId, false);
}

void KeyManageController::onReturnSelectedTicket()
{
    if (m_selectedSystemTicketId.trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("未选中系统票，无法执行回传"));
        return;
    }

    tryStartTicketReturn(m_selectedSystemTicketId, false);
}

void KeyManageController::onGetSystemTicketListClicked()
{
    emit systemTicketsUpdated(systemTickets());
}

void KeyManageController::onSystemTicketSelected(int row)
{
    const QList<SystemTicketDto> tickets = systemTickets();
    if (row < 0 || row >= tickets.size()) {
        m_selectedSystemTicketId.clear();
        emit selectedSystemTicketChanged(SystemTicketDto{});
        return;
    }

    m_selectedSystemTicketId = tickets.at(row).taskId;
    emit selectedSystemTicketChanged(tickets.at(row));
}

void KeyManageController::onKeyTaskSelected(int row)
{
    if (row < 0 || row >= m_lastKeyTasks.size()) {
        m_selectedKeyTaskRaw.clear();
        return;
    }

    m_selectedKeyTaskRaw = m_lastKeyTasks.at(row).taskId;
}

void KeyManageController::onExpertModeChanged(bool enabled)
{
    m_logManager->setExpertMode(enabled);
    emit logTableRefreshRequested();
}

void KeyManageController::onShowHexChanged(bool enabled)
{
    m_logManager->setShowHex(enabled);
}

void KeyManageController::onClearLogsClicked()
{
    m_logManager->clear();
    emit logsCleared();
    emit statusMessage(QStringLiteral("串口日志已清空"));
}

void KeyManageController::onClearHttpClientLogsClicked()
{
    m_httpClientLogLines.clear();
    emit statusMessage(QStringLiteral("HTTP客户端报文已清空"));
}

void KeyManageController::onClearHttpServerLogsClicked()
{
    m_httpServerLogLines.clear();
    emit statusMessage(QStringLiteral("HTTP服务端报文已清空"));
}

bool KeyManageController::exportLogs(const QString &path, QString *error) const
{
    return m_logManager->exportText(path, error);
}

QList<LogItem> KeyManageController::visibleLogs() const
{
    return m_logManager->filteredItems();
}

bool KeyManageController::shouldDisplay(const LogItem &item) const
{
    return m_logManager->shouldDisplay(item);
}

KeySessionSnapshot KeyManageController::currentSnapshot() const
{
    return m_session->snapshot();
}

QList<SystemTicketDto> KeyManageController::systemTickets() const
{
    return m_ticketStore->tickets();
}

QString KeyManageController::httpServerLogText() const
{
    return m_httpServerLogLines.join("\n");
}

QString KeyManageController::httpClientLogText() const
{
    return m_httpClientLogLines.join("\n");
}

void KeyManageController::handleSessionEvent(const KeySessionEvent &event)
{
    switch (event.kind) {
    case KeySessionEvent::Notice:
        emit statusMessage(event.data.value("what").toString());
        break;
    case KeySessionEvent::Timeout: {
        const QString what = event.data.value("what").toString();
        if (what.contains(QStringLiteral("DEL"), Qt::CaseInsensitive)) {
            m_pendingDeletedSystemTicketId.clear();
            m_pendingDeletedKeyTaskRaw.clear();
            m_pendingDeleteAllowsRetransfer = false;
        }
        if (!m_activeReturnTaskId.isEmpty()
                && (what.contains(QStringLiteral("I_TASK_LOG"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("0X05"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("UP_TASK_LOG"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("0X15"), Qt::CaseInsensitive))) {
            m_ticketStore->updateReturnState(m_activeReturnTaskId, QStringLiteral("return-failed"), what);
            m_activeReturnTaskId.clear();
            m_activeReturnTaskIdRaw.clear();
            m_pendingReturnHandshakeTaskId.clear();
            m_pendingReturnHandshakeTaskIdRaw.clear();
        }
        emit timeoutOccurred(what);
        emit statusMessage(what);
        break;
    }
    case KeySessionEvent::Ack:
        if (static_cast<quint8>(event.data.value("ackedCmd").toInt()) == KeyProtocol::CmdSetCom
                && !m_pendingReturnHandshakeTaskId.isEmpty()
                && !m_pendingReturnHandshakeTaskIdRaw.isEmpty()) {
            CommandRequest req;
            req.id = CommandId::QueryTaskLog;
            req.opId = nextOpId();
            req.payload = m_pendingReturnHandshakeTaskIdRaw;
            m_session->execute(req);
            emit statusMessage(QStringLiteral("回传前握手已确认，开始读取钥匙回传日志：%1")
                                   .arg(m_pendingReturnHandshakeTaskId));
            m_pendingReturnHandshakeTaskId.clear();
            m_pendingReturnHandshakeTaskIdRaw.clear();
        }
        emit ackReceived(static_cast<quint8>(event.data.value("ackedCmd").toInt()));
        break;
    case KeySessionEvent::Nak:
        if (static_cast<quint8>(event.data.value("origCmd").toInt()) == KeyProtocol::CmdDel) {
            m_pendingDeletedSystemTicketId.clear();
            m_pendingDeletedKeyTaskRaw.clear();
            m_pendingDeleteAllowsRetransfer = false;
        }
        if (!m_activeReturnTaskId.isEmpty()) {
            const quint8 origCmd = static_cast<quint8>(event.data.value("origCmd").toInt());
            if (origCmd == KeyProtocol::CmdITaskLog || origCmd == KeyProtocol::CmdUpTaskLog) {
                const QString reason = QStringLiteral("收到 NAK，origCmd=0x%1")
                        .arg(origCmd, 2, 16, QLatin1Char('0'))
                        .toUpper();
                m_ticketStore->updateReturnState(m_activeReturnTaskId, QStringLiteral("return-failed"), reason);
                m_activeReturnTaskId.clear();
                m_activeReturnTaskIdRaw.clear();
            }
        }
        emit nakReceived(static_cast<quint8>(event.data.value("origCmd").toInt()),
                         static_cast<quint8>(event.data.value("errCode").toInt()));
        break;
    case KeySessionEvent::TasksUpdated: {
        const QList<KeyTaskDto> tasks = toTaskDtos(event.data.value("tasks").toList());
        m_lastKeyTasks = tasks;
        reconcileSystemTicketsFromKeyTasks(tasks);
        recoverAutoTransferWhenKeyEmpty(tasks);
        tryAutoCleanupReturnedTicket(tasks);
        bool selectedStillExists = false;
        for (const KeyTaskDto &task : tasks) {
            if (task.taskId == m_selectedKeyTaskRaw) {
                selectedStillExists = true;
                break;
            }
        }
        if (!selectedStillExists) {
            m_selectedKeyTaskRaw.clear();
        }
        emit tasksUpdated(tasks);
        emit statusMessage(QStringLiteral("Q_TASK 完成，任务数=%1").arg(tasks.size()));
        if (!m_pendingDeletedSystemTicketId.isEmpty()) {
            bool stillExists = false;
            for (const KeyTaskDto &task : tasks) {
                if (task.taskId == m_pendingDeletedKeyTaskRaw) {
                    stillExists = true;
                    break;
                }
            }
            if (!stillExists) {
                if (m_pendingDeleteAllowsRetransfer) {
                    m_ticketStore->markKeyTaskDeleted(m_pendingDeletedSystemTicketId);
                    emit statusMessage(QStringLiteral("钥匙任务已删除，可再次传票"));
                } else {
                    emit statusMessage(QStringLiteral("钥匙任务已删除"));
                }
            } else {
                emit statusMessage(QStringLiteral("删除后验证发现钥匙任务仍存在，请重新读取钥匙票列表确认"));
            }
            m_pendingDeletedSystemTicketId.clear();
            m_pendingDeletedKeyTaskRaw.clear();
            m_pendingDeleteAllowsRetransfer = false;
        }
        tryAutoReturnCompletedTicket(tasks);
        if (!m_selectedSystemTicketId.isEmpty()) {
            emit selectedSystemTicketChanged(m_ticketStore->ticketById(m_selectedSystemTicketId));
        }
        break;
    }
    case KeySessionEvent::TaskLogReady: {
        if (m_activeReturnTaskId.isEmpty()) {
            emit statusMessage(QStringLiteral("收到任务回传日志，但当前无活动回传任务"));
            break;
        }

        QJsonObject payload;
        payload.insert("stationNo", event.data.value("stationNo").toInt());
        payload.insert("steps", event.data.value("steps").toInt());
        payload.insert("taskId", event.data.value("taskId").toString());

        QJsonArray items;
        const QVariantList rawItems = event.data.value("items").toList();
        for (const QVariant &itemVar : rawItems) {
            items.append(QJsonObject::fromVariantMap(itemVar.toMap()));
        }
        payload.insert("taskLogItems", items);

        m_ticketStore->updateReturnState(m_activeReturnTaskId, QStringLiteral("return-uploading"));
        m_ticketReturnClient->uploadReturnPayload(payload);
        emit statusMessage(QStringLiteral("已解析钥匙回传日志，准备上传到服务端"));
        break;
    }
    case KeySessionEvent::TicketTransferProgress: {
        const int frameIndex = event.data.value("frameIndex").toInt();
        const int totalFrames = event.data.value("totalFrames").toInt();
        if (!m_activeTransferTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(m_activeTransferTaskId, QStringLiteral("sending"));
        }
        emit statusMessage(QStringLiteral("传票发送中：第 %1/%2 帧").arg(frameIndex).arg(totalFrames));
        break;
    }
    case KeySessionEvent::TicketTransferFinished:
        if (!m_activeTransferTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(m_activeTransferTaskId, QStringLiteral("success"));
        }
        m_activeTransferTaskId.clear();
        emit statusMessage(QStringLiteral("传票发送完成"));
        tryAutoTransferPendingTicket();
        break;
    case KeySessionEvent::TicketTransferFailed: {
        const QString what = event.data.value("what").toString();
        if (!m_activeTransferTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(m_activeTransferTaskId, QStringLiteral("failed"), what);
        }
        m_activeTransferTaskId.clear();
        emit statusMessage(QStringLiteral("传票发送失败：%1").arg(what));
        break;
    }
    case KeySessionEvent::KeyStateChanged: {
        const KeySessionSnapshot snapshot = m_session->snapshot();
        emit sessionSnapshotChanged(snapshot);
        tryAutoRefreshKeyTasksOnReady(snapshot);
        tryAutoTransferPendingTicket();
        break;
    }
    case KeySessionEvent::RawProtocol:
        break;
    }
}

void KeyManageController::handleLogItem(const LogItem &item)
{
    m_logManager->append(item);
    if (m_logManager->shouldDisplay(item)) {
        emit logRowAppended(item);
    }
}

void KeyManageController::handleSystemTicketsChanged(const QList<SystemTicketDto> &tickets)
{
    emit systemTicketsUpdated(tickets);
    if (!m_selectedSystemTicketId.isEmpty()) {
        for (const SystemTicketDto &ticket : tickets) {
            if (ticket.taskId == m_selectedSystemTicketId) {
                emit selectedSystemTicketChanged(ticket);
                return;
            }
        }
    }
}

void KeyManageController::handleJsonReceived(const QByteArray &jsonBytes, const QString &savedPath)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &pe);
    QString taskId;
    if (!doc.isNull() && doc.isObject()) {
        const QJsonObject root = doc.object();
        const QJsonObject ticketObj = (root.contains("data") && root.value("data").isObject())
                ? root.value("data").toObject()
                : root;
        taskId = ticketObj.value("taskId").toString();
    }
    const SystemTicketDto oldTicket = m_ticketStore->ticketById(taskId);

    QString errorMsg;
    if (!m_ticketStore->ingestJson(jsonBytes, savedPath, &errorMsg)) {
        emit statusMessage(QStringLiteral("系统票入池失败：%1").arg(errorMsg));
        return;
    }

    if (oldTicket.valid) {
        if (oldTicket.transferState == QLatin1String("success")) {
            emit statusMessage(QStringLiteral("系统票已存在且已传票到钥匙，本次重复请求已忽略"));
            return;
        }
        if (oldTicket.transferState == QLatin1String("sending")) {
            emit statusMessage(QStringLiteral("系统票正在传票到钥匙，本次重复请求已忽略"));
            return;
        }
        if (oldTicket.transferState == QLatin1String("key-cleared")) {
            if (autoTransferEnabled()) {
                emit statusMessage(QStringLiteral("系统票对应钥匙任务已删除，准备再次自动传票"));
                tryStartTicketTransfer(taskId, true);
            } else {
                emit statusMessage(QStringLiteral("系统票对应钥匙任务已删除，可手动再次传票"));
            }
            return;
        }
    }

    if (autoTransferEnabled()) {
        emit statusMessage(QStringLiteral("系统票已接收并入池，准备自动传票"));
    } else {
        emit statusMessage(QStringLiteral("系统票已接收并入池（自动传票已关闭）"));
    }
    tryAutoTransferPendingTicket();
}

void KeyManageController::handleHttpServerLog(const QString &text)
{
    m_httpServerLogLines << text;
    emit httpServerLogAppended(text);
}

void KeyManageController::handleHttpClientLog(const QString &text)
{
    m_httpClientLogLines << text;
    emit httpClientLogAppended(text);
}

void KeyManageController::handleInitPayloadReady(const QByteArray &payload)
{
    CommandRequest req;
    req.id = CommandId::InitKey;
    req.opId = nextOpId();
    req.payload = payload;
    m_session->execute(req);
    emit statusMessage(QStringLiteral("初始化数据已准备完成，开始下发到钥匙"));
}

void KeyManageController::handleRfidPayloadReady(const QByteArray &payload)
{
    CommandRequest req;
    req.id = CommandId::DownloadRfid;
    req.opId = nextOpId();
    req.payload = payload;
    m_session->execute(req);
    emit statusMessage(QStringLiteral("RFID 数据已准备完成，开始下发到钥匙"));
}

void KeyManageController::handleProvisionRequestFailed(const QString &reason)
{
    emit statusMessage(reason);
}

void KeyManageController::handleReturnUploadSucceeded(const QString &taskId)
{
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-success"));
    emit statusMessage(QStringLiteral("回传上传成功：%1").arg(taskId));

    if (!m_activeReturnTaskIdRaw.isEmpty()) {
        // 自动回传成功后进入钥匙任务清理阶段，沿用手动 DEL 的“删除后再查一次”
        // 验证链，确保后续 Q_TASK 结果能和当前系统票关联起来。
        m_pendingDeletedSystemTicketId = taskId;
        m_pendingDeletedKeyTaskRaw = m_activeReturnTaskIdRaw;
        m_pendingDeleteAllowsRetransfer = false;

        CommandRequest req;
        req.id = CommandId::DeleteTask;
        req.opId = nextOpId();
        req.payload = m_activeReturnTaskIdRaw;
        m_session->execute(req);
        emit statusMessage(QStringLiteral("回传上传成功，开始删除钥匙中的已完成任务"));
    } else {
        emit statusMessage(QStringLiteral("回传上传成功，但缺少钥匙任务原始ID，无法自动删除钥匙任务"));
    }

    m_activeReturnTaskId.clear();
    m_activeReturnTaskIdRaw.clear();
}

void KeyManageController::handleReturnUploadFailed(const QString &taskId, const QString &reason)
{
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-failed"), reason);
    emit statusMessage(QStringLiteral("回传上传失败：%1").arg(reason));
    m_activeReturnTaskId.clear();
    m_activeReturnTaskIdRaw.clear();
}

bool KeyManageController::autoTransferEnabled() const
{
    return ConfigManager::instance()->value("ticket/autoTransferEnabled", false).toBool();
}

void KeyManageController::reconcileSystemTicketsFromKeyTasks(const QList<KeyTaskDto> &tasks)
{
    for (const KeyTaskDto &task : tasks) {
        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (!ticket.valid) {
            continue;
        }

        const bool needsReconcile = ticket.transferState == QLatin1String("received")
                || ticket.transferState == QLatin1String("auto-pending")
                || ticket.transferState == QLatin1String("sending")
                || ticket.transferState == QLatin1String("failed");
        if (!needsReconcile) {
            continue;
        }

        m_ticketStore->updateTransferState(taskId, QStringLiteral("success"));
        emit statusMessage(QStringLiteral("检测到钥匙中已存在系统票，已将本地状态纠正为“已传票到钥匙”：%1")
                               .arg(taskId));
    }
}

void KeyManageController::recoverAutoTransferWhenKeyEmpty(const QList<KeyTaskDto> &tasks)
{
    if (!autoTransferEnabled()) {
        return;
    }
    if (!tasks.isEmpty()) {
        return;
    }
    if (!m_activeTransferTaskId.isEmpty() || !m_activeReturnTaskId.isEmpty()) {
        return;
    }

    bool requeued = false;
    const QList<SystemTicketDto> tickets = systemTickets();
    for (const SystemTicketDto &ticket : tickets) {
        const bool shouldRecover = ticket.transferState == QLatin1String("sending")
                || ticket.transferState == QLatin1String("failed");
        if (!shouldRecover) {
            continue;
        }
        if (ticket.returnState == QLatin1String("return-success")
                || ticket.returnState == QLatin1String("return-requesting-log")
                || ticket.returnState == QLatin1String("return-uploading")) {
            continue;
        }

        m_ticketStore->updateTransferState(ticket.taskId, QStringLiteral("auto-pending"));
        emit statusMessage(QStringLiteral("钥匙中未检测到系统票，已将任务重新加入自动传票队列：%1")
                               .arg(ticket.taskId));
        requeued = true;
    }

    if (requeued) {
        tryAutoTransferPendingTicket();
    }
}

void KeyManageController::tryAutoCleanupReturnedTicket(const QList<KeyTaskDto> &tasks)
{
    if (!m_activeReturnTaskId.isEmpty()) {
        return;
    }
    if (!m_pendingDeletedSystemTicketId.isEmpty()) {
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected || !snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        return;
    }

    for (const KeyTaskDto &task : tasks) {
        if (task.status != 0x02) {
            continue;
        }

        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (!ticket.valid) {
            continue;
        }
        if (ticket.returnState != QLatin1String("return-success")) {
            continue;
        }

        m_pendingDeletedSystemTicketId = taskId;
        m_pendingDeletedKeyTaskRaw = task.taskId;
        m_pendingDeleteAllowsRetransfer = false;

        CommandRequest req;
        req.id = CommandId::DeleteTask;
        req.opId = nextOpId();
        req.payload = task.taskId;
        m_session->execute(req);
        emit statusMessage(QStringLiteral("检测到回传已成功但钥匙任务仍存在，自动继续清理钥匙任务：%1")
                               .arg(taskId));
        return;
    }
}

void KeyManageController::tryAutoRefreshKeyTasksOnReady(const KeySessionSnapshot &snapshot)
{
    const bool ready = snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
    const bool becameReady = (!m_lastReadyState && ready);
    m_lastReadyState = ready;

    if (!becameReady) {
        return;
    }

    if (!m_activeTransferTaskId.isEmpty()) {
        return;
    }

    emit statusMessage(QStringLiteral("钥匙已就绪，自动读取钥匙票列表以检查待回传任务"));
    onQueryTasksClicked();
}

void KeyManageController::tryStartTicketReturn(const QString &taskId, bool automatic)
{
    if (taskId.trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("回传失败：taskId 为空"));
        return;
    }

    if (!m_activeReturnTaskId.isEmpty()) {
        if (!automatic) {
            emit statusMessage(QStringLiteral("已有系统票正在回传中，请稍后再试"));
        }
        return;
    }

    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    if (!ticket.valid) {
        emit statusMessage(QStringLiteral("回传失败：系统票不存在"));
        return;
    }
    if (ticket.transferState != QLatin1String("success")) {
        emit statusMessage(QStringLiteral("回传失败：该系统票尚未完成传票到钥匙"));
        return;
    }
    if (ticket.returnState == QLatin1String("return-success")) {
        if (!automatic) {
            emit statusMessage(QStringLiteral("该系统票已完成回传，本次不重复上传"));
        }
        return;
    }
    if (!m_ticketReturnClient->isConfigured()) {
        emit statusMessage(QStringLiteral("回传接口未配置，暂不发起回传"));
        return;
    }

    quint8 keyTaskStatus = 0;
    const QByteArray taskIdRaw = findKeyTaskIdRaw(taskId, &keyTaskStatus);
    if (taskIdRaw.isEmpty()) {
        if (!automatic) {
            emit statusMessage(QStringLiteral("钥匙内未找到该系统票，请先读取钥匙票列表"));
        }
        return;
    }
    if (keyTaskStatus != 0x02) {
        if (!automatic) {
            emit statusMessage(QStringLiteral("该系统票在钥匙中的状态未完成，暂不回传"));
        }
        return;
    }

    m_activeReturnTaskId = taskId;
    m_activeReturnTaskIdRaw = taskIdRaw;
    m_pendingReturnHandshakeTaskId = taskId;
    m_pendingReturnHandshakeTaskIdRaw = taskIdRaw;
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-requesting-log"));

    CommandRequest req;
    req.id = CommandId::SetCom;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral("%1回传前重新握手：%2")
                           .arg(automatic ? QStringLiteral("自动") : QStringLiteral("执行"))
                           .arg(taskId));
}

void KeyManageController::tryAutoReturnCompletedTicket(const QList<KeyTaskDto> &tasks)
{
    if (!m_activeReturnTaskId.isEmpty()) {
        return;
    }
    if (!m_ticketReturnClient->isConfigured()) {
        return;
    }

    for (const KeyTaskDto &task : tasks) {
        if (task.status != 0x02) {
            continue;
        }

        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (!ticket.valid) {
            continue;
        }
        if (ticket.transferState != QLatin1String("success")) {
            continue;
        }
        if (ticket.returnState == QLatin1String("return-success")
                || ticket.returnState == QLatin1String("return-uploading")
                || ticket.returnState == QLatin1String("return-requesting-log")
                || ticket.returnState == QLatin1String("return-failed")) {
            continue;
        }

        tryStartTicketReturn(taskId, true);
        return;
    }

    for (const KeyTaskDto &task : tasks) {
        if (task.status != 0x00 && task.status != 0x01) {
            continue;
        }

        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (!ticket.valid) {
            continue;
        }
        if (ticket.transferState != QLatin1String("success")) {
            continue;
        }
        if (ticket.returnState == QLatin1String("return-success")
                || ticket.returnState == QLatin1String("return-uploading")
                || ticket.returnState == QLatin1String("return-requesting-log")) {
            continue;
        }

        emit statusMessage(QStringLiteral("任务未全部完成，暂不回传，请完成全部步骤后再放回钥匙"));
        return;
    }
}

QByteArray KeyManageController::findKeyTaskIdRaw(const QString &taskId, quint8 *status) const
{
    for (const KeyTaskDto &task : m_lastKeyTasks) {
        if (taskIdFromRaw(task.taskId) == taskId) {
            if (status) {
                *status = task.status;
            }
            return task.taskId;
        }
    }
    if (status) {
        *status = 0xFF;
    }
    return QByteArray();
}

QString KeyManageController::taskIdFromRaw(const QByteArray &taskIdRaw)
{
    return rawTaskIdToDecimalString(taskIdRaw);
}

void KeyManageController::tryStartTicketTransfer(const QString &taskId, bool automatic)
{
    if (taskId.trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("传票发送失败：taskId 为空"));
        return;
    }

    if (!m_activeTransferTaskId.isEmpty()) {
        if (!automatic) {
            emit statusMessage(QStringLiteral("已有传票发送中，请稍后再试"));
        }
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);

    if (ticket.valid && ticket.transferState == QLatin1String("success")) {
        emit statusMessage(QStringLiteral("该系统票已传票到钥匙，本次不重复下发"));
        return;
    }
    if (ticket.valid && ticket.transferState == QLatin1String("sending")) {
        emit statusMessage(QStringLiteral("该系统票正在传票中，请稍后再试"));
        return;
    }

    if (!snapshot.connected) {
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(QStringLiteral("系统票已入池，等待串口连接后自动传票"));
        } else {
            emit statusMessage(QStringLiteral("串口未连接，无法发送传票"));
        }
        return;
    }

    if (!snapshot.keyPresent || !snapshot.keyStable) {
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(QStringLiteral("系统票已入池，等待钥匙就绪后自动传票"));
        } else {
            emit statusMessage(QStringLiteral("钥匙未就绪，无法发送传票"));
        }
        return;
    }

    if (!snapshot.sessionReady) {
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(QStringLiteral("系统票已入池，等待握手完成后自动传票"));
        } else {
            emit statusMessage(QStringLiteral("钥匙通讯会话未就绪，无法发送传票"));
        }
        return;
    }

    const QByteArray jsonBytes = m_ticketStore->rawJsonById(taskId);
    if (jsonBytes.isEmpty()) {
        m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"),
                                           QStringLiteral("raw json missing"));
        emit statusMessage(QStringLiteral("选中系统票缺少原始 JSON，无法发送传票"));
        return;
    }

    TicketTransferRequest request;
    request.jsonBytes = jsonBytes;
    request.opId = nextOpId();
    request.debugFrameChunkSize = ConfigManager::instance()
            ->value("ticket/debugFrameChunkSize", 0).toInt();
    m_activeTransferTaskId = taskId;
    m_ticketStore->updateTransferState(taskId, QStringLiteral("sending"));
    m_session->transferTicket(request);
    emit statusMessage(QStringLiteral("%1传票发送：%2")
                           .arg(automatic ? QStringLiteral("自动") : QStringLiteral("执行"))
                           .arg(taskId));
}

void KeyManageController::tryAutoTransferPendingTicket()
{
    if (!autoTransferEnabled()) {
        return;
    }

    if (!m_activeTransferTaskId.isEmpty()) {
        return;
    }

    const QList<SystemTicketDto> tickets = systemTickets();
    for (const SystemTicketDto &ticket : tickets) {
    if (ticket.transferState == QLatin1String("received")
                || ticket.transferState == QLatin1String("auto-pending")) {
            emit statusMessage(QStringLiteral("发现可自动传票任务：%1").arg(ticket.taskId));
            tryStartTicketTransfer(ticket.taskId, true);
            return;
        }
    }
}

void KeyManageController::tryAutoConnectKeyPort()
{
    const int cfgBaud = ConfigManager::instance()->baudRate();
    const int baud = (cfgBaud > 0) ? cfgBaud : 115200;
    const QString configPort = ConfigManager::instance()->keySerialPort().trimmed();

    const KeySessionSnapshot snap = m_session->snapshot();
    if (!snap.verifiedPortName.isEmpty()) {
        if (m_session->connectPort(snap.verifiedPortName, baud)) {
            emit statusMessage(QStringLiteral("串口已连接(已验证端口): %1").arg(snap.verifiedPortName));
            emit sessionSnapshotChanged(m_session->snapshot());
            return;
        }
    }

    QStringList candidates;
    if (!configPort.isEmpty()) {
        candidates << configPort;
    }

    if (candidates.isEmpty()) {
        emit statusMessage(QStringLiteral("未检测到可用串口"));
        emit sessionSnapshotChanged(m_session->snapshot());
        return;
    }

    for (const QString &port : candidates) {
        if (m_session->connectPort(port, baud)) {
            ConfigManager::instance()->setKeySerialPort(port);
            emit statusMessage(QStringLiteral("串口已连接: %1").arg(port));
            emit sessionSnapshotChanged(m_session->snapshot());
            return;
        }
    }

    emit statusMessage(QStringLiteral("串口连接失败，请检查底座或配置"));
    emit sessionSnapshotChanged(m_session->snapshot());
}

int KeyManageController::nextOpId()
{
    return m_nextOpId++;
}

QList<KeyTaskDto> KeyManageController::toTaskDtos(const QVariantList &taskList) const
{
    QList<KeyTaskDto> tasks;
    tasks.reserve(taskList.size());

    for (const QVariant &v : taskList) {
        const QVariantMap item = v.toMap();
        KeyTaskDto task;
        task.taskId = item.value("taskId").toByteArray();
        task.status = static_cast<quint8>(item.value("status").toInt());
        task.type = static_cast<quint8>(item.value("type").toInt());
        task.source = static_cast<quint8>(item.value("source").toInt());
        task.reserved = static_cast<quint8>(item.value("reserved").toInt());
        tasks.append(task);
    }

    return tasks;
}
