/**
 * @file KeyDataFrameBuilder.h
 * @brief INIT/DN_RFID 数据文件外层帧构建器
 */
#ifndef KEYDATAFRAMEBUILDER_H
#define KEYDATAFRAMEBUILDER_H

#include <QByteArray>
#include <QList>

class KeyDataFrameBuilder
{
public:
    static constexpr quint8 FRAME_HEADER_0 = 0x7E;
    static constexpr quint8 FRAME_HEADER_1 = 0x6C;
    static constexpr quint8 DEFAULT_KEY_ID = 0x00;
    static constexpr quint8 DEFAULT_STATION_BYTE = 0xFF;
    static constexpr quint8 DEFAULT_DEVICE_BYTE = 0x00;

    explicit KeyDataFrameBuilder(quint8 keyId = DEFAULT_KEY_ID,
                                 quint8 stationByte = DEFAULT_STATION_BYTE,
                                 quint8 deviceByte = DEFAULT_DEVICE_BYTE);

    QList<QByteArray> buildFrames(quint8 baseCmd, const QByteArray &payload) const;
    void setMaxChunkSize(int bytes) { m_maxChunkSize = bytes; }

    static quint16 calcCrc(const QByteArray &data);

private:
    QByteArray buildOneFrame(quint8 cmd, quint8 seqLo, quint8 seqHi,
                             const QByteArray &chunk) const;

    quint8 m_keyId;
    quint8 m_stationByte;
    quint8 m_deviceByte;
    int m_maxChunkSize;
    mutable quint8 m_frameNo;
};

#endif // KEYDATAFRAMEBUILDER_H
