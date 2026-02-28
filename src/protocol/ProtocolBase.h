/**
 * @file ProtocolBase.h
 * @brief 通讯协议基类 - 所有设备协议的抽象基类
 *
 * 使用方法：
 * 1. 继承此类
 * 2. 实现 parseResponse() 解析响应
 * 3. 实现 buildCommand() 构建命令
 * 4. 根据需要重写校验方法
 */

#ifndef PROTOCOLBASE_H
#define PROTOCOLBASE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QMap>

/**
 * @brief 协议解析结果
 */
struct ProtocolResult
{
    bool success;         // 解析是否成功
    QString errorMessage; // 错误信息
    int commandType;      // 命令类型
    QVariantMap data;     // 解析出的数据
    QByteArray rawData;   // 原始数据

    ProtocolResult() : success(false), commandType(0) {}
};

/**
 * @class ProtocolBase
 * @brief 通讯协议抽象基类
 */
class ProtocolBase : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolBase(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ProtocolBase() {}

    /**
     * @brief 获取协议名称
     */
    virtual QString protocolName() const = 0;

    /**
     * @brief 获取协议版本
     */
    virtual QString protocolVersion() const { return "1.0"; }

    /**
     * @brief 解析响应数据
     * @param data 原始响应数据
     * @return 解析结果
     */
    virtual ProtocolResult parseResponse(const QByteArray &data) = 0;

    /**
     * @brief 构建命令数据包
     * @param commandType 命令类型
     * @param params 命令参数
     * @return 完整的命令数据包
     */
    virtual QByteArray buildCommand(int commandType, const QVariantMap &params = QVariantMap()) = 0;

    /**
     * @brief 校验数据完整性
     * @param data 数据
     * @return 是否有效
     */
    virtual bool validateChecksum(const QByteArray &data)
    {
        Q_UNUSED(data);
        return true;
    }

    /**
     * @brief 计算校验和
     */
    virtual quint8 calculateChecksum(const QByteArray &data)
    {
        quint8 sum = 0;
        for (int i = 0; i < data.size(); ++i)
        {
            sum ^= static_cast<quint8>(data[i]);
        }
        return sum;
    }

    /**
     * @brief 检查是否是完整数据包
     * @param data 数据
     * @return 完整数据包的长度，0表示不完整，-1表示无效
     */
    virtual int isCompletePacket(const QByteArray &data)
    {
        Q_UNUSED(data);
        return data.size();
    }

    /**
     * @brief 字节数组转十六进制字符串（用于调试）
     */
    static QString toHexString(const QByteArray &data, const QString &separator = " ")
    {
        QString result;
        for (int i = 0; i < data.size(); ++i)
        {
            if (i > 0)
                result += separator;
            result += QString("%1").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
        }
        return result;
    }

signals:
    /**
     * @brief 协议事件信号
     */
    void protocolEvent(const QString &eventType, const QVariantMap &eventData);
};

#endif // PROTOCOLBASE_H
