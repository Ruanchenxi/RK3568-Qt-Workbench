/**
 * @file KeyManageController.cpp
 * @brief 钥匙管理页控制器实现
 */
#include "features/key/application/KeyManageController.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QStringList>
#include <QTimer>

#include "core/ConfigManager.h"
#include "features/key/application/KeySessionService.h"
#include "features/key/application/KeyProvisioningService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketIngressService.h"
#include "features/key/application/TicketReturnHttpClient.h"
#include "features/key/protocol/KeyProtocolDefs.h"

namespace {

int resolveBusinessStationNo()
{
    ConfigManager *config = ConfigManager::instance();
    bool ok = false;
    const int stationNo = config->stationId().trimmed().toInt(&ok);
    if (ok && stationNo >= 0) {
        return stationNo;
    }

    const int legacyStationNo = config->value(QStringLiteral("key/backendStationNo"), 0).toInt(&ok);
    return (ok && legacyStationNo >= 0) ? legacyStationNo : 0;
}

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

bool isSessionReadySnapshot(const KeySessionSnapshot &snapshot)
{
    return snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
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
    m_ticketIngress->setIngressHandler(
            [this](const QByteArray &jsonBytes,
                   const QString &savedPath) {
        TicketIngressService::IngressResult result;
        QString taskId;
        QString resultMessage;
        result.accepted = ingestWorkbenchJson(jsonBytes, savedPath, &taskId, &resultMessage);
        result.statusCode = result.accepted ? 200 : 400;
        result.message = result.accepted
                ? (resultMessage.isEmpty() ? QStringLiteral("ticket accepted") : resultMessage)
                : (resultMessage.isEmpty() ? QStringLiteral("ticket ingest failed") : resultMessage);
        result.taskId = taskId;
        return result;
    });
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

    const int stationNo = resolveBusinessStationNo();
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

    const int stationNo = resolveBusinessStationNo();
    emit statusMessage(QStringLiteral("下载 RFID，正在获取后端数据..."));
    m_keyProvisioning->requestRfidPayload(stationNo);
}

void KeyManageController::onQueryBatteryClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法查询钥匙电量"));
        return;
    }

    CommandRequest req;
    req.id = CommandId::QueryBattery;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral("执行获取电量 (Q_KEYEQ)"));
}

