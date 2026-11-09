/**
 * @file QtSerialTransport.h
 * @brief ISerialTransport 的 QSerialPort 实现
 *
 * [LEARNING GUIDE — 串口传输边界]
 *
 * 文件位置：src/platform/serial
 * - platform：与操作系统、Qt 框架和外部设备连接的基础设施，不放票据业务规则。
 * - serial：串口传输相关代码。
 * - .h：声明类“能做什么、内部保存什么”；对应 .cpp 实现“具体怎么做”。
 *
 * 在运行链中的位置：
 * 上层协议代码 -> ISerialTransport -> QtSerialTransport
 *              -> Qt 的 QSerialPort -> Linux /dev/ttyS4 或 Windows COM 口。
 *
 * 本类负责：设置端口、打开/关闭、读写原始 QByteArray 字节，以及转发数据到达和错误通知。
 * 本类不负责：协议帧格式、CRC、ACK/NAK、超时重试、Transfer/Return 等业务判断。
 *
 * [READ NOW / MAP+] 类的继承关系、构造函数、open()/write()/readAll()、m_serial。
 * [RETURN LATER] readyRead/errorOccurred 的信号转发、waitForBytesWritten()。
 * [SKIP FOR NOW] DTR/RTS、flush() 等辅助接口的逐行实现。
 */
#ifndef QTSERIALTRANSPORT_H
#define QTSERIALTRANSPORT_H

#include <QSerialPort>

#include "platform/serial/ISerialTransport.h"

class QtSerialTransport : public ISerialTransport
{
    Q_OBJECT
public:
    explicit QtSerialTransport(QObject *parent = nullptr);
    ~QtSerialTransport() override = default;

    void setPortName(const QString &name) override;
    QString portName() const override;
    bool open(int baudRate) override;
    void close() override;
    bool isOpen() const override;

    qint64 write(const QByteArray &data) override;
    QByteArray readAll() override;
    qint64 bytesAvailable() const override;
    bool flush() override;
    bool waitForBytesWritten(int msecs) override;
    QString errorString() const override;
    bool setDataTerminalReady(bool set) override;
    bool setRequestToSend(bool set) override;
    bool isDataTerminalReady() override;
    bool isRequestToSend() override;

private:
    QSerialPort m_serial;
    bool m_dtr = false;
    bool m_rts = false;
};

#endif // QTSERIALTRANSPORT_H
