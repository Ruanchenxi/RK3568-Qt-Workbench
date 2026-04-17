#include "features/key/protocol/TicketPayloadEncoder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTextCodec>

void TicketPayloadEncoder::writeLE32(QByteArray &buf, int offset, quint32 v)
{
    buf[offset + 0] = static_cast<char>(v & 0xFF);
    buf[offset + 1] = static_cast<char>((v >> 8) & 0xFF);
    buf[offset + 2] = static_cast<char>((v >> 16) & 0xFF);
    buf[offset + 3] = static_cast<char>((v >> 24) & 0xFF);
}

void TicketPayloadEncoder::writeLE16(QByteArray &buf, int offset, quint16 v)
{
    buf[offset + 0] = static_cast<char>(v & 0xFF);
    buf[offset + 1] = static_cast<char>((v >> 8) & 0xFF);
}

QString TicketPayloadEncoder::hexDump(const QByteArray &data, int maxBytes)
{
    const int n = qMin(data.size(), maxBytes);
    QStringList parts;
    for (int i = 0; i < n; ++i) {
        parts << QString::fromLatin1("%1")
                    .arg(static_cast<quint8>(data[i]), 2, 16, QLatin1Char('0'))
                    .toUpper();
    }
    if (data.size() > maxBytes)
        parts << "...";
    return parts.join(' ');
}