void KeyManageController::onSyncDeviceTimeClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法执行钥匙校时"));
        return;
    }

    CommandRequest req;
    req.id = CommandId::SyncDeviceTime;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral("执行钥匙校时 (SET_TIME)"));
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
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] onGetSystemTicketListClicked count=%1 selectedTaskId=%2")
                  .arg(QString::number(systemTickets().size()),
                       m_selectedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_selectedSystemTicketId);
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
            if (!m_pendingDeletedSystemTicketId.isEmpty()) {
                m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                                 QStringLiteral("return-delete-verifying"),
                                                 QStringLiteral("DEL 超时，等待后续 Q_TASK 对账确认钥匙任务是否已删除"));
                m_pendingDeleteSessionId = 0;
                emit statusMessage(QStringLiteral("DEL 超时，保留删除验证意图，等待后续 Q_TASK 继续对账"));
            }
        }
        if ((!m_activeReturnTaskId.isEmpty() || !m_pendingReturnHandshakeTaskId.isEmpty())
                && (what.contains(QStringLiteral("SET_COM"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("I_TASK_LOG"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("0X05"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("UP_TASK_LOG"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("0X15"), Qt::CaseInsensitive))) {
            failActiveReturnHandshake(what);
        }
        emit timeoutOccurred(what);
        emit statusMessage(what);
        break;
    }
    case KeySessionEvent::Ack:
        if (static_cast<quint8>(event.data.value("ackedCmd").toInt()) == KeyProtocol::CmdSetCom
                && !m_pendingReturnHandshakeTaskId.isEmpty()
                && !m_pendingReturnHandshakeTaskIdRaw.isEmpty()
                && isCurrentKeySession(m_activeReturnSessionId)) {
            m_ticketStore->updateReturnState(m_pendingReturnHandshakeTaskId,
                                             QStringLiteral("return-log-requesting"));
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
        if (static_cast<quint8>(event.data.value("ackedCmd").toInt()) == KeyProtocol::CmdITaskLog
                && !m_activeReturnTaskId.isEmpty()
                && isCurrentKeySession(m_activeReturnSessionId)) {
            m_ticketStore->updateReturnState(m_activeReturnTaskId,
                                             QStringLiteral("return-log-receiving"));
        }
        emit ackReceived(static_cast<quint8>(event.data.value("ackedCmd").toInt()));
        break;
    case KeySessionEvent::Nak:
        if (static_cast<quint8>(event.data.value("origCmd").toInt()) == KeyProtocol::CmdDel) {
            if (!m_pendingDeletedSystemTicketId.isEmpty()) {
                m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                                 QStringLiteral("return-delete-verifying"),
                                                 QStringLiteral("DEL 收到 NAK，等待后续 Q_TASK 对账确认钥匙任务状态"));
                m_pendingDeleteSessionId = 0;
            }
        }
        if (!m_activeReturnTaskId.isEmpty() || !m_pendingReturnHandshakeTaskId.isEmpty()) {
            const quint8 origCmd = static_cast<quint8>(event.data.value("origCmd").toInt());
            if (origCmd == KeyProtocol::CmdSetCom
                    || origCmd == KeyProtocol::CmdITaskLog
                    || origCmd == KeyProtocol::CmdUpTaskLog) {
                const QString reason = QStringLiteral("收到 NAK，origCmd=0x%1")
                        .arg(origCmd, 2, 16, QLatin1Char('0'))
                        .toUpper();
                failActiveReturnHandshake(reason);
            }
        }
        emit nakReceived(static_cast<quint8>(event.data.value("origCmd").toInt()),
                         static_cast<quint8>(event.data.value("errCode").toInt()));
        break;
    case KeySessionEvent::TasksUpdated: {
        const QList<KeyTaskDto> tasks = toTaskDtos(event.data.value("tasks").toList());
        const KeySessionSnapshot snapshot = m_session->snapshot();
        const bool ready = isSessionReadySnapshot(snapshot);
        m_lastKeyTasks = tasks;
        if (ready) {
            reconcileSystemTicketsFromKeyTasks(tasks);
            recoverAutoTransferWhenKeyEmpty(tasks);
            tryAutoCleanupReturnedTicket(tasks);
        } else {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] TasksUpdated arrived while session not ready, skip auto actions currentSession=%1")
                          .arg(QString::number(m_keySessionId));
        }
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
        if (ready && !m_pendingDeletedSystemTicketId.isEmpty()) {
            finalizePendingReturnDelete(tasks);
        }
        if (ready) {
            tryAutoReturnCompletedTicket(tasks);
        }
        if (!m_selectedSystemTicketId.isEmpty()) {
            emit selectedSystemTicketChanged(m_ticketStore->ticketById(m_selectedSystemTicketId));
        }
        if (ready && (!tasks.isEmpty() || m_activeTransferTaskId.isEmpty())) {
            tryAutoTransferPendingTicket();
        }
        break;
    }
    case KeySessionEvent::TaskLogReady: {
        if (m_activeReturnTaskId.isEmpty() || !isCurrentKeySession(m_activeReturnSessionId)) {
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
        if (m_activeTransferTaskId.isEmpty() || !isCurrentKeySession(m_activeTransferSessionId)) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore stale TicketTransferProgress activeTransfer=%1 activeTransferSession=%2 currentSession=%3")
                          .arg(m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                               QString::number(m_activeTransferSessionId),
                               QString::number(m_keySessionId));
            break;
        }
        const int frameIndex = event.data.value("frameIndex").toInt();
        const int totalFrames = event.data.value("totalFrames").toInt();
        if (!m_activeTransferTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(m_activeTransferTaskId, QStringLiteral("sending"));
        }
        emit statusMessage(QStringLiteral("传票发送中：第 %1/%2 帧").arg(frameIndex).arg(totalFrames));
        break;
    }
    case KeySessionEvent::TicketTransferFinished:
        if (m_activeTransferTaskId.isEmpty() || !isCurrentKeySession(m_activeTransferSessionId)) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore stale TicketTransferFinished activeTransfer=%1 activeTransferSession=%2 currentSession=%3")
                          .arg(m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                               QString::number(m_activeTransferSessionId),
                               QString::number(m_keySessionId));
            break;
        }
        if (!m_activeTransferTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(m_activeTransferTaskId, QStringLiteral("success"));
        }
        m_activeTransferTaskId.clear();
        m_activeTransferSessionId = 0;
        emit statusMessage(QStringLiteral("传票发送完成"));
        tryAutoTransferPendingTicket();
        break;
    case KeySessionEvent::TicketTransferFailed: {
        const QString what = event.data.value("what").toString();
        if (m_activeTransferTaskId.isEmpty() || !isCurrentKeySession(m_activeTransferSessionId)) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore stale TicketTransferFailed reason=%1 activeTransfer=%2 activeTransferSession=%3 currentSession=%4")
                          .arg(what,
                               m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                               QString::number(m_activeTransferSessionId),
                               QString::number(m_keySessionId));
            break;
        }

        const QString failedTaskId = m_activeTransferTaskId;
        const bool inflightBusy = what.contains(QStringLiteral("当前有命令在途"));
        if (inflightBusy) {
            m_ticketStore->updateTransferState(failedTaskId,
                                               QStringLiteral("auto-pending"),
                                               QStringLiteral("底层仍有命令在途，等待当前命令完成后自动重试"));
            m_activeTransferTaskId.clear();
            m_activeTransferSessionId = 0;
            emit statusMessage(QStringLiteral("传票暂未发送：当前仍有命令在途，已回到自动等待队列：%1")
                                   .arg(failedTaskId));
            scheduleAutoTransferRetry(QStringLiteral("底层命令在途"));
            break;
        }

        if (!m_activeTransferTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(m_activeTransferTaskId, QStringLiteral("failed"), what);
        }
        m_activeTransferTaskId.clear();
        m_activeTransferSessionId = 0;
        emit statusMessage(QStringLiteral("传票发送失败：%1").arg(what));
        break;
    }
    case KeySessionEvent::KeyStateChanged: {
        const KeySessionSnapshot snapshot = m_session->snapshot();
        const bool ready = snapshot.connected
                && snapshot.keyPresent
                && snapshot.keyStable
                && snapshot.sessionReady;
        const bool linkInterrupted = !snapshot.connected || !snapshot.keyPresent;
        if (linkInterrupted) {
            invalidateCurrentKeySession(!snapshot.connected
                    ? QStringLiteral("串口断开")
                    : QStringLiteral("钥匙移除"));
            if (!m_activeTransferTaskId.isEmpty()) {
                const QString reason = !snapshot.connected
                        ? QStringLiteral("串口已断开，当前传票已中断，请重新连接后重试")
                        : QStringLiteral("钥匙已移除，当前传票已中断，请重新放回钥匙后重试");
                m_ticketStore->updateTransferState(m_activeTransferTaskId,
                                                   QStringLiteral("failed"),
                                                   reason);
                m_activeTransferTaskId.clear();
                m_activeTransferSessionId = 0;
                emit statusMessage(reason);
            }

            if (!m_activeReturnTaskId.isEmpty() || !m_pendingReturnHandshakeTaskId.isEmpty()) {
                const QString reason = !snapshot.connected
                        ? QStringLiteral("串口已断开，当前回传已中断，请重新连接后重试")
                        : QStringLiteral("钥匙已移除，当前回传已中断，请重新放回钥匙后重试");
                failActiveReturnHandshake(reason);
                emit statusMessage(reason);
            }

            if (!m_pendingDeletedSystemTicketId.isEmpty()) {
                const QString taskId = m_pendingDeletedSystemTicketId;
                m_ticketStore->updateReturnState(taskId,
                                                 QStringLiteral("return-delete-verifying"),
                                                 QStringLiteral("钥匙已移除，删除验证已中断，等待重新放回后继续对账"));
                m_pendingDeleteSessionId = 0;
                emit statusMessage(QStringLiteral("钥匙已移除，任务 %1 的钥匙清理验证已中断；重新放回后将继续检查")
                                       .arg(taskId));
            }
        }

        if (ready && !m_lastReadyState) {
            openNewKeySession();
        }

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
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] handleSystemTicketsChanged count=%1 selectedTaskId=%2 activeTransfer=%3 activeReturn=%4 pendingDelete=%5")
                  .arg(QString::number(tickets.size()),
                       m_selectedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_selectedSystemTicketId,
                       m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                       m_activeReturnTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeReturnTaskId,
                       m_pendingDeletedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_pendingDeletedSystemTicketId);
    emit statusMessage(QStringLiteral("系统票列表已刷新，当前票数=%1").arg(tickets.size()));
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
    QString taskId;
    QString resultMessage;
    ingestWorkbenchJson(jsonBytes, savedPath, &taskId, &resultMessage);
}

