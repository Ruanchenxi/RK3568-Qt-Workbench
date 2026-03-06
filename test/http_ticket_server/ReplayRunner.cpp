// ════════════════════════════════════════════════════════════════
//  ReplayRunner.cpp  – Stage 5.4
//  回放测试 + 字节级对比 + 字段区域提示
// ════════════════════════════════════════════════════════════════
#include "ReplayRunner.h"
#include "TicketPayloadEncoder.h"
#include "TicketFrameBuilder.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QtMath>

// ────────────────────────────────────────────────────────────
//  内部小工具
// ────────────────────────────────────────────────────────────
static QString fmtByte(quint8 b)
{
    return QString::fromLatin1("%1").arg(b, 2, 16, QChar('0')).toUpper();
}

// 把字节数组格式化为 XX XX XX ... 字符串
static QString hexRow(const QByteArray &d, int from, int len)
{
    QStringList parts;
    for (int i = from; i < from + len && i < d.size(); ++i)
        parts << fmtByte(static_cast<quint8>(d[i]));
    return parts.join(' ');
}

// ────────────────────────────────────────────────────────────
//  JSON 读取 / 摘要
// ────────────────────────────────────────────────────────────
bool ReplayRunner::loadTicketObjectFromFile(const QString &jsonPath,
                                            QJsonObject &ticketObj,
                                            QString &errorMsg)
{
    QFile jf(jsonPath);
    if (!jf.open(QIODevice::ReadOnly)) {
        errorMsg = QString::fromUtf8("无法打开 JSON 文件: %1  (%2)")
                .arg(jsonPath, jf.errorString());
        return false;
    }

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(jf.readAll(), &pe);
    jf.close();

    if (doc.isNull() || !doc.isObject()) {
        errorMsg = QString::fromUtf8("JSON 解析失败: %1").arg(pe.errorString());
        return false;
    }

    QJsonObject root = doc.object();
    ticketObj = (root.contains("data") && root["data"].isObject())
            ? root["data"].toObject()
            : root;
    return true;
}

QStringList ReplayRunner::ticketFieldSummary(const QJsonObject &ticketObj)
{
    QStringList out;
    out << QString::fromUtf8("  === JSON 摘要 ===");
    out << QString::fromUtf8("  taskId     : %1").arg(ticketObj.value("taskId").toString());
    out << QString::fromUtf8("  taskName   : %1").arg(ticketObj.value("taskName").toString());
    out << QString::fromUtf8("  stepNum    : %1").arg(ticketObj.value("stepNum").toInt(0));
    out << QString::fromUtf8("  createTime : %1").arg(ticketObj.value("createTime").toString());
    out << QString::fromUtf8("  planTime   : %1").arg(ticketObj.value("planTime").toString());

    QJsonArray steps = ticketObj.value("steps").toArray();
    if (steps.isEmpty()) {
        out << QString::fromUtf8("  steps      : (空)");
        return out;
    }

    for (int i = 0; i < steps.size(); ++i) {
        if (!steps[i].isObject()) {
            out << QString::fromUtf8("  step[%1]   : (非对象，跳过)").arg(i);
            continue;
        }
        QJsonObject step = steps[i].toObject();
        out << QString::fromUtf8("  step[%1]:").arg(i);
        out << QString::fromUtf8("    innerCode      = %1").arg(step.value("innerCode").toInt(0));
        out << QString::fromUtf8("    lockerOperate  = %1").arg(step.value("lockerOperate").toInt(0));
        out << QString::fromUtf8("    stepDetail     = %1").arg(step.value("stepDetail").toString());
    }

    return out;
}

