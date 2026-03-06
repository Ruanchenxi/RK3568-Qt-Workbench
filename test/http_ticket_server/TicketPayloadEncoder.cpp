#include "TicketPayloadEncoder.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QTextCodec>

// ════════════════════════════════════════════════════════════
//  工具函数
// ════════════════════════════════════════════════════════════

void TicketPayloadEncoder::writeLE32(QByteArray &buf, int offset, quint32 v)
{
    buf[offset + 0] = static_cast<char>(v & 0xFF);
    buf[offset + 1] = static_cast<char>((v >>  8) & 0xFF);
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
    int n = qMin(data.size(), maxBytes);
    QStringList parts;
    for (int i = 0; i < n; ++i)
        parts << QString::fromLatin1("%1").arg(
            static_cast<quint8>(data[i]), 2, 16, QLatin1Char('0')).toUpper();
    if (data.size() > maxBytes)
        parts << "...";
    return parts.join(' ');
}

// ────────────────────────────────────────────────────────────
// 时间字符串 -> BCD
// "202603042321" -> 20 26 03 04 23 21 00 FF
// ────────────────────────────────────────────────────────────
QByteArray TicketPayloadEncoder::timeToBcd(const QString &timeStr, int targetLen)
{
    QByteArray result;
    result.reserve(targetLen);

    // 逐对字符打包成 BCD 字节
    int i = 0;
    while (i + 1 < timeStr.length() && result.size() < targetLen) {
        int hi = timeStr[i    ].digitValue();
        int lo = timeStr[i + 1].digitValue();
        if (hi < 0 || lo < 0) break;  // 非数字字符停止
        result.append(static_cast<char>((hi << 4) | lo));
        i += 2;
    }

    // 秒及后续字节：原产品填 FF
    while (result.size() < targetLen)
        result.append('\xFF');

    return result;
}

// ────────────────────────────────────────────────────────────
// uint64 -> LE 字节（写 8 字节数值，再用 00 填满 totalLen）
// ────────────────────────────────────────────────────────────
QByteArray TicketPayloadEncoder::uint64LeBytes(quint64 val, int totalLen)
{
    QByteArray result;
    result.reserve(totalLen);
    for (int i = 0; i < 8 && i < totalLen; ++i)
        result.append(static_cast<char>((val >> (i * 8)) & 0xFF));
    while (result.size() < totalLen)
        result.append('\x00');  // taskId 后 8 字节填 00
    return result;
}