bool KeyManageController::ingestWorkbenchJson(const QByteArray &jsonBytes,
                                              const QString &savedPath,
                                              QString *taskIdOut,
                                              QString *resultMessage)
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
    if (taskIdOut) {
        *taskIdOut = taskId;
    }
    const SystemTicketDto oldTicket = m_ticketStore->ticketById(taskId);

    qInfo().noquote()
            << QStringLiteral("[KeyManageController] ingestWorkbenchJson begin taskId=%1 payloadBytes=%2 savedPath=%3 oldValid=%4 oldTransfer=%5 oldReturn=%6")
                  .arg(taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                       QString::number(jsonBytes.size()),
                       savedPath,
                       oldTicket.valid ? QStringLiteral("true") : QStringLiteral("false"),
                       oldTicket.valid ? oldTicket.transferState : QStringLiteral("<none>"),
                       oldTicket.valid ? oldTicket.returnState : QStringLiteral("<none>"));

    QString ingestError;
    if (!m_ticketStore->ingestJson(jsonBytes, savedPath, &ingestError)) {
        if (resultMessage) {
            *resultMessage = ingestError;
        }
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] ingestWorkbenchJson failed taskId=%1 reason=%2")
                      .arg(taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                           ingestError);
        emit statusMessage(QStringLiteral("系统票入池失败：%1").arg(ingestError));
        return false;
    }
    if (resultMessage) {
        resultMessage->clear();
    }

    qInfo().noquote()
            << QStringLiteral("[KeyManageController] ingestWorkbenchJson accepted taskId=%1 autoTransfer=%2")
                  .arg(taskId,
                       autoTransferEnabled() ? QStringLiteral("true") : QStringLiteral("false"));
    emit statusMessage(QStringLiteral("系统票入池成功：taskId=%1，当前状态=received").arg(taskId));

    if (oldTicket.valid) {
        if (oldTicket.transferState == QLatin1String("success")) {
            if (resultMessage) {
                *resultMessage = QStringLiteral("ticket already transferred, duplicate ignored");
            }
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ingestWorkbenchJson duplicate success taskId=%1 ignored=true")
                          .arg(taskId);
            emit statusMessage(QStringLiteral("系统票已存在且已传票到钥匙，本次重复请求已忽略"));
            return true;
        }
        if (oldTicket.transferState == QLatin1String("sending")) {
            if (resultMessage) {
                *resultMessage = QStringLiteral("ticket is already sending, duplicate ignored");
            }
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ingestWorkbenchJson duplicate sending taskId=%1 ignored=true")
                          .arg(taskId);
            emit statusMessage(QStringLiteral("系统票正在传票到钥匙，本次重复请求已忽略"));
            return true;
        }
        if (oldTicket.transferState == QLatin1String("key-cleared")) {
            if (autoTransferEnabled()) {
                if (resultMessage) {
                    *resultMessage = QStringLiteral("ticket accepted, key task cleared, auto retransfer scheduled");
                }
                qInfo().noquote()
                        << QStringLiteral("[KeyManageController] ingestWorkbenchJson key-cleared taskId=%1 action=auto-retransfer")
                              .arg(taskId);
                emit statusMessage(QStringLiteral("系统票对应钥匙任务已删除，准备再次自动传票"));
                tryStartTicketTransfer(taskId, true);
            } else {
                if (resultMessage) {
                    *resultMessage = QStringLiteral("ticket accepted, key task cleared, manual retransfer allowed");
                }
                qInfo().noquote()
                        << QStringLiteral("[KeyManageController] ingestWorkbenchJson key-cleared taskId=%1 action=manual-retransfer")
                              .arg(taskId);
                emit statusMessage(QStringLiteral("系统票对应钥匙任务已删除，可手动再次传票"));
            }
            return true;
        }
    }

    if (autoTransferEnabled()) {
        if (resultMessage) {
            *resultMessage = QStringLiteral("ticket accepted, queued for auto transfer");
        }
        emit statusMessage(QStringLiteral("系统票已接收并入池，准备自动传票"));
    } else {
        if (resultMessage) {
            *resultMessage = QStringLiteral("ticket accepted, auto transfer disabled");
        }
        emit statusMessage(QStringLiteral("系统票已接收并入池（自动传票已关闭）"));
    }
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] ingestWorkbenchJson queue-auto-transfer taskId=%1")
                  .arg(taskId);
    tryAutoTransferPendingTicket();
    return true;
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
    emit statusMessage(QStringLiteral("回传上传成功：%1").arg(taskId));

    if (!m_activeReturnTaskIdRaw.isEmpty() && isCurrentKeySession(m_activeReturnSessionId)) {
        markReturnDeletePending(taskId, m_activeReturnTaskIdRaw);

        CommandRequest req;
        req.id = CommandId::DeleteTask;
        req.opId = nextOpId();
        req.payload = m_activeReturnTaskIdRaw;
        m_session->execute(req);
        emit statusMessage(QStringLiteral("回传上传成功，开始删除钥匙中的已完成任务"));
    } else {
        const QString reason = QStringLiteral("回传上传成功，但缺少钥匙任务原始ID，无法自动删除钥匙任务");
        m_ticketStore->updateReturnState(taskId, QStringLiteral("manual-required"), reason);
        emit statusMessage(reason);
    }

    clearActiveReturnContext();
}