// ────────────────────────────────────────────────────────────
//  字段区域标签（帧内 offset）
//
//  outer header : [0..10]   7E 6C KeyId FrameNo Station Device Len_Lo Len_Hi Cmd Seq_Lo Seq_Hi
//  payload      : [11 .. 11+payloadLen-1]
//    payload[0..3]  : fileSize
//    payload[4..5]  : version
//    payload[6..7]  : reserved
//    payload[8..23] : taskId
//    payload[24]    : stationId
//    payload[25]    : ticketAttr
//    payload[26..27]: stepNum
//    payload[28..31]: taskNameOff
//    payload[32..39]: createTime
//    payload[40..47]: planTime
//    payload[48..95]: auth
//    payload[96..127]: addFields
//    payload[128..159]: reserved2
//    payload[160..167]: step[0] innerCode/lockerOp/state
//    payload[168..171]: step[0] displayOff
//    payload[172+]: string area (stepDetail / taskName)
//  CRC  : last 2 bytes
// ────────────────────────────────────────────────────────────
QString ReplayRunner::fieldLabel(int frameOffset, int payloadLen)
{
    // outer header (0-10)
    if (frameOffset < 11) {
        static const char *names[] = {
            "SOF(7E)","魔字(6C)","KeyId","FrameNo","Station","Device",
            "Len_Lo","Len_Hi","Cmd","Seq_Lo","Seq_Hi"
        };
        return QString::fromUtf8(names[frameOffset]);
    }

    // crc (last 2 bytes, offset from start)
    if (payloadLen > 0) {
        int frameEnd = 11 + payloadLen + 2; // total frame length
        if (frameOffset >= frameEnd - 2)
            return QString::fromUtf8("CRC");
    }

    // payload area
    int po = frameOffset - 11;  // offset within payload
    if (po < 0) return "?";
    if (po < 4)   return QString::fromUtf8("fileSize");
    if (po < 6)   return QString::fromUtf8("version");
    if (po < 8)   return QString::fromUtf8("reserved");
    if (po < 24)  return QString::fromUtf8("taskId");
    if (po < 25)  return QString::fromUtf8("stationId");
    if (po < 26)  return QString::fromUtf8("ticketAttr");
    if (po < 28)  return QString::fromUtf8("stepNum");
    if (po < 32)  return QString::fromUtf8("taskNameOff");
    if (po < 40)  return QString::fromUtf8("createTime");
    if (po < 48)  return QString::fromUtf8("planTime");
    if (po < 96)  return QString::fromUtf8("auth(FF×48)");
    if (po < 128) return QString::fromUtf8("addFields(FF×32)");
    if (po < 160) return QString::fromUtf8("reserved2(FF×32)");
    if (po < 162) return QString::fromUtf8("step[0].innerCode");
    if (po < 163) return QString::fromUtf8("step[0].lockerOp");
    if (po < 164) return QString::fromUtf8("step[0].state");
    if (po < 168) return QString::fromUtf8("step[0].displayOff");
    if (po < 172) return QString::fromUtf8("step[0].displayOff");
    return QString::fromUtf8("stringArea");
}

