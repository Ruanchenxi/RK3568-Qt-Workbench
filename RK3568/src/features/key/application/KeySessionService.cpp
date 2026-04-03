/**
 * @file KeySessionService.cpp
 * @brief 钥匙会话服务默认实现（真机串口/回放串口注入点）
 *
 * Layer:
 *   features/key/application
 *
 * Responsibilities:
 *   1) 按配置选择传输层（replay 或真实串口）并注入 KeySerialClient
 *   2) 从 ConfigManager 读取"当前站号"并注入协议层（影响 DEL 命令 Addr2）
 *   3) 监听 ConfigManager::configChanged，站号变更时实时同步到协议层
 *   4) 桥接 KeySerialClient 信号与上层 IKeySessionService 契约事件
 *
 * Dependencies:
 *   - Allowed : IKeySessionService, KeySerialClient, ISerialTransport,
 *               QtSerialTransport, ReplaySerialTransport, ConfigManager
 *   - Forbidden: UI page classes, QSerialPort 直接使用
 *
 * Constraints:
 *   - 协议帧语义由 KeySerialClient/KeyProtocolDefs 维护，此文件不直接组帧
 *   - replay 脚本加载失败必须回退真实串口（已有日志输出）
 *   - stationId 为空或非正整数时静默回退为 1（见 parseStationId 注释）
 */
#include "features/key/application/KeySessionService.h"

#include <QDebug>
#include <QFile>

#include "core/ConfigManager.h"
#include "platform/serial/QtSerialTransport.h"
#include "platform/serial/ReplaySerialTransport.h"

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
    ISerialTransport *transport = nullptr;
    const bool replayEnabled = ConfigManager::instance()->value("serial/replayEnabled", false).toBool();
    if (replayEnabled) {
        auto *replay = new ReplaySerialTransport(this);
        const QString scriptPath = ConfigManager::instance()
                                       ->value("serial/replayScript", "test/replay/scenario_half_and_sticky.jsonl")
                                       .toString();
        QString error;
        if (replay->loadScript(scriptPath, &error)) {
            transport = replay;
            qInfo().noquote() << "[KeySessionService] Replay enabled, script:" << scriptPath;
        } else {
            qWarning().noquote() << "[KeySessionService] Replay script load failed:" << error
                                 << ", fallback to real serial transport.";
            replay->deleteLater();
        }
    }

    if (!transport) {
        transport = new QtSerialTransport(this);
    }
    m_client = new KeySerialClient(transport, this);

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
    connect(m_client, &KeySerialClient::timeoutOccurred, this, [this](const QString &what) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Timeout;
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

void KeySessionService::setPortSwitchInProgress(bool inProgress)
{
    m_client->setPortSwitchInProgress(inProgress);
}

void KeySessionService::clearVerifiedPort()
{
    m_client->clearVerifiedPort();
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
    case CommandId::QueryBattery:
        m_client->queryBattery();
        break;
    case CommandId::SyncDeviceTime:
        m_client->syncDeviceTime();
        break;
    case CommandId::QueryTasks:
        m_client->queryTasksAll();
        break;
    case CommandId::QueryTaskLog:
        m_client->requestTaskLog(request.payload);
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

void KeySessionService::transferTicket(const TicketTransferRequest &request)
{
    QByteArray jsonBytes = request.jsonBytes;
    if (jsonBytes.isEmpty() && !request.jsonPath.trimmed().isEmpty()) {
        QFile f(request.jsonPath);
        if (!f.open(QIODevice::ReadOnly)) {
            KeySessionEvent event;
            event.kind = KeySessionEvent::Notice;
            event.data.insert("what",
                              QStringLiteral("传票发送失败：无法读取 JSON 文件 %1")
                                  .arg(request.jsonPath));
            emit eventOccurred(event);
            return;
        }
        jsonBytes = f.readAll();
        f.close();
    }

    if (jsonBytes.isEmpty()) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Notice;
        event.data.insert("what", QStringLiteral("传票发送失败：JSON 内容为空"));
        emit eventOccurred(event);
        return;
    }

    if (request.opId >= 0)
        m_client->setCurrentOpId(request.opId);
    m_client->transferTicketJson(jsonBytes,
                                 request.stationId,
                                 request.debugFrameChunkSize);
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
