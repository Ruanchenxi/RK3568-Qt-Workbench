/**
 * @file KeyManageController.cpp
 * @brief 钥匙管理页控制器实现
 */
#include "features/key/application/KeyManageController.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include "core/ConfigManager.h"
#include "features/key/application/KeySessionService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketIngressService.h"

KeyManageController::KeyManageController(IKeySessionService *session,
                                         SerialLogManager *logManager,
                                         QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_logManager(logManager)
    , m_ticketStore(new TicketStore(this))
    , m_ticketIngress(new TicketIngressService(this))
    , m_nextOpId(1)
    , m_selectedSystemTicketId()
    , m_activeTransferTaskId()
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
    emit sessionSnapshotChanged(currentSnapshot());
    emit systemTicketsUpdated(systemTickets());
}

void KeyManageController::onInitClicked()
{
    emit statusMessage(QStringLiteral("重新初始化通讯..."));
    m_session->disconnectPort();
    tryAutoConnectKeyPort();
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

    CommandRequest req;
    req.id = CommandId::DeleteTask;
    req.opId = nextOpId();
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

bool KeyManageController::exportLogs(const QString &path, QString *error) const
{
    return m_logManager->exportCsv(path, error);
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

void KeyManageController::handleSessionEvent(const KeySessionEvent &event)
{
    switch (event.kind) {
    case KeySessionEvent::Notice:
        emit statusMessage(event.data.value("what").toString());
        break;
    case KeySessionEvent::Timeout: {
        const QString what = event.data.value("what").toString();
        emit timeoutOccurred(what);
        emit statusMessage(what);
        break;
    }
    case KeySessionEvent::Ack:
        emit ackReceived(static_cast<quint8>(event.data.value("ackedCmd").toInt()));
        break;
    case KeySessionEvent::Nak:
        emit nakReceived(static_cast<quint8>(event.data.value("origCmd").toInt()),
                         static_cast<quint8>(event.data.value("errCode").toInt()));
        break;
    case KeySessionEvent::TasksUpdated: {
        const QList<KeyTaskDto> tasks = toTaskDtos(event.data.value("tasks").toList());
        emit tasksUpdated(tasks);
        emit statusMessage(QStringLiteral("Q_TASK 完成，任务数=%1").arg(tasks.size()));
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
    case KeySessionEvent::KeyStateChanged:
        emit sessionSnapshotChanged(m_session->snapshot());
        tryAutoTransferPendingTicket();
        break;
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

bool KeyManageController::autoTransferEnabled() const
{
    return ConfigManager::instance()->value("ticket/autoTransferEnabled", false).toBool();
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

    emit statusMessage(QStringLiteral("自动传票已开启，但当前没有待发送系统票"));
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
