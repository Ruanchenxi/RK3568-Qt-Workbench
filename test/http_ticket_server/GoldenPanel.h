#pragma once
// ════════════════════════════════════════════════════════════════
//  GoldenPanel  —— 从原产品串口抓包 HEX 反解字段 → 生成 replay JSON → diff 验证
//
//  流程：
//    ① 按钮 A "解析 golden"  : 读单帧 Golden HEX，打印所有关键字段（含 GBK 中文）
//    ② 按钮 B "生成 replay.json" : 把字段写成 JSON，供编码器使用
//    ③ 按钮 C "生成帧并对比 diff" : 用 replay.json 走完整编码 → byte diff vs golden
// ════════════════════════════════════════════════════════════════
#ifndef GOLDENPANEL_H
#define GOLDENPANEL_H

#include <QWidget>
#include <QByteArray>
#include <QList>
#include <QString>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QLineEdit;

struct GoldenStepField {
    quint16 innerCode = 0;
    quint8  lockerOp = 0;
    quint8  stepState = 0;
    quint32 displayOff = 0;
    QString stepDetail;
};

// ── 解析结果结构体 ───────────────────────────────────────────────
struct GoldenFields {
    // 外层帧头
    quint8  keyId    = 0;
    quint8  frameNo  = 0;
    quint8  station  = 0;
    quint8  device   = 0;
    quint16 len      = 0;
    quint8  cmd      = 0;
    quint16 seq      = 0;
    quint16 crc      = 0;    // BE，帧最后2字节

    // payload 头部
    quint32 fileSize    = 0;
    quint8  versionByte0 = 0;
    quint8  versionByte1 = 0;
    quint64 taskIdU64   = 0;   // p[8..15] LE
    QByteArray taskIdRaw;      // p[8..23] 原始16字节
    quint8  stationIdP  = 0;   // p[24]
    quint8  ticketAttr  = 0;   // p[25]
    quint16 stepNum     = 0;   // p[26..27] LE
    quint32 taskNameOff = 0;   // p[28..31] LE
    QString createTime;        // BCD 解码后 "YYYYMMDDHHMMSS"（去除末尾填充）
    QString planTime;

    // steps / 字符串（GBK 解码）
    QList<GoldenStepField> steps;
    QString taskName;

    // 有效性
    bool    valid    = false;
    QString errorMsg;
};

// ── 面板类 ────────────────────────────────────────────────────────
class GoldenPanel : public QWidget
{
    Q_OBJECT
public:
    explicit GoldenPanel(QWidget *parent = nullptr);

private slots:
    void onSelectGolden();
    void onParse();
    void onGenerateJson();
    void onDiff();
    void onClear();

private:
    // ---- 核心逻辑 ----
    GoldenFields parseFrame(const QByteArray &frame);

    /// BCD 时间字节 → 字符串（每字节两位 BCD），遇 0xFF 停止，最多取 maxBytes 字节
    static QString decodeBcdTime(const QByteArray &data, int offset, int maxBytes = 6);

    /// GBK null-terminated 字节串解码为 QString
    static QString decodeGbkNullTerm(const QByteArray &data, int offset);

    void appendOutput(const QString &text);
    void setOutput(const QString &text);

    // ---- 控件 ----
    QLineEdit      *m_goldenPathEdit;
    QPushButton    *m_btnSelectGolden;
    QPushButton    *m_btnParse;
    QPushButton    *m_btnGenJson;
    QPushButton    *m_btnDiff;
    QPushButton    *m_btnClear;
    QLabel         *m_statusLabel;
    QPlainTextEdit *m_output;

    // ---- 状态 ----
    QByteArray   m_goldenFrame;    // 原始帧字节
    GoldenFields m_fields;         // 最后一次解析结果
    QString      m_replayJsonPath; // 最后生成的 replay JSON 路径
};

#endif // GOLDENPANEL_H