// ════════════════════════════════════════════════════════════
//  主编码函数（Stage 5.4 — 160B 头部 + 多步 step table + fileSize 回填）
// ════════════════════════════════════════════════════════════
TicketPayloadEncoder::EncodeResult
TicketPayloadEncoder::encode(const QJsonObject &ticketJson,
                             quint8 stationId,
                             StringEncoding enc)
{
    EncodeResult res;
    QStringList &log = res.fieldLog;

    const QString SEP = QString::fromUtf8(
        "────────────────────────────────────────────");

    log << QString::fromUtf8("[PAYLOAD_ENCODE] Stage 5.3  stationId=0x%1"
                             "  HEADER=%2B")
           .arg(stationId, 2, 16, QChar('0')).toUpper()
           .arg(HEADER_SIZE);
    log << SEP;

    // ---- 读取 JSON 字段 ----
    QString    taskId     = ticketJson.value("taskId").toString();
    QString    taskName   = ticketJson.value("taskName").toString();
    int        taskType   = ticketJson.value("taskType").toInt(0);
    QString    createTime = ticketJson.value("createTime").toString();
    QString    planTime   = ticketJson.value("planTime").toString();
    int        declaredStepNum = ticketJson.value("stepNum").toInt(0);
    QJsonArray steps      = ticketJson.value("steps").toArray();

    struct StepInfo {
        quint16 innerCode = 0xFFFF;
        quint8  lockerOperate = 0xFF;
        QString stepDetail;
    };

    int encodedStepCount = declaredStepNum;
    if (encodedStepCount <= 0)
        encodedStepCount = steps.isEmpty() ? 1 : steps.size();
    if (!steps.isEmpty())
        encodedStepCount = qMin(encodedStepCount, steps.size());
    if (encodedStepCount <= 0)
        encodedStepCount = 1;

    const int stepNumForPayload = declaredStepNum > 0 ? declaredStepNum
                                                      : encodedStepCount;

    QList<StepInfo> stepInfos;
    stepInfos.reserve(encodedStepCount);
    for (int i = 0; i < encodedStepCount; ++i) {
        StepInfo info;
        if (i < steps.size() && steps[i].isObject()) {
            QJsonObject stepObj = steps[i].toObject();
            info.innerCode = static_cast<quint16>(stepObj.value("innerCode").toInt(0));
            info.lockerOperate = static_cast<quint8>(stepObj.value("lockerOperate").toInt(0xFF));
            info.stepDetail = stepObj.value("stepDetail").toString();
        }
        stepInfos.append(info);
    }

    // ════════════════════════════════════════════════════════
    //  1. 分配缓冲区 HEADER_SIZE(160) + stepTable(N*8)，全 FF
    // ════════════════════════════════════════════════════════
    const int stepTableStart = HEADER_SIZE;
    const int stepTableLen = encodedStepCount * STEP_ENTRY_SIZE;
    const int fixedPartLen = HEADER_SIZE + stepTableLen;
    const int stringAreaStart = fixedPartLen;
    QByteArray buf(fixedPartLen, '\xFF');

    // ════════════════════════════════════════════════════════
    //  2. 头区字段写入（offset 基于 Stage5.3 常量）
    // ════════════════════════════════════════════════════════

    // [0..3]  fileSize 占位 00 00 00 00，最后回填
    writeLE32(buf, OFF_FILE_SIZE, 0x00000000);
    log << QString::fromUtf8("  [%1..%2]  fileSize      len=4  00 00 00 00  (占位，回填)")
           .arg(OFF_FILE_SIZE, 3).arg(OFF_FILE_SIZE+3, 3);

    // [4..5]  version 01 11
    buf[OFF_VERSION]     = static_cast<char>(0x01);
    buf[OFF_VERSION + 1] = static_cast<char>(0x11);
    log << QString::fromUtf8("  [%1..%2]  version        len=2  %3")
           .arg(OFF_VERSION, 3).arg(OFF_VERSION+1, 3)
           .arg(hexDump(buf.mid(OFF_VERSION, 2)));

    // [6..7]  reserved 00 00
    buf[OFF_RESERVED]     = 0x00;
    buf[OFF_RESERVED + 1] = 0x00;
    log << QString::fromUtf8("  [%1..%2]  reserved       len=2  %3")
           .arg(OFF_RESERVED, 3).arg(OFF_RESERVED+1, 3)
           .arg(hexDump(buf.mid(OFF_RESERVED, 2)));

    // [8..23] ticketId 16B  uint64-LE + FF×8
    {
        bool convOk = false;
        quint64 tid = taskId.toULongLong(&convOk);
        QByteArray tidBytes = uint64LeBytes(tid, 16);
        buf.replace(OFF_TASK_ID, 16, tidBytes);
        QString note = convOk ? "" : QString::fromUtf8("  [TODO: taskId非纯数字]");
        log << QString::fromUtf8("  [%1..%2] ticketId       len=16 %3%4")
               .arg(OFF_TASK_ID, 3).arg(OFF_TASK_ID+15, 3)
               .arg(hexDump(tidBytes, 16)).arg(note);
    }

    // [24]    stationId
    buf[OFF_STATION_ID] = static_cast<char>(stationId);
    log << QString::fromUtf8("  [%1]    stationId      len=1  %2")
           .arg(OFF_STATION_ID, 3)
           .arg(hexDump(buf.mid(OFF_STATION_ID, 1)));

    // [25]    ticketAttr 00
    buf[OFF_TICKET_ATTR] = 0x00;
    log << QString::fromUtf8("  [%1]    ticketAttr     len=1  %2  (00=普通票, taskType=%3 [TODO])")
           .arg(OFF_TICKET_ATTR, 3)
           .arg(hexDump(buf.mid(OFF_TICKET_ATTR, 1)))
           .arg(taskType);

    // [26..27] stepNum LE
    writeLE16(buf, OFF_STEP_NUM, static_cast<quint16>(stepNumForPayload));
    log << QString::fromUtf8("  [%1..%2] stepNum        len=2  %3  (=%4)")
           .arg(OFF_STEP_NUM, 3).arg(OFF_STEP_NUM+1, 3)
           .arg(hexDump(buf.mid(OFF_STEP_NUM, 2))).arg(stepNumForPayload);

    // [28..31] taskNameOff 占位
    writeLE32(buf, OFF_TASKNAME_PTR, 0x00000000);
    log << QString::fromUtf8("  [%1..%2] taskNameOff    len=4  00 00 00 00  (占位，回填)")
           .arg(OFF_TASKNAME_PTR, 3).arg(OFF_TASKNAME_PTR+3, 3);

    // [32..39] createTime BCD
    {
        QByteArray ct = timeToBcd(createTime, 8);
        buf.replace(OFF_CREATE_TIME, 8, ct);
        log << QString::fromUtf8("  [%1..%2] createTime     len=8  %3  (\"%4\")")
               .arg(OFF_CREATE_TIME, 3).arg(OFF_CREATE_TIME+7, 3)
               .arg(hexDump(ct)).arg(createTime);
    }

    // [40..47] planTime BCD
    {
        QByteArray pt = timeToBcd(planTime, 8);
        buf.replace(OFF_PLAN_TIME, 8, pt);
        log << QString::fromUtf8("  [%1..%2] planTime       len=8  %3  (\"%4\")")
               .arg(OFF_PLAN_TIME, 3).arg(OFF_PLAN_TIME+7, 3)
               .arg(hexDump(pt)).arg(planTime);
    }

    // [48..95] 授权信息 48B FF
    log << QString::fromUtf8("  [%1..%2] auth           len=48 FF×48  [TODO: 暂全FF]")
           .arg(OFF_AUTH, 3).arg(OFF_AUTH+47, 3);

    // [96..127] 额外字段 32B FF（跳步码偏移/压板/锁控/RFID格式/票源/生效时间等）
    log << QString::fromUtf8("  [%1..127] addFields    len=32 FF×32  [TODO: 跳步码偏移等]")
           .arg(OFF_ADD_FIELDS, 3);

    // [128..159] 保留 32B FF
    log << QString::fromUtf8("  [%1..159] reserved2    len=32 FF×32")
           .arg(OFF_RESERVED2, 3);

    // ════════════════════════════════════════════════════════
    //  3. 步骤区 step[0..N-1]（offset = HEADER_SIZE = 160）
    // ════════════════════════════════════════════════════════
    log << SEP;
    log << QString::fromUtf8("  === 步骤区 stepTable  start=%1  count=%2  len=%3B ===")
           .arg(stepTableStart).arg(encodedStepCount).arg(stepTableLen);

    for (int i = 0; i < encodedStepCount; ++i) {
        const StepInfo &info = stepInfos[i];
        const int stepBase = stepTableStart + i * STEP_ENTRY_SIZE;

        log << QString::fromUtf8("  --- step[%1]  offset=%2 ---").arg(i).arg(stepBase);

        writeLE16(buf, stepBase + STEP_OFF_INNER_CODE, info.innerCode);
        log << QString::fromUtf8("  [%1..%2] innerCode      len=2  %3  (=%4)")
               .arg(stepBase+STEP_OFF_INNER_CODE, 3)
               .arg(stepBase+STEP_OFF_INNER_CODE+1, 3)
               .arg(hexDump(buf.mid(stepBase+STEP_OFF_INNER_CODE, 2)))
               .arg(info.innerCode);

        buf[stepBase + STEP_OFF_LOCKER_OP] = static_cast<char>(info.lockerOperate);
        log << QString::fromUtf8("  [%1]    lockerOp       len=1  %2  (=%3)")
               .arg(stepBase+STEP_OFF_LOCKER_OP, 3)
               .arg(hexDump(buf.mid(stepBase+STEP_OFF_LOCKER_OP, 1)))
               .arg(info.lockerOperate);

        buf[stepBase + STEP_OFF_STATE] = 0x00;
        log << QString::fromUtf8("  [%1]    state          len=1  00  (未操作)")
               .arg(stepBase+STEP_OFF_STATE, 3);

        writeLE32(buf, stepBase + STEP_OFF_DISP_PTR, 0x00000000);
        log << QString::fromUtf8("  [%1..%2] displayOff     len=4  00 00 00 00  (占位，回填)")
               .arg(stepBase+STEP_OFF_DISP_PTR, 3)
               .arg(stepBase+STEP_OFF_DISP_PTR+3, 3);
    }

    // ════════════════════════════════════════════════════════
    //  4. 字符串区（offset = HEADER_SIZE + stepTableLen）
    // ════════════════════════════════════════════════════════
    log << SEP;
    log << QString::fromUtf8("  === 字符串区  start=%1 ===").arg(stringAreaStart);

    QTextCodec *codec = nullptr;
    QString encName;
    if (enc == StringEncoding::GBK) {
        codec = QTextCodec::codecForName("GBK");
        encName = "GBK";
    }
    if (!codec) {
        codec = QTextCodec::codecForName("UTF-8");
        encName = "UTF-8";
    }

    QList<quint32> stepDetailOffsets;
    stepDetailOffsets.reserve(encodedStepCount);

    for (int i = 0; i < encodedStepCount; ++i) {
        const StepInfo &info = stepInfos[i];
        quint32 offsetOfStepDetail = static_cast<quint32>(buf.size());
        stepDetailOffsets.append(offsetOfStepDetail);

        QByteArray encoded = codec->fromUnicode(info.stepDetail);
        encoded.append('\x00');
        buf.append(encoded);
        log << QString::fromUtf8("  [%1..%2] stepDetail[%3](%4)  len=%5  %6...")
               .arg(offsetOfStepDetail, 3)
               .arg(offsetOfStepDetail + encoded.size() - 1, 3)
               .arg(i).arg(encName).arg(encoded.size())
               .arg(hexDump(encoded, 16));
    }

    // taskName GBK + 0x00
    quint32 offsetOfTaskName = static_cast<quint32>(buf.size());
    {
        QByteArray encoded = codec->fromUnicode(taskName);
        encoded.append('\x00');
        buf.append(encoded);
        log << QString::fromUtf8("  [%1..%2] taskName(%3)    len=%4  %5...")
               .arg(offsetOfTaskName, 3)
               .arg(offsetOfTaskName + encoded.size() - 1, 3)
               .arg(encName).arg(encoded.size())
               .arg(hexDump(encoded, 16));
    }

    // ════════════════════════════════════════════════════════
    //  5. 回填偏移
    // ════════════════════════════════════════════════════════
    log << SEP;
    log << QString::fromUtf8("  === 回填 ===");

    // taskNameOff @ [28..31]
    writeLE32(buf, OFF_TASKNAME_PTR, offsetOfTaskName);
    {
        int diff = static_cast<int>(offsetOfTaskName) - static_cast<int>(TARGET_TASKNAME_OFF);
        log << QString::fromUtf8("  [%1..%2] taskNameOff -> %3 (0x%4)  写: %5  diff=%6%7")
               .arg(OFF_TASKNAME_PTR, 3).arg(OFF_TASKNAME_PTR+3, 3)
               .arg(offsetOfTaskName)
               .arg(offsetOfTaskName, 4, 16, QChar('0')).toUpper()
               .arg(hexDump(buf.mid(OFF_TASKNAME_PTR, 4)))
               .arg(diff >= 0 ? "+" : "").arg(diff);
    }

    // displayOff @ step[i]+4
    for (int i = 0; i < encodedStepCount; ++i) {
        const int dispPtrAbs = stepTableStart + i * STEP_ENTRY_SIZE + STEP_OFF_DISP_PTR;
        const quint32 displayOff = stepDetailOffsets.value(i, 0);
        writeLE32(buf, dispPtrAbs, displayOff);
        if (encodedStepCount == 1) {
            int diff = static_cast<int>(displayOff) - static_cast<int>(TARGET_DISPLAY_OFF);
            log << QString::fromUtf8("  [%1..%2] displayOff[%3] -> %4 (0x%5)  写: %6  diff=%7%8")
                   .arg(dispPtrAbs, 3).arg(dispPtrAbs+3, 3)
                   .arg(i)
                   .arg(displayOff)
                   .arg(displayOff, 4, 16, QChar('0')).toUpper()
                   .arg(hexDump(buf.mid(dispPtrAbs, 4)))
                   .arg(diff >= 0 ? "+" : "").arg(diff);
        } else {
            log << QString::fromUtf8("  [%1..%2] displayOff[%3] -> %4 (0x%5)  写: %6")
                   .arg(dispPtrAbs, 3).arg(dispPtrAbs+3, 3)
                   .arg(i)
                   .arg(displayOff)
                   .arg(displayOff, 4, 16, QChar('0')).toUpper()
                   .arg(hexDump(buf.mid(dispPtrAbs, 4)));
        }
    }

    // ════════════════════════════════════════════════════════
    //  6. fileSize 回填 @ [0..3]
    // ════════════════════════════════════════════════════════
    quint32 fileSize = static_cast<quint32>(buf.size());
    writeLE32(buf, OFF_FILE_SIZE, fileSize);
    log << QString::fromUtf8("  [%1..%2]  fileSize    -> %3 (0x%4)  写: %5")
           .arg(OFF_FILE_SIZE, 3).arg(OFF_FILE_SIZE+3, 3)
           .arg(fileSize)
           .arg(fileSize, 4, 16, QChar('0')).toUpper()
           .arg(hexDump(buf.mid(OFF_FILE_SIZE, 4)));

    // ════════════════════════════════════════════════════════
    //  7. 验收摘要
    // ════════════════════════════════════════════════════════
    log << SEP;
    log << QString::fromUtf8("  [SUMMARY] payloadLen   = %1 bytes").arg(buf.size());
    log << QString::fromUtf8("  [SUMMARY] len          = %1 bytes (Cmd+Seq+Payload)")
           .arg(1 + 2 + buf.size());
    log << QString::fromUtf8("  [SUMMARY] stepNum      = declared:%1  encoded:%2  payload:%3")
           .arg(declaredStepNum).arg(encodedStepCount).arg(stepNumForPayload);
    log << QString::fromUtf8("  [SUMMARY] header        = 0..159     (160B)");
    log << QString::fromUtf8("  [SUMMARY] stepTableStart= %1").arg(stepTableStart);
    log << QString::fromUtf8("  [SUMMARY] stepTable     = %1..%2  (%3B)")
           .arg(stepTableStart)
           .arg(stepTableStart + stepTableLen - 1)
           .arg(stepTableLen);
    log << QString::fromUtf8("  [SUMMARY] stringAreaStart= %1").arg(stringAreaStart);
    for (int i = 0; i < encodedStepCount; ++i) {
        const quint32 begin = stepDetailOffsets.value(i, 0);
        const quint32 end = (i + 1 < stepDetailOffsets.size())
                ? (stepDetailOffsets[i + 1] - 1)
                : (offsetOfTaskName - 1);
        log << QString::fromUtf8("  [SUMMARY] stepDetail[%1] = %2..%3  (%4B encoder=%5)")
               .arg(i).arg(begin).arg(end).arg(end - begin + 1).arg(encName);
    }
    log << QString::fromUtf8("  [SUMMARY] taskName      = %1..%2  (%3B)")
           .arg(offsetOfTaskName)
           .arg(buf.size() - 1)
           .arg(buf.size() - offsetOfTaskName);
    log << QString::fromUtf8("  [SUMMARY] fileSize      = %1 (0x%2)")
           .arg(fileSize)
           .arg(fileSize, 4, 16, QChar('0')).toUpper();
    log << QString::fromUtf8("  [SUMMARY] taskNameOff   = %1 (0x%2)")
           .arg(offsetOfTaskName)
           .arg(offsetOfTaskName, 4, 16, QChar('0')).toUpper();
    for (int i = 0; i < encodedStepCount; ++i) {
        const quint32 displayOff = stepDetailOffsets.value(i, 0);
        log << QString::fromUtf8("  [SUMMARY] displayOff[%1] = %2 (0x%3)")
               .arg(i).arg(displayOff)
               .arg(displayOff, 4, 16, QChar('0')).toUpper();
    }
    if (encodedStepCount == 1) {
        log << QString::fromUtf8("  [ALIGN]  taskNameOff目标=%1(0xD6)  实际=%2  diff=%3")
               .arg(TARGET_TASKNAME_OFF).arg(offsetOfTaskName)
               .arg(static_cast<int>(offsetOfTaskName)-static_cast<int>(TARGET_TASKNAME_OFF));
        log << QString::fromUtf8("  [ALIGN]  displayOff 目标=%1(0xA8)  实际=%2  diff=%3")
               .arg(TARGET_DISPLAY_OFF).arg(stepDetailOffsets.value(0, 0))
               .arg(static_cast<int>(stepDetailOffsets.value(0, 0))-static_cast<int>(TARGET_DISPLAY_OFF));
    }

    // 验证 payload 中 fileSize+version 连续序列（原产品特征）
    // 11 01 00 00 01 11 00 00 -> fileSize(LE4) + version(01 11) + reserved(00 00)
    // fileSize = buf.size() -> bytes[0..3]
    {
        QStringList chk;
        for (int i = 0; i < 8 && i < buf.size(); ++i)
            chk << QString::fromLatin1("%1").arg(static_cast<quint8>(buf[i]), 2, 16, QChar('0')).toUpper();
        log << QString::fromUtf8("  [CHECK] payload[0..7] = %1").arg(chk.join(' '));
        log << QString::fromUtf8("          原产品目标:   11 01 00 00 01 11 00 00  (fileSize=%1)")
               .arg(fileSize);
    }
    log << SEP;

    res.payload = buf;
    res.ok = true;
    return res;
}
