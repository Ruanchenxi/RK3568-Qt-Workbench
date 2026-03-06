// ════════════════════════════════════════════════════════════════
//  ReplayPanel.cpp  – Stage 5.4 GUI 回放/对比面板
// ════════════════════════════════════════════════════════════════
#include "ReplayPanel.h"
#include "TicketPayloadEncoder.h"
#include "TicketFrameBuilder.h"
#include "ReplayRunner.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QFile>
#include <QJsonArray>
#include <QTextStream>

// ────────────────────────────────────────────────────────────
static QString fmtHex(quint32 v, int width = 4)
{
    return QString::fromLatin1("%1").arg(v, width, 16, QChar('0')).toUpper();
}

// ────────────────────────────────────────────────────────────
QString ReplayPanel::toHexOneLine(const QByteArray &frame)
{
    return ReplayRunner::toHexDump(frame, frame.size());
}

// ────────────────────────────────────────────────────────────
QByteArray ReplayPanel::parseHexFileForReplay(const QString &path,
                                              QStringList &errors)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errors << QString::fromUtf8("  [ERROR] 无法打开 golden hex 文件: %1  (%2)")
                  .arg(path).arg(f.errorString());
        return QByteArray();
    }

    QStringList keptLines;
    QStringList lines = QString::fromLatin1(f.readAll()).split(QRegularExpression(R"([\r\n]+)"));
    f.close();

    for (const QString &line : qAsConst(lines)) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("[Frame ", Qt::CaseInsensitive))
            continue;
        keptLines << line;
    }

    return ReplayRunner::parseHexString(keptLines.join("\n"), errors);
}

// ────────────────────────────────────────────────────────────
bool ReplayPanel::splitFramesFromByteStream(const QByteArray &data,
                                            QList<QByteArray> &frames,
                                            QString &errorMsg)
{
    frames.clear();
    int offset = 0;
    while (offset < data.size()) {
        if (offset + 10 > data.size()) {
            errorMsg = QString::fromUtf8("剩余字节不足以解析帧头: offset=%1").arg(offset);
            return false;
        }
        if ((quint8)data[offset] != 0x7E || (quint8)data[offset + 1] != 0x6C) {
            errorMsg = QString::fromUtf8("帧头不匹配: offset=%1, bytes=%2 %3")
                    .arg(offset)
                    .arg(fmtHex((quint8)data[offset], 2))
                    .arg(fmtHex((quint8)data[offset + 1], 2));
            return false;
        }

        quint16 lenField = (quint8)data[offset + 6]
                         | ((quint16)(quint8)data[offset + 7] << 8);
        const int frameLen = 10 + lenField;
        if (frameLen <= 0 || offset + frameLen > data.size()) {
            errorMsg = QString::fromUtf8("帧长度非法: offset=%1, len=%2, remain=%3")
                    .arg(offset).arg(frameLen).arg(data.size() - offset);
            return false;
        }

        frames.append(data.mid(offset, frameLen));
        offset += frameLen;
    }

    return !frames.isEmpty();
}

// ────────────────────────────────────────────────────────────
QStringList ReplayPanel::frameOverviewLines(const QList<QByteArray> &frames)
{
    QStringList out;
    out << QString::fromUtf8("  === 帧总览 ===");
    out << QString::fromUtf8("  frameCount      : %1").arg(frames.size());

    for (int i = 0; i < frames.size(); ++i) {
        const QByteArray &f = frames[i];
        if (f.size() < 12) {
            out << QString::fromUtf8("  frame[%1]       : 帧过短 (%2 B)").arg(i).arg(f.size());
            continue;
        }
        quint8 frameNo = (quint8)f[3];
        quint16 lenField = (quint8)f[6] | ((quint16)(quint8)f[7] << 8);
        quint8 cmd = (quint8)f[8];
        quint16 seq = (quint8)f[9] | ((quint16)(quint8)f[10] << 8);
        int payloadLen = lenField - 3;
        quint16 crc = ((quint16)(quint8)f[f.size() - 2] << 8)
                    | (quint16)(quint8)f[f.size() - 1];
        out << QString::fromUtf8("  frame[%1]       : FrameNo=0x%2 Cmd=0x%3 Seq=%4 Len=%5 Payload=%6 CRC=0x%7 Total=%8")
               .arg(i)
               .arg(fmtHex(frameNo, 2))
               .arg(fmtHex(cmd, 2))
               .arg(seq)
               .arg(lenField)
               .arg(payloadLen)
               .arg(fmtHex(crc, 4))
               .arg(f.size());
    }

    return out;
}