// ────────────────────────────────────────────────────────────
//  帧字段摘要
// ────────────────────────────────────────────────────────────
QStringList ReplayRunner::frameFieldSummary(const QByteArray &frame)
{
    QStringList out;
    const int sz = frame.size();
    if (sz < 13) {
        out << QString::fromUtf8("  [ERROR] 帧太短 (%1 B)，无法解析").arg(sz);
        return out;
    }

    auto b = [&](int i) -> quint8 { return static_cast<quint8>(frame[i]); };

    // outer header
    quint16 lenField = (quint16)b(6) | ((quint16)b(7) << 8);
    int payloadLen = (int)lenField - 3; // Len = Cmd(1)+Seq(2)+payload

    out << QString::fromUtf8("┌──────────── 外层帧头 ────────────");
    out << QString::fromUtf8("│  SOF     [0]  = %1  [1]  = %2").arg(fmtByte(b(0))).arg(fmtByte(b(1)));
    out << QString::fromUtf8("│  KeyId   [2]  = %1").arg(fmtByte(b(2)));
    out << QString::fromUtf8("│  FrameNo [3]  = %1").arg(fmtByte(b(3)));
    out << QString::fromUtf8("│  Station [4]  = %1").arg(fmtByte(b(4)));
    out << QString::fromUtf8("│  Device  [5]  = %1").arg(fmtByte(b(5)));
    out << QString::fromUtf8("│  Len     [6..7] = %1 %2  (LE=%3 = 0x%4)  → payloadLen=%5")
           .arg(fmtByte(b(6))).arg(fmtByte(b(7)))
           .arg(lenField).arg(lenField,4,16,QChar('0')).toUpper()
           .arg(payloadLen);
    out << QString::fromUtf8("│  Cmd     [8]  = %1").arg(fmtByte(b(8)));
    out << QString::fromUtf8("│  Seq     [9..10] = %1 %2").arg(fmtByte(b(9))).arg(fmtByte(b(10)));

    // CRC (last 2 bytes, big-endian)
    if (sz >= 2) {
        quint8 chi = b(sz-2), clo = b(sz-1);
        out << QString::fromUtf8("│  CRC     [%1..%2] = %3 %4  (BE=0x%5%6)")
               .arg(sz-2).arg(sz-1)
               .arg(fmtByte(chi)).arg(fmtByte(clo))
               .arg(fmtByte(chi)).arg(fmtByte(clo));
    }

    if (payloadLen < 1 || sz < 11 + payloadLen + 2) {
        out << QString::fromUtf8("└──────────── payload 解析异常 ────────────");
        return out;
    }

    out << QString::fromUtf8("├──────────── Payload (%1 B, frame[11..%2]) ────────────")
           .arg(payloadLen).arg(11+payloadLen-1);

    // payload 从 frame[11] 开始
    auto pb = [&](int pOffset) -> quint8 {
        int fi = 11 + pOffset;
        if (fi >= sz) return 0xFF;
        return b(fi);
    };
    auto leU32 = [&](int pOff) -> quint32 {
        return (quint32)pb(pOff)
             | ((quint32)pb(pOff+1)<<8)
             | ((quint32)pb(pOff+2)<<16)
             | ((quint32)pb(pOff+3)<<24);
    };
    auto leU16 = [&](int pOff) -> quint16 {
        return (quint16)pb(pOff) | ((quint16)pb(pOff+1)<<8);
    };

    // fileSize
    quint32 fileSize = leU32(0);
    quint16 stepNum = leU16(26);
    const int stepTableStart = 160;
    const int stepTableLen = stepNum * 8;
    const int stringAreaStart = stepTableStart + stepTableLen;

    out << QString::fromUtf8("│  p[ 0.. 3] fileSize     = %1 (0x%2)  %3")
           .arg(fileSize).arg(fileSize,4,16,QChar('0')).toUpper()
           .arg(hexRow(frame, 11+0, 4));
    out << QString::fromUtf8("│  frameLen              = %1").arg(sz);
    out << QString::fromUtf8("│  payloadLen            = %1").arg(payloadLen);
    out << QString::fromUtf8("│  Len                   = %1 (0x%2)")
           .arg(lenField).arg(lenField,4,16,QChar('0')).toUpper();

    // version
    out << QString::fromUtf8("│  p[ 4.. 5] version      = %1 %2")
           .arg(fmtByte(pb(4))).arg(fmtByte(pb(5)));

    // reserved
    out << QString::fromUtf8("│  p[ 6.. 7] reserved     = %1 %2")
           .arg(fmtByte(pb(6))).arg(fmtByte(pb(7)));

    // taskId (8 bytes meaningful LE, rest FF)
    quint64 taskIdVal = 0;
    for (int i = 7; i >= 0; --i) taskIdVal = (taskIdVal << 8) | pb(8+i);
    out << QString::fromUtf8("│  p[ 8..23] taskId       = %1  (uint64-LE=%2)")
           .arg(hexRow(frame,11+8,16)).arg(taskIdVal);

    // stationId / ticketAttr / stepNum
    out << QString::fromUtf8("│  p[24]     stationId    = %1").arg(fmtByte(pb(24)));
    out << QString::fromUtf8("│  p[25]     ticketAttr   = %1").arg(fmtByte(pb(25)));
    out << QString::fromUtf8("│  p[26..27] stepNum      = %1").arg(stepNum);
    out << QString::fromUtf8("│  stringAreaStart       = %1 (0x%2)")
           .arg(stringAreaStart)
           .arg(stringAreaStart, 4, 16, QChar('0')).toUpper();

    // taskNameOff
    quint32 taskNameOff = leU32(28);
    out << QString::fromUtf8("│  p[28..31] taskNameOff  = %1 (0x%2)  %3  目标:0xD6=214")
           .arg(taskNameOff).arg(taskNameOff,2,16,QChar('0')).toUpper()
           .arg(hexRow(frame,11+28,4));

    // createTime / planTime (show first 8 bytes raw)
    out << QString::fromUtf8("│  p[32..39] createTime   = %1").arg(hexRow(frame,11+32,8));
    out << QString::fromUtf8("│  p[40..47] planTime     = %1").arg(hexRow(frame,11+40,8));

    // step table
    if (payloadLen > stepTableStart && stepNum > 0) {
        for (int i = 0; i < stepNum; ++i) {
            const int pBase = stepTableStart + i * 8;
            if (pBase + 7 >= payloadLen)
                break;

            quint16 ic  = leU16(pBase);
            quint8  lop = pb(pBase + 2);
            quint8  st  = pb(pBase + 3);
            quint32 doff= leU32(pBase + 4);

            out << QString::fromUtf8("├──────────── step[%1] (p[%2..%3]) ────────────")
                   .arg(i).arg(pBase).arg(pBase + 7);
            out << QString::fromUtf8("│  p[%1..%2] innerCode  = %3 %4  (=%5)")
                   .arg(pBase, 3).arg(pBase + 1, 3)
                   .arg(fmtByte(pb(pBase))).arg(fmtByte(pb(pBase + 1))).arg(ic);
            out << QString::fromUtf8("│  p[%1]      lockerOp   = %2")
                   .arg(pBase + 2, 3).arg(fmtByte(lop));
            out << QString::fromUtf8("│  p[%1]      state      = %2")
                   .arg(pBase + 3, 3).arg(fmtByte(st));
            out << QString::fromUtf8("│  p[%1..%2] displayOff = %3 (0x%4)  %5")
                   .arg(pBase + 4, 3).arg(pBase + 7, 3)
                   .arg(doff).arg(doff,2,16,QChar('0')).toUpper()
                   .arg(hexRow(frame,11 + pBase + 4,4));
        }
    }

    out << QString::fromUtf8("└──────────────────────────────────────────────────");
    out << QString::fromUtf8("   帧总长 = %1 B").arg(sz);

    return out;
}

