#include "features/key/protocol/KeyDataFrameBuilder.h"
#include "KeyCrc16.h"



quint16 KeyDataFrameBuilder::calcCrc(const QByteArray &data)
{
    return KeyCrc16::calc(data);
}

KeyDataFrameBuilder::KeyDataFrameBuilder(quint8 keyId, quint8 stationByte, quint8 deviceByte)
    : m_keyId(keyId)
    , m_stationByte(stationByte)
    , m_deviceByte(deviceByte)
    , m_maxChunkSize(512)
    , m_frameNo(0)
{
}

QList<QByteArray> KeyDataFrameBuilder::buildFrames(quint8 baseCmd, const QByteArray &payload) const
{
    QList<QByteArray> frames;
    if (payload.isEmpty()) {
        return frames;
    }

    int offset = 0;
    quint16 seq = 0;
    while (offset < payload.size()) {
        const int remaining = payload.size() - offset;
        const int chunkSize = qMin(m_maxChunkSize, remaining);
        const bool isLast = (offset + chunkSize >= payload.size());
        const quint8 cmd = isLast ? baseCmd : static_cast<quint8>(baseCmd | 0x80);
        frames.append(buildOneFrame(cmd,
                                    static_cast<quint8>(seq & 0xFF),
                                    static_cast<quint8>((seq >> 8) & 0xFF),
                                    payload.mid(offset, chunkSize)));
        offset += chunkSize;
        ++seq;
        ++m_frameNo;
    }

    return frames;
}

QByteArray KeyDataFrameBuilder::buildOneFrame(quint8 cmd, quint8 seqLo, quint8 seqHi,
                                              const QByteArray &chunk) const
{
    QByteArray crcData;
    crcData.reserve(1 + 2 + chunk.size());
    crcData.append(static_cast<char>(cmd));
    crcData.append(static_cast<char>(seqLo));
    crcData.append(static_cast<char>(seqHi));
    crcData.append(chunk);

    const quint16 crc = calcCrc(crcData);
    const quint16 len = static_cast<quint16>(1 + 2 + chunk.size());

    QByteArray frame;
    frame.reserve(11 + 2 + chunk.size());
    frame.append(static_cast<char>(FRAME_HEADER_0));
    frame.append(static_cast<char>(FRAME_HEADER_1));
    frame.append(static_cast<char>(m_keyId));
    frame.append(static_cast<char>(m_frameNo));
    frame.append(static_cast<char>(m_stationByte));
    frame.append(static_cast<char>(m_deviceByte));
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(cmd));
    frame.append(static_cast<char>(seqLo));
    frame.append(static_cast<char>(seqHi));
    frame.append(chunk);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    frame.append(static_cast<char>(crc & 0xFF));
    return frame;
}