// ────────────────────────────────────────────────────────────
QString ReplayPanel::buildFramesPlainHexText() const
{
    if (m_lastFrames.isEmpty())
        return QString();
    if (m_lastFrames.size() == 1)
        return m_lastHexOneLine;

    QStringList out;
    for (int i = 0; i < m_lastFrames.size(); ++i) {
        out << QString::fromUtf8("[Frame %1/%2]").arg(i + 1).arg(m_lastFrames.size());
        out << m_lastHexOneLines.value(i);
        if (i + 1 < m_lastFrames.size())
            out << "";
    }
    return out.join('\n');
}

// ────────────────────────────────────────────────────────────
QStringList ReplayPanel::buildFramesDisplayLines() const
{
    QStringList out;
    for (int i = 0; i < m_lastFrames.size(); ++i) {
        const QByteArray &frame = m_lastFrames[i];
        out << QString::fromUtf8("  [Frame %1/%2]  one-line").arg(i + 1).arg(m_lastFrames.size());
        out << "  " + m_lastHexOneLines.value(i);
        out << QString::fromUtf8("  [Frame %1/%2]  32B/line").arg(i + 1).arg(m_lastFrames.size());
        QStringList lines = ReplayRunner::toHexDump(frame, 32).split('\n');
        for (const QString &line : qAsConst(lines))
            out << "  " + line;
        if (i + 1 < m_lastFrames.size())
            out << "";
    }
    return out;
}

// ────────────────────────────────────────────────────────────
QStringList ReplayPanel::payloadSummaryLines(const QJsonObject &ticketObj,
                                             const QByteArray &payload)
{
    QStringList out;
    if (payload.size() < 32)
        return out;

    auto pb = [&](int idx) -> quint8 { return static_cast<quint8>(payload[idx]); };
    auto le16 = [&](int off) -> quint16 {
        return (quint16)pb(off) | ((quint16)pb(off + 1) << 8);
    };
    auto le32 = [&](int off) -> quint32 {
        return (quint32)pb(off)
             | ((quint32)pb(off + 1) << 8)
             | ((quint32)pb(off + 2) << 16)
             | ((quint32)pb(off + 3) << 24);
    };

    quint32 fileSize = le32(0);
    quint16 stepNum = le16(26);
    quint32 taskNameOff = le32(28);
    const int stringAreaStart = 160 + stepNum * 8;

    out << QString::fromUtf8("  === 联调关键结果 ===");
    out << QString::fromUtf8("  taskId          : %1").arg(ticketObj.value("taskId").toString());
    out << QString::fromUtf8("  taskName        : %1").arg(ticketObj.value("taskName").toString());
    out << QString::fromUtf8("  stepNum         : %1").arg(stepNum);
    out << QString::fromUtf8("  payloadLen      : %1 (0x%2)")
           .arg(payload.size()).arg(fmtHex(payload.size(), 4));
    out << QString::fromUtf8("  fileSize        : %1 (0x%2)")
           .arg(fileSize).arg(fmtHex(fileSize, 4));
    out << QString::fromUtf8("  stringAreaStart : %1 (0x%2)")
           .arg(stringAreaStart).arg(fmtHex(stringAreaStart, 4));
    out << QString::fromUtf8("  taskNameOff     : %1 (0x%2)")
           .arg(taskNameOff).arg(fmtHex(taskNameOff, 4));

    for (int i = 0; i < stepNum; ++i) {
        const int off = 160 + i * 8 + 4;
        if (off + 3 >= payload.size())
            break;
        quint32 displayOff = le32(off);
        out << QString::fromUtf8("  step[%1].displayOff: %2 (0x%3)")
               .arg(i).arg(displayOff).arg(fmtHex(displayOff, 4));
    }

    return out;
}

