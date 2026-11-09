/**
 * @file KeySessionService.cpp
 * @brief Adapts the real Qt serial transport to the key-session contract.
 *
 * The product has one runtime transport. Protocol state remains in
 * KeySerialClient, while this service maps typed commands and events.
 */
#include "features/key/application/KeySessionService.h"

#include <QDebug>

#include "core/ConfigManager.h"
#include "platform/serial/QtSerialTransport.h"

/**
 * @brief 将配置中的 stationId 字符串转为协议层 quint16
 * @param s 原始配置字符串（如 "001"、"1"、"2"）
 * @return 非负整数解析结果；空/负数/解析失败一律回退为 1，0 作为“全站模式”保留合法语义
 *
 * 失败策略：静默回退 1，不抛出异常、不产生额外日志。
 * 原因：站号异常时让 DEL 仍能发出合法帧、由运维排查配置问题，
 *       而不是让整条钥匙链路因配置错误停止工作。
 * KeySerialClient::setStationId 会输出 [CONF] 日志记录最终使用值。
 */
static quint16 parseStationId(const QString &s)
{
    bool ok = false;
    const int v = s.trimmed().toInt(&ok);
    return (ok && v >= 0) ? static_cast<quint16>(v) : 1;
}

KeySessionService::KeySessionService(QObject *parent)
    : IKeySessionService(parent)
    , m_client(nullptr)
{
    auto *transport = new QtSerialTransport(this);
    m_client = new KeySerialClient(transport, this);
    // [LEARNING] 上面先创建具体传输对象，再把它的地址交给 KeySerialClient。
    // KeySerialClient 构造函数用 m_serial(transport) 保存地址；没有创建第二份传输对象。

    // ── 注入当前站号（DEL Addr2 来源）────────────────────────────────────────
    // WHY：系统设置界面"当前站号"（如 001/002）决定 DEL 命令 Addr2 字段的值：
    //   stationId=1 → DEL Addr2 = [01 00]；stationId=2 → DEL Addr2 = [02 00]
    //   SET_COM/Q_TASK/Q_KEYEQ 的 Addr2 始终为 0x0000，与站号无关（已抓包验证）
    // WHY configChanged：保证用户在系统设置界面修改站号后，下次 DEL 即刻生效，
    //   无需重启应用也无需重连串口。
    // 失败反馈：stationId 非法时 parseStationId 静默回退 1；
    //   KeySerialClient::setStationId 会输出 [CONF] 日志记录最终写入值，供诊断。
    //   此处不再向上层抛事件，因为链路不中断、属配置层面问题。
    m_client->setStationId(parseStationId(ConfigManager::instance()->stationId()));
    connect(ConfigManager::instance(), &ConfigManager::configChanged,
            this, [this](const QString &key, const QVariant &value) {
        if (key == "system/stationId") {
            // 站号变更实时同步，无需重连；DEL 下次发送即使用新值
            m_client->setStationId(parseStationId(value.toString()));
        }
    });
    // ─────────────────────────────────────────────────────────────────────────

    connect(m_client, &KeySerialClient::connectedChanged, this, [this](bool) {
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::keyPlacedChanged, this, [this](bool) {
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::keyStableChanged, this, [this](bool) {
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::stateFlagsChanged, this, [this]() {
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::tasksUpdated, this, [this](const QList<KeyTaskInfo> &tasks) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::TasksUpdated;
        event.data.insert("count", tasks.size());
        event.data.insert("tasks", toTaskVariantList(tasks));
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::taskLogReady, this, [this](const QVariantMap &payload) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::TaskLogReady;
        event.data = payload;
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::taskLogFailed, this, [this](const QString &reason) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::TaskLogFailed;
        event.data.insert(QStringLiteral("what"), reason);
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::captureRfidReady,
            this, [this](const QVariantMap &payload) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::CaptureRfidReady;
        event.data = payload;
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::captureRfidFailed,
            this, [this](const QString &taskId, const QString &reason) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::CaptureRfidFailed;
        event.data.insert(QStringLiteral("taskId"), taskId);
        event.data.insert(QStringLiteral("what"), reason);
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::ackReceived, this, [this](quint8 ackedCmd) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Ack;
        event.data.insert("ackedCmd", static_cast<int>(ackedCmd));
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::nakReceived, this, [this](quint8 origCmd, quint8 errCode) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Nak;
        event.data.insert("origCmd", static_cast<int>(origCmd));
        event.data.insert("errCode", static_cast<int>(errCode));
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::timeoutOccurred, this,
            [this](quint8 command, const QString &what) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Timeout;
        event.data.insert("command", static_cast<int>(command));
        event.data.insert("what", what);
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::notice, this, [this](const QString &what) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Notice;
        event.data.insert("what", what);
        stampEvent(event);
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::ticketTransferProgress, this,
            [this](int frameIndex, int totalFrames, quint8 cmd) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::TicketTransferProgress;
        event.data.insert("frameIndex", frameIndex);
        event.data.insert("totalFrames", totalFrames);
        event.data.insert("cmd", static_cast<int>(cmd));
        stampEvent(event);
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::ticketTransferFinished, this, [this]() {
        KeySessionEvent event;
        event.kind = KeySessionEvent::TicketTransferFinished;
        stampEvent(event);
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::ticketTransferFailed, this, [this](const QString &reason) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::TicketTransferFailed;
        event.data.insert("what", reason);
        stampEvent(event);
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::bKeyPresenceChanged, this, [this](bool) {
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::auxCommandFinished, this,
            [this](quint8 seat, quint8 cmd, bool success, const QString &reason) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::AuxCommandFinished;
        event.data.insert("seat", static_cast<int>(seat));
        event.data.insert("cmd", static_cast<int>(cmd));
        event.data.insert("success", success);
        event.data.insert("reason", reason);
        stampEvent(event);
        emit eventOccurred(event);
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::logItemEmitted, this, &KeySessionService::logItemEmitted);
}

bool KeySessionService::connectPort(const QString &portName, int baud)
{
    ++m_bridgeEpoch;
    return m_client->connectPort(portName, baud);
}

void KeySessionService::disconnectPort()
{
    ++m_bridgeEpoch;
    m_client->disconnectPort();
}


KeySessionSnapshot KeySessionService::snapshot() const
{
    KeySessionSnapshot s;
    s.bridgeEpoch = m_bridgeEpoch;
    s.connected = m_client->isConnected();
    s.keyPresent = m_client->isKeyPresent();
    s.keyStable = m_client->isKeyStable();
    s.sessionReady = m_client->isSessionReady();
    s.protocolHealthy = m_client->isProtocolHealthy();
    s.protocolConfirmedOnce = m_client->hasProtocolConfirmedOnce();
    s.commandInFlight = m_client->hasInFlightCommand();
    s.lastBusinessSuccessMs = m_client->lastBusinessSuccessMs();
    s.lastProtocolFailureMs = m_client->lastProtocolFailureMs();
    s.recoveryWindowActive = m_client->recoveryWindowActive();
    s.batteryPercent = m_client->batteryPercent();
    s.portName = m_client->currentPortName();
    s.verifiedPortName = m_client->hasVerifiedPort() ? m_client->verifiedPortName() : QString();
    s.bKeyPresent = m_client->isBKeyPresent();
    s.bBatteryPercent = m_client->bBatteryPercent();
    return s;
}

void KeySessionService::execute(const CommandRequest &request)
{
    if (request.opId >= 0) {
        m_client->setCurrentOpId(request.opId);
    }

    switch (request.id) {
    case CommandId::SetCom:
        m_client->refreshHandshake();
        break;
    case CommandId::InitKey:
        m_client->sendInitPayload(request.payload);
        break;
    case CommandId::QueryBattery: {
        const quint8 seat = static_cast<quint8>(request.options.value("seat", 0x01).toUInt());
        m_client->queryBattery(seat);
        break;
    }
    case CommandId::SyncDeviceTime: {
        const quint8 seat = static_cast<quint8>(request.options.value("seat", 0x01).toUInt());
        m_client->syncDeviceTime(seat);
        break;
    }
    case CommandId::QueryTasks:
        m_client->queryTasksAll();
        break;
    case CommandId::QueryTaskLog:
        m_client->requestTaskLog(request.payload);
        break;
    case CommandId::QueryCaptureRfid:
        m_client->requestCaptureRfid(request.payload);
        break;
    case CommandId::DeleteTask:
        if (request.payload.isEmpty()) {
            m_client->deleteFirstTask();
        } else {
            m_client->deleteTask(request.payload);
        }
        break;
    case CommandId::DownloadRfid:
        m_client->sendRfidPayload(request.payload);
        break;
    case CommandId::Custom:
        break;
    }
}

// Maps the application maintenance request onto the protocol adapter's
// existing sequence enum; protocol timing remains owned by KeySerialClient.
void KeySessionService::startAuxSequence(
        quint8 seat,
        KeyAuxSequenceMode mode,
        KeyAuxSequenceOrigin origin,
        const QString &reason)
{
    m_client->startAuxSequence(
                seat,
                static_cast<KeySerialClient::AuxSequenceMode>(mode),
                static_cast<KeySerialClient::AuxSequenceOrigin>(origin),
                reason);
}

// Accepts only the in-memory workbench payload; file-based transfer was an
// unused development path and must not become a second product input source.
void KeySessionService::transferTicket(const TicketTransferRequest &request)
{
    if (request.jsonBytes.isEmpty() && request.encodedPayload.isEmpty()) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Notice;
        event.data.insert("what", QStringLiteral("\u4F20\u7968\u53D1\u9001\u5931\u8D25\uFF1AJSON \u5185\u5BB9\u4E3A\u7A7A"));
        emit eventOccurred(event);
        return;
    }

    if (request.opId >= 0) {
        m_client->setCurrentOpId(request.opId);
    }
    if (!request.encodedPayload.isEmpty()) {
        m_client->transferTicketPayload(request.encodedPayload,
                                        request.stationId);
    } else {
        m_client->transferTicketJson(request.jsonBytes,
                                     request.stationId,
                                     request.debugFrameChunkSize);
    }
}

QVariantList KeySessionService::toTaskVariantList(const QList<KeyTaskInfo> &tasks) const
{
    QVariantList list;
    list.reserve(tasks.size());

    for (const KeyTaskInfo &task : tasks) {
        QVariantMap item;
        item.insert("taskId", task.taskId);
        item.insert("status", static_cast<int>(task.status));
        item.insert("type", static_cast<int>(task.type));
        item.insert("source", static_cast<int>(task.source));
        item.insert("reserved", static_cast<int>(task.reserved));
        list.append(item);
    }

    return list;
}

void KeySessionService::stampEvent(KeySessionEvent &event) const
{
    event.data.insert(QStringLiteral("bridgeEpoch"), m_bridgeEpoch);
    event.data.insert(QStringLiteral("portName"), m_client->currentPortName());
}

void KeySessionService::emitStateChanged()
{
    const KeySessionSnapshot s = snapshot();

    KeySessionEvent event;
    event.kind = KeySessionEvent::KeyStateChanged;
    event.data.insert("connected", s.connected);
    event.data.insert("keyPresent", s.keyPresent);
    event.data.insert("keyStable", s.keyStable);
    event.data.insert("sessionReady", s.sessionReady);
    event.data.insert("protocolHealthy", s.protocolHealthy);
    event.data.insert("protocolConfirmedOnce", s.protocolConfirmedOnce);
    event.data.insert("lastBusinessSuccessMs", s.lastBusinessSuccessMs);
    event.data.insert("lastProtocolFailureMs", s.lastProtocolFailureMs);
    event.data.insert("recoveryWindowActive", s.recoveryWindowActive);
    event.data.insert("portName", s.portName);
    event.data.insert("bridgeEpoch", s.bridgeEpoch);
    emit eventOccurred(event);
}
