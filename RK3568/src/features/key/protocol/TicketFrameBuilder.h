/**
 * @file TicketFrameBuilder.h
 * @brief 传票外层帧构建器（payload -> 单帧/多帧）
 */
#ifndef TICKETFRAMEBUILDER_H
#define TICKETFRAMEBUILDER_H

#include <QByteArray>
#include <QList>
#include <QStringList>

class TicketFrameBuilder
{
public:
    static constexpr quint8 FRAME_HEADER_0 = 0x7E;
    static constexpr quint8 FRAME_HEADER_1 = 0x6C;
    static constexpr quint8 CMD_TICKET = 0x03;
    static constexpr quint8 CMD_TICKET_MORE = 0x83;

    explicit TicketFrameBuilder(quint8 stationId = 0x01,
                                quint8 deviceByte = 0x00,
                                quint8 keyId = 0x00);

    void setStationId(quint8 id) { m_stationId = id; }
    void setDeviceByte(quint8 dev) { m_deviceByte = dev; }
    void setKeyId(quint8 kid) { m_keyId = kid; }
    void setMaxChunkSize(int sz) { m_maxChunkSize = sz; }

    QList<QByteArray> buildFrames(const QByteArray &payload) const;
    void formatFrameLog(const QList<QByteArray> &frames,
                        QStringList &logLines,
                        QStringList &hexLines) const;

    static quint16 calcCrc(const QByteArray &data);

private:
    QByteArray buildOneFrame(quint8 cmd, quint8 seqLo, quint8 seqHi,
                             const QByteArray &chunk) const;

    quint8 m_stationId;
    quint8 m_deviceByte;
    quint8 m_keyId;
    int    m_maxChunkSize;
    mutable quint8 m_frameNo;
};

#endif // TICKETFRAMEBUILDER_H