// ────────────────────────────────────────────────────────────
//  构造函数 / UI 搭建
// ────────────────────────────────────────────────────────────
ReplayPanel::ReplayPanel(QWidget *parent)
    : QWidget(parent)
{
    // ── 文件选择区 ───────────────────────────────────────────
    QGroupBox *fileGroup = new QGroupBox(
        QString::fromUtf8("📂  文件选择"), this);

    auto makeRow = [&](const QString &labelText,
                       QLineEdit *&edit,
                       QPushButton *&btn,
                       const QString &btnText) -> QHBoxLayout *
    {
        QLabel *lbl = new QLabel(labelText, this);
        lbl->setFixedWidth(90);
        edit = new QLineEdit(this);
        edit->setReadOnly(true);
        edit->setPlaceholderText(QString::fromUtf8("（未选择）"));
        btn  = new QPushButton(btnText, this);
        btn->setFixedWidth(110);

        QHBoxLayout *row = new QHBoxLayout;
        row->addWidget(lbl);
        row->addWidget(edit, 1);
        row->addWidget(btn);
        return row;
    };

    QVBoxLayout *fileLayout = new QVBoxLayout(fileGroup);
    fileLayout->addLayout(makeRow(
        QString::fromUtf8("票据 JSON："),
        m_jsonPathEdit, m_btnSelectJson,
        QString::fromUtf8("选择 JSON…")));
    fileLayout->addLayout(makeRow(
        QString::fromUtf8("Golden HEX："),
        m_goldenPathEdit, m_btnSelectGolden,
        QString::fromUtf8("选择 Golden…")));
    fileLayout->addLayout(makeRow(
        QString::fromUtf8("输出路径："),
        m_outputPathEdit, m_btnSelectOutput,
        QString::fromUtf8("选择输出…")));

    QHBoxLayout *latestRow = new QHBoxLayout;
    latestRow->addSpacing(90);
    m_btnUseLatestJson = new QPushButton(QString::fromUtf8("使用最新收到的 JSON"), this);
    m_chkAutoFollowLatest = new QCheckBox(QString::fromUtf8("自动跟随最新 HTTP JSON"), this);
    m_chkAutoFollowLatest->setChecked(true);
    latestRow->addWidget(m_btnUseLatestJson);
    latestRow->addWidget(m_chkAutoFollowLatest);
    latestRow->addStretch();
    fileLayout->addLayout(latestRow);

    // ── 操作按钮区 ───────────────────────────────────────────
    QGroupBox *btnGroup = new QGroupBox(
        QString::fromUtf8("⚙  操作"), this);

    m_btnReplay   = new QPushButton(QString::fromUtf8("▶  生成 (Replay)"), this);
    m_btnDiff     = new QPushButton(QString::fromUtf8("🔍  生成并对比 (Diff)"), this);
    m_btnCopyHex  = new QPushButton(QString::fromUtf8("📋  复制 HEX 到剪贴板"), this);
    m_btnSaveHex  = new QPushButton(QString::fromUtf8("💾  保存 HEX 文件"), this);
    m_btnClear    = new QPushButton(QString::fromUtf8("🗑  清空结果"), this);

    m_btnReplay  ->setMinimumHeight(32);
    m_btnDiff    ->setMinimumHeight(32);
    m_btnCopyHex ->setMinimumHeight(32);
    m_btnSaveHex ->setMinimumHeight(32);
    m_btnClear   ->setMinimumHeight(32);

    m_btnCopyHex->setEnabled(false);
    m_btnSaveHex->setEnabled(false);

    QHBoxLayout *btnRow = new QHBoxLayout(btnGroup);
    btnRow->addWidget(m_btnReplay);
    btnRow->addWidget(m_btnDiff);
    btnRow->addStretch();
    btnRow->addWidget(m_btnCopyHex);
    btnRow->addWidget(m_btnSaveHex);
    btnRow->addWidget(m_btnClear);

    // Future serial integration hook (from serial_test):
    // 1) 串口枚举
    // 2) 打开串口
    // 3) HEX 发送
    // 4) 接收 ACK
    // 5) 超时/重试处理
    // 本阶段仅保留 JSON → HEX → 人工串口联调流程，不引入 QSerialPort。

    // ── 状态行 ───────────────────────────────────────────────
    m_statusLabel = new QLabel(
        QString::fromUtf8("就绪 — 选择 JSON 文件后点击 [生成]"), this);
    m_statusLabel->setStyleSheet("font-weight:bold; padding:4px;");

    // ── 结果展示区 ───────────────────────────────────────────
    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    {
        QFont mono("Consolas", 9);
        mono.setStyleHint(QFont::Monospace);
        m_output->setFont(mono);
    }
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setMinimumHeight(280);

    // ── 主布局 ───────────────────────────────────────────────
    QVBoxLayout *main = new QVBoxLayout(this);
    main->addWidget(fileGroup);
    main->addWidget(btnGroup);
    main->addWidget(m_statusLabel);
    main->addWidget(m_output, 1);

    // ── 信号 ─────────────────────────────────────────────────
    connect(m_btnSelectJson,   &QPushButton::clicked, this, &ReplayPanel::onSelectJson);
    connect(m_btnSelectGolden, &QPushButton::clicked, this, &ReplayPanel::onSelectGolden);
    connect(m_btnSelectOutput, &QPushButton::clicked, this, &ReplayPanel::onSelectOutput);
    connect(m_btnUseLatestJson, &QPushButton::clicked, this, &ReplayPanel::onUseLatestJson);
    connect(m_btnReplay,       &QPushButton::clicked, this, &ReplayPanel::onReplay);
    connect(m_btnDiff,         &QPushButton::clicked, this, &ReplayPanel::onDiff);
    connect(m_btnCopyHex,      &QPushButton::clicked, this, &ReplayPanel::onCopyHex);
    connect(m_btnSaveHex,      &QPushButton::clicked, this, &ReplayPanel::onSaveHex);
    connect(m_btnClear,        &QPushButton::clicked, this, &ReplayPanel::onClearOutput);
}

