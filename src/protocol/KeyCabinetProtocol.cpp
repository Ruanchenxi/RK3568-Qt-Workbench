/**
 * @file KeyCabinetProtocol.cpp
 * @brief 钥匙柜通讯协议实现
 *
 * 注意：协议格式需要根据实际设备厂家提供的文档进行调整
 */

#include "KeyCabinetProtocol.h"

KeyCabinetProtocol::KeyCabinetProtocol(QObject *parent)
    : ProtocolBase(parent)
{
}

KeyCabinetProtocol::~KeyCabinetProtocol()
{
}

// ========== 协议接口实现 ==========

ProtocolResult KeyCabinetProtocol::parseResponse(const QByteArray &data)
{
    ProtocolResult result;
    result.rawData = data;

    // 检查最小长度
    if (data.size() < MIN_PACKET_LENGTH)
    {
        result.success = false;
        result.errorMessage = "数据包长度不足";
        return result;
    }

    // 检查帧头
    if (static_cast<quint8>(data[0]) != FRAME_HEADER)
    {
        result.success = false;
        result.errorMessage = "帧头错误";
        return result;
    }

    // 检查校验
    if (!validateChecksum(data))
    {
        result.success = false;
        result.errorMessage = "校验和错误";
        return result;
    }

    // 获取命令类型
    quint8 cmd = static_cast<quint8>(data[1]);
    result.commandType = cmd;

    // 根据命令类型解析
    switch (cmd)
    {
    case CMD_QUERY_KEY_STATUS:
        return parseQueryKeyResponse(data);
    case CMD_OPEN_KEY_SLOT:
        return parseOpenKeyResponse(data);
    case CMD_QUERY_ALL_STATUS:
        return parseQueryAllResponse(data);
    case CMD_HEARTBEAT:
        result.success = true;
        result.data["type"] = "heartbeat";
        return result;
    default:
        result.success = false;
        result.errorMessage = QString("未知命令类型: 0x%1").arg(cmd, 2, 16, QChar('0'));
        return result;
    }
}

QByteArray KeyCabinetProtocol::buildCommand(int commandType, const QVariantMap &params)
{
    switch (commandType)
    {
    case CMD_QUERY_KEY_STATUS:
        return buildQueryKeyStatus(params.value("slotId", 1).toInt());
    case CMD_OPEN_KEY_SLOT:
        return buildOpenKeySlot(params.value("slotId", 1).toInt());
    case CMD_QUERY_ALL_STATUS:
        return buildQueryAllStatus();
    case CMD_HEARTBEAT:
        return buildHeartbeat();
    default:
        return QByteArray();
    }
}

bool KeyCabinetProtocol::validateChecksum(const QByteArray &data)
{
    if (data.size() < MIN_PACKET_LENGTH)
    {
        return false;
    }

    // 获取数据长度
    int dataLen = static_cast<quint8>(data[2]);
    int expectedLen = dataLen + 4; // 帧头+命令+长度+数据+校验

    if (data.size() < expectedLen)
    {
        return false;
    }

    // 计算校验（不含帧尾的异或校验）
    quint8 checksum = 0;
    for (int i = 0; i < expectedLen - 1; ++i)
    {
        checksum ^= static_cast<quint8>(data[i]);
    }

    return checksum == static_cast<quint8>(data[expectedLen - 1]);
}

int KeyCabinetProtocol::isCompletePacket(const QByteArray &data)
{
    if (data.size() < 3)
    {
        return 0; // 不完整
    }

    // 检查帧头
    if (static_cast<quint8>(data[0]) != FRAME_HEADER)
    {
        return -1; // 无效
    }

    int dataLen = static_cast<quint8>(data[2]);
    int expectedLen = dataLen + 4; // 帧头+命令+长度+数据+校验

    if (data.size() >= expectedLen)
    {
        return expectedLen; // 完整
    }

    return 0; // 不完整
}

// ========== 便捷命令构建方法 ==========

QByteArray KeyCabinetProtocol::buildQueryKeyStatus(int slotId)
{
    QByteArray cmd;
    cmd.append(static_cast<char>(FRAME_HEADER));           // 帧头
    cmd.append(static_cast<char>(CMD_QUERY_KEY_STATUS));   // 命令
    cmd.append(static_cast<char>(1));                      // 数据长度
    cmd.append(static_cast<char>(slotId));                 // 槽位号
    cmd.append(static_cast<char>(calculateChecksum(cmd))); // 校验
    return cmd;
}

