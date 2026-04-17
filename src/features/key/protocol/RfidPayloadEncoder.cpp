#include "features/key/protocol/RfidPayloadEncoder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace {

QByteArray currentTimestampBcd()
{
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
    QByteArray out;
    out.reserve(7);
    for (int i = 0; i + 1 < stamp.size(); i += 2) {
        const int hi = stamp[i].digitValue();
        const int lo = stamp[i + 1].digitValue();
        out.append(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

QByteArray parseRfidBytes(const QString &text)
{
    QString normalized = text;
    normalized.remove(QRegularExpression("[^0-9A-Fa-f]"));
    if (normalized.size() != 10) {
        return QByteArray();
    }
    return QByteArray::fromHex(normalized.toLatin1());
}

quint8 rfidStationByte(const QList<int> &stationNos)
{
    QSet<int> unique;
    for (int stationNo : stationNos) {
        unique.insert(stationNo);
    }
    if (unique.size() == 1) {
        return static_cast<quint8>(*unique.begin());
    }
    return 0xFF;
}

}

void RfidPayloadEncoder::writeLe32(QByteArray &buf, int offset, quint32 value)
{
    buf[offset + 0] = static_cast<char>(value & 0xFF);
    buf[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
    buf[offset + 2] = static_cast<char>((value >> 16) & 0xFF);
    buf[offset + 3] = static_cast<char>((value >> 24) & 0xFF);
}

RfidPayloadEncoder::EncodeResult RfidPayloadEncoder::encode(const QJsonObject &dataObj)
{
    EncodeResult result;
    const QJsonArray list = dataObj.value("rfidList").toArray();
    if (list.isEmpty()) {
        result.errorMsg = QStringLiteral("RFID 数据为空：rfidList 为空");
        return result;
    }

    QByteArray recordArea;
    QList<int> stationNos;
    int recordCount = 0;

    for (const QJsonValue &value : list) {
        if (!value.isObject()) {
            result.errorMsg = QStringLiteral("RFID 数据格式错误：rfidList 项不是对象");
            return result;
        }
        const QJsonObject obj = value.toObject();
        const quint16 innerCode = static_cast<quint16>(obj.value("innerCode").toInt());
        const quint8 lockerType = static_cast<quint8>(obj.value("lockerType").toInt(0xFF));
        const int stationNo = obj.value("stationNo").toInt();
        const QJsonArray rfids = obj.value("rfid").toArray();

        for (const QJsonValue &rfidValue : rfids) {
            const QByteArray rfidBytes = parseRfidBytes(rfidValue.toString());
            if (rfidBytes.size() != 5) {
                continue;
            }

            recordArea.append(static_cast<char>(innerCode & 0xFF));
            recordArea.append(static_cast<char>((innerCode >> 8) & 0xFF));
            recordArea.append(static_cast<char>(lockerType));
            recordArea.append(rfidBytes);
            stationNos.append(stationNo);
            ++recordCount;
        }
    }

    if (recordCount == 0) {
        result.errorMsg = QStringLiteral("RFID 数据为空：没有可编码的 5 字节 RFID");
        return result;
    }

    QByteArray payload(16, '\0');
    payload.append(recordArea);
    writeLe32(payload, 0, static_cast<quint32>(payload.size()));
    payload[4] = 0x01;   // 基于原产品样本固定的版本字节
    payload[5] = 0x00;
    payload[6] = 0x00;
    payload[7] = 0x00;
    payload[8] = static_cast<char>(rfidStationByte(stationNos));
    payload.replace(9, 7, currentTimestampBcd());

    result.fieldLog << QStringLiteral("[RFID_PAYLOAD] payload=%1B records=%2")
                           .arg(payload.size()).arg(recordCount);
    result.payload = payload;
    result.ok = true;
    return result;
}