// ────────────────────────────────────────────────────────────
void ReplayPanel::onClearOutput()
{
    m_output->clear();
    m_lastFrames.clear();
    m_lastHexOneLines.clear();
    m_lastFrame.clear();
    m_lastHexOneLine.clear();
    m_btnCopyHex->setEnabled(false);
    m_btnSaveHex->setEnabled(false);
    m_statusLabel->setText(QString::fromUtf8("就绪 — 选择 JSON 文件后点击 [生成]"));
    m_statusLabel->setStyleSheet("font-weight:bold; padding:4px;");
}

// ────────────────────────────────────────────────────────────
//  自动填充默认路径
// ────────────────────────────────────────────────────────────
void ReplayPanel::autoPopulateDefaults()
{
    const QString exeDir = QCoreApplication::applicationDirPath();

    // 最新 JSON（logs/ 下）
    QString logsDir = exeDir + "/logs";
    QString latestJson = findLatestJson(logsDir);
    if (!latestJson.isEmpty()) {
        setSelectedJsonPath(latestJson);
    }

    // Golden HEX：优先找 tests/sample_golden.hex（相对 exe 向上 3 层）
    QString goldenPath = findGoldenHex(exeDir);
    if (!goldenPath.isEmpty()) {
        m_goldenPathEdit->setText(QDir::toNativeSeparators(goldenPath));
        m_goldenPathEdit->setToolTip(goldenPath);
    }

    // 默认输出路径
    QString outPath = logsDir + "/generated_frame.hex";
    m_outputPathEdit->setText(QDir::toNativeSeparators(outPath));
    m_outputPathEdit->setToolTip(outPath);
}

// ────────────────────────────────────────────────────────────
//  路径查找工具
// ────────────────────────────────────────────────────────────
QString ReplayPanel::findLatestJson(const QString &logsDir)
{
    QDir dir(logsDir);
    if (!dir.exists()) return {};
    QFileInfoList list = dir.entryInfoList(
        QStringList() << "http_ticket_*.json",
        QDir::Files, QDir::Time);
    if (list.isEmpty()) return {};
    return list.first().absoluteFilePath();
}

QString ReplayPanel::findGoldenHex(const QString &exeDir)
{
    // exe 位于 build/release/release/, 上溯3层到 http_ticket_server/
    QStringList candidates = {
        QDir::cleanPath(exeDir + "/../../../tests/sample_golden.hex"),
        QDir::cleanPath(exeDir + "/tests/sample_golden.hex"),
        QDir::cleanPath(exeDir + "/../tests/sample_golden.hex"),
    };
    for (const QString &p : candidates) {
        if (QFile::exists(p)) return p;
    }
    return {};
}

// ────────────────────────────────────────────────────────────
//  文件选择槽
// ────────────────────────────────────────────────────────────
void ReplayPanel::onSelectJson()
{
    QString initial = m_jsonPathEdit->text();
    if (initial.isEmpty())
        initial = QCoreApplication::applicationDirPath() + "/logs";

    QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("选择票据 JSON 文件"),
        initial,
        QString::fromUtf8("JSON 文件 (*.json);;所有文件 (*)"));

    if (!path.isEmpty()) {
        setSelectedJsonPath(path);
    }
}

void ReplayPanel::onSelectGolden()
{
    QString initial = m_goldenPathEdit->text();
    if (initial.isEmpty())
        initial = QCoreApplication::applicationDirPath();

    QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("选择 Golden HEX 文件（原产品抓包）"),
        initial,
        QString::fromUtf8("HEX 文件 (*.hex *.txt);;所有文件 (*)"));

    if (!path.isEmpty()) {
        m_goldenPathEdit->setText(QDir::toNativeSeparators(path));
        m_goldenPathEdit->setToolTip(path);
    }
}

