// ════════════════════════════════════════════════════════════════
//  GoldenPanel.cpp  —— 原产品串口抓包 HEX 反解 / replay / diff 验证
// ════════════════════════════════════════════════════════════════
#include "GoldenPanel.h"
#include "ReplayRunner.h"
#include "TicketPayloadEncoder.h"
#include "TicketFrameBuilder.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCodec>
#include <QTextStream>
#include <QVBoxLayout>

// ────────────────────────────────────────────────────────────────
//  帧布局常量（Stage 5.3 确认值）
// ────────────────────────────────────────────────────────────────
static const int FRAME_HEADER_SIZE = 11;   // SOF(2)+KeyId+FrameNo+Station+Device+Len(2)+Cmd+Seq(2)
static const int FRAME_CRC_SIZE    = 2;
static const int P_FILESIZE        = 0;
static const int P_VERSION         = 4;
static const int P_TASKID          = 8;    // 16 bytes (uint64 LE in first 8, rest FF)
static const int P_STATION         = 24;
static const int P_TICKETATTR      = 25;
static const int P_STEPNUM         = 26;   // LE uint16
static const int P_TASKNAMEOFF     = 28;   // LE uint32
static const int P_CREATETIME      = 32;   // BCD 8 bytes
static const int P_PLANTIME        = 40;   // BCD 8 bytes
static const int P_STEPS_START     = 160;  // step[0] 起始 payload offset
static const int STEP_SIZE         = 8;    // 每条 step 8 字节
// step 内部偏移
static const int STEP_INNERCODE    = 0;    // LE uint16
static const int STEP_LOCKEROP     = 2;    // uint8
static const int STEP_STATE        = 3;    // uint8
static const int STEP_DISPLAYOFF   = 4;    // LE uint32

// ────────────────────────────────────────────────────────────────
//  构造函数 / UI
// ────────────────────────────────────────────────────────────────
GoldenPanel::GoldenPanel(QWidget *parent)
    : QWidget(parent)
{
    // ── 文件选择 ───────────────────────────────────────────────
    QGroupBox *fileGroup = new QGroupBox(
        QString::fromUtf8("📂  原产品串口抓包帧（Golden HEX）"), this);
    QHBoxLayout *fileRow = new QHBoxLayout(fileGroup);

    QLabel *lbl = new QLabel(QString::fromUtf8("Golden HEX："), this);
    lbl->setFixedWidth(90);
    m_goldenPathEdit = new QLineEdit(this);
    m_goldenPathEdit->setReadOnly(true);
    m_goldenPathEdit->setPlaceholderText(
        QString::fromUtf8("选择一条原产品传票帧 .hex 文件"));
    m_btnSelectGolden = new QPushButton(
        QString::fromUtf8("选择…"), this);
    m_btnSelectGolden->setFixedWidth(80);

    fileRow->addWidget(lbl);
    fileRow->addWidget(m_goldenPathEdit, 1);
    fileRow->addWidget(m_btnSelectGolden);

    // ── 操作按钮 ──────────────────────────────────────────────
    QGroupBox *btnGroup = new QGroupBox(
        QString::fromUtf8("⚙  操作"), this);
    QHBoxLayout *btnRow = new QHBoxLayout(btnGroup);

    m_btnParse   = new QPushButton(
        QString::fromUtf8("🔍  A. 解析 golden"), this);
    m_btnGenJson = new QPushButton(
        QString::fromUtf8("📝  B. 生成 replay.json"), this);
    m_btnDiff    = new QPushButton(
        QString::fromUtf8("⚡  C. 生成帧并 diff"), this);
    m_btnClear   = new QPushButton(
        QString::fromUtf8("🗑  清空"), this);

    for (auto *b : {m_btnParse, m_btnGenJson, m_btnDiff, m_btnClear})
        b->setMinimumHeight(32);

    m_btnGenJson->setEnabled(false);
    m_btnDiff->setEnabled(false);

    btnRow->addWidget(m_btnParse);
    btnRow->addWidget(m_btnGenJson);
    btnRow->addWidget(m_btnDiff);
    btnRow->addStretch();
    btnRow->addWidget(m_btnClear);

    // ── 状态行 ────────────────────────────────────────────────
    m_statusLabel = new QLabel(
        QString::fromUtf8("就绪 — 选择 Golden HEX 后点击 [解析 golden]"), this);
    m_statusLabel->setStyleSheet("font-weight:bold; padding:4px;");

    // ── 输出区 ────────────────────────────────────────────────
    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    {
        QFont mono("Consolas", 9);
        mono.setStyleHint(QFont::Monospace);
        m_output->setFont(mono);
    }
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setMinimumHeight(320);

    // ── 主布局 ────────────────────────────────────────────────
    QVBoxLayout *main = new QVBoxLayout(this);
    main->addWidget(fileGroup);
    main->addWidget(btnGroup);
    main->addWidget(m_statusLabel);
    main->addWidget(m_output, 1);

    // ── 信号 ──────────────────────────────────────────────────
    connect(m_btnSelectGolden, &QPushButton::clicked, this, &GoldenPanel::onSelectGolden);
    connect(m_btnParse,        &QPushButton::clicked, this, &GoldenPanel::onParse);
    connect(m_btnGenJson,      &QPushButton::clicked, this, &GoldenPanel::onGenerateJson);
    connect(m_btnDiff,         &QPushButton::clicked, this, &GoldenPanel::onDiff);
    connect(m_btnClear,        &QPushButton::clicked, this, &GoldenPanel::onClear);
}

