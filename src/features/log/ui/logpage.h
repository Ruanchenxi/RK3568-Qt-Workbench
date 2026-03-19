/**
 * @file logpage.h
 * @brief 服务日志页面（仅负责日志渲染与交互）
 * @author RK3568 项目组
 * @date 2026-02-10
 *
 * 设计说明：
 * 1. 该类仅负责日志显示，不再直接治理 QProcess/端口。
 * 2. 进程启动与端口检查下沉到 IProcessService 实现，页面层保持轻量。
 * 3. 页面只订阅 logGenerated 信号并渲染文本，避免 UI 与平台命令耦合。
 */

#ifndef LOGPAGE_H
#define LOGPAGE_H

#include <QWidget>
#include <QString>
#include <QStringList>

class QShowEvent;

class QPlainTextEdit;
class LogController;

namespace Ui
{
    class LogPage;
}

/**
 * @class LogPage
 * @brief 服务日志页面（View）
 */
class LogPage : public QWidget
{
    Q_OBJECT

public:
    explicit LogPage(QWidget *parent = nullptr);
    ~LogPage();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onServiceLogGenerated(const QString &text);

private:
    void ensureStarted();
    void flushPendingLogs();
    void initServiceLog();
    void appendLog(const QString &text);

    Ui::LogPage *ui;
    QPlainTextEdit *m_logDisplay;
    LogController *m_controller;
    bool m_started;
    bool m_visibleOnce;
    QStringList m_pendingLogs;
};

#endif // LOGPAGE_H