void KeyManageController::handleReturnUploadFailed(const QString &taskId, const QString &reason)
{
    m_ticketStore->updateReturnState(taskId, QStringLiteral("manual-required"), reason);
    emit statusMessage(QStringLiteral("回传上传失败：%1").arg(reason));
    clearActiveReturnContext();
}

void KeyManageController::failActiveReturnHandshake(const QString &reason)
{
    const QString taskId = !m_activeReturnTaskId.isEmpty()
            ? m_activeReturnTaskId
            : m_pendingReturnHandshakeTaskId;
    if (!taskId.isEmpty()) {
        m_ticketStore->updateReturnState(taskId,
                                         QStringLiteral("return-interrupted-retryable"),
                                         reason);
    }
    clearActiveReturnContext();
}

void KeyManageController::clearActiveReturnContext(bool clearHandshakeOnly)
{
    if (!clearHandshakeOnly) {
        m_activeReturnTaskId.clear();
        m_activeReturnTaskIdRaw.clear();
        m_activeReturnSessionId = 0;
    }
    m_pendingReturnHandshakeTaskId.clear();
    m_pendingReturnHandshakeTaskIdRaw.clear();
}

void KeyManageController::markReturnDeletePending(const QString &taskId, const QByteArray &taskIdRaw)
{
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-upload-success"));
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-delete-verifying"));
    m_pendingDeletedSystemTicketId = taskId;
    m_pendingDeletedKeyTaskRaw = taskIdRaw;
    m_pendingDeleteAllowsRetransfer = false;
    m_pendingDeleteSessionId = m_activeReturnSessionId;
}

