/**
 * @file IKeySessionService.h
 * @brief 钥匙会话服务抽象接口
 *
 * 设计说明：
 * 1. Controller 层仅依赖该接口，不依赖 KeySerialClient 具体实现。
 * 2. 第一版只暴露最小可用能力，避免接口过早膨胀。
 * 3. 命令入口统一为 execute(CommandRequest)，后续扩展命令不需要改 Page。
 */
#ifndef IKEYSESSIONSERVICE_H
#define IKEYSESSIONSERVICE_H

#include <QObject>
#include <QByteArray>
#include <QVariantMap>
#include <QString>

#include "features/key/protocol/LogItem.h"
#include "shared/contracts/TicketTransferRequest.h"

enum class CommandId
{
    SetCom,
    InitKey,
    QueryTasks,
    QueryTaskLog,
    DeleteTask,
    DownloadRfid,
    Custom
};

struct CommandRequest
{
    CommandId id = CommandId::Custom;
    int opId = -1;
    QByteArray payload;
    QVariantMap options;
};

struct KeySessionSnapshot
{
    bool connected = false;
    bool keyPresent = false;
    bool keyStable = false;
    bool sessionReady = false;
    bool protocolHealthy = false;
    bool protocolConfirmedOnce = false;
    QString portName;
    QString verifiedPortName;  ///< 已验证的端口名（收到合法协议帧后锁定），空=未验证
};

struct KeySessionEvent
{
    enum Kind
    {
        Notice,
        Timeout,
        Ack,
        Nak,
        TasksUpdated,
        TaskLogReady,
        KeyStateChanged,
        RawProtocol,
        TicketTransferProgress,
        TicketTransferFinished,
        TicketTransferFailed
    };

    Kind kind = Notice;
    QVariantMap data;
};

class IKeySessionService : public QObject
{
    Q_OBJECT
public:
    explicit IKeySessionService(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IKeySessionService() = default;

    virtual bool connectPort(const QString &portName, int baud) = 0;
    virtual void disconnectPort() = 0;
    virtual KeySessionSnapshot snapshot() const = 0;
    virtual void execute(const CommandRequest &request) = 0;
    virtual void transferTicket(const TicketTransferRequest &request) = 0;

signals:
    void eventOccurred(const KeySessionEvent &event);
    void logItemEmitted(const LogItem &item);
};

#endif // IKEYSESSIONSERVICE_H
