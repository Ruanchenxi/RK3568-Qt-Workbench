#include "features/key/protocol/InitPayloadEncoder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QTextCodec>

namespace {

QByteArray encodeGbk(const QString &text)
{
    QTextCodec *codec = QTextCodec::codecForName("GBK");
    if (!codec) {
        codec = QTextCodec::codecForName("UTF-8");
    }
    QByteArray bytes = codec ? codec->fromUnicode(text) : text.toUtf8();
    bytes.append('\0');
    return bytes;
}

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

quint8 initStationByte(const QJsonArray &devices, int stationCount)
{
    if (stationCount != 1) {
        return 0xFF;
    }
    if (devices.isEmpty() || !devices.first().isObject()) {
        return 0xFF;
    }
    return static_cast<quint8>(devices.first().toObject().value("stationNo").toInt(0xFF));
}

}

void InitPayloadEncoder::writeLe16(QByteArray &buf, int offset, quint16 value)
{
    buf[offset + 0] = static_cast<char>(value & 0xFF);
    buf[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
}

void InitPayloadEncoder::writeLe32(QByteArray &buf, int offset, quint32 value)
{
    buf[offset + 0] = static_cast<char>(value & 0xFF);
    buf[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
    buf[offset + 2] = static_cast<char>((value >> 16) & 0xFF);
    buf[offset + 3] = static_cast<char>((value >> 24) & 0xFF);
}

InitPayloadEncoder::EncodeResult InitPayloadEncoder::encode(const QJsonObject &dataObj)
{
    EncodeResult result;

    const QJsonArray devices = dataObj.value("devices").toArray();
    if (devices.isEmpty()) {
        result.errorMsg = QStringLiteral("初始化数据为空：devices 为空");
        return result;
    }

    QJsonArray stationNames = dataObj.value("stationNames").toArray();
    int stationCount = dataObj.value("stationCount").toInt(stationNames.size());
    if (stationCount <= 0) {
        QSet<int> stations;
        for (const QJsonValue &value : devices) {
            if (value.isObject()) {
                stations.insert(value.toObject().value("stationNo").toInt());
            }
        }
        stationCount = stations.size();
    }
    if (stationNames.isEmpty()) {
        for (int i = 0; i < stationCount; ++i) {
            stationNames.append(QStringLiteral("站点%1").arg(i + 1));
        }
    }

    QByteArray stationNameArea;
    stationNameArea.reserve(128);
    for (const QJsonValue &value : stationNames) {
        stationNameArea.append(encodeGbk(value.toString()));
    }

    struct DeviceTermInfo {
        quint16 innerCode = 0xFFFF;
        quint8 stationNo = 0xFF;
        quint8 lockerType = 0xFF;
        quint8 devProp = 0xFF;
        quint8 rfidCount = 0x00;
        quint16 opOffset = 0xFFFF;
    };

    QList<DeviceTermInfo> infos;
    infos.reserve(devices.size());
    QByteArray deviceNameArea;
    QByteArray opTermArea;

    for (const QJsonValue &value : devices) {
        if (!value.isObject()) {
            result.errorMsg = QStringLiteral("初始化数据格式错误：device 项不是对象");
            return result;
        }

        const QJsonObject obj = value.toObject();
        DeviceTermInfo info;
        info.innerCode = static_cast<quint16>(obj.value("innerCode").toInt());
        info.stationNo = static_cast<quint8>(obj.value("stationNo").toInt());
        info.lockerType = static_cast<quint8>(obj.value("lockerType").toInt(0xFF));
        info.devProp = static_cast<quint8>(obj.value("devProp").toInt(0xFF));

        const QJsonArray rfidArray = obj.value("rfid").toArray();
        const int declaredRfidCount = obj.value("rfidCount").toInt(rfidArray.size());
        info.rfidCount = static_cast<quint8>(qMax(0, qMin(255, declaredRfidCount)));

        deviceNameArea.append(encodeGbk(obj.value("devName").toString()));

        const QJsonArray opTerms = obj.value("opDescrip").toArray();
        if (!opTerms.isEmpty()) {
            info.opOffset = static_cast<quint16>(opTermArea.size());
            for (const QJsonValue &term : opTerms) {
                opTermArea.append(encodeGbk(term.toString()));
            }
        }

        infos.append(info);
    }

    const quint32 stationNameOffset = 36;
    const quint32 deviceInfoOffset = stationNameOffset + static_cast<quint32>(stationNameArea.size());
    const quint32 deviceNameOffset = deviceInfoOffset + static_cast<quint32>(infos.size() * 12);
    const quint32 opTermOffset = deviceNameOffset + static_cast<quint32>(deviceNameArea.size());

    QByteArray payload(36, '\0');
    payload.append(stationNameArea);

    QByteArray deviceInfoArea(infos.size() * 12, '\0');
    for (int i = 0; i < infos.size(); ++i) {
        const DeviceTermInfo &info = infos.at(i);
        const int base = i * 12;
        writeLe16(deviceInfoArea, base, info.innerCode);
        deviceInfoArea[base + 2] = static_cast<char>(info.stationNo);
        deviceInfoArea[base + 3] = static_cast<char>(info.lockerType);
        deviceInfoArea[base + 4] = static_cast<char>(info.devProp);
        deviceInfoArea[base + 5] = static_cast<char>(info.rfidCount);
        writeLe16(deviceInfoArea, base + 6, info.opOffset);
    }

    // Rebuild device name offsets with the absolute addresses in one pass.
    int deviceNameCursor = 0;
    for (int i = 0; i < infos.size(); ++i) {
        const int base = i * 12;
        writeLe32(deviceInfoArea, base + 8, deviceNameOffset + static_cast<quint32>(deviceNameCursor));
        const QString devName = devices.at(i).toObject().value("devName").toString();
        deviceNameCursor += encodeGbk(devName).size();
    }

    payload.append(deviceInfoArea);
    payload.append(deviceNameArea);
    payload.append(opTermArea);

    writeLe32(payload, 0, static_cast<quint32>(payload.size()));
    payload[4] = 0x02;   // 基于原产品样本固定的版本字节
    payload[5] = 0x01;
    payload[6] = 0x00;
    payload[7] = 0x00;
    payload[8] = static_cast<char>(initStationByte(devices, stationCount));
    payload.replace(9, 7, currentTimestampBcd());
    payload[16] = 0x00;
    payload[17] = static_cast<char>(qMax(0, qMin(255, stationCount)));
    writeLe16(payload, 18, static_cast<quint16>(infos.size()));
    writeLe32(payload, 20, stationNameOffset);
    writeLe32(payload, 24, deviceInfoOffset);
    writeLe32(payload, 28, deviceNameOffset);
    writeLe32(payload, 32, opTermOffset);

    result.fieldLog << QStringLiteral("[INIT_PAYLOAD] payload=%1B stations=%2 devices=%3")
                           .arg(payload.size()).arg(stationCount).arg(infos.size());
    result.payload = payload;
    result.ok = true;
    return result;
}
