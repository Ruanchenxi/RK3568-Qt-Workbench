/**
 * @file KeyCabinetProtocol.h
 * @brief 钥匙柜通讯协议
 *
 * 协议格式（示例，需要根据实际设备文档调整）：
 * [帧头 0xAA][命令码][数据长度][数据...][校验和]
 *
 * 命令列表：
 * 0x01 - 查询单个钥匙状态
 * 0x02 - 开启钥匙槽
 * 0x03 - 查询所有钥匙状态
 * 0x04 - 设置柜门密码
 * 0x05 - 查询柜门状态
 */

#ifndef KEYCABINETPROTOCOL_H
#define KEYCABINETPROTOCOL_H

#include "ProtocolBase.h"

/**
 * @brief 钥匙柜命令类型
 */
enum KeyCabinetCommand
{
    CMD_QUERY_KEY_STATUS = 0x01,  // 查询单个钥匙状态
    CMD_OPEN_KEY_SLOT = 0x02,     // 开启钥匙槽
    CMD_QUERY_ALL_STATUS = 0x03,  // 查询所有钥匙状态
    CMD_SET_PASSWORD = 0x04,      // 设置密码
    CMD_QUERY_DOOR_STATUS = 0x05, // 查询柜门状态
    CMD_HEARTBEAT = 0xFE,         // 心跳
};

/**
 * @brief 钥匙状态
 */
enum KeyStatus
{
    KEY_UNKNOWN = 0x00, // 未知
    KEY_PRESENT = 0x01, // 钥匙在位
    KEY_ABSENT = 0x02,  // 钥匙不在
    KEY_FAULT = 0xFF,   // 故障
};

/**
 * @class KeyCabinetProtocol
 * @brief 钥匙柜通讯协议实现
 */
class KeyCabinetProtocol : public ProtocolBase
{
    Q_OBJECT

public:
    explicit KeyCabinetProtocol(QObject *parent = nullptr);
    ~KeyCabinetProtocol();

    // ProtocolBase 接口实现
    QString protocolName() const override { return "KeyCabinet"; }
    QString protocolVersion() const override { return "1.0"; }
    ProtocolResult parseResponse(const QByteArray &data) override;
    QByteArray buildCommand(int commandType, const QVariantMap &params = QVariantMap()) override;
    bool validateChecksum(const QByteArray &data) override;
    int isCompletePacket(const QByteArray &data) override;

    // ========== 便捷命令构建方法 ==========

    /**
     * @brief 构建查询钥匙状态命令
     * @param slotId 槽位号(1-N)
     */
    QByteArray buildQueryKeyStatus(int slotId);

    /**
     * @brief 构建开启钥匙槽命令
     * @param slotId 槽位号
     */
    QByteArray buildOpenKeySlot(int slotId);

    /**
     * @brief 构建查询所有钥匙状态命令
     */
    QByteArray buildQueryAllStatus();

    /**
     * @brief 构建心跳命令
     */
    QByteArray buildHeartbeat();

    // ========== 响应解析辅助方法 ==========

    /**
     * @brief 从解析结果获取钥匙状态
     * @param result 解析结果
     * @return 钥匙状态映射 <槽位号, 状态>
     */
    static QMap<int, KeyStatus> extractKeyStatusMap(const ProtocolResult &result);

    /**
     * @brief 状态转字符串
     */
    static QString keyStatusToString(KeyStatus status);

private:
    static const quint8 FRAME_HEADER = 0xAA;
    static const quint8 FRAME_TAIL = 0x55;
    static const int MIN_PACKET_LENGTH = 5; // 帧头+命令+长度+校验+帧尾

    ProtocolResult parseQueryKeyResponse(const QByteArray &data);
    ProtocolResult parseOpenKeyResponse(const QByteArray &data);
    ProtocolResult parseQueryAllResponse(const QByteArray &data);
};

#endif // KEYCABINETPROTOCOL_H
