#pragma once
// ════════════════════════════════════════════════════════════════
//  ReplayPanel  – Stage 5.4 GUI 面板
//
//  功能：
//    1) 选择票据 JSON 文件 → 生成串口帧 HEX
//    2) 选择 Golden HEX 文件 → 字节级 diff 报告（PASS / FAIL）
//    3) 一键复制 HEX 到剪贴板（直接粘贴到串口助手）
//    4) 保存 HEX 到文件
//    5) 自动填充默认路径：logs/ 最新 JSON，tests/sample_golden.hex
// ════════════════════════════════════════════════════════════════
#ifndef REPLAYPANEL_H
#define REPLAYPANEL_H

#include <QWidget>
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

class QCheckBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QLineEdit;

class ReplayPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ReplayPanel(QWidget *parent = nullptr);

    /// 由外部（LogWindow）在启动后调用，自动填充默认路径
    void autoPopulateDefaults();

public slots:
    /// HTTP 层保存新 JSON 后通知 Replay 面板
    void onLatestHttpJsonSaved(const QString &jsonPath);

private slots:
    void onSelectJson();
    void onSelectGolden();
    void onSelectOutput();
    void onUseLatestJson();
    void onReplay();
    void onDiff();
    void onCopyHex();
    void onSaveHex();
    void onClearOutput();   ///< 清空结果展示区

private:
    // ---- 核心流程 ----
    void setSelectedJsonPath(const QString &path);
    /// 选择 JSON 后预览摘要；失败时显示错误，不崩溃
    void previewSelectedJson();
    /// 读 JSON → encode → buildFrame，返回 false 表示出错
    bool doGenerate();
    /// 生成（若未生成）并对比 golden
    void doGenerateAndDiff();
    /// 解析原始字节流中的多帧传输结果
    static bool splitFramesFromByteStream(const QByteArray &data,
                                          QList<QByteArray> &frames,
                                          QString &errorMsg);
    /// 纯单行 HEX
    static QString toHexOneLine(const QByteArray &frame);
    /// 解析带 [Frame x/N] 标记的 HEX 文件
    static QByteArray parseHexFileForReplay(const QString &path,
                                            QStringList &errors);
    /// 多帧总览文本
    static QStringList frameOverviewLines(const QList<QByteArray> &frames);
    /// 多帧复制/保存文本，仅包含帧标记和纯 HEX
    QString buildFramesPlainHexText() const;
    /// 多帧分块展示文本
    QStringList buildFramesDisplayLines() const;
    /// 从 payload 提取联调关键结果
    static QStringList payloadSummaryLines(const QJsonObject &ticketObj,
                                           const QByteArray &payload);
    /// 向 m_output 追加带前缀的文字
    void appendOutput(const QString &text);
    /// 清空并写入（替换全文）
    void setOutput(const QString &text);

    // ---- 路径工具 ----
    static QString findLatestJson(const QString &logsDir);
    static QString findGoldenHex(const QString &exeDir);

    // ---- 控件 ----
    QLineEdit      *m_jsonPathEdit;
    QLineEdit      *m_goldenPathEdit;
    QLineEdit      *m_outputPathEdit;

    QPushButton    *m_btnSelectJson;
    QPushButton    *m_btnSelectGolden;
    QPushButton    *m_btnSelectOutput;
    QPushButton    *m_btnUseLatestJson;
    QPushButton    *m_btnReplay;
    QPushButton    *m_btnDiff;
    QPushButton    *m_btnCopyHex;
    QPushButton    *m_btnSaveHex;
    QPushButton    *m_btnClear;    ///< 清空结果区
    QCheckBox      *m_chkAutoFollowLatest;

    QLabel         *m_statusLabel;   ///< PASS / FAIL / 就绪 等单行状态
    QPlainTextEdit *m_output;        ///< 详细结果展示区

    // ---- 状态 ----
    QList<QByteArray> m_lastFrames;      ///< 上次生成的整组帧
    QStringList       m_lastHexOneLines; ///< 上次生成的单行 HEX 列表
    QByteArray  m_lastFrame;         ///< 上次生成的完整帧（含 CRC）
    QString     m_lastHexOneLine;    ///< 上次生成的单行 HEX（用于剪贴板）
    QString     m_latestHttpJsonPath;///< 最近一次由 HTTP 层落盘的 JSON
};

#endif // REPLAYPANEL_H
