/**
 * @file SerialService.h
 * @brief 串口通讯服务 - 管理所有串口设备通讯
 * @author RK3568项目组
 * @date 2026-02-01
 *
 * 支持的设备：
 * - 钥匙柜控制器
 * - IC卡读卡器
 * - 指纹仪
 *
 * 后续添加通讯协议时，只需要：
 * 1. 在 protocol/ 目录创建协议解析类
 * 2. 在此服务中注册协议处理器
 */

#ifndef SERIALSERVICE_H
#define SERIALSERVICE_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QTimer>
#include <QDateTime>

// 前向声明（Qt串口类，需要在.pro中添加 QT += serialport）
class QSerialPort;

/**
 * @brief 串口设备类型枚举
 */
enum class SerialDeviceType
{
    KeyCabinet,  // 钥匙柜
    CardReader,  // 读卡器
    Fingerprint, // 指纹仪
    Unknown
};

/**
 * @brief 通讯数据包结构体
 */
struct CommPacket
{
    SerialDeviceType deviceType; // 设备类型
    QString portName;            // 串口名称
    QByteArray data;             // 原始数据
    QString parsedMessage;       // 解析后的消息
    bool isOutgoing;             // true=发送, false=接收
    QDateTime timestamp;         // 时间戳
    bool success;                // 操作是否成功
    QString errorMessage;        // 错误信息
};

/**
 * @class SerialService
 * @brief 串口通讯服务单例类
 */
class SerialService : public QObject
{
    Q_OBJECT

private:
    explicit SerialService(QObject *parent = nullptr);
    ~SerialService();

    SerialService(const SerialService &) = delete;
    SerialService &operator=(const SerialService &) = delete;

public:
    static SerialService *instance();

    // ========== 串口管理 ==========

    /**
     * @brief 打开串口
     * @param deviceType 设备类型
     * @param portName 串口名称(如 COM1)
     * @param baudRate 波特率
     * @return 是否成功
     */
    bool openPort(SerialDeviceType deviceType, const QString &portName, int baudRate = 9600);

    /**
     * @brief 关闭串口
     */
    void closePort(SerialDeviceType deviceType);

    /**
     * @brief 关闭所有串口
     */
    void closeAllPorts();

    /**
     * @brief 检查串口是否打开
     */
    bool isPortOpen(SerialDeviceType deviceType) const;

    /**
     * @brief 获取可用串口列表
     */
    QStringList availablePorts() const;

    // ========== 数据收发 ==========

    /**
     * @brief 发送数据
     * @param deviceType 设备类型
     * @param data 要发送的数据
     * @return 发送的字节数，-1表示失败
     */
    qint64 sendData(SerialDeviceType deviceType, const QByteArray &data);

    /**
     * @brief 发送命令并等待响应（同步）
     * @param deviceType 设备类型
     * @param command 命令数据
     * @param timeout 超时时间(毫秒)
     * @return 响应数据
     */
    QByteArray sendCommand(SerialDeviceType deviceType, const QByteArray &command, int timeout = 3000);

    // ========== 钥匙柜专用接口（预留） ==========

    /**
     * @brief 查询钥匙状态
     * @param slotId 钥匙槽位号
     * @return true=钥匙在位, false=钥匙不在
     */
    bool queryKeyStatus(int slotId);

    /**
     * @brief 开启钥匙槽位
     * @param slotId 钥匙槽位号
     * @return 是否成功
     */
    bool openKeySlot(int slotId);

    /**
     * @brief 查询所有钥匙状态
     * @return 钥匙状态映射 <槽位号, 是否在位>
     */
    QMap<int, bool> queryAllKeyStatus();

    // ========== 读卡器专用接口（预留） ==========

    /**
     * @brief 读取卡号
     * @return 卡号，空表示无卡或读取失败
     */
    QString readCardId();

    /**
     * @brief 启动读卡监听
     */
    void startCardListener();

    /**
     * @brief 停止读卡监听
     */
    void stopCardListener();

signals:
    /**
     * @brief 通讯数据包信号（用于日志记录）
     */
    void packetTransmitted(const CommPacket &packet);

    /**
     * @brief 接收到数据信号
     */
    void dataReceived(SerialDeviceType deviceType, const QByteArray &data);

    /**
     * @brief 读取到卡号信号
     */
    void cardDetected(const QString &cardId);

    /**
     * @brief 指纹检测信号
     */
    void fingerprintDetected(const QString &fingerprintId);

    /**
     * @brief 钥匙状态变化信号
     */
    void keyStatusChanged(int slotId, bool isPresent);

    /**
     * @brief 通讯错误信号
     */
    void communicationError(SerialDeviceType deviceType, const QString &error);

    /**
     * @brief 设备连接状态变化
     */
    void deviceConnectionChanged(SerialDeviceType deviceType, bool connected);

private slots:
    void onReadyRead();
    void onErrorOccurred();

private:
    void logPacket(const CommPacket &packet);
    QString deviceTypeToString(SerialDeviceType type) const;

    static SerialService *s_instance;
    QMap<SerialDeviceType, QSerialPort *> m_ports;
    QTimer *m_cardPollTimer;
    bool m_cardListenerActive;
};

#endif // SERIALSERVICE_H