QByteArray KeyCabinetProtocol::buildOpenKeySlot(int slotId)
{
    QByteArray cmd;
    cmd.append(static_cast<char>(FRAME_HEADER));
    cmd.append(static_cast<char>(CMD_OPEN_KEY_SLOT));
    cmd.append(static_cast<char>(1));
    cmd.append(static_cast<char>(slotId));
    cmd.append(static_cast<char>(calculateChecksum(cmd)));
    return cmd;
}

QByteArray KeyCabinetProtocol::buildQueryAllStatus()
{
    QByteArray cmd;
    cmd.append(static_cast<char>(FRAME_HEADER));
    cmd.append(static_cast<char>(CMD_QUERY_ALL_STATUS));
    cmd.append(static_cast<char>(0)); // 无数据
    cmd.append(static_cast<char>(calculateChecksum(cmd)));
    return cmd;
}

QByteArray KeyCabinetProtocol::buildHeartbeat()
{
    QByteArray cmd;
    cmd.append(static_cast<char>(FRAME_HEADER));
    cmd.append(static_cast<char>(CMD_HEARTBEAT));
    cmd.append(static_cast<char>(0));
    cmd.append(static_cast<char>(calculateChecksum(cmd)));
    return cmd;
}

// ========== 响应解析辅助方法 ==========

ProtocolResult KeyCabinetProtocol::parseQueryKeyResponse(const QByteArray &data)
{
    ProtocolResult result;
    result.rawData = data;
    result.commandType = CMD_QUERY_KEY_STATUS;

    if (data.size() < 5)
    {
        result.success = false;
        result.errorMessage = "响应数据长度不足";
        return result;
    }

    int slotId = static_cast<quint8>(data[3]);
    int status = static_cast<quint8>(data[4]);

    result.success = true;
    result.data["slotId"] = slotId;
    result.data["status"] = status;
    result.data["statusText"] = keyStatusToString(static_cast<KeyStatus>(status));

    return result;
}

ProtocolResult KeyCabinetProtocol::parseOpenKeyResponse(const QByteArray &data)
{
    ProtocolResult result;
    result.rawData = data;
    result.commandType = CMD_OPEN_KEY_SLOT;

    if (data.size() < 5)
    {
        result.success = false;
        result.errorMessage = "响应数据长度不足";
        return result;
    }

    int slotId = static_cast<quint8>(data[3]);
    bool success = static_cast<quint8>(data[4]) == 0x00;

    result.success = success;
    result.data["slotId"] = slotId;
    result.data["opened"] = success;

    if (!success)
    {
        result.errorMessage = QString("开启槽位%1失败").arg(slotId);
    }

    return result;
}

ProtocolResult KeyCabinetProtocol::parseQueryAllResponse(const QByteArray &data)
{
    ProtocolResult result;
    result.rawData = data;
    result.commandType = CMD_QUERY_ALL_STATUS;

    int dataLen = static_cast<quint8>(data[2]);
    if (data.size() < 3 + dataLen)
    {
        result.success = false;
        result.errorMessage = "响应数据长度不足";
        return result;
    }

    // 解析所有钥匙状态
    QVariantMap keyStatus;
    for (int i = 0; i < dataLen; i += 2)
    {
        if (3 + i + 1 < data.size())
        {
            int slotId = static_cast<quint8>(data[3 + i]);
            int status = static_cast<quint8>(data[3 + i + 1]);
            keyStatus[QString::number(slotId)] = status;
        }
    }

    result.success = true;
    result.data["keyStatus"] = keyStatus;
    result.data["keyCount"] = dataLen / 2;

    return result;
}

QMap<int, KeyStatus> KeyCabinetProtocol::extractKeyStatusMap(const ProtocolResult &result)
{
    QMap<int, KeyStatus> statusMap;

    if (result.data.contains("keyStatus"))
    {
        QVariantMap vm = result.data["keyStatus"].toMap();
        for (auto it = vm.begin(); it != vm.end(); ++it)
        {
            int slotId = it.key().toInt();
            KeyStatus status = static_cast<KeyStatus>(it.value().toInt());
            statusMap[slotId] = status;
        }
    }
    else if (result.data.contains("slotId") && result.data.contains("status"))
    {
        int slotId = result.data["slotId"].toInt();
        KeyStatus status = static_cast<KeyStatus>(result.data["status"].toInt());
        statusMap[slotId] = status;
    }

    return statusMap;
}

QString KeyCabinetProtocol::keyStatusToString(KeyStatus status)
{
    switch (status)
    {
    case KEY_PRESENT:
        return "在位";
    case KEY_ABSENT:
        return "不在";
    case KEY_FAULT:
        return "故障";
    default:
        return "未知";
    }
}