void ReplayPanel::onSelectOutput()
{
    QString initial = m_outputPathEdit->text();
    if (initial.isEmpty())
        initial = QCoreApplication::applicationDirPath() + "/logs/generated_frame.hex";

    QString path = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8("选择输出 HEX 文件"),
        initial,
        QString::fromUtf8("HEX 文件 (*.hex);;所有文件 (*)"));

    if (!path.isEmpty()) {
        m_outputPathEdit->setText(QDir::toNativeSeparators(path));
        m_outputPathEdit->setToolTip(path);
    }
}

// ────────────────────────────────────────────────────────────
void ReplayPanel::onUseLatestJson()
{
    QString latestPath = m_latestHttpJsonPath;
    if (latestPath.isEmpty()) {
        latestPath = findLatestJson(QCoreApplication::applicationDirPath() + "/logs");
    }

    if (latestPath.isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("⚠ 未找到最新 HTTP JSON"));
        m_statusLabel->setStyleSheet("color:#996600; font-weight:bold; padding:4px;");
        return;
    }

    setSelectedJsonPath(latestPath);
    m_statusLabel->setText(QString::fromUtf8("已切换到最新收到的 HTTP JSON"));
    m_statusLabel->setStyleSheet("color:#006699; font-weight:bold; padding:4px;");
}

// ────────────────────────────────────────────────────────────
void ReplayPanel::onLatestHttpJsonSaved(const QString &jsonPath)
{
    m_latestHttpJsonPath = QDir::cleanPath(jsonPath);
    if (m_chkAutoFollowLatest && m_chkAutoFollowLatest->isChecked())
        setSelectedJsonPath(m_latestHttpJsonPath);
}

// ────────────────────────────────────────────────────────────
void ReplayPanel::setSelectedJsonPath(const QString &path)
{
    if (path.isEmpty())
        return;
    m_jsonPathEdit->setText(QDir::toNativeSeparators(path));
    m_jsonPathEdit->setToolTip(path);
    previewSelectedJson();
}