void KeyManageController::finalizePendingReturnDelete(const QList<KeyTaskDto> &tasks)
{
    if (m_pendingDeleteSessionId > 0 && !isCurrentKeySession(m_pendingDeleteSessionId)) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] finalizePendingReturnDelete skipped stale session pendingDeleteSession=%1 currentSession=%2 taskId=%3")
                      .arg(QString::number(m_pendingDeleteSessionId),
                           QString::number(m_keySessionId),
                           m_pendingDeletedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_pendingDeletedSystemTicketId);
        return;
    }

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
            const SystemTicketDto ticket = m_ticketStore->ticketById(m_pendingDeletedSystemTicketId);
            if (ticket.valid && (ticket.returnState == QLatin1String("return-delete-verifying")
                                 || ticket.returnState == QLatin1String("return-delete-pending")
                                 || ticket.returnState == QLatin1String("return-upload-success"))) {
                m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                                 QStringLiteral("return-delete-success"));
                emit statusMessage(QStringLiteral("回传完成，钥匙任务已清理：%1")
                                       .arg(m_pendingDeletedSystemTicketId));
            } else {
                emit statusMessage(QStringLiteral("钥匙任务已删除"));
            }
        }
        m_pendingDeletedSystemTicketId.clear();
        m_pendingDeletedKeyTaskRaw.clear();
        m_pendingDeleteAllowsRetransfer = false;
        m_pendingDeleteSessionId = 0;
    } else {
        m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                         QStringLiteral("return-delete-verifying"),
                                         QStringLiteral("删除后验证发现钥匙任务仍存在，等待后续自动清理"));
        emit statusMessage(QStringLiteral("删除后验证发现钥匙任务仍存在，请继续等待自动清理"));
    }
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
                || ticket.returnState == QLatin1String("return-upload-success")
                || ticket.returnState == QLatin1String("return-delete-pending")
                || ticket.returnState == QLatin1String("return-delete-verifying")
                || ticket.returnState == QLatin1String("return-delete-success")
                || ticket.returnState == QLatin1String("return-handshake")
                || ticket.returnState == QLatin1String("return-log-requesting")
                || ticket.returnState == QLatin1String("return-log-receiving")
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
        if (ticket.returnState != QLatin1String("return-upload-success")
                && ticket.returnState != QLatin1String("return-delete-pending")
                && ticket.returnState != QLatin1String("return-delete-verifying")) {
            continue;
        }

        m_pendingDeletedSystemTicketId = taskId;
        m_pendingDeletedKeyTaskRaw = task.taskId;
        m_pendingDeleteAllowsRetransfer = false;
        m_pendingDeleteSessionId = m_keySessionId;

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

    if (!m_activeTransferTaskId.isEmpty() || m_autoQuerySessionId == m_keySessionId) {
        return;
    }

    m_autoQuerySessionId = m_keySessionId;
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
    if (ticket.returnState == QLatin1String("return-success")
            || ticket.returnState == QLatin1String("return-upload-success")
            || ticket.returnState == QLatin1String("return-delete-pending")
            || ticket.returnState == QLatin1String("return-delete-verifying")
            || ticket.returnState == QLatin1String("return-delete-success")) {
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

    QString blockingTaskId;
    if (hasBlockingIncompleteKeyTask(m_lastKeyTasks, &blockingTaskId, taskIdRaw)) {
        const QString message = blockingTaskId.isEmpty()
                ? QStringLiteral("钥匙中仍有未完成任务，必须全部任务完成后才允许回传")
                : QStringLiteral("钥匙中任务 %1 尚未完成，必须全部任务完成后才允许回传")
                      .arg(blockingTaskId);
        emit statusMessage(message);
        return;
    }

    m_activeReturnTaskId = taskId;
    m_activeReturnTaskIdRaw = taskIdRaw;
    m_activeReturnSessionId = m_keySessionId;
    m_pendingReturnHandshakeTaskId = taskId;
    m_pendingReturnHandshakeTaskIdRaw = taskIdRaw;
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-handshake"));

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

    QString blockingTaskId;
    if (hasBlockingIncompleteKeyTask(tasks, &blockingTaskId)) {
        const QString message = blockingTaskId.isEmpty()
                ? QStringLiteral("钥匙中仍有未完成任务，暂不回传，请全部任务完成后再放回钥匙")
                : QStringLiteral("钥匙中任务 %1 尚未完成，暂不回传，请全部任务完成后再放回钥匙")
                      .arg(blockingTaskId);
        emit statusMessage(message);
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
                || ticket.returnState == QLatin1String("return-upload-success")
                || ticket.returnState == QLatin1String("return-delete-pending")
                || ticket.returnState == QLatin1String("return-delete-verifying")
                || ticket.returnState == QLatin1String("return-delete-success")
                || ticket.returnState == QLatin1String("return-uploading")
                || ticket.returnState == QLatin1String("return-handshake")
                || ticket.returnState == QLatin1String("return-log-requesting")
                || ticket.returnState == QLatin1String("return-log-receiving")
                || ticket.returnState == QLatin1String("manual-required")
                || ticket.returnState == QLatin1String("upload_uncertain")
                || ticket.returnState == QLatin1String("return-failed")) {
            continue;
        }

        tryStartTicketReturn(taskId, true);
        return;
    }
}

