/**
 * @file ISerialTransport.h
 * @brief 串口传输抽象接口
 *
 * 说明：
 * 1. 协议层不直接依赖 QSerialPort，便于替换为回放/仿真实现。
 * 2. 接口最小化，仅覆盖当前协议客户端必需能力。
 */
#ifndef ISERIALTRANSPORT_H
#define ISERIALTRANSPORT_H

#include <QObject>
#include <QByteArray>
#include <QString>

class ISerialTransport : public QObject
{
    Q_OBJECT
public:
    explicit ISerialTransport(QObject *parent = nullptr) : QObject(parent) {}
    ~ISerialTransport() override = default;

    virtual void setPortName(const QString &name) = 0;
    virtual QString portName() const = 0;
    virtual bool open(int baudRate) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual qint64 write(const QByteArray &data) = 0;
    virtual QByteArray readAll() = 0;
    virtual qint64 bytesAvailable() const = 0;
    virtual bool flush() = 0;
    virtual bool waitForBytesWritten(int msecs) = 0;
    virtual QString errorString() const = 0;
    virtual bool setDataTerminalReady(bool set) = 0;
    virtual bool setRequestToSend(bool set) = 0;
    virtual bool isDataTerminalReady() = 0;
    virtual bool isRequestToSend() = 0;

signals:
    void readyRead();
    void errorOccurred(int code, const QString &message);
    void openedChanged(bool opened);
};

#endif // ISERIALTRANSPORT_H