// ────────────────────────────────────────────────────────────
void ReplayPanel::previewSelectedJson()
{
    const QString jsonPath = m_jsonPathEdit->text().trimmed();
    if (jsonPath.isEmpty())
        return;

    QJsonObject ticketObj;
    QString errorMsg;
    if (!ReplayRunner::loadTicketObjectFromFile(jsonPath, ticketObj, errorMsg)) {
        setOutput(QString::fromUtf8("[REPLAY]\nJSON: %1\n\n❌ %2")
                  .arg(jsonPath, errorMsg));
        m_statusLabel->setText(QString::fromUtf8("❌ JSON 无法解析"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }

    QStringList out;
    out << QString::fromUtf8("[REPLAY]");
    out << QString::fromUtf8("JSON: %1").arg(jsonPath);
    out << "";
    out << ReplayRunner::ticketFieldSummary(ticketObj);
    out << "";
    out << QString::fromUtf8("就绪：可点击 [生成 (Replay)] 生成 HEX，或点击 [生成并对比 (Diff)] 与 Golden 对比。");

    setOutput(out.join('\n'));
    m_statusLabel->setText(QString::fromUtf8("JSON 已解析，可生成 HEX"));
    m_statusLabel->setStyleSheet("color:#006699; font-weight:bold; padding:4px;");
}

// ────────────────────────────────────────────────────────────
//  核心：读 JSON → 编码 → 封帧
//  返回 false = 失败（错误已写入 m_output）
// ────────────────────────────────────────────────────────────
bool ReplayPanel::doGenerate()
{
    m_lastFrames.clear();
    m_lastHexOneLines.clear();
    m_lastFrame.clear();
    m_lastHexOneLine.clear();
    m_btnCopyHex->setEnabled(false);
    m_btnSaveHex->setEnabled(false);

    const QString jsonPath = m_jsonPathEdit->text().trimmed();
    if (jsonPath.isEmpty()) {
        setOutput(QString::fromUtf8("❌ 请先选择票据 JSON 文件。"));
        m_statusLabel->setText(QString::fromUtf8("❌ 未选择 JSON 文件"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return false;
    }

    QStringList out;
    const QString SEP = "────────────────────────────────────────────────────";
    out << SEP;
    out << QString::fromUtf8("[REPLAY]  %1")
           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    out << QString::fromUtf8("  JSON  : %1").arg(jsonPath);
    out << SEP;

    // ── 1. 读 JSON ────────────────────────────────────────────
    QJsonObject ticketObj;
    QString jsonError;
    if (!ReplayRunner::loadTicketObjectFromFile(jsonPath, ticketObj, jsonError)) {
        out << QString::fromUtf8("❌ %1").arg(jsonError);
        setOutput(out.join('\n'));
        m_statusLabel->setText(QString::fromUtf8("❌ 无法打开 JSON 文件"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return false;
    }

    out << QString::fromUtf8("  JSON 解析 OK  字段: %1")
           .arg(ticketObj.keys().join(", "));
    out << "";
    QStringList ticketSummary = ReplayRunner::ticketFieldSummary(ticketObj);
    for (const QString &line : qAsConst(ticketSummary))
        out << "  " + line;

    // ── 2. 编码 payload ───────────────────────────────────────
    out << SEP;
    TicketPayloadEncoder::EncodeResult enc =
        TicketPayloadEncoder::encode(ticketObj, 0x01,
                                     TicketPayloadEncoder::StringEncoding::GBK);

    // 字段详细日志
    for (const QString &line : qAsConst(enc.fieldLog))
        out << "  " + line;

    if (!enc.ok) {
        out << QString::fromUtf8("❌ 编码失败: %1").arg(enc.errorMsg);
        setOutput(out.join('\n'));
        m_statusLabel->setText(QString::fromUtf8("❌ 编码失败"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return false;
    }
    out << QString::fromUtf8("  payload: %1 B").arg(enc.payload.size());

    // ── 3. 封帧 ───────────────────────────────────────────────
    out << SEP;
    TicketFrameBuilder builder(0x01, 0x00, 0x00);
    QList<QByteArray> frames = builder.buildFrames(enc.payload);
    if (frames.isEmpty()) {
        out << QString::fromUtf8("❌ buildFrames 返回空");
        setOutput(out.join('\n'));
        m_statusLabel->setText(QString::fromUtf8("❌ 封帧失败"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return false;
    }

    QStringList frameLogLines, hexLines;
    builder.formatFrameLog(frames, frameLogLines, hexLines);
    for (const QString &l : qAsConst(frameLogLines))
        out << "  " + l;

    m_lastFrames = frames;
    for (const QByteArray &frame : qAsConst(m_lastFrames))
        m_lastHexOneLines << toHexOneLine(frame);
    m_lastFrame = m_lastFrames.first();
    m_lastHexOneLine = m_lastHexOneLines.first();

    out << SEP;
    QStringList payloadSummary = payloadSummaryLines(ticketObj, enc.payload);
    for (const QString &line : qAsConst(payloadSummary))
        out << line;
    out << QString::fromUtf8("  frameCount      : %1").arg(m_lastFrames.size());

    out << SEP;
    QStringList overviewLines = frameOverviewLines(m_lastFrames);
    for (const QString &line : qAsConst(overviewLines))
        out << line;

    // ── 4. 字段摘要 ──────────────────────────────────────────
    out << SEP;
    if (m_lastFrames.size() == 1) {
        out << QString::fromUtf8("  === 字段摘要 ===");
        QStringList summary = ReplayRunner::frameFieldSummary(m_lastFrame);
        for (const QString &s : qAsConst(summary))
            out << "  " + s;
    } else {
        out << QString::fromUtf8("  === 多帧提示 ===");
        out << QString::fromUtf8("  当前结果为多帧，请按 Frame 1/N, Frame 2/N 顺序发送。");
        out << QString::fromUtf8("  单帧字段摘要不再只取第一帧，以免误导。");
    }

    // ── 5. 完整 HEX ──────────────────────────────────────────
    out << SEP;
    if (m_lastFrames.size() == 1)
        out << QString::fromUtf8("  === 完整 HEX（%1 B）===").arg(m_lastFrame.size());
    else
        out << QString::fromUtf8("  === 完整 HEX（多帧，共 %1 帧）===").arg(m_lastFrames.size());
    QStringList displayLines = buildFramesDisplayLines();
    for (const QString &line : qAsConst(displayLines))
        out << line;

    setOutput(out.join('\n'));

    m_btnCopyHex->setEnabled(true);
    m_btnSaveHex->setEnabled(true);

    if (m_lastFrames.size() == 1) {
        m_statusLabel->setText(
            QString::fromUtf8("✅ 生成成功  |  单帧 %1 B").arg(m_lastFrame.size()));
    } else {
        m_statusLabel->setText(
            QString::fromUtf8("✅ 生成成功  |  多帧 %1 帧（Frame1=%2 B）")
                .arg(m_lastFrames.size()).arg(m_lastFrames.first().size()));
    }
    m_statusLabel->setStyleSheet("color:green; font-weight:bold; padding:4px;");
    return true;
}

// ────────────────────────────────────────────────────────────
//  生成 + Diff
// ────────────────────────────────────────────────────────────
void ReplayPanel::doGenerateAndDiff()
{
    if (!doGenerate()) return;  // 生成失败直接返回

    const QString goldenPath = m_goldenPathEdit->text().trimmed();
    if (goldenPath.isEmpty()) {
        appendOutput("\n" + QString::fromUtf8(
            "⚠  未选择 Golden HEX 文件，仅生成模式。"));
        m_statusLabel->setText(
            QString::fromUtf8("ℹ  未选择 Golden，仅显示生成帧"));
        m_statusLabel->setStyleSheet("color:#996600; font-weight:bold; padding:4px;");
        return;
    }

    appendOutput("\n\n" + QString::fromUtf8(
        "════════════════════════════════════════════════") + "\n"
        + QString::fromUtf8("  === DIFF vs Golden ===") + "\n"
        + QString::fromUtf8("  Golden: %1").arg(goldenPath));

    // 解析 golden
    QStringList parseErrors;
    QByteArray golden = parseHexFileForReplay(goldenPath, parseErrors);
    for (const QString &e : qAsConst(parseErrors))
        appendOutput(e);

    if (golden.isEmpty()) {
        appendOutput(QString::fromUtf8("❌ Golden 文件为空或解析失败"));
        m_statusLabel->setText(QString::fromUtf8("❌ Golden 解析失败"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }

    QList<QByteArray> goldenFrames;
    QString splitError;
    if (!splitFramesFromByteStream(golden, goldenFrames, splitError)) {
        appendOutput(QString::fromUtf8("❌ Golden 多帧解析失败: %1").arg(splitError));
        m_statusLabel->setText(QString::fromUtf8("❌ Golden 多帧解析失败"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }

    appendOutput(QString::fromUtf8("\n  Golden 帧总览："));
    QStringList goldenOverview = frameOverviewLines(goldenFrames);
    for (const QString &line : qAsConst(goldenOverview))
        appendOutput(line);

    appendOutput(QString::fromUtf8("\n  === 多帧 DIFF ==="));
    appendOutput(QString::fromUtf8("  生成帧数=%1  Golden帧数=%2")
                 .arg(m_lastFrames.size()).arg(goldenFrames.size()));

    bool pass = true;
    QString firstDiff;

    if (m_lastFrames.size() != goldenFrames.size()) {
        pass = false;
        firstDiff = QString::fromUtf8("帧数不一致：生成=%1 Golden=%2")
                .arg(m_lastFrames.size()).arg(goldenFrames.size());
    }

    const int compareCount = qMin(m_lastFrames.size(), goldenFrames.size());
    int globalOffsetBase = 0;
    for (int i = 0; i < compareCount; ++i) {
        const QByteArray &genFrame = m_lastFrames[i];
        const QByteArray &goldFrame = goldenFrames[i];
        appendOutput(QString::fromUtf8("  [Frame %1/%2] generated=%3B golden=%4B")
                     .arg(i + 1).arg(compareCount)
                     .arg(genFrame.size()).arg(goldFrame.size()));

        ReplayRunner::DiffReport dr = ReplayRunner::buildDiff(genFrame, goldFrame);
        if (!dr.pass && pass) {
            pass = false;
            if (!dr.diffs.isEmpty()) {
                const ReplayRunner::DiffRange &d = dr.diffs[0];
                quint8 genB = (d.start < genFrame.size()) ? (quint8)genFrame[d.start] : 0;
                quint8 golB = (d.start < goldFrame.size()) ? (quint8)goldFrame[d.start] : 0;
                firstDiff = QString::fromUtf8("首差异: Frame %1/%2, frameOffset=%3, globalOffset=%4, 生成=%5, 期望=%6, 字段=%7")
                        .arg(i + 1).arg(m_lastFrames.size())
                        .arg(d.start).arg(globalOffsetBase + d.start)
                        .arg(fmtHex(genB, 2)).arg(fmtHex(golB, 2)).arg(d.fieldHint);
            } else {
                firstDiff = QString::fromUtf8("首差异: Frame %1/%2 长度不一致").arg(i + 1).arg(m_lastFrames.size());
            }
        }
        globalOffsetBase += genFrame.size();
    }

    if (pass) {
        appendOutput(QString::fromUtf8("  ✅ PASS — 多帧序列全部一致（包括 CRC）"));
        m_statusLabel->setText(
            QString::fromUtf8("✅ PASS — %1 帧全部一致（包括 CRC）").arg(m_lastFrames.size()));
        m_statusLabel->setStyleSheet(
            "color:green; font-weight:bold; font-size:13px; padding:4px;");
    } else {
        appendOutput(QString::fromUtf8("  ❌ FAIL — %1").arg(firstDiff));
        m_statusLabel->setText(
            QString::fromUtf8("❌ FAIL — %1").arg(firstDiff));
        m_statusLabel->setStyleSheet(
            "color:red; font-weight:bold; font-size:11px; padding:4px;");
    }
}

// ────────────────────────────────────────────────────────────
//  按钮槽
// ────────────────────────────────────────────────────────────
void ReplayPanel::onReplay() { doGenerate(); }

void ReplayPanel::onDiff()   { doGenerateAndDiff(); }

void ReplayPanel::onCopyHex()
{
    if (m_lastFrames.isEmpty()) return;
    QApplication::clipboard()->setText(buildFramesPlainHexText());
    if (m_lastFrames.size() == 1) {
        m_statusLabel->setText(
            QString::fromUtf8("📋  已复制 1 帧 HEX 到剪贴板"));
    } else {
        m_statusLabel->setText(
            QString::fromUtf8("📋  已复制 %1 帧 HEX 到剪贴板").arg(m_lastFrames.size()));
    }
    m_statusLabel->setStyleSheet("color:#006699; font-weight:bold; padding:4px;");
}

void ReplayPanel::onSaveHex()
{
    if (m_lastFrames.isEmpty()) return;

    QString path = m_outputPathEdit->text().trimmed();
    if (path.isEmpty()) {
        path = QCoreApplication::applicationDirPath()
               + "/logs/generated_frame_"
               + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")
               + ".hex";
    }

    // 确保目录存在
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this,
            QString::fromUtf8("保存失败"),
            QString::fromUtf8("无法写入文件：\n%1\n\n%2").arg(path).arg(f.errorString()));
        return;
    }
    QTextStream ts(&f);
    ts << buildFramesPlainHexText() << "\n";
    f.close();

    QStringList savedBinPaths;
    if (m_lastFrames.size() == 1) {
        QString binPath = QFileInfo(path).dir().filePath(
            QFileInfo(path).completeBaseName() + ".bin");
        QFile bf(binPath);
        if (bf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            bf.write(m_lastFrame);
            bf.close();
            savedBinPaths << binPath;
        }
    } else {
        for (int i = 0; i < m_lastFrames.size(); ++i) {
            QString binPath = QFileInfo(path).dir().filePath(
                QFileInfo(path).completeBaseName() + QString("_frame%1.bin").arg(i));
            QFile bf(binPath);
            if (bf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                bf.write(m_lastFrames[i]);
                bf.close();
                savedBinPaths << binPath;
            }
        }
    }

    if (m_lastFrames.size() == 1) {
        m_statusLabel->setText(
            QString::fromUtf8("💾  已保存 HEX/BIN: %1")
                .arg(QDir::toNativeSeparators(path)));
    } else {
        m_statusLabel->setText(
            QString::fromUtf8("💾  已保存多帧 HEX: %1  |  每帧 BIN 已分别保存")
                .arg(QDir::toNativeSeparators(path)));
    }
    m_statusLabel->setStyleSheet("color:#006699; font-weight:bold; padding:4px;");

    appendOutput("\n" + QString::fromUtf8("  HEX 已保存: %1").arg(path));
    if (savedBinPaths.isEmpty()) {
        appendOutput(QString::fromUtf8("  BIN 未保存"));
    } else {
        for (const QString &binPath : qAsConst(savedBinPaths))
            appendOutput(QString::fromUtf8("  BIN 已保存: %1").arg(binPath));
    }
}

// ────────────────────────────────────────────────────────────
//  输出区辅助
// ────────────────────────────────────────────────────────────
void ReplayPanel::appendOutput(const QString &text)
{
    m_output->appendPlainText(text);
    QTextCursor c = m_output->textCursor();
    c.movePosition(QTextCursor::End);
    m_output->setTextCursor(c);
}

void ReplayPanel::setOutput(const QString &text)
{
    m_output->setPlainText(text);
    QTextCursor c = m_output->textCursor();
    c.movePosition(QTextCursor::Start);
    m_output->setTextCursor(c);
}
