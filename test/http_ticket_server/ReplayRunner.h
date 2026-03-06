#pragma once
// ════════════════════════════════════════════════════════════════
//  ReplayRunner – Stage 5.4
//
//  功能：
//    1) 从 JSON 文件加载票据数据（UTF-8），调用 TicketPayloadEncoder +
//       TicketFrameBuilder 生成最终串口帧
//    2) 从 golden.hex 加载"原产品抓包参考帧"：
//         - 支持空格/换行分隔的 hex 文本
//         - 自动过滤前缀（"SEN:"/"RCV:" 等串口日志标记）和注释行
//    3) 做字节级对比，输出 diff 报告：
//         - 总长度差异
//         - 第一处 / 所有连续不同区间（带前后 16B 上下文）
//         - 每个差异区间标注所属字段（外层头 / payload 各段）
//    4) 若完全一致：输出 PASS（含 CRC 确认）
//
//  命令行示例（exe 不启 GUI，仅控制台）：
//    http_ticket_server.exe --replay tests/sample_http.json
//    http_ticket_server.exe --replay tests/sample_http.json --golden tests/sample_golden.hex
//    http_ticket_server.exe --replay tests/sample_http.json --golden tests/sample_golden.hex --out out_frame.hex
//
//  返回值：0 = PASS / 无 golden 时生成成功，1 = FAIL / 读文件错误
// ════════════════════════════════════════════════════════════════
#ifndef REPLAYRUNNER_H
#define REPLAYRUNNER_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class ReplayRunner
{
public:
    // ---- 差异区间 ----
    struct DiffRange {
        int start;      // 帧字节偏移（含）
        int end;        // 帧字节偏移（含）
        QString fieldHint;  // 字段区域提示
    };

    // ---- 对比报告 ----
    struct DiffReport {
        bool pass = false;
        int  generatedLen = 0;
        int  goldenLen    = 0;
        QList<DiffRange> diffs;   // 连续差异区间（空 = PASS）
        QStringList lines;        // 可直接打印的报告行
    };

    // ────────────────────────────────────────────────────────────
    //  主入口：--replay / --golden / --out 参数值直接传入
    //  jsonPath  : 票据 JSON 文件路径（UTF-8，可以是平铺对象或 {data:{...}}）
    //  goldenPath: 可选，原产品 hex 文件路径；为空则仅生成不对比
    //  outPath   : 可选，输出生成帧的 hex 文件路径
    //  返回 0 = OK/PASS，1 = FAIL/错误
    // ────────────────────────────────────────────────────────────
    static int run(const QString &jsonPath,
                   const QString &goldenPath = QString(),
                   const QString &outPath    = QString());

    // ────────────────────────────────────────────────────────────
    //  工具函数（public 方便单元测试）
    // ────────────────────────────────────────────────────────────

    // 从 JSON 文件读取票据对象；兼容根对象和 {data:{...}} 两种格式
    static bool loadTicketObjectFromFile(const QString &jsonPath,
                                         QJsonObject &ticketObj,
                                         QString &errorMsg);

    // 生成人工联调用 JSON 摘要（taskId/taskName/stepNum/steps...）
    static QStringList ticketFieldSummary(const QJsonObject &ticketObj);

    // 解析 hex 文件：空格/换行分隔，自动跳过"SEN:"/"RCV:"等前缀及注释行
    // errors 返回解析警告（不影响继续）
    static QByteArray parseHexFile(const QString &path, QStringList &errors);

    // 从已有的 hex 文本字符串中解析（上面调用它）
    static QByteArray parseHexString(const QString &text, QStringList &errors);

    // 生成 hex 字符串（空格分隔，每行 16 字节）
    static QString toHexDump(const QByteArray &data, int bytesPerRow = 16);

    // 字节级对比，返回报告
    static DiffReport buildDiff(const QByteArray &generated,
                                const QByteArray &golden);

    // 生成"字段级解释"：给定帧偏移 → 字段区域名
    // payloadLen 用于区分 payload 区 / CRC 区
    static QString fieldLabel(int frameOffset, int payloadLen = -1);

    // 打印帧字段摘要（外层头 + 关键 payload 字段）
    static QStringList frameFieldSummary(const QByteArray &frame);

private:
    static QStringList buildDiffLines(const QByteArray &generated,
                                      const QByteArray &golden);
};

#endif // REPLAYRUNNER_H