// ────────────────────────────────────────────────────────────────
//  文件选择
// ────────────────────────────────────────────────────────────────
void GoldenPanel::onSelectGolden()
{
    QString initial = m_goldenPathEdit->text().trimmed();
    if (initial.isEmpty())
        initial = QCoreApplication::applicationDirPath();

    QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("选择原产品串口抓包 HEX 文件"),
        initial,
        QString::fromUtf8("HEX 文件 (*.hex *.txt);;所有文件 (*)"));

    if (!path.isEmpty()) {
        m_goldenPathEdit->setText(QDir::toNativeSeparators(path));
        m_goldenPathEdit->setToolTip(path);
        m_btnGenJson->setEnabled(false);
        m_btnDiff->setEnabled(false);
        m_goldenFrame.clear();
    }
}

// ────────────────────────────────────────────────────────────────
//  BCD 时间解码：每字节两位 BCD，遇 0xFF 停止
//  返回纯数字字符串，如 "202603050241"
// ────────────────────────────────────────────────────────────────
QString GoldenPanel::decodeBcdTime(const QByteArray &data, int offset, int maxBytes)
{
    QString result;
    for (int i = 0; i < maxBytes; ++i) {
        if (offset + i >= data.size()) break;
        quint8 b = static_cast<quint8>(data[offset + i]);
        if (b == 0xFF) break;           // 填充字节，停止
        if (b == 0x00 && i >= 4) break; // 秒的 00 以后不再需要
        result += QString::number((b >> 4) & 0x0F);
        result += QString::number(b & 0x0F);
    }
    return result;
}

// ────────────────────────────────────────────────────────────────
//  GBK null-terminated 字节串 → QString
// ────────────────────────────────────────────────────────────────
QString GoldenPanel::decodeGbkNullTerm(const QByteArray &data, int offset)
{
    if (offset < 0 || offset >= data.size()) return {};

    // 找到下一个 0x00
    int end = offset;
    while (end < data.size() && data[end] != '\0') ++end;

    QByteArray raw = data.mid(offset, end - offset);
    if (raw.isEmpty()) return {};

    QTextCodec *codec = QTextCodec::codecForName("GBK");
    if (!codec) return QString::fromLatin1(raw);
    return codec->toUnicode(raw);
}

