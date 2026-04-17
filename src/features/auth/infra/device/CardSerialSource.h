/**
 * @file CardSerialSource.h
 * @brief 读卡串口采集源（/dev/ttyS3 刷卡登录）
 */
#ifndef CARD_SERIAL_SOURCE_H
#define CARD_SERIAL_SOURCE_H

#include <QByteArray>

#include "features/auth/domain/ports/ICredentialSource.h"

class ISerialTransport;
class QTimer;

class CardSerialSource : public ICredentialSource
{
    Q_OBJECT
public:
    enum ReaderStatus
    {
        Idle = 0,
        Detecting = 1,
        Ready = 2,
        Error = 3,
        Unconfigured = 4
    };
    Q_ENUM(ReaderStatus)

    explicit CardSerialSource(QObject *parent = nullptr);
    ~CardSerialSource() override = default;

    AuthMode mode() const override;
    void start() override;
    void stop() override;

signals:
    void readerStatusChanged(int status, const QString &message);

private slots:
    void onTransportReadyRead();
    void onTransportError(int code, const QString &message);
    void onReopenTimeout();

private:
    void openTransport();
    void scheduleReopen(const QString &reason);
    void processBuffer();
    void resetBuffer();
    void reportStatus(ReaderStatus status, const QString &message);
    QString configuredPortName() const;
    bool isSupportedFrame(const QByteArray &frame) const;
    QString extractCardNo(const QByteArray &frame) const;

    ISerialTransport *m_transport;
    QTimer *m_reopenTimer;
    QByteArray m_buffer;
    QString m_lastCardNo;
    qint64 m_lastEmitMs;
    bool m_started;
    bool m_openReported;
    ReaderStatus m_lastStatus;
    QString m_lastStatusMessage;
};

#endif // CARD_SERIAL_SOURCE_H
