#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QWidget>

class QPlainTextEdit;
class QPushButton;
class QLabel;
class QTabWidget;
class ReplayPanel;
class GoldenPanel;

/// HTTP 服务端报文日志展示窗口（含 Replay/Diff 面板）
class LogWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LogWindow(QWidget *parent = nullptr);

    /// 启动后调用，让 ReplayPanel 自动填充默认路径
    void initReplayPanel();

public slots:
    /// 追加一条格式化日志到展示区（同时打印到控制台）
    void appendLog(const QString &text);

    /// 显示当前状态信息（如监听地址）
    void setStatusText(const QString &text);

    /// 收到新的 HTTP JSON 样本文件时，转发给 Replay 面板
    void onHttpJsonSaved(const QString &filePath);

private slots:
    void onClearClicked();

private:
    // ── HTTP 日志 Tab ────────────────────────────
    QPlainTextEdit *m_logView;
    QPushButton    *m_btnClear;
    QLabel         *m_statusLabel;
    int             m_requestCount;

    // ── Tab 容器 ─────────────────────────────────
    QTabWidget     *m_tabs;
    ReplayPanel    *m_replayPanel;
    GoldenPanel    *m_goldenPanel;
};

#endif // LOGWINDOW_H