bool KeyManageController::hasBlockingIncompleteKeyTask(const QList<KeyTaskDto> &tasks,
                                                       QString *blockingTaskId,
                                                       const QByteArray &excludeTaskIdRaw) const
{
    for (const KeyTaskDto &task : tasks) {
        if (!excludeTaskIdRaw.isEmpty() && task.taskId == excludeTaskIdRaw) {
            continue;
        }
        if (task.status == 0x02) {
            continue;
        }
        if (blockingTaskId) {
            *blockingTaskId = taskIdFromRaw(task.taskId);
        }
        return true;
    }

    if (blockingTaskId) {
        blockingTaskId->clear();
    }
    return false;
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
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked reason=empty-taskId automatic=%1")
                      .arg(automatic ? QStringLiteral("true") : QStringLiteral("false"));
        emit statusMessage(QStringLiteral("传票发送失败：taskId 为空"));
        return;
    }

    if (!m_activeTransferTaskId.isEmpty()) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=active-transfer activeTransfer=%2 automatic=%3")
                      .arg(taskId,
                           m_activeTransferTaskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (!automatic) {
            emit statusMessage(QStringLiteral("已有传票发送中，请稍后再试"));
        }
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);

    if (ticket.valid && ticket.transferState == QLatin1String("success")) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=already-success automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        emit statusMessage(QStringLiteral("该系统票已传票到钥匙，本次不重复下发"));
        return;
    }
    if (ticket.valid && ticket.transferState == QLatin1String("sending")) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=already-sending automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        emit statusMessage(QStringLiteral("该系统票正在传票中，请稍后再试"));
        return;
    }

    if (!snapshot.connected) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=serial-disconnected automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(QStringLiteral("系统票已入池，等待串口连接后自动传票"));
        } else {
            emit statusMessage(QStringLiteral("串口未连接，无法发送传票"));
        }
        return;
    }

    if (!snapshot.keyPresent || !snapshot.keyStable) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=key-not-ready keyPresent=%2 keyStable=%3 automatic=%4")
                      .arg(taskId,
                           snapshot.keyPresent ? QStringLiteral("true") : QStringLiteral("false"),
                           snapshot.keyStable ? QStringLiteral("true") : QStringLiteral("false"),
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(QStringLiteral("系统票已入池，等待钥匙就绪后自动传票"));
        } else {
            emit statusMessage(QStringLiteral("钥匙未就绪，无法发送传票"));
        }
        return;
    }

    if (!snapshot.sessionReady) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=session-not-ready automatic=%2 currentSession=%3")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"),
                           QString::number(m_keySessionId));
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
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=raw-json-missing automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
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
    m_activeTransferSessionId = m_keySessionId;
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] tryStartTicketTransfer execute taskId=%1 automatic=%2 session=%3 payloadBytes=%4")
                  .arg(taskId,
                       automatic ? QStringLiteral("true") : QStringLiteral("false"),
                       QString::number(m_keySessionId),
                       QString::number(jsonBytes.size()));
    m_ticketStore->updateTransferState(taskId, QStringLiteral("sending"));
    m_session->transferTicket(request);
    emit statusMessage(QStringLiteral("%1传票发送：%2")
                           .arg(automatic ? QStringLiteral("自动") : QStringLiteral("执行"))
                           .arg(taskId));
}

