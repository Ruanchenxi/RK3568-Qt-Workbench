/**
 * @file KeySessionService.cpp
 * @brief 钥匙会话服务默认实现（适配 KeySerialClient）
 */
#include "features/key/application/KeySessionService.h"

#include <QDebug>

#include "core/ConfigManager.h"
#include "platform/serial/QtSerialTransport.h"
#include "platform/serial/ReplaySerialTransport.h"

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

    connect(m_client, &KeySerialClient::connectedChanged, this, [this](bool) {
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::keyPlacedChanged, this, [this](bool) {
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::keyStableChanged, this, [this](bool) {
        emitStateChanged();
    });
    connect(m_client, &KeySerialClient::tasksUpdated, this, [this](const QList<KeyTaskInfo> &tasks) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::TasksUpdated;
        event.data.insert("count", tasks.size());
        event.data.insert("tasks", toTaskVariantList(tasks));
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::ackReceived, this, [this](quint8 ackedCmd) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Ack;
        event.data.insert("ackedCmd", static_cast<int>(ackedCmd));
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::nakReceived, this, [this](quint8 origCmd, quint8 errCode) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Nak;
        event.data.insert("origCmd", static_cast<int>(origCmd));
        event.data.insert("errCode", static_cast<int>(errCode));
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::timeoutOccurred, this, [this](const QString &what) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Timeout;
        event.data.insert("what", what);
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::notice, this, [this](const QString &what) {
        KeySessionEvent event;
        event.kind = KeySessionEvent::Notice;
        event.data.insert("what", what);
        emit eventOccurred(event);
    });
    connect(m_client, &KeySerialClient::logItemEmitted, this, &KeySessionService::logItemEmitted);
}

bool KeySessionService::connectPort(const QString &portName, int baud)
{
    return m_client->connectPort(portName, baud);
}

void KeySessionService::disconnectPort()
{
    m_client->disconnectPort();
}

KeySessionSnapshot KeySessionService::snapshot() const
{
    KeySessionSnapshot s;
    s.connected = m_client->isConnected();
    s.keyPresent = m_client->isKeyPresent();
    s.keyStable = m_client->isKeyStable();
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
        // NOTE:
        // KeySerialClient 暂无公开“仅发 SET_COM”接口。
        // 这里保持最小语义：通过重连触发握手，不改协议行为。
        if (m_client->isConnected()) {
            m_client->disconnectPort();
        }
        break;
    case CommandId::QueryTasks:
        m_client->queryTasksAll();
        break;
    case CommandId::DeleteTask:
        if (request.payload.isEmpty()) {
            m_client->deleteFirstTask();
        } else {
            m_client->deleteTask(request.payload);
        }
        break;
    case CommandId::Custom:
        break;
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

void KeySessionService::emitStateChanged()
{
    const KeySessionSnapshot s = snapshot();

    KeySessionEvent event;
    event.kind = KeySessionEvent::KeyStateChanged;
    event.data.insert("connected", s.connected);
    event.data.insert("keyPresent", s.keyPresent);
    event.data.insert("keyStable", s.keyStable);
    event.data.insert("portName", s.portName);
    emit eventOccurred(event);
}
