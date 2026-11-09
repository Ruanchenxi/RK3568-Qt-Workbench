/**
 * @file QtSerialTransport.cpp
 * @brief ISerialTransport 的 QSerialPort 实现
 */
#include "platform/serial/QtSerialTransport.h"

QtSerialTransport::QtSerialTransport(QObject *parent)
    : ISerialTransport(parent)
{
    // [LEARNING][串口传输入口]
    // 构造阶段只登记“底层串口以后有数据/错误时，向上层转发通知”的规则；
    // 此时还没有打开串口，也没有主动读取数据。m_serial 是本对象内部的 QSerialPort 成员对象。
    connect(&m_serial, &QSerialPort::readyRead, this, &QtSerialTransport::readyRead);
    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) {
            return;
        }
        emit errorOccurred(static_cast<int>(error), m_serial.errorString());
    });
}

void QtSerialTransport::setPortName(const QString &name)
{
    m_serial.setPortName(name);
}

QString QtSerialTransport::portName() const
{
    return m_serial.portName();
}

bool QtSerialTransport::open(int baudRate)
{
    // [LEARNING][打开串口]
    // 输入 baudRate，配置串口参数并以读写方式打开；输出 bool 表示本次打开是否成功。
    // 115200 等具体速率由上层传入，本层固定使用 8 数据位、无校验、1 停止位、无流控。
    if (m_serial.isOpen()) {
        m_serial.close();
    }

    //配置串口
    m_serial.setBaudRate(baudRate);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    
    const bool ok = m_serial.open(QIODevice::ReadWrite);
    if (ok) {
        // Apply cached line states (some callers set DTR/RTS before open).
        m_serial.setDataTerminalReady(m_dtr);
        m_serial.setRequestToSend(m_rts);
    }
    emit openedChanged(ok);
    if (!ok) {
        emit errorOccurred(static_cast<int>(QSerialPort::OpenError), m_serial.errorString());
    }
    return ok;
}

void QtSerialTransport::close()
{
    if (!m_serial.isOpen()) {
        return;
    }
    m_serial.close();
    emit openedChanged(false);
}

bool QtSerialTransport::isOpen() const
{
    return m_serial.isOpen();
}

qint64 QtSerialTransport::write(const QByteArray &data)
{
    // [LEARNING][发送原始字节]
    // data 是上层已经组装好的协议帧；本层不解释命令、CRC 或业务含义，只原样交给 QSerialPort。
    // 返回值表示本次被 Qt 串口层接收用于发送的字节数，不等于设备已经处理成功。
    return m_serial.write(data);
}

QByteArray QtSerialTransport::readAll()
{
    // [LEARNING][读取当前已到达的字节]
    // readyRead 只表示“现在有字节可读”；调用者随后通过本函数取走当前可读数据。
    // 一次结果可能是半帧、一帧或多帧，完整协议帧由上层 KeySerialClient 累积并拆分。
    return m_serial.readAll();
}

qint64 QtSerialTransport::bytesAvailable() const
{
    return m_serial.bytesAvailable();
}

bool QtSerialTransport::flush()
{
    return m_serial.flush();
}

bool QtSerialTransport::waitForBytesWritten(int msecs)
{
    return m_serial.waitForBytesWritten(msecs);
}

QString QtSerialTransport::errorString() const
{
    return m_serial.errorString();
}

bool QtSerialTransport::setDataTerminalReady(bool set)
{
    m_dtr = set;
    if (!m_serial.isOpen()) {
        return true;
    }
    return m_serial.setDataTerminalReady(set);
}

bool QtSerialTransport::setRequestToSend(bool set)
{
    m_rts = set;
    if (!m_serial.isOpen()) {
        return true;
    }
    return m_serial.setRequestToSend(set);
}

bool QtSerialTransport::isDataTerminalReady()
{
    return m_dtr;
}

bool QtSerialTransport::isRequestToSend()
{
    return m_rts;
}