// ────────────────────────────────────────────────────────────────
//  核心：解析单帧传票（支持动态长度，不写死 286B）
// ────────────────────────────────────────────────────────────────
GoldenFields GoldenPanel::parseFrame(const QByteArray &frame)
{
    GoldenFields f;

    if (frame.size() < FRAME_HEADER_SIZE + FRAME_CRC_SIZE) {
        f.errorMsg = QString::fromUtf8(
            "帧长度 %1 过短，无法解析").arg(frame.size());
        return f;
    }
    if ((quint8)frame[0] != 0x7E || (quint8)frame[1] != 0x6C) {
        f.errorMsg = QString::fromUtf8(
            "SOF 不匹配: %1 %2，期望 7E 6C")
            .arg((quint8)frame[0], 2, 16, QChar('0')).toUpper()
            .arg((quint8)frame[1], 2, 16, QChar('0')).toUpper();
        return f;
    }

    // ── 外层帧头 ────────────────────────────────────────────
    f.keyId   = (quint8)frame[2];
    f.frameNo = (quint8)frame[3];
    f.station = (quint8)frame[4];
    f.device  = (quint8)frame[5];
    f.len     = (quint8)frame[6] | ((quint16)(quint8)frame[7] << 8);  // LE
    f.cmd     = (quint8)frame[8];
    f.seq     = (quint8)frame[9] | ((quint16)(quint8)frame[10] << 8); // LE
    const int payloadLen = f.len - 3;
    const int expectedFrameLen = FRAME_HEADER_SIZE + payloadLen + FRAME_CRC_SIZE;
    if (payloadLen <= 0 || frame.size() < expectedFrameLen) {
        f.errorMsg = QString::fromUtf8(
            "帧长度/Len 不匹配: frame=%1, payloadLen=%2, expected=%3")
                .arg(frame.size()).arg(payloadLen).arg(expectedFrameLen);
        return f;
    }
    f.crc     = ((quint16)(quint8)frame[expectedFrameLen - 2] << 8)
              | (quint8)frame[expectedFrameLen - 1]; // BE

    // ── payload (frame[11..]) ───────────────────────────────
    // 用 payload 偏移宏，实际帧位置 = pOff + 11
#define P(off) ((int)(FRAME_HEADER_SIZE + (off)))

    f.fileSize     = (quint8)frame[P(0)]
                   | ((quint32)(quint8)frame[P(1)] << 8)
                   | ((quint32)(quint8)frame[P(2)] << 16)
                   | ((quint32)(quint8)frame[P(3)] << 24);

    f.versionByte0 = (quint8)frame[P(4)];
    f.versionByte1 = (quint8)frame[P(5)];

    // taskId: p[8..15] LE uint64
    f.taskIdRaw    = frame.mid(P(P_TASKID), 16);
    quint64 tid = 0;
    for (int i = 0; i < 8; ++i)
        tid |= ((quint64)(quint8)frame[P(P_TASKID) + i]) << (8 * i);
    f.taskIdU64    = tid;

    f.stationIdP   = (quint8)frame[P(P_STATION)];
    f.ticketAttr   = (quint8)frame[P(P_TICKETATTR)];
    f.stepNum      = (quint8)frame[P(P_STEPNUM)]
                   | ((quint16)(quint8)frame[P(P_STEPNUM + 1)] << 8);

    f.taskNameOff  = (quint8)frame[P(P_TASKNAMEOFF)]
                   | ((quint32)(quint8)frame[P(P_TASKNAMEOFF + 1)] << 8)
                   | ((quint32)(quint8)frame[P(P_TASKNAMEOFF + 2)] << 16)
                   | ((quint32)(quint8)frame[P(P_TASKNAMEOFF + 3)] << 24);

    f.createTime   = decodeBcdTime(frame, P(P_CREATETIME), 6);
    f.planTime     = decodeBcdTime(frame, P(P_PLANTIME),   6);

    for (int i = 0; i < (int)f.stepNum; ++i) {
        const int pBase = P_STEPS_START + i * STEP_SIZE;
        if (pBase + STEP_SIZE > payloadLen)
            break;

        GoldenStepField step;
        step.innerCode = (quint8)frame[P(pBase + STEP_INNERCODE)]
                       | ((quint16)(quint8)frame[P(pBase + STEP_INNERCODE + 1)] << 8);
        step.lockerOp  = (quint8)frame[P(pBase + STEP_LOCKEROP)];
        step.stepState = (quint8)frame[P(pBase + STEP_STATE)];
        step.displayOff = (quint8)frame[P(pBase + STEP_DISPLAYOFF)]
                        | ((quint32)(quint8)frame[P(pBase + STEP_DISPLAYOFF + 1)] << 8)
                        | ((quint32)(quint8)frame[P(pBase + STEP_DISPLAYOFF + 2)] << 16)
                        | ((quint32)(quint8)frame[P(pBase + STEP_DISPLAYOFF + 3)] << 24);

        int frameDispOff = FRAME_HEADER_SIZE + (int)step.displayOff;
        if (frameDispOff < expectedFrameLen)
            step.stepDetail = decodeGbkNullTerm(frame, frameDispOff);

        f.steps.append(step);
    }
#undef P

    // 字符串：taskNameOff 是 payload 偏移，帧偏移 = +11
    int frameNameOff = FRAME_HEADER_SIZE + (int)f.taskNameOff;

    if (frameNameOff < expectedFrameLen)
        f.taskName   = decodeGbkNullTerm(frame, frameNameOff);

    f.valid = true;
    return f;
}

