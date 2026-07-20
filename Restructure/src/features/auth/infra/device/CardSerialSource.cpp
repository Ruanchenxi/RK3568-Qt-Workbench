/**
 * @file CardSerialSource.cpp
 * @brief 读卡串口采集源实现
 */
#include "features/auth/infra/device/CardSerialSource.h"

#include "core/ConfigManager.h"
#include "platform/serial/ISerialTransport.h"
#include "platform/serial/QtSerialTransport.h"

#include <QDateTime>
#include <QTimer>

namespace
{
constexpr int kCardReaderBaudRate = 9600;
constexpr int kFrameSize = 8;
constexpr int kReopenIntervalMs = 1500;
constexpr qint64 kSameCardDedupWindowMs = 1500;
}

CardSerialSource::CardSerialSource(QObject *parent)
    : ICredentialSource(parent)
    , m_transport(new QtSerialTransport(this))
    , m_reopenTimer(new QTimer(this))
    , m_lastEmitMs(0)
    , m_started(false)
    , m_openReported(false)
    , m_lastStatus(Idle)
{
    m_reopenTimer->setSingleShot(true);
    m_reopenTimer->setInterval(kReopenIntervalMs);

    connect(m_transport, &ISerialTransport::readyRead,
            this, &CardSerialSource::onTransportReadyRead);
    connect(m_transport, &ISerialTransport::errorOccurred,
            this, &CardSerialSource::onTransportError);
    connect(m_reopenTimer, &QTimer::timeout,
            this, &CardSerialSource::onReopenTimeout);
}

AuthMode CardSerialSource::mode() const
{
    return AuthMode::Card;
}

void CardSerialSource::start()
{
    if (m_started)
    {
        return;
    }

    m_started = true;
    m_openReported = false;
    m_lastCardNo.clear();
    m_lastEmitMs = 0;
    resetBuffer();
    reportStatus(Detecting, QStringLiteral("检测中"));
    openTransport();
}

void CardSerialSource::stop()
{
    m_started = false;
    m_reopenTimer->stop();
    resetBuffer();

    if (m_transport->isOpen())
    {
        m_transport->close();
    }

    reportStatus(Idle, QStringLiteral("空闲"));
}

void CardSerialSource::onTransportReadyRead()
{
    m_buffer.append(m_transport->readAll());
    processBuffer();
}

void CardSerialSource::onTransportError(int code, const QString &message)
{
    Q_UNUSED(code);

    if (!m_started)
    {
        return;
    }

    if (m_transport->isOpen())
    {
        m_transport->close();
    }

    scheduleReopen(message.trimmed().isEmpty()
                       ? QStringLiteral("读卡串口发生异常")
                       : QStringLiteral("读卡串口异常：%1").arg(message.trimmed()));
}

void CardSerialSource::onReopenTimeout()
{
    openTransport();
}

void CardSerialSource::openTransport()
{
    if (!m_started)
    {
        return;
    }

    const QString portName = configuredPortName();
    if (portName.isEmpty())
    {
        reportStatus(Unconfigured, QStringLiteral("未配置串口"));
        emit sourceError(QStringLiteral("未配置读卡器串口"));
        return;
    }

    m_transport->setPortName(portName);
    m_transport->setDataTerminalReady(true);
    m_transport->setRequestToSend(true);
    reportStatus(Detecting, QStringLiteral("检测中"));

    if (!m_transport->open(kCardReaderBaudRate))
    {
        scheduleReopen(QStringLiteral("读卡串口打开失败：%1").arg(m_transport->errorString()));
        return;
    }

    m_openReported = false;
    resetBuffer();
    if (m_transport->bytesAvailable() > 0)
    {
        m_transport->readAll();
    }

    reportStatus(Ready, QStringLiteral("就绪"));
}

void CardSerialSource::scheduleReopen(const QString &reason)
{
    if (!m_started)
    {
        return;
    }

    if (!reason.trimmed().isEmpty() && !m_openReported)
    {
        reportStatus(Error, reason.trimmed());
        emit sourceError(reason);
        m_openReported = true;
    }

    if (!m_reopenTimer->isActive())
    {
        m_reopenTimer->start();
    }
}

void CardSerialSource::processBuffer()
{
    while (!m_buffer.isEmpty())
    {
        const int headIndex = m_buffer.indexOf(static_cast<char>(0xAA));
        if (headIndex < 0)
        {
            resetBuffer();
            return;
        }

        if (headIndex > 0)
        {
            m_buffer.remove(0, headIndex);
        }

        if (m_buffer.size() < kFrameSize)
        {
            return;
        }

        const QByteArray frame = m_buffer.left(kFrameSize);
        if (!isSupportedFrame(frame))
        {
            m_buffer.remove(0, 1);
            continue;
        }

        m_buffer.remove(0, kFrameSize);

        const QString cardNo = extractCardNo(frame);
        if (cardNo.isEmpty())
        {
            continue;
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (cardNo == m_lastCardNo && (nowMs - m_lastEmitMs) < kSameCardDedupWindowMs)
        {
            continue;
        }

        m_lastCardNo = cardNo;
        m_lastEmitMs = nowMs;

        CardCredential credential;
        credential.tenantId = ConfigManager::instance()->tenantCode().trimmed();
        if (credential.tenantId.isEmpty())
        {
            credential.tenantId = QStringLiteral("000000");
        }
        credential.cardNo = cardNo;
        emit cardCaptured(credential);
    }
}

void CardSerialSource::resetBuffer()
{
    m_buffer.clear();
}

void CardSerialSource::reportStatus(ReaderStatus status, const QString &message)
{
    const QString normalizedMessage = message.trimmed();
    if (m_lastStatus == status && m_lastStatusMessage == normalizedMessage)
    {
        return;
    }

    m_lastStatus = status;
    m_lastStatusMessage = normalizedMessage;
    emit readerStatusChanged(static_cast<int>(status), normalizedMessage);
}

QString CardSerialSource::configuredPortName() const
{
    QString portName = ConfigManager::instance()->cardSerialPort().trimmed();
#ifdef Q_OS_LINUX
    if (portName.isEmpty())
    {
        portName = QStringLiteral("/dev/ttyS3");
    }
#endif
    return portName;
}

bool CardSerialSource::isSupportedFrame(const QByteArray &frame) const
{
    if (frame.size() != kFrameSize)
    {
        return false;
    }

    const unsigned char head = static_cast<unsigned char>(frame.at(0));
    const unsigned char eventCode = static_cast<unsigned char>(frame.at(1));
    return head == 0xAA && eventCode >= 0x30 && eventCode <= 0x33;
}

QString CardSerialSource::extractCardNo(const QByteArray &frame) const
{
    if (!isSupportedFrame(frame))
    {
        return QString();
    }

    return QString::fromLatin1(frame.mid(2, 6).toHex());
}