void KeyManageController::tryAutoTransferPendingTicket()
{
    if (!autoTransferEnabled()) {
        qInfo().noquote() << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket skipped reason=auto-transfer-disabled");
        return;
    }

    if (!m_activeTransferTaskId.isEmpty()
            || !m_activeReturnTaskId.isEmpty()
            || !m_pendingReturnHandshakeTaskId.isEmpty()
            || !m_pendingDeletedSystemTicketId.isEmpty()) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket skipped reason=busy activeTransfer=%1 activeReturn=%2 pendingHandshake=%3 pendingDelete=%4")
                      .arg(m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                           m_activeReturnTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeReturnTaskId,
                           m_pendingReturnHandshakeTaskId.isEmpty() ? QStringLiteral("<none>") : m_pendingReturnHandshakeTaskId,
                           m_pendingDeletedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_pendingDeletedSystemTicketId);
        return;
    }

    const QList<SystemTicketDto> tickets = systemTickets();
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket scanning count=%1")
                  .arg(QString::number(tickets.size()));
    for (const SystemTicketDto &ticket : tickets) {
        if (ticket.transferState == QLatin1String("received")
                || ticket.transferState == QLatin1String("auto-pending")) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket selected taskId=%1 transferState=%2 returnState=%3")
                          .arg(ticket.taskId,
                               ticket.transferState,
                               ticket.returnState);
            emit statusMessage(QStringLiteral("发现可自动传票任务：%1").arg(ticket.taskId));
            tryStartTicketTransfer(ticket.taskId, true);
            return;
        }
    }
    qInfo().noquote() << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket no-eligible-ticket");
}