// ────────────────────────────────────────────────────────────
//  toHexDump : 二进制 → 空格分隔 HEX，每行 bytesPerRow 字节
// ────────────────────────────────────────────────────────────
QString ReplayRunner::toHexDump(const QByteArray &data, int bytesPerRow)
{
    QStringList lines;
    int sz = data.size();
    for (int r = 0; r < sz; r += bytesPerRow) {
        QStringList row;
        for (int c = r; c < r+bytesPerRow && c < sz; ++c)
            row << fmtByte(static_cast<quint8>(data[c]));
        lines << row.join(' ');
    }
    return lines.join('\n');
}

// ────────────────────────────────────────────────────────────
//  parseHexString / parseHexFile
//  支持格式：
//    - 空格/Tab/换行分隔的 HEX token（"7E 6C 00 ..."）
//    - 连续串（"7E6C00..."），自动按 2 字符拆分
//    - 行首的 "SEN:"/"RCV:"/时间戳等非 hex 前缀自动跳过
//    - '#' 开头的行视为注释行
// ────────────────────────────────────────────────────────────
QByteArray ReplayRunner::parseHexString(const QString &text, QStringList &errors)
{
    QByteArray result;

    // 按行处理，方便过滤注释和有prefix的行
    QStringList lines = text.split(QRegularExpression(R"([\r\n]+)"));
    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('#')) continue;  // 全行注释

        // 去掉行内注释（# 之后的内容）
        int hashPos = line.indexOf('#');
        if (hashPos >= 0) line = line.left(hashPos).trimmed();
        if (line.isEmpty()) continue;

        // 去掉常见串口日志前缀（不区分大小写）
        static const QRegularExpression prefixRe(
            "^(SEN|RCV|SEND|RECV|TX|RX|<|>)\\s*:?\\s*",
            QRegularExpression::CaseInsensitiveOption);
        line = line.replace(prefixRe, "");
        line = line.trimmed();

        // 去掉行内可能出现的 "|" 分隔符（wireshark格式）
        line = line.replace('|', ' ');

        // 提取所有 hex token
        // 先尝试以空格分隔的方式解析
        QStringList tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        for (const QString &tok : tokens) {
            // 一个 token 可能是纯 hex （"7E" 或 "7E6C00"）
            // 也可能是带下标的 "7E[0]" 之类（wireshark），只保留 hex 部分
            QString cleaned;
            for (QChar c : tok) {
                if ((c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F'))
                    cleaned += c;
            }
            if (cleaned.isEmpty()) continue;
            if (cleaned.length() % 2 != 0) {
                errors << QString::fromUtf8("  [WARN] token '%1' 长度奇数，跳过").arg(tok);
                continue;
            }
            bool ok;
            for (int i = 0; i < cleaned.length(); i += 2) {
                quint8 byte = static_cast<quint8>(cleaned.mid(i, 2).toUInt(&ok, 16));
                if (!ok) {
                    errors << QString::fromUtf8("  [WARN] 无法解析 hex '%1'").arg(cleaned.mid(i,2));
                    continue;
                }
                result.append(static_cast<char>(byte));
            }
        }
    }
    return result;
}