// ────────────────────────────────────────────────────────────────
//  按钮 A：解析 golden
// ────────────────────────────────────────────────────────────────
void GoldenPanel::onParse()
{
    m_fields = GoldenFields{};
    m_goldenFrame.clear();
    m_btnGenJson->setEnabled(false);
    m_btnDiff->setEnabled(false);

    const QString path = m_goldenPathEdit->text().trimmed();
    if (path.isEmpty()) {
        m_statusLabel->setText(
            QString::fromUtf8("❌ 请先选择 Golden HEX 文件"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }

    // 解析 HEX 文件
    QStringList errors;
    QByteArray frame = ReplayRunner::parseHexFile(path, errors);
    for (const QString &e : qAsConst(errors))
        appendOutput(e);

    if (frame.isEmpty()) {
        setOutput(QString::fromUtf8("❌ HEX 文件解析失败或为空"));
        m_statusLabel->setText(QString::fromUtf8("❌ 解析失败"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }

    // 解析帧结构
    GoldenFields f = parseFrame(frame);
    if (!f.valid) {
        setOutput(QString::fromUtf8("❌ 帧解析失败：") + f.errorMsg);
        m_statusLabel->setText(QString::fromUtf8("❌ 帧解析失败"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }

    m_goldenFrame = frame;
    m_fields      = f;

    // ── 格式化输出 ────────────────────────────────────────
    const QString SEP = "────────────────────────────────────────────────────";
    QStringList out;
    out << SEP;
    out << QString::fromUtf8("[GOLDEN PARSE]  %1")
           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    out << QString::fromUtf8("  文件:  %1").arg(path);
    out << QString::fromUtf8("  帧长:  %1 B").arg(frame.size());
    out << SEP;

    out << QString::fromUtf8("  ┌─── 外层帧头 ───────────────────────────────────");
    out << QString::fromUtf8("  │  SOF     = 7E 6C");
    out << QString::fromUtf8("  │  KeyId   = 0x%1").arg(f.keyId,   2, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  FrameNo = 0x%1").arg(f.frameNo, 2, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  Station = 0x%1").arg(f.station, 2, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  Device  = 0x%1").arg(f.device,  2, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  Len     = 0x%1 (LE=%2)")
           .arg(f.len, 4, 16, QChar('0')).toUpper().arg(f.len);
    out << QString::fromUtf8("  │  Cmd     = 0x%1").arg(f.cmd, 2, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  Seq     = 0x%1 (LE=%2)")
           .arg(f.seq, 4, 16, QChar('0')).toUpper().arg(f.seq);
    out << QString::fromUtf8("  │  CRC     = 0x%1 (BE)")
           .arg(f.crc, 4, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  └────────────────────────────────────────────────");

    out << SEP;
    out << QString::fromUtf8("  ┌─── Payload 头部 ────────────────────────────────");
    out << QString::fromUtf8("  │  p[0..3]   fileSize    = %1 (0x%2)")
           .arg(f.fileSize).arg(f.fileSize, 4, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  p[4..5]   version     = %1 %2")
           .arg(f.versionByte0, 2, 16, QChar('0')).toUpper()
           .arg(f.versionByte1, 2, 16, QChar('0')).toUpper();
    // taskId 原始 HEX
    {
        QString tidHex;
        for (int i = 0; i < 16; ++i)
            tidHex += QString("%1 ").arg((quint8)f.taskIdRaw[i], 2, 16, QChar('0')).toUpper();
        out << QString::fromUtf8("  │  p[8..23]  taskId(raw) = %1").arg(tidHex.trimmed());
        out << QString::fromUtf8("  │  p[8..15]  taskId(u64) = %1  (decimal)").arg(f.taskIdU64);
    }
    out << QString::fromUtf8("  │  p[24]     stationId   = 0x%1").arg(f.stationIdP, 2, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  p[25]     ticketAttr  = 0x%1").arg(f.ticketAttr, 2, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  p[26..27] stepNum     = %1").arg(f.stepNum);
    out << QString::fromUtf8("  │  p[28..31] taskNameOff = %1 (0x%2)")
           .arg(f.taskNameOff).arg(f.taskNameOff, 4, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  │  p[32..39] createTime  = %1  (BCD→\"%2\")")
           .arg(QString::fromLatin1(frame.mid(FRAME_HEADER_SIZE + P_CREATETIME, 8).toHex(' ').toUpper()))
           .arg(f.createTime);
    out << QString::fromUtf8("  \u2502  p[40..47] planTime    = %1  (BCD\u2192\"%2\")")
           .arg(QString::fromLatin1(frame.mid(FRAME_HEADER_SIZE + P_PLANTIME, 8).toHex(' ').toUpper()))
           .arg(f.planTime);
    out << QString::fromUtf8("  └────────────────────────────────────────────────");

    out << SEP;
    out << SEP;
    out << QString::fromUtf8("  ┌─── 步骤区 / 字符串（GBK 解码）──────────────────");
    for (int i = 0; i < f.steps.size(); ++i) {
        const GoldenStepField &step = f.steps[i];
        const int pBase = P_STEPS_START + i * STEP_SIZE;
        out << QString::fromUtf8("  │  step[%1]  p[%2..%3]")
               .arg(i).arg(pBase).arg(pBase + STEP_SIZE - 1);
        out << QString::fromUtf8("  │    innerCode  = %1").arg(step.innerCode);
        out << QString::fromUtf8("  │    lockerOp   = %1").arg(step.lockerOp);
        out << QString::fromUtf8("  │    state      = 0x%1")
               .arg(step.stepState, 2, 16, QChar('0')).toUpper();
        out << QString::fromUtf8("  │    displayOff = %1 (0x%2)")
               .arg(step.displayOff).arg(step.displayOff, 4, 16, QChar('0')).toUpper();
        out << QString::fromUtf8("  │    stepDetail = %1").arg(step.stepDetail);
    }
    out << QString::fromUtf8("  │  taskName    [p%1..]: %2")
           .arg(f.taskNameOff).arg(f.taskName);
    out << QString::fromUtf8("  └────────────────────────────────────────────────");

    setOutput(out.join('\n'));
    m_btnGenJson->setEnabled(true);

    m_statusLabel->setText(
        QString::fromUtf8("✅ 解析成功  |  CRC=%1  |  taskName=%2")
            .arg(QString("0x%1").arg(f.crc, 4, 16, QChar('0')).toUpper())
            .arg(f.taskName));
    m_statusLabel->setStyleSheet("color:green; font-weight:bold; padding:4px;");
}

// ────────────────────────────────────────────────────────────────
//  按钮 B：生成 replay.json
// ────────────────────────────────────────────────────────────────
void GoldenPanel::onGenerateJson()
{
    if (!m_fields.valid) {
        m_statusLabel->setText(
            QString::fromUtf8("⚠  请先点击 [解析 golden]"));
        return;
    }
    const GoldenFields &f = m_fields;

    // ── 构建 JSON ─────────────────────────────────────────
    QJsonObject data;
    data["taskId"]   = QString::number(f.taskIdU64);
    data["taskName"] = f.taskName;
    data["createTime"] = f.createTime;
    data["planTime"]   = f.planTime;
    data["stepNum"]    = (int)f.stepNum;
    data["taskType"]   = (int)f.ticketAttr;  // 0=普通票

    // 生成所有步骤（遍历 stepNum）
    QJsonArray steps;
    for (int i = 0; i < (int)f.stepNum && i < 16; ++i) {
        int pBase = P_STEPS_START + i * STEP_SIZE;
        int fBase = FRAME_HEADER_SIZE + pBase;
        if (fBase + STEP_SIZE > m_goldenFrame.size()) break;

        quint16 ic = (quint8)m_goldenFrame[fBase + STEP_INNERCODE]
                   | ((quint16)(quint8)m_goldenFrame[fBase + STEP_INNERCODE + 1] << 8);
        quint8  lo = (quint8)m_goldenFrame[fBase + STEP_LOCKEROP];
        quint32 dOff = (quint8)m_goldenFrame[fBase + STEP_DISPLAYOFF]
                     | ((quint32)(quint8)m_goldenFrame[fBase + STEP_DISPLAYOFF + 1] << 8)
                     | ((quint32)(quint8)m_goldenFrame[fBase + STEP_DISPLAYOFF + 2] << 16)
                     | ((quint32)(quint8)m_goldenFrame[fBase + STEP_DISPLAYOFF + 3] << 24);

        QString detail;
        int frameDispOff = FRAME_HEADER_SIZE + (int)dOff;
        if (frameDispOff < m_goldenFrame.size())
            detail = decodeGbkNullTerm(m_goldenFrame, frameDispOff);

        QJsonObject step;
        step["innerCode"]     = (int)ic;
        step["lockerOperate"] = (int)lo;
        step["stepDetail"]    = detail;
        step["stepId"]        = QString::number(f.taskIdU64 + i); // 占位
        step["stepType"]      = 1;
        step["rfid"]          = "";
        steps.append(step);
    }
    data["steps"] = steps;

    QJsonObject root;
    root["data"] = data;

    QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Indented);

    // ── 确定保存路径 ──────────────────────────────────────
    const QString exeDir = QCoreApplication::applicationDirPath();
    QString testsDir = exeDir + "/tests";
    QDir().mkpath(testsDir);

    // 文件名：golden 文件基础名 + _replay.json
    QString baseName = QFileInfo(m_goldenPathEdit->text()).completeBaseName();
    if (baseName.isEmpty()) baseName = "golden";
    m_replayJsonPath = testsDir + "/" + baseName + "_replay.json";

    QFile jf(m_replayJsonPath);
    if (!jf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_statusLabel->setText(
            QString::fromUtf8("❌ 无法写入 JSON: %1").arg(jf.errorString()));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }
    jf.write(jsonBytes);
    jf.close();

    appendOutput("\n" + QString("═").repeated(56));
    appendOutput(QString::fromUtf8("[REPLAY JSON 已生成]"));
    appendOutput(QString::fromUtf8("  路径   : %1").arg(m_replayJsonPath));
    appendOutput(QString::fromUtf8("  taskId : %1").arg(f.taskIdU64));
    appendOutput(QString::fromUtf8("  taskName: %1").arg(f.taskName));
    for (int i = 0; i < steps.size(); ++i) {
        QJsonObject s = steps[i].toObject();
        appendOutput(QString::fromUtf8("  step[%1] innerCode=%2  lockerOp=%3")
                     .arg(i).arg(s["innerCode"].toInt()).arg(s["lockerOperate"].toInt()));
        appendOutput(QString::fromUtf8("          stepDetail: %1")
                     .arg(s["stepDetail"].toString()));
    }
    appendOutput(QString("─").repeated(56));

    m_btnDiff->setEnabled(true);
    m_statusLabel->setText(
        QString::fromUtf8("✅ replay.json 已生成 → %1")
            .arg(QDir::toNativeSeparators(m_replayJsonPath)));
    m_statusLabel->setStyleSheet("color:green; font-weight:bold; padding:4px;");
}

// ────────────────────────────────────────────────────────────────
//  按钮 C：用 replay.json 编码 → diff vs golden
// ────────────────────────────────────────────────────────────────
void GoldenPanel::onDiff()
{
    if (m_replayJsonPath.isEmpty() || m_goldenFrame.isEmpty()) {
        m_statusLabel->setText(
            QString::fromUtf8("⚠  请先完成解析和生成 JSON"));
        return;
    }

    appendOutput("\n" + QString("═").repeated(56));
    appendOutput(QString::fromUtf8("[DIFF] 用 replay.json 重新编码 → 对比 golden"));

    // ── 读 JSON ──────────────────────────────────────────
    QFile jf(m_replayJsonPath);
    if (!jf.open(QIODevice::ReadOnly)) {
        appendOutput(QString::fromUtf8("❌ 无法打开 replay.json: %1").arg(jf.errorString()));
        return;
    }
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(jf.readAll(), &pe);
    jf.close();
    if (doc.isNull()) {
        appendOutput(QString::fromUtf8("❌ JSON 解析失败: %1").arg(pe.errorString()));
        return;
    }
    QJsonObject root = doc.object();
    QJsonObject ticketObj = (root.contains("data") && root["data"].isObject())
                            ? root["data"].toObject() : root;

    // ── 编码 + 封帧 ────────────────────────────────────
    TicketPayloadEncoder::EncodeResult enc =
        TicketPayloadEncoder::encode(ticketObj, 0x01,
                                     TicketPayloadEncoder::StringEncoding::GBK);
    if (!enc.ok) {
        appendOutput(QString::fromUtf8("❌ 编码失败: %1").arg(enc.errorMsg));
        m_statusLabel->setText(QString::fromUtf8("❌ 编码失败"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }
    appendOutput(QString::fromUtf8("  payload: %1 B").arg(enc.payload.size()));

    TicketFrameBuilder builder(0x01, 0x00, 0x00);
    QList<QByteArray> frames = builder.buildFrames(enc.payload);
    if (frames.isEmpty()) {
        appendOutput(QString::fromUtf8("❌ 封帧失败"));
        return;
    }
    QByteArray generated = frames[0];
    appendOutput(QString::fromUtf8("  生成帧: %1 B").arg(generated.size()));

    // ── Diff ────────────────────────────────────────────
    ReplayRunner::DiffReport dr = ReplayRunner::buildDiff(generated, m_goldenFrame);
    appendOutput("");
    for (const QString &l : qAsConst(dr.lines))
        appendOutput(l);

    if (dr.pass) {
        m_statusLabel->setText(
            QString::fromUtf8("✅ PASS — %1 B 完全匹配！编码逻辑与原产品一致")
                .arg(generated.size()));
        m_statusLabel->setStyleSheet(
            "color:green; font-weight:bold; font-size:13px; padding:4px;");
    } else {
        QString hint;
        if (!dr.diffs.isEmpty()) {
            const auto &d = dr.diffs[0];
            quint8 gb  = (d.start < m_goldenFrame.size())
                         ? (quint8)m_goldenFrame[d.start] : 0;
            quint8 gen = (d.start < generated.size())
                         ? (quint8)generated[d.start]     : 0;
            hint = QString::fromUtf8("首差异 offset=%1  生成=%2  golden=%3  字段=%4")
                .arg(d.start)
                .arg(QString::number(gen, 16).toUpper().rightJustified(2,'0'))
                .arg(QString::number(gb,  16).toUpper().rightJustified(2,'0'))
                .arg(d.fieldHint);
        }
        m_statusLabel->setText(
            QString::fromUtf8("❌ FAIL — %1 处差异  |  %2")
                .arg(dr.diffs.size()).arg(hint));
        m_statusLabel->setStyleSheet(
            "color:red; font-weight:bold; font-size:11px; padding:4px;");
    }
}

// ────────────────────────────────────────────────────────────────
//  清空
// ────────────────────────────────────────────────────────────────
void GoldenPanel::onClear()
{
    m_output->clear();
    m_goldenFrame.clear();
    m_fields = GoldenFields{};
    m_replayJsonPath.clear();
    m_btnGenJson->setEnabled(false);
    m_btnDiff->setEnabled(false);
    m_statusLabel->setText(
        QString::fromUtf8("就绪 — 选择 Golden HEX 后点击 [解析 golden]"));
    m_statusLabel->setStyleSheet("font-weight:bold; padding:4px;");
}

// ────────────────────────────────────────────────────────────────
//  输出区辅助
// ────────────────────────────────────────────────────────────────
void GoldenPanel::appendOutput(const QString &text)
{
    m_output->appendPlainText(text);
    QTextCursor c = m_output->textCursor();
    c.movePosition(QTextCursor::End);
    m_output->setTextCursor(c);
}

void GoldenPanel::setOutput(const QString &text)
{
    m_output->setPlainText(text);
    QTextCursor c = m_output->textCursor();
    c.movePosition(QTextCursor::Start);
    m_output->setTextCursor(c);
}
