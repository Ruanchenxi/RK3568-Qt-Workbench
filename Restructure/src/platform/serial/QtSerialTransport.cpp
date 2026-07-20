/**
 * @file QtSerialTransport.cpp
 * @brief ISerialTransport 的 QSerialPort 实现
 */
#include "platform/serial/QtSerialTransport.h"

QtSerialTransport::QtSerialTransport(QObject *parent)
    : ISerialTransport(parent)
{
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
    if (m_serial.isOpen()) {
        m_serial.close();
    }

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
    return m_serial.write(data);
}

QByteArray QtSerialTransport::readAll()
{
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
