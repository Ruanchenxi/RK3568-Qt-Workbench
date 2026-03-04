/**
 * @file KeyManageController.cpp
 * @brief 钥匙管理页控制器实现
 */
#include "features/key/application/KeyManageController.h"

#include <QStringList>

#include "core/ConfigManager.h"
#include "features/key/application/KeySessionService.h"

KeyManageController::KeyManageController(IKeySessionService *session,
                                         SerialLogManager *logManager,
                                         QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_logManager(logManager)
    , m_nextOpId(1)
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
    connect(m_logManager, &SerialLogManager::overflowDropped, this, [this]() {
        emit statusMessage(QStringLiteral("日志达到上限，已自动丢弃旧日志"));
    });
}

void KeyManageController::start()
{
    tryAutoConnectKeyPort();
    emit sessionSnapshotChanged(currentSnapshot());
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
    case KeySessionEvent::KeyStateChanged:
        emit sessionSnapshotChanged(m_session->snapshot());
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

void KeyManageController::tryAutoConnectKeyPort()
{
    const int cfgBaud = ConfigManager::instance()->baudRate();
    const int baud = (cfgBaud > 0) ? cfgBaud : 115200;
    const QString configPort = ConfigManager::instance()->keySerialPort().trimmed();

    // 已验证端口优先：避免超时/重连后扫描到错误端口（如 COM6/COM7）
    const KeySessionSnapshot snap = m_session->snapshot();
    if (!snap.verifiedPortName.isEmpty()) {
        if (m_session->connectPort(snap.verifiedPortName, baud)) {
            emit statusMessage(QStringLiteral("串口已连接(已验证端口): %1").arg(snap.verifiedPortName));
            emit sessionSnapshotChanged(m_session->snapshot());
            return;
        }
    }

    // 仅使用配置项指定的端口，不做盲扫（避免连错设备）
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
