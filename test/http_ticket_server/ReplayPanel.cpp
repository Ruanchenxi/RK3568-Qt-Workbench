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
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>

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

    m_lastFrame = frames[0];

    auto b = [&](int idx) -> quint8 {
        return static_cast<quint8>(m_lastFrame[idx]);
    };
    quint16 lenField = (quint16)b(6) | ((quint16)b(7) << 8);
    int payloadLen = lenField - 3;
    quint32 fileSize = (quint32)b(11)
                     | ((quint32)b(12) << 8)
                     | ((quint32)b(13) << 16)
                     | ((quint32)b(14) << 24);
    quint16 stepNum = (quint16)b(37) | ((quint16)b(38) << 8);
    quint32 taskNameOff = (quint32)b(39)
                        | ((quint32)b(40) << 8)
                        | ((quint32)b(41) << 16)
                        | ((quint32)b(42) << 24);
    int stringAreaStart = 160 + stepNum * 8;
    quint16 crc = ((quint16)b(m_lastFrame.size() - 2) << 8)
                | (quint16)b(m_lastFrame.size() - 1);

    out << SEP;
    out << QString::fromUtf8("  === 联调关键结果 ===");
    out << QString::fromUtf8("  taskId          : %1").arg(ticketObj.value("taskId").toString());
    out << QString::fromUtf8("  taskName        : %1").arg(ticketObj.value("taskName").toString());
    out << QString::fromUtf8("  stepNum         : %1").arg(stepNum);
    out << QString::fromUtf8("  payloadLen      : %1 (0x%2)")
           .arg(payloadLen).arg(payloadLen, 4, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  frameLen        : %1 (0x%2)")
           .arg(m_lastFrame.size()).arg(m_lastFrame.size(), 4, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  Len             : %1 (0x%2)")
           .arg(lenField).arg(lenField, 4, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  fileSize        : %1 (0x%2)")
           .arg(fileSize).arg(fileSize, 4, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  stringAreaStart : %1 (0x%2)")
           .arg(stringAreaStart).arg(stringAreaStart, 4, 16, QChar('0')).toUpper();
    out << QString::fromUtf8("  taskNameOff     : %1 (0x%2)")
           .arg(taskNameOff).arg(taskNameOff, 4, 16, QChar('0')).toUpper();
    for (int i = 0; i < stepNum; ++i) {
        const int dispPos = 11 + 160 + i * 8 + 4;
        if (dispPos + 3 >= m_lastFrame.size())
            break;
        quint32 displayOff = (quint32)b(dispPos)
                           | ((quint32)b(dispPos + 1) << 8)
                           | ((quint32)b(dispPos + 2) << 16)
                           | ((quint32)b(dispPos + 3) << 24);
        out << QString::fromUtf8("  step[%1].displayOff: %2 (0x%3)")
               .arg(i).arg(displayOff)
               .arg(displayOff, 4, 16, QChar('0')).toUpper();
    }
    out << QString::fromUtf8("  CRC             : 0x%1")
           .arg(crc, 4, 16, QChar('0')).toUpper();

    // ── 4. 字段摘要 ──────────────────────────────────────────
    out << SEP;
    out << QString::fromUtf8("  === 字段摘要 ===");
    QStringList summary = ReplayRunner::frameFieldSummary(m_lastFrame);
    for (const QString &s : qAsConst(summary))
        out << "  " + s;

    // ── 5. 完整 HEX ──────────────────────────────────────────
    out << SEP;
    out << QString::fromUtf8("  === 完整 HEX（%1 B）===").arg(m_lastFrame.size());

    // 单行（用于复制到串口助手）
    m_lastHexOneLine = ReplayRunner::toHexDump(m_lastFrame, m_lastFrame.size());
    out << "  " + m_lastHexOneLine;
    out << "";
    out << QString::fromUtf8("  （分行，每行32B）");
    QStringList multiLine = ReplayRunner::toHexDump(m_lastFrame, 32).split('\n');
    for (const QString &l : qAsConst(multiLine))
        out << "  " + l;

    setOutput(out.join('\n'));

    m_btnCopyHex->setEnabled(true);
    m_btnSaveHex->setEnabled(true);

    m_statusLabel->setText(
        QString::fromUtf8("✅ 生成成功  |  帧长 = %1 B").arg(m_lastFrame.size()));
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
    QByteArray golden = ReplayRunner::parseHexFile(goldenPath, parseErrors);
    for (const QString &e : qAsConst(parseErrors))
        appendOutput(e);

    if (golden.isEmpty()) {
        appendOutput(QString::fromUtf8("❌ Golden 文件为空或解析失败"));
        m_statusLabel->setText(QString::fromUtf8("❌ Golden 解析失败"));
        m_statusLabel->setStyleSheet("color:red; font-weight:bold; padding:4px;");
        return;
    }

    // Golden 字段摘要
    appendOutput(QString::fromUtf8("\n  Golden 字段摘要："));
    QStringList gsum = ReplayRunner::frameFieldSummary(golden);
    for (const QString &s : qAsConst(gsum))
        appendOutput("    " + s);

    // Diff
    ReplayRunner::DiffReport dr = ReplayRunner::buildDiff(m_lastFrame, golden);
    int payloadLen = ((int)(quint8)m_lastFrame[6] | ((int)(quint8)m_lastFrame[7] << 8)) - 3;
    appendOutput(QString::fromUtf8("  frameLen=%1  payloadLen=%2")
                 .arg(m_lastFrame.size()).arg(payloadLen));
    appendOutput("");
    for (const QString &l : qAsConst(dr.lines))
        appendOutput(l);

    if (dr.pass) {
        m_statusLabel->setText(
            QString::fromUtf8("✅ PASS — frameLen=%1 payloadLen=%2，所有字节完全一致（包括 CRC）")
                .arg(m_lastFrame.size()).arg(payloadLen));
        m_statusLabel->setStyleSheet(
            "color:green; font-weight:bold; font-size:13px; padding:4px;");
    } else {
        QString firstDiff;
        if (!dr.diffs.isEmpty()) {
            const auto &d = dr.diffs[0];
            quint8 genB = (d.start < m_lastFrame.size())
                          ? static_cast<quint8>(m_lastFrame[d.start]) : 0;
            quint8 golB = (d.start < golden.size())
                          ? static_cast<quint8>(golden[d.start]) : 0;
            firstDiff = QString::fromUtf8("首差异 offset=%1  生成=%2  期望=%3  字段=%4")
                .arg(d.start)
                .arg(QString::fromLatin1("%1").arg(genB, 2, 16, QChar('0')).toUpper())
                .arg(QString::fromLatin1("%1").arg(golB, 2, 16, QChar('0')).toUpper())
                .arg(d.fieldHint);
        }
        m_statusLabel->setText(
            QString::fromUtf8("❌ FAIL — frameLen=%1 payloadLen=%2 | %3")
                .arg(m_lastFrame.size()).arg(payloadLen).arg(firstDiff));
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
    if (m_lastHexOneLine.isEmpty()) return;
    QApplication::clipboard()->setText(m_lastHexOneLine);
    m_statusLabel->setText(
        QString::fromUtf8("📋  已复制 HEX 到剪贴板（%1 B，可直接粘贴到串口助手）")
            .arg(m_lastFrame.size()));
    m_statusLabel->setStyleSheet("color:#006699; font-weight:bold; padding:4px;");
}

void ReplayPanel::onSaveHex()
{
    if (m_lastFrame.isEmpty()) return;

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
    ts << ReplayRunner::toHexDump(m_lastFrame, 32) << "\n";
    f.close();

    // 同时写 .bin
    QString binPath = QFileInfo(path).dir().filePath(
        QFileInfo(path).completeBaseName() + ".bin");
    QFile bf(binPath);
    if (bf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        bf.write(m_lastFrame);
        bf.close();
    }

    m_statusLabel->setText(
        QString::fromUtf8("💾  已保存 HEX: %1  BIN: %2")
            .arg(QDir::toNativeSeparators(path))
            .arg(QDir::toNativeSeparators(binPath)));
    m_statusLabel->setStyleSheet("color:#006699; font-weight:bold; padding:4px;");

    appendOutput("\n" + QString::fromUtf8("  HEX 已保存: %1").arg(path));
    appendOutput(QString::fromUtf8("  BIN 已保存: %1").arg(binPath));
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
