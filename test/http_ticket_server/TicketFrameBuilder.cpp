#include "TicketFrameBuilder.h"

#include <QFile>
#include <QDateTime>
#include <QTextStream>

// ────────────────────────────────────────────────────────────
// CRC-16 查找表（与主工程 KeyCrc16.h 完全一致，真机验证通过）
// 算法：右移查表，初值 0x0000，多项式 0x1021
// 验证向量：[0F 01] → 0x1C11 / [04 FF×16] → 0xF326
// ────────────────────────────────────────────────────────────
static const quint16 kCrcTable[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
};

// ────────────────────────────────────────────────────────────
quint16 TicketFrameBuilder::calcCrc(const QByteArray &data)
{
    quint16 crc = 0x0000;
    for (int i = 0; i < data.size(); ++i) {
        const quint8 b = static_cast<quint8>(data[i]);
        crc = (crc >> 8) ^ kCrcTable[(b ^ (crc & 0xFF)) & 0xFF];
    }
    return crc;
}

// ────────────────────────────────────────────────────────────
TicketFrameBuilder::TicketFrameBuilder(quint8 stationId, quint8 deviceByte,
                                       quint8 keyId)
    : m_stationId(stationId)
    , m_deviceByte(deviceByte)
    , m_keyId(keyId)
    , m_maxChunkSize(512)   // 默认 **单帧模式**，第 4.1 模板帧乚可调小到 240
    , m_frameNo(0)
{
}

// ────────────────────────────────────────────────────────────
QList<QByteArray> TicketFrameBuilder::buildTemplateFrames(int payloadSize) const
{
    // 模板 payload：'TICK' magic + 0xFF 填充
    QByteArray payload(payloadSize, static_cast<char>(0xFF));
    // 写入 magic：前4字节 'T' 'I' 'C' 'K'
    if (payloadSize >= 4) {
        payload[0] = 'T';
        payload[1] = 'I';
        payload[2] = 'C';
        payload[3] = 'K';
    }
    return buildFrames(payload);
}

// ────────────────────────────────────────────────────────────
QList<QByteArray> TicketFrameBuilder::buildFrames(const QByteArray &payload) const
{
    QList<QByteArray> frames;

    const int total     = payload.size();
    const int chunkSize = m_maxChunkSize;   // 使用可配置值（默认 512 = 单帧模式）

    if (total == 0) {
        // 发单帧空 payload
        frames.append(buildOneFrame(CMD_TICKET, 0, 0, QByteArray()));
        m_frameNo++;
        return frames;
    }

    int offset = 0;
    quint16 seq = 0;

    while (offset < total) {
        int remaining = total - offset;
        int thisChunk = qMin(chunkSize, remaining);
        bool isLast = (offset + thisChunk >= total);

        quint8 cmd = isLast ? CMD_TICKET : CMD_TICKET_MORE;
        quint8 seqLo = static_cast<quint8>(seq & 0xFF);
        quint8 seqHi = static_cast<quint8>((seq >> 8) & 0xFF);

        QByteArray chunk = payload.mid(offset, thisChunk);
        frames.append(buildOneFrame(cmd, seqLo, seqHi, chunk));

        offset += thisChunk;
        seq++;
        m_frameNo++;
    }

    return frames;
}

