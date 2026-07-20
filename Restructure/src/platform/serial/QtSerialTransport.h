/**
 * @file QtSerialTransport.h
 * @brief ISerialTransport 的 QSerialPort 实现
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