QByteArray ReplayRunner::parseHexFile(const QString &path, QStringList &errors)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errors << QString::fromUtf8("  [ERROR] 无法打开 golden hex 文件: %1  (%2)")
                  .arg(path).arg(f.errorString());
        return QByteArray();
    }
    QString content = QString::fromLatin1(f.readAll());
    f.close();
    return parseHexString(content, errors);
}

// ────────────────────────────────────────────────────────────
//  buildDiff
// ────────────────────────────────────────────────────────────
ReplayRunner::DiffReport ReplayRunner::buildDiff(const QByteArray &generated,
                                                  const QByteArray &golden)
{
    DiffReport report;
    report.generatedLen = generated.size();
    report.goldenLen    = golden.size();

    QStringList &out = report.lines;
    const int CONTEXT = 16;  // 差异区间前后各打印 16B

    // 从 Len 字段推断 payloadLen
    int payloadLen = -1;
    if (generated.size() >= 8) {
        quint16 lenField = (quint16)(quint8)generated[6]
                         | ((quint16)(quint8)generated[7] << 8);
        payloadLen = (int)lenField - 3;
    }

    out << QString::fromUtf8("═══════════════════ DIFF REPORT ═══════════════════");
    out << QString::fromUtf8("  生成帧长度  : %1 B").arg(report.generatedLen);
    out << QString::fromUtf8("  Golden 帧长 : %1 B").arg(report.goldenLen);

    if (report.generatedLen != report.goldenLen) {
        out << QString::fromUtf8("  [WARN] 长度不等 → diff 基于较短长度 (%1 B) 对比")
               .arg(qMin(report.generatedLen, report.goldenLen));
    }

    int cmpLen = qMin(generated.size(), golden.size());

    // 找出所有连续差异区间
    QList<DiffRange> diffs;
    int i = 0;
    while (i < cmpLen) {
        if ((quint8)generated[i] != (quint8)golden[i]) {
            DiffRange dr;
            dr.start = i;
            while (i < cmpLen && (quint8)generated[i] != (quint8)golden[i])
                ++i;
            dr.end = i - 1;
            // 用首字节的 offset 确定字段标签
            dr.fieldHint = fieldLabel(dr.start, payloadLen);
            diffs << dr;
        } else {
            ++i;
        }
    }

    // 若长度不等，尾部当作全部差异
    if (report.generatedLen != report.goldenLen) {
        int trailStart = cmpLen;
        int trailEnd   = qMax(generated.size(), golden.size()) - 1;
        DiffRange dr;
        dr.start = trailStart;
        dr.end   = trailEnd;
        dr.fieldHint = fieldLabel(trailStart, payloadLen);
        diffs << dr;
    }

    report.diffs = diffs;

    if (diffs.isEmpty()) {
        report.pass = true;
        out << "";
        out << QString::fromUtf8("  ✅ PASS — 所有 %1 字节完全一致（包括 CRC）").arg(cmpLen);
        out << QString::fromUtf8("═══════════════════════════════════════════════════");
        return report;
    }

    report.pass = false;
    out << QString::fromUtf8("  ❌ FAIL — 共 %1 处连续差异区间").arg(diffs.size());
    out << "";

    for (int di = 0; di < diffs.size(); ++di) {
        const DiffRange &dr = diffs[di];
        int len = dr.end - dr.start + 1;
        out << QString::fromUtf8("  ── 差异区间 #%1 : offset [%2..%3]  (%4 B)  字段: %5")
               .arg(di+1).arg(dr.start).arg(dr.end).arg(len).arg(dr.fieldHint);

        // 上下文窗口
        int ctxFrom = qMax(0, dr.start - CONTEXT);
        int ctxTo   = qMin(qMax(generated.size(), golden.size()) - 1, dr.end + CONTEXT);

        // 打印: 行号 | 偏移 | 生成 | golden
        out << QString::fromUtf8("     offset  生成(gen)   golden   diff");
        for (int k = ctxFrom; k <= ctxTo; ++k) {
            bool inDiff = (k >= dr.start && k <= dr.end);
            QString genStr = (k < generated.size()) ? fmtByte((quint8)generated[k]) : "--";
            QString golStr = (k < golden.size())    ? fmtByte((quint8)golden[k])    : "--";
            bool differ    = (genStr != golStr);
            QString marker = differ ? " <--" : "";
            QString fieldNote = differ ? QString("  (%1)").arg(fieldLabel(k, payloadLen)) : "";
            out << QString::fromUtf8("     [%1]  %2   %3%4%5")
                   .arg(k, 4)
                   .arg(genStr, 2).arg(golStr, 2)
                   .arg(inDiff ? marker : "")
                   .arg(fieldNote);
        }
        out << "";
    }

    // 首处差异摘要
    const DiffRange &first = diffs[0];
    if (first.start < generated.size() && first.start < golden.size()) {
        out << QString::fromUtf8("  首处差异: offset=%1  生成=%2  期望=%3  字段=%4")
               .arg(first.start)
               .arg(fmtByte((quint8)generated[first.start]))
               .arg(fmtByte((quint8)golden[first.start]))
               .arg(first.fieldHint);
    }

    out << QString::fromUtf8("═══════════════════════════════════════════════════");
    return report;
}