// ────────────────────────────────────────────────────────────
QByteArray TicketFrameBuilder::buildOneFrame(quint8 cmd, quint8 seqLo,
                                              quint8 seqHi,
                                              const QByteArray &chunk) const
{
    // ---- 组装 CRC 输入区（Cmd + Seq + Payload） ----
    QByteArray crcData;
    crcData.reserve(1 + 2 + chunk.size());
    crcData.append(static_cast<char>(cmd));
    crcData.append(static_cast<char>(seqLo));
    crcData.append(static_cast<char>(seqHi));
    crcData.append(chunk);

    quint16 crc = calcCrc(crcData);

    // ---- 计算 Len = Cmd(1) + Seq(2) + ChunkLen ----
    quint16 len = static_cast<quint16>(1 + 2 + chunk.size());

    // ---- 组装完整帧 ----
    QByteArray frame;
    frame.reserve(2 + 1 + 1 + 1 + 1 + 2 + 1 + 2 + chunk.size() + 2);

    frame.append(static_cast<char>(FRAME_HEADER_0));        // 7E
    frame.append(static_cast<char>(FRAME_HEADER_1));        // 6C
    frame.append(static_cast<char>(m_keyId));               // KeyId
    frame.append(static_cast<char>(m_frameNo));             // FrameNoï¼跨调用累加）
    frame.append(static_cast<char>(m_stationId));           // Station
    frame.append(static_cast<char>(m_deviceByte));          // Device
    frame.append(static_cast<char>(len & 0xFF));            // Len_Lo
    frame.append(static_cast<char>((len >> 8) & 0xFF));     // Len_Hi
    frame.append(static_cast<char>(cmd));                   // Cmd
    frame.append(static_cast<char>(seqLo));                 // Seq_Lo
    frame.append(static_cast<char>(seqHi));                 // Seq_Hi
    frame.append(chunk);                                    // Payload
    frame.append(static_cast<char>((crc >> 8) & 0xFF));    // CRC_Hi（大端）
    frame.append(static_cast<char>(crc & 0xFF));            // CRC_Lo

    return frame;
}

