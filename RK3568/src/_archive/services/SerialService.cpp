/**
 * @file SerialService.cpp
 * @brief 串口通讯服务实现
 *
 * 注意：要使用串口功能，需要在 .pro 文件中添加：
 * QT += serialport
 */

#include "SerialService.h"
#include "LogService.h"

// 如果没有串口模块，使用条件编译
#ifdef QT_SERIALPORT_LIB
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

SerialService *SerialService::s_instance = nullptr;

SerialService::SerialService(QObject *parent)
    : QObject(parent), m_cardPollTimer(nullptr), m_cardListenerActive(false)
{
    // 初始化读卡轮询定时器
    m_cardPollTimer = new QTimer(this);
    m_cardPollTimer->setInterval(500); // 每500ms检测一次
    connect(m_cardPollTimer, &QTimer::timeout, [this]()
            {
        if (m_cardListenerActive) {
            QString cardId = readCardId();
            if (!cardId.isEmpty()) {
                emit cardDetected(cardId);
            }
        } });

    LOG_INFO("SerialService", "串口通讯服务初始化完成");
}

SerialService::~SerialService()
{
    closeAllPorts();
}

SerialService *SerialService::instance()
{
    if (!s_instance)
    {
        s_instance = new SerialService();
    }
    return s_instance;
}

bool SerialService::openPort(SerialDeviceType deviceType, const QString &portName, int baudRate)
{
#ifdef QT_SERIALPORT_LIB
    // 如果已打开，先关闭
    if (m_ports.contains(deviceType))
    {
        closePort(deviceType);
    }

    QSerialPort *port = new QSerialPort(this);
    port->setPortName(portName);
    port->setBaudRate(baudRate);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);

    if (port->open(QIODevice::ReadWrite))
    {
        m_ports[deviceType] = port;

        // 连接信号
        connect(port, &QSerialPort::readyRead, this, &SerialService::onReadyRead);
        connect(port, &QSerialPort::errorOccurred, this, &SerialService::onErrorOccurred);

        LOG_INFO("SerialService", QString("打开串口成功: %1, 设备: %2, 波特率: %3")
                                      .arg(portName)
                                      .arg(deviceTypeToString(deviceType))
                                      .arg(baudRate));

        emit deviceConnectionChanged(deviceType, true);
        return true;
    }
    else
    {
        QString error = port->errorString();
        delete port;

        LOG_ERROR("SerialService", QString("打开串口失败: %1, 错误: %2")
                                       .arg(portName)
                                       .arg(error));

        emit communicationError(deviceType, error);
        return false;
    }
#else
    // 没有串口模块，模拟成功
    LOG_WARNING("SerialService", QString("串口模块未启用，模拟打开: %1").arg(portName));
    return true;
#endif
}

void SerialService::closePort(SerialDeviceType deviceType)
{
#ifdef QT_SERIALPORT_LIB
    if (m_ports.contains(deviceType))
    {
        QSerialPort *port = m_ports[deviceType];
        if (port->isOpen())
        {
            port->close();
        }
        delete port;
        m_ports.remove(deviceType);

        LOG_INFO("SerialService", QString("关闭串口: %1").arg(deviceTypeToString(deviceType)));
        emit deviceConnectionChanged(deviceType, false);
    }
#endif
}

void SerialService::closeAllPorts()
{
    QList<SerialDeviceType> types = m_ports.keys();
    for (SerialDeviceType type : types)
    {
        closePort(type);
    }
}

bool SerialService::isPortOpen(SerialDeviceType deviceType) const
{
#ifdef QT_SERIALPORT_LIB
    if (m_ports.contains(deviceType))
    {
        return m_ports[deviceType]->isOpen();
    }
    return false;
#else
    return false;
#endif
}

QStringList SerialService::availablePorts() const
{
    QStringList ports;
#ifdef QT_SERIALPORT_LIB
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
    {
        ports << info.portName();
    }
#else
    // 模拟端口列表
    ports << "COM1" << "COM2" << "COM3" << "COM4";
#endif
    return ports;
}

qint64 SerialService::sendData(SerialDeviceType deviceType, const QByteArray &data)
{
#ifdef QT_SERIALPORT_LIB
    if (!m_ports.contains(deviceType) || !m_ports[deviceType]->isOpen())
    {
        LOG_ERROR("SerialService", QString("发送失败：串口未打开 - %1")
                                       .arg(deviceTypeToString(deviceType)));
        return -1;
    }

    qint64 written = m_ports[deviceType]->write(data);

    // 记录通讯日志
    CommPacket packet;
    packet.deviceType = deviceType;
    packet.portName = m_ports[deviceType]->portName();
    packet.data = data;
    packet.isOutgoing = true;
    packet.timestamp = QDateTime::currentDateTime();
    packet.success = (written == data.size());

    logPacket(packet);
    emit packetTransmitted(packet);

    return written;
#else
    LOG_DEBUG("SerialService", QString("模拟发送数据: %1 字节").arg(data.size()));
    return data.size();
#endif
}