// ────────────────────────────────────────────────────────────
//  主入口
// ────────────────────────────────────────────────────────────
int ReplayRunner::run(const QString &jsonPath,
                      const QString &goldenPath,
                      const QString &outPath)
{
    QStringList log;
    const QString SEP = QString::fromUtf8("────────────────────────────────────────────────");

    log << SEP;
    log << QString::fromUtf8("[REPLAY] %1")
           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    log << QString::fromUtf8("  JSON   : %1").arg(jsonPath);
    if (!goldenPath.isEmpty())
        log << QString::fromUtf8("  Golden : %1").arg(goldenPath);
    if (!outPath.isEmpty())
        log << QString::fromUtf8("  Out    : %1").arg(outPath);
    log << SEP;

    // ────────────────────────────────────────────────────────
    //  1. 读取 JSON 文件
    // ────────────────────────────────────────────────────────
    QJsonObject ticketObj;
    QString loadError;
    if (!loadTicketObjectFromFile(jsonPath, ticketObj, loadError)) {
        log << QString::fromUtf8("  [ERROR] %1").arg(loadError);
        for (const QString &l : qAsConst(log)) fprintf(stdout, "%s\n", l.toUtf8().constData());
        return 1;
    }

    log << QString::fromUtf8("  JSON 解析成功，字段数=%1").arg(ticketObj.keys().size());
    log << QString::fromUtf8("  字段: %1").arg(ticketObj.keys().join(", "));
    QStringList ticketSummary = ticketFieldSummary(ticketObj);
    for (const QString &line : qAsConst(ticketSummary))
        log << "  " + line;

    // ────────────────────────────────────────────────────────
    //  2. 编码 payload
    // ────────────────────────────────────────────────────────
    log << SEP;
    log << QString::fromUtf8("  === [ENCODER] ===");

    TicketPayloadEncoder::EncodeResult enc =
        TicketPayloadEncoder::encode(ticketObj, 0x01,
                                     TicketPayloadEncoder::StringEncoding::GBK);

    for (const QString &fl : qAsConst(enc.fieldLog))
        log << "  " + fl;

    if (!enc.ok) {
        log << QString::fromUtf8("  [ERROR] 编码失败: %1").arg(enc.errorMsg);
        for (const QString &l : qAsConst(log)) fprintf(stdout, "%s\n", l.toUtf8().constData());
        return 1;
    }
    log << QString::fromUtf8("  payload 生成成功: %1 B").arg(enc.payload.size());

    // ────────────────────────────────────────────────────────
    //  3. 封帧
    // ────────────────────────────────────────────────────────
    log << SEP;
    log << QString::fromUtf8("  === [FRAME] ===");

    TicketFrameBuilder builder(0x01, 0x00, 0x00);
    QList<QByteArray> frames = builder.buildFrames(enc.payload);

    if (frames.isEmpty()) {
        log << QString::fromUtf8("  [ERROR] buildFrames 返回空列表");
        for (const QString &l : qAsConst(log)) fprintf(stdout, "%s\n", l.toUtf8().constData());
        return 1;
    }

    QStringList frameLogLines, hexLines;
    builder.formatFrameLog(frames, frameLogLines, hexLines);
    for (const QString &fl : qAsConst(frameLogLines))
        log << "  " + fl;

    // 取第一帧（单帧模式）
    QByteArray generated = frames[0];
    log << QString::fromUtf8("  生成帧大小: %1 B").arg(generated.size());

    // ────────────────────────────────────────────────────────
    //  4. 字段摘要
    // ────────────────────────────────────────────────────────
    log << SEP;
    log << QString::fromUtf8("  === [FIELD SUMMARY] ===");
    QStringList summary = frameFieldSummary(generated);
    for (const QString &s : qAsConst(summary))
        log << "  " + s;

    // ────────────────────────────────────────────────────────
    //  5. 保存生成帧
    // ────────────────────────────────────────────────────────
    // 如果 outPath 指定了就写到那里，否则写到 JSON 旁边
    QString actualOut = outPath;
    if (actualOut.isEmpty()) {
        QFileInfo ji(jsonPath);
        actualOut = ji.dir().filePath(
            ji.completeBaseName() + "_generated.hex");
    }
    {
        QFile of(actualOut);
        if (of.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream ts(&of);
            ts << toHexDump(generated, 16) << "\n";
            of.close();
            log << SEP;
            log << QString::fromUtf8("  生成帧已写入: %1").arg(actualOut);
        } else {
            log << QString::fromUtf8("  [WARN] 写文件失败: %1").arg(of.errorString());
        }
    }
    // 同时写 .bin
    {
        QFileInfo fi(actualOut);
        QString binPath = fi.dir().filePath(fi.completeBaseName() + ".bin");
        QFile bf(binPath);
        if (bf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            bf.write(generated);
            bf.close();
            log << QString::fromUtf8("  生成帧 BIN  : %1").arg(binPath);
        }
    }

    // 也写一行紧凑 HEX
    log << "";
    log << QString::fromUtf8("  生成帧（单行 HEX）:");
    log << "  " + toHexDump(generated, generated.size());
    log << "";
    log << QString::fromUtf8("  生成帧（分行 HEX，每行16B）:");
    QStringList dumpLines = toHexDump(generated, 16).split('\n');
    for (const QString &dl : qAsConst(dumpLines))
        log << "  " + dl;

    // ────────────────────────────────────────────────────────
    //  6. Golden 对比（可选）
    // ────────────────────────────────────────────────────────
    int exitCode = 0;
    if (!goldenPath.isEmpty()) {
        log << SEP;
        log << QString::fromUtf8("  === [DIFF vs Golden] ===");

        QStringList parseErrs;
        QByteArray golden = parseHexFile(goldenPath, parseErrs);
        for (const QString &e : qAsConst(parseErrs))
            log << e;

        if (golden.isEmpty()) {
            log << QString::fromUtf8("  [ERROR] golden 文件为空或解析失败: %1").arg(goldenPath);
            exitCode = 1;
        } else {
            log << QString::fromUtf8("  Golden  字段摘要:");
            QStringList gsum = frameFieldSummary(golden);
            for (const QString &s : qAsConst(gsum))
                log << "    " + s;
            log << "";

            DiffReport dr = buildDiff(generated, golden);
            for (const QString &dl : qAsConst(dr.lines))
                log << dl;

            exitCode = dr.pass ? 0 : 1;
        }
    } else {
        log << SEP;
        log << QString::fromUtf8("  [INFO] 未指定 --golden，仅生成模式，跳过对比");
        log << QString::fromUtf8("         如需对比请传入 --golden <原产品抓包.hex>");
    }

    // ────────────────────────────────────────────────────────
    //  7. 打印所有日志
    // ────────────────────────────────────────────────────────
    for (const QString &l : qAsConst(log))
        fprintf(stdout, "%s\n", l.toUtf8().constData());
    fflush(stdout);

    return exitCode;
}