// ────────────────────────────────────────────────────────────
void TicketFrameBuilder::formatFrameLog(const QList<QByteArray> &frames,
                                         QStringList &logLines,
                                         QStringList &hexLines) const
{
    logLines.clear();
    hexLines.clear();

    const QString sep = QString::fromUtf8(
        "────────────────────────────────────────────────────────────");

    logLines << QString::fromUtf8(
        "[TICKET_FRAME_GEN] keyId=0x%1  frameNo=0x%2  station=0x%3  device=0x%4  frames=%5")
        .arg(m_keyId,    2, 16, QLatin1Char('0')).toUpper()
        .arg(m_frameNo > 0 ? m_frameNo - frames.size() : 0, 2, 16, QLatin1Char('0')).toUpper()
        .arg(m_stationId, 2, 16, QLatin1Char('0')).toUpper()
        .arg(m_deviceByte, 2, 16, QLatin1Char('0')).toUpper()
        .arg(frames.size());

    // 外层帧头字段汇总（对齐原产品用）
    if (!frames.isEmpty()) {
        const QByteArray &f0 = frames[0];
        if (f0.size() >= 11) {
            quint16 lenLE = static_cast<quint8>(f0[6]) | (static_cast<quint8>(f0[7]) << 8);
            logLines << QString::fromUtf8("  外层帧头解析：")
                     << QString::fromUtf8("    KeyId   = 0x%1  (原产品目标 0x00)").arg(static_cast<quint8>(f0[2]), 2, 16, QChar('0')).toUpper()
                     << QString::fromUtf8("    FrameNo = 0x%1").arg(static_cast<quint8>(f0[3]), 2, 16, QChar('0')).toUpper()
                     << QString::fromUtf8("    Station = 0x%1").arg(static_cast<quint8>(f0[4]), 2, 16, QChar('0')).toUpper()
                     << QString::fromUtf8("    Device  = 0x%1  (原产品目标 0x00)").arg(static_cast<quint8>(f0[5]), 2, 16, QChar('0')).toUpper()
                     << QString::fromUtf8("    Len(LE) = 0x%1 (%2)  原产品目标 0x0114=276")
                        .arg(lenLE, 4, 16, QChar('0')).toUpper().arg(lenLE)
                     << QString::fromUtf8("    Len diff= %1").arg(static_cast<int>(lenLE) - 276)
                     << QString::fromUtf8("    Cmd     = 0x%1").arg(static_cast<quint8>(f0[8]), 2, 16, QChar('0')).toUpper()
                     << QString::fromUtf8("    Seq(LE) = 0x%1 %2")
                           .arg(static_cast<quint8>(f0[9]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[10]), 2, 16, QChar('0')).toUpper()
                     << QString::fromUtf8("  帧头前11字节 HEX: 7E 6C %1 %2 %3 %4 %5 %6 %7 %8 %9")
                           .arg(static_cast<quint8>(f0[2]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[3]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[4]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[5]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[6]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[7]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[8]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[9]), 2, 16, QChar('0')).toUpper()
                           .arg(static_cast<quint8>(f0[10]), 2, 16, QChar('0')).toUpper();
        }
    }

    for (int i = 0; i < frames.size(); ++i) {
        const QByteArray &f = frames[i];
        if (f.size() < 14) {
            logLines << QString::fromUtf8("  [Frame %1] ERROR: frame too short (%2 bytes)").arg(i).arg(f.size());
            continue;
        }

        // 解包字段
        quint8  frameNo   = static_cast<quint8>(f[3]);
        quint8  station   = static_cast<quint8>(f[4]);
        quint8  device    = static_cast<quint8>(f[5]);
        quint16 lenLE     = static_cast<quint8>(f[6]) | (static_cast<quint8>(f[7]) << 8);
        quint8  cmd       = static_cast<quint8>(f[8]);
        quint8  seqLo     = static_cast<quint8>(f[9]);
        quint8  seqHi     = static_cast<quint8>(f[10]);
        quint16 seq       = seqLo | (seqHi << 8);
        int     chunkLen  = f.size() - 14;  // 总长 - 固定头(12) - CRC(2)
        quint8  crcHi     = static_cast<quint8>(f[f.size() - 2]);
        quint8  crcLo     = static_cast<quint8>(f[f.size() - 1]);
        quint16 crc       = (crcHi << 8) | crcLo;

        QString cmdName = (cmd == CMD_TICKET) ?
            QString::fromUtf8("0x03 (TICKET, 单帧/最后帧)") :
            QString::fromUtf8("0x83 (TICKET_MORE, 还有后续帧)");

        logLines << sep;
        logLines << QString::fromUtf8("  [Frame %1/%2]").arg(i + 1).arg(frames.size());
        logLines << QString::fromUtf8("    FrameNo  = 0x%1").arg(frameNo, 2, 16, QLatin1Char('0')).toUpper();
        logLines << QString::fromUtf8("    Station  = 0x%1").arg(station,  2, 16, QLatin1Char('0')).toUpper();
        logLines << QString::fromUtf8("    Device   = 0x%1").arg(device,   2, 16, QLatin1Char('0')).toUpper();
        logLines << QString::fromUtf8("    Len      = %1 (0x%2) [Cmd+Seq+PayloadChunk]")
                    .arg(lenLE).arg(lenLE, 4, 16, QLatin1Char('0')).toUpper();
        logLines << QString::fromUtf8("    Cmd      = %1").arg(cmdName);
        logLines << QString::fromUtf8("    Seq      = %1 (0x%2)")
                    .arg(seq).arg(seq, 4, 16, QLatin1Char('0')).toUpper();
        logLines << QString::fromUtf8("    Payload  = %1 bytes").arg(chunkLen);
        logLines << QString::fromUtf8("    CRC      = 0x%1%2 (%3)")
                    .arg(crcHi, 2, 16, QLatin1Char('0')).toUpper()
                    .arg(crcLo, 2, 16, QLatin1Char('0')).toUpper()
                    .arg(crc);
        logLines << QString::fromUtf8("    Total    = %1 bytes").arg(f.size());

        // HEX 行（空格分隔，便于复制到串口助手）
        QStringList hexParts;
        hexParts.reserve(f.size());
        for (int b = 0; b < f.size(); ++b) {
            hexParts << QString::fromLatin1("%1")
                        .arg(static_cast<quint8>(f[b]), 2, 16, QLatin1Char('0'))
                        .toUpper();
        }
        QString hexLine = QString::fromUtf8("SEN_HEX[%1]: ").arg(i) + hexParts.join(' ');
        logLines << hexLine;
        hexLines << hexLine;
    }
    logLines << sep;
}

// ────────────────────────────────────────────────────────────
void TicketFrameBuilder::saveFrameFiles(const QByteArray &frame,
                                         const QString &basePath)
{
    // ---- .bin 原始二进制 ----
    {
        QFile f(basePath + ".bin");
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(frame);
            f.close();
        }
    }

    // ---- .hex 文本（空格分隔大写 HEX + 换行） ----
    {
        QFile f(basePath + ".hex");
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream ts(&f);
            for (int i = 0; i < frame.size(); ++i) {
                if (i > 0) ts << ' ';
                ts << QString::fromLatin1("%1")
                      .arg(static_cast<quint8>(frame[i]), 2, 16, QChar('0'))
                      .toUpper();
            }
            ts << '\n';
            f.close();
        }
    }
}