bool TicketPayloadEncoder::extractTicketObject(const QByteArray &jsonBytes,
                                               QJsonObject &ticketObj,
                                               QString &errorMsg)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &pe);
    if (doc.isNull() || !doc.isObject()) {
        errorMsg = QString::fromUtf8("JSON 解析失败: %1").arg(pe.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    ticketObj = (root.contains("data") && root.value("data").isObject())
            ? root.value("data").toObject()
            : root;
    return true;
}

QByteArray TicketPayloadEncoder::timeToBcd(const QString &timeStr, int targetLen)
{
    QByteArray result;
    result.reserve(targetLen);

    int i = 0;
    while (i + 1 < timeStr.length() && result.size() < targetLen) {
        const int hi = timeStr[i].digitValue();
        const int lo = timeStr[i + 1].digitValue();
        if (hi < 0 || lo < 0)
            break;
        result.append(static_cast<char>((hi << 4) | lo));
        i += 2;
    }

    while (result.size() < targetLen)
        result.append('\xFF');

    return result;
}

QByteArray TicketPayloadEncoder::uint64LeBytes(quint64 val, int totalLen)
{
    QByteArray result;
    result.reserve(totalLen);
    for (int i = 0; i < 8 && i < totalLen; ++i)
        result.append(static_cast<char>((val >> (i * 8)) & 0xFF));
    while (result.size() < totalLen)
        result.append('\x00');
    return result;
}

TicketPayloadEncoder::EncodeResult
TicketPayloadEncoder::encode(const QJsonObject &ticketJson,
                             quint8 stationId,
                             StringEncoding enc)
{
    EncodeResult res;
    QStringList &log = res.fieldLog;
    const QString SEP = QString::fromUtf8("────────────────────────────────────────────");

    log << QString::fromUtf8("[TICKET_PAYLOAD] stationId=0x%1 header=%2B")
              .arg(stationId, 2, 16, QChar('0')).toUpper()
              .arg(HEADER_SIZE);
    log << SEP;

    const QString taskId = ticketJson.value("taskId").toString();
    const QString taskName = ticketJson.value("taskName").toString();
    const int taskType = ticketJson.value("taskType").toInt(0);
    const QString createTime = ticketJson.value("createTime").toString();
    const QString planTime = ticketJson.value("planTime").toString();
    const int declaredStepNum = ticketJson.value("stepNum").toInt(0);
    const QJsonArray steps = ticketJson.value("steps").toArray();

    struct StepInfo {
        quint16 innerCode = 0xFFFF;
        quint8 lockerOperate = 0xFF;
        QString stepDetail;
    };

    int encodedStepCount = declaredStepNum;
    if (encodedStepCount <= 0)
        encodedStepCount = steps.isEmpty() ? 1 : steps.size();
    if (!steps.isEmpty())
        encodedStepCount = qMin(encodedStepCount, steps.size());
    if (encodedStepCount <= 0)
        encodedStepCount = 1;

    const int stepNumForPayload = declaredStepNum > 0 ? declaredStepNum : encodedStepCount;

    QList<StepInfo> stepInfos;
    stepInfos.reserve(encodedStepCount);
    for (int i = 0; i < encodedStepCount; ++i) {
        StepInfo info;
        if (i < steps.size() && steps[i].isObject()) {
            const QJsonObject stepObj = steps[i].toObject();
            info.innerCode = static_cast<quint16>(stepObj.value("innerCode").toInt(0));
            info.lockerOperate = static_cast<quint8>(stepObj.value("lockerOperate").toInt(0xFF));
            info.stepDetail = stepObj.value("stepDetail").toString();
        }
        stepInfos.append(info);
    }

    const int stepTableStart = HEADER_SIZE;
    const int stepTableLen = encodedStepCount * STEP_ENTRY_SIZE;
    const int fixedPartLen = HEADER_SIZE + stepTableLen;
    const int stringAreaStart = fixedPartLen;
    QByteArray buf(fixedPartLen, '\xFF');

    writeLE32(buf, OFF_FILE_SIZE, 0x00000000);
    buf[OFF_VERSION] = static_cast<char>(0x01);
    buf[OFF_VERSION + 1] = static_cast<char>(0x11);
    buf[OFF_RESERVED] = 0x00;
    buf[OFF_RESERVED + 1] = 0x00;

    bool convOk = false;
    const quint64 tid = taskId.toULongLong(&convOk);
    const QByteArray tidBytes = uint64LeBytes(tid, 16);
    buf.replace(OFF_TASK_ID, 16, tidBytes);
    buf[OFF_STATION_ID] = static_cast<char>(stationId);
    buf[OFF_TICKET_ATTR] = 0x00;
    writeLE16(buf, OFF_STEP_NUM, static_cast<quint16>(stepNumForPayload));
    writeLE32(buf, OFF_TASKNAME_PTR, 0x00000000);
    buf.replace(OFF_CREATE_TIME, 8, timeToBcd(createTime, 8));
    buf.replace(OFF_PLAN_TIME, 8, timeToBcd(planTime, 8));

    log << QString::fromUtf8("  taskId=%1 convOk=%2 taskType=%3")
              .arg(taskId).arg(convOk ? "true" : "false").arg(taskType);
    log << QString::fromUtf8("  stepNum=%1 stepTableStart=%2 stringAreaStart=%3")
              .arg(stepNumForPayload).arg(stepTableStart).arg(stringAreaStart);

    for (int i = 0; i < encodedStepCount; ++i) {
        const StepInfo &info = stepInfos[i];
        const int stepBase = stepTableStart + i * STEP_ENTRY_SIZE;
        writeLE16(buf, stepBase + STEP_OFF_INNER_CODE, info.innerCode);
        buf[stepBase + STEP_OFF_LOCKER_OP] = static_cast<char>(info.lockerOperate);
        buf[stepBase + STEP_OFF_STATE] = 0x00;
        writeLE32(buf, stepBase + STEP_OFF_DISP_PTR, 0x00000000);
    }

    QTextCodec *codec = nullptr;
    if (enc == StringEncoding::GBK)
        codec = QTextCodec::codecForName("GBK");
    if (!codec)
        codec = QTextCodec::codecForName("UTF-8");

    QList<quint32> stepDetailOffsets;
    stepDetailOffsets.reserve(encodedStepCount);
    for (int i = 0; i < encodedStepCount; ++i) {
        const StepInfo &info = stepInfos[i];
        const quint32 offset = static_cast<quint32>(buf.size());
        stepDetailOffsets.append(offset);
        QByteArray encoded = codec->fromUnicode(info.stepDetail);
        encoded.append('\x00');
        buf.append(encoded);
        log << QString::fromUtf8("  stepDetail[%1] off=%2 len=%3")
                  .arg(i).arg(offset).arg(encoded.size());
    }

    const quint32 offsetOfTaskName = static_cast<quint32>(buf.size());
    {
        QByteArray encoded = codec->fromUnicode(taskName);
        encoded.append('\x00');
        buf.append(encoded);
        log << QString::fromUtf8("  taskName off=%1 len=%2")
                  .arg(offsetOfTaskName).arg(encoded.size());
    }

    writeLE32(buf, OFF_TASKNAME_PTR, offsetOfTaskName);
    for (int i = 0; i < encodedStepCount; ++i) {
        const int dispPtrAbs = stepTableStart + i * STEP_ENTRY_SIZE + STEP_OFF_DISP_PTR;
        writeLE32(buf, dispPtrAbs, stepDetailOffsets.value(i, 0));
    }

    const quint32 fileSize = static_cast<quint32>(buf.size());
    writeLE32(buf, OFF_FILE_SIZE, fileSize);
    log << QString::fromUtf8("  payloadLen=%1 fileSize=%2 check=%3")
              .arg(buf.size()).arg(fileSize).arg(hexDump(buf.left(8), 8));

    res.payload = buf;
    res.ok = true;
    return res;
}
