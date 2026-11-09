#include "features/key/protocol/TicketFrameBuilder.h"
#include "KeyCrc16.h"



quint16 TicketFrameBuilder::calcCrc(const QByteArray &data)
{
    return KeyCrc16::calc(data);
}

TicketFrameBuilder::TicketFrameBuilder(quint8 stationId, quint8 deviceByte,
                                       quint8 keyId)
    : m_stationId(stationId)
    , m_deviceByte(deviceByte)
    , m_keyId(keyId)
    , m_maxChunkSize(512)
    , m_frameNo(0)
{
}

QList<QByteArray> TicketFrameBuilder::buildFrames(const QByteArray &payload) const
{
    QList<QByteArray> frames;
    const int total = payload.size();
    const int chunkSize = m_maxChunkSize;

    if (total == 0) {
        frames.append(buildOneFrame(CMD_TICKET, 0, 0, QByteArray()));
        m_frameNo++;
        return frames;
    }

    int offset = 0;
    quint16 seq = 0;
    while (offset < total) {
        const int remaining = total - offset;
        const int thisChunk = qMin(chunkSize, remaining);
        const bool isLast = (offset + thisChunk >= total);
        const quint8 cmd = isLast ? CMD_TICKET : CMD_TICKET_MORE;
        const quint8 seqLo = static_cast<quint8>(seq & 0xFF);
        const quint8 seqHi = static_cast<quint8>((seq >> 8) & 0xFF);
        frames.append(buildOneFrame(cmd, seqLo, seqHi, payload.mid(offset, thisChunk)));
        offset += thisChunk;
        ++seq;
        ++m_frameNo;
    }

    return frames;
}

QByteArray TicketFrameBuilder::buildOneFrame(quint8 cmd, quint8 seqLo,
                                             quint8 seqHi,
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
    frame.reserve(2 + 1 + 1 + 1 + 1 + 2 + 1 + 2 + chunk.size() + 2);
    frame.append(static_cast<char>(FRAME_HEADER_0));
    frame.append(static_cast<char>(FRAME_HEADER_1));
    frame.append(static_cast<char>(m_keyId));
    frame.append(static_cast<char>(m_frameNo));
    frame.append(static_cast<char>(m_stationId));
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

void TicketFrameBuilder::formatFrameLog(const QList<QByteArray> &frames,
                                        QStringList &logLines,
                                        QStringList &hexLines) const
{
    logLines.clear();
    hexLines.clear();
    logLines << QString::fromUtf8("[TICKET_FRAME_GEN] frames=%1").arg(frames.size());
    for (int i = 0; i < frames.size(); ++i) {
        const QByteArray &f = frames[i];
        QStringList hexParts;
        for (int b = 0; b < f.size(); ++b) {
            hexParts << QString::fromLatin1("%1")
                        .arg(static_cast<quint8>(f[b]), 2, 16, QLatin1Char('0'))
                        .toUpper();
        }
        const QString hexLine = QString::fromUtf8("SEN_HEX[%1]: ").arg(i) + hexParts.join(' ');
        logLines << hexLine;
        hexLines << hexLine;
    }
}
