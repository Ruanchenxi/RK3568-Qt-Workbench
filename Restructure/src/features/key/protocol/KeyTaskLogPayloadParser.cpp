#include "KeyTaskLogPayloadParser.h"

#include <QtGlobal>

namespace {

constexpr int kMinimumFileSize = 28;
constexpr int kTaskIdLength = 16;
constexpr int kItemLength = 16;

quint16 readLe16(const QByteArray &bytes, int offset)
{
    return static_cast<quint16>(static_cast<quint8>(bytes[offset]))
            | (static_cast<quint16>(static_cast<quint8>(bytes[offset + 1])) << 8);
}

quint32 readLe32(const QByteArray &bytes, int offset)
{
    return static_cast<quint32>(static_cast<quint8>(bytes[offset]))
            | (static_cast<quint32>(static_cast<quint8>(bytes[offset + 1])) << 8)
            | (static_cast<quint32>(static_cast<quint8>(bytes[offset + 2])) << 16)
            | (static_cast<quint32>(static_cast<quint8>(bytes[offset + 3])) << 24);
}

QString taskIdToDecimal(const QByteArray &taskId)
{
    qulonglong value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<qulonglong>(static_cast<quint8>(taskId[i])) << (i * 8);
    }
    return QString::number(value);
}

QString bcdDateTime(const QByteArray &bytes)
{
    auto part = [&bytes](int index) -> QString {
        const quint8 value = static_cast<quint8>(bytes[index]);
        return QStringLiteral("%1%2")
                .arg((value >> 4) & 0x0F)
                .arg(value & 0x0F);
    };

    return QStringLiteral("20%1-%2-%3 %4:%5:%6")
            .arg(part(0), part(1), part(2), part(3), part(4), part(5));
}

} // namespace

namespace KeyTaskLogPayloadParser {

ParseResult parse(const QByteArray &bytes, const QByteArray &expectedTaskId)
{
    ParseResult result;
    if (bytes.size() < kMinimumFileSize) {
        result.error = ParseError::PayloadTooShort;
        return result;
    }

    result.fileSize = readLe32(bytes, 0);
    if (result.fileSize < kMinimumFileSize
            || static_cast<int>(result.fileSize) > bytes.size()) {
        result.error = ParseError::InvalidFileSize;
        return result;
    }

    int offset = 4;
    const QByteArray versionBytes = bytes.mid(offset, 2);
    offset += 4; // version and reserved bytes
    result.payload.taskIdRaw = bytes.mid(offset, kTaskIdLength);
    offset += kTaskIdLength;

    if (!expectedTaskId.isEmpty() && result.payload.taskIdRaw != expectedTaskId) {
        result.error = ParseError::TaskIdMismatch;
        return result;
    }

    result.payload.stationNo = static_cast<quint8>(bytes[offset++]);
    result.payload.taskAttr = static_cast<quint8>(bytes[offset++]);
    result.payload.steps = readLe16(bytes, offset);
    offset += 2;

    result.itemBytes = static_cast<int>(result.fileSize) - offset;
    if (result.itemBytes < 0 || result.itemBytes % kItemLength != 0) {
        result.error = ParseError::InvalidItemBytes;
        return result;
    }

    result.payload.taskId = taskIdToDecimal(result.payload.taskIdRaw);
    result.payload.version = QString(versionBytes.toHex().toUpper());
    for (int itemOffset = offset;
         itemOffset < static_cast<int>(result.fileSize);
         itemOffset += kItemLength) {
        TaskLogItem item;
        item.serialNumber = readLe16(bytes, itemOffset);
        item.opStatus = static_cast<quint8>(bytes[itemOffset + 2]);
        item.rfid = QString(bytes.mid(itemOffset + 3, 5).toHex().toUpper());
        item.opTime = bcdDateTime(bytes.mid(itemOffset + 8, 6));
        item.opType = static_cast<quint8>(bytes[itemOffset + 14]);
        result.payload.items.append(item);
    }
    return result;
}

} // namespace KeyTaskLogPayloadParser