bool KeyManageController::isCurrentKeySession(int sessionId) const
{
    return sessionId > 0 && sessionId == m_keySessionId;
}

void KeyManageController::openNewKeySession()
{
    m_keySessionId = ++m_keySessionSeed;
    m_autoQuerySessionId = 0;
    emit statusMessage(QStringLiteral("钥匙会话已建立：session=%1").arg(m_keySessionId));
}

void KeyManageController::invalidateCurrentKeySession(const QString &reason)
{
    if (m_keySessionId <= 0) {
        return;
    }

    const int expiredSessionId = m_keySessionId;
    m_keySessionId = 0;
    m_autoQuerySessionId = 0;

    emit statusMessage(
            reason.trimmed().isEmpty()
            ? QStringLiteral("钥匙会话已失效：session=%1").arg(expiredSessionId)
            : QStringLiteral("钥匙会话已失效：session=%1，原因=%2")
                  .arg(expiredSessionId)
                  .arg(reason));
}

void KeyManageController::scheduleAutoTransferRetry(const QString &reason)
{
    if (m_autoTransferRetryScheduled) {
        return;
    }

    m_autoTransferRetryScheduled = true;
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] scheduleAutoTransferRetry reason=%1 currentSession=%2")
                  .arg(reason.isEmpty() ? QStringLiteral("<none>") : reason,
                       QString::number(m_keySessionId));
    QTimer::singleShot(300, this, [this]() {
        m_autoTransferRetryScheduled = false;
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] auto transfer retry timer fired currentSession=%1")
                      .arg(QString::number(m_keySessionId));
        tryAutoTransferPendingTicket();
    });
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
