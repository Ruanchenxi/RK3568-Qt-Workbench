#include "features/key/protocol/CaptureRfidPayloadParser.h"

namespace {
quint16 readLe16(const QByteArray &bytes, int offset)
{
    return static_cast<quint16>(static_cast<quint8>(bytes.at(offset)))
            | (static_cast<quint16>(static_cast<quint8>(bytes.at(offset + 1))) << 8);
}
quint32 readLe32(const QByteArray &bytes, int offset)
{
    return static_cast<quint32>(readLe16(bytes, offset))
            | (static_cast<quint32>(readLe16(bytes, offset + 2)) << 16);
}
}

CaptureRfidParseResult CaptureRfidPayloadParser::parse(const QByteArray &payload)
{
    CaptureRfidParseResult result;
    constexpr int headerSize = 16;
    if (payload.size() < headerSize) {
        result.error = QStringLiteral("UP_RFID payload too short");
        return result;
    }
    const quint32 fileSize = readLe32(payload, 0);
    if (fileSize < headerSize || fileSize > static_cast<quint32>(payload.size())) {
        result.error = QStringLiteral("UP_RFID invalid file size");
        return result;
    }
    const QByteArray version = payload.mid(4, 2);
    const int recordBytes = static_cast<int>(fileSize) - headerSize;
    int recordSize = static_cast<quint8>(version.at(0)) == 0x01
            && static_cast<quint8>(version.at(1)) >= 0x13 ? 10 : 8;
    if (recordBytes % recordSize != 0) {
        recordSize = recordSize == 8 ? 10 : 8;
        if (recordBytes % recordSize != 0) {
            result.error = QStringLiteral("UP_RFID invalid record bytes");
            return result;
        }
    }
    for (int offset = headerSize; offset < static_cast<int>(fileSize); offset += recordSize) {
        QVariantMap record;
        record.insert(QStringLiteral("innerCode"),
                      static_cast<int>(readLe16(payload, offset)));
        record.insert(QStringLiteral("lockerType"),
                      static_cast<int>(static_cast<quint8>(payload.at(offset + 2))));
        record.insert(QStringLiteral("rfid"),
                      QString::fromLatin1(payload.mid(offset + 3, 5).toHex().toUpper()));
        result.records.append(record);
    }
    result.ok = true;
    result.stationNo = static_cast<quint8>(payload.at(8));
    result.recordSize = recordSize;
    result.version = QString::fromLatin1(version.toHex().toUpper());
    return result;
}