QByteArray SerialService::sendCommand(SerialDeviceType deviceType, const QByteArray &command, int timeout)
{
    Q_UNUSED(timeout);

    sendData(deviceType, command);

#ifdef QT_SERIALPORT_LIB
    if (m_ports.contains(deviceType) && m_ports[deviceType]->isOpen())
    {
        if (m_ports[deviceType]->waitForReadyRead(timeout))
        {
            return m_ports[deviceType]->readAll();
        }
    }
#endif

    return QByteArray();
}

// ========== 钥匙柜专用接口 ==========

bool SerialService::queryKeyStatus(int slotId)
{
    // TODO: 根据实际协议实现
    // 示例命令格式：[帧头][命令][槽位号][校验]
    QByteArray command;
    command.append(static_cast<char>(0xAA)); // 帧头
    command.append(static_cast<char>(0x01)); // 查询命令
    command.append(static_cast<char>(slotId));
    command.append(static_cast<char>(0xAA ^ 0x01 ^ slotId)); // 校验

    QByteArray response = sendCommand(SerialDeviceType::KeyCabinet, command, 1000);

    // TODO: 解析响应
    // 这里返回模拟数据
    LOG_DEBUG("SerialService", QString("查询钥匙槽位 %1 状态").arg(slotId));
    return true;
}

bool SerialService::openKeySlot(int slotId)
{
    // TODO: 根据实际协议实现
    QByteArray command;
    command.append(static_cast<char>(0xAA));
    command.append(static_cast<char>(0x02)); // 开锁命令
    command.append(static_cast<char>(slotId));
    command.append(static_cast<char>(0xAA ^ 0x02 ^ slotId));

    QByteArray response = sendCommand(SerialDeviceType::KeyCabinet, command, 2000);

    LOG_AUDIT("开启钥匙槽", QString("槽位%1").arg(slotId), true, "");
    return true;
}

QMap<int, bool> SerialService::queryAllKeyStatus()
{
    QMap<int, bool> status;
    // TODO: 根据实际协议批量查询
    // 模拟8个槽位
    for (int i = 1; i <= 8; ++i)
    {
        status[i] = queryKeyStatus(i);
    }
    return status;
}

// ========== 读卡器专用接口 ==========

QString SerialService::readCardId()
{
    // TODO: 根据实际协议实现
    // 这里返回空表示无卡
    return QString();
}

void SerialService::startCardListener()
{
    m_cardListenerActive = true;
    m_cardPollTimer->start();
    LOG_INFO("SerialService", "启动读卡监听");
}

void SerialService::stopCardListener()
{
    m_cardListenerActive = false;
    m_cardPollTimer->stop();
    LOG_INFO("SerialService", "停止读卡监听");
}

// ========== 私有槽函数 ==========

void SerialService::onReadyRead()
{
#ifdef QT_SERIALPORT_LIB
    QSerialPort *port = qobject_cast<QSerialPort *>(sender());
    if (!port)
        return;

    QByteArray data = port->readAll();

    // 找到设备类型
    SerialDeviceType deviceType = SerialDeviceType::Unknown;
    for (auto it = m_ports.begin(); it != m_ports.end(); ++it)
    {
        if (it.value() == port)
        {
            deviceType = it.key();
            break;
        }
    }

    // 记录通讯日志
    CommPacket packet;
    packet.deviceType = deviceType;
    packet.portName = port->portName();
    packet.data = data;
    packet.isOutgoing = false;
    packet.timestamp = QDateTime::currentDateTime();
    packet.success = true;

    logPacket(packet);
    emit packetTransmitted(packet);
    emit dataReceived(deviceType, data);
#endif
}

void SerialService::onErrorOccurred()
{
#ifdef QT_SERIALPORT_LIB
    QSerialPort *port = qobject_cast<QSerialPort *>(sender());
    if (!port)
        return;

    // 找到设备类型
    SerialDeviceType deviceType = SerialDeviceType::Unknown;
    for (auto it = m_ports.begin(); it != m_ports.end(); ++it)
    {
        if (it.value() == port)
        {
            deviceType = it.key();
            break;
        }
    }

    QString error = port->errorString();
    LOG_ERROR("SerialService", QString("串口错误 [%1]: %2")
                                   .arg(deviceTypeToString(deviceType))
                                   .arg(error));

    emit communicationError(deviceType, error);
#endif
}

void SerialService::logPacket(const CommPacket &packet)
{
    QString direction = packet.isOutgoing ? "发送" : "接收";
    QString hexData = packet.data.toHex(' ').toUpper();

    // 限制显示长度
    if (hexData.length() > 100)
    {
        hexData = hexData.left(100) + "...";
    }

    LOG_DEBUG("COMM", QString("[%1][%2] %3: %4")
                          .arg(deviceTypeToString(packet.deviceType))
                          .arg(packet.portName)
                          .arg(direction)
                          .arg(hexData));
}

QString SerialService::deviceTypeToString(SerialDeviceType type) const
{
    switch (type)
    {
    case SerialDeviceType::KeyCabinet:
        return "钥匙柜";
    case SerialDeviceType::CardReader:
        return "读卡器";
    case SerialDeviceType::Fingerprint:
        return "指纹仪";
    default:
        return "未知设备";
    }
}
