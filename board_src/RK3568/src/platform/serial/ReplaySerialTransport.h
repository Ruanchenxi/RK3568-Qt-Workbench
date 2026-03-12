/**
 * @file ReplaySerialTransport.h
 * @brief 串口回放传输实现（无硬件验证用）
 *
 * 支持动作：
 * 1. open
 * 2. chunk(hex, delayMs)
 * 3. timeout(waitMs)
 * 4. error(code, text)
 */
#ifndef REPLAYSERIALTRANSPORT_H
#define REPLAYSERIALTRANSPORT_H

#include <QQueue>
#include <QTimer>
#include <QVariantMap>

#include "platform/serial/ISerialTransport.h"

class ReplaySerialTransport : public ISerialTransport
{
    Q_OBJECT
public:
    explicit ReplaySerialTransport(QObject *parent = nullptr);
    ~ReplaySerialTransport() override = default;

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

    bool loadScript(const QString &path, QString *error);
    void clearScript();

private slots:
    void runNextAction();

private:
    QByteArray parseHex(const QString &hex) const;

    QString m_portName;
    bool m_opened;
    QByteArray m_readBuffer;
    QQueue<QVariantMap> m_actions;
    QTimer m_timer;
};

#endif // REPLAYSERIALTRANSPORT_H
