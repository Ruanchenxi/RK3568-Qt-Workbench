/**
 * @file logpage.h
 * @brief 服务日志页面（静态骨架在 .ui，动态逻辑在 .cpp）
 * @author RK3568 项目组
 * @date 2026-02-10
 *
 * 设计说明：
 * 1. .ui 负责页面骨架和样式，代码只做生命周期与日志缓冲。
 * 2. 服务治理的启动责任由主窗口持有的共享 ProcessService 承担。
 * 3. 页面在 showEvent 中只负责刷新隐藏期间缓冲的日志，不再负责拉起后端。
 */

#ifndef LOGPAGE_H
#define LOGPAGE_H

#include <QWidget>
#include <QString>
#include <QStringList>

class QShowEvent;
class LogController;
class IProcessService;

namespace Ui
{
    class LogPage;
}

/**
 * @class LogPage
 * @brief 服务日志页面（View）
 * @details 页面只承接 .ui 里的静态骨架，运行时负责绑定共享日志源并刷新隐藏期间缓冲的日志。
 */
class LogPage : public QWidget
{
    Q_OBJECT

public:
    explicit LogPage(IProcessService *processService, QWidget *parent = nullptr);
    ~LogPage();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onServiceLogGenerated(const QString &text);

private:
    void flushBufferedLogs();
    void appendOrBufferLog(const QString &text);

    Ui::LogPage *ui;
    LogController *m_controller;
    QStringList m_pendingLogs;
};

#endif // LOGPAGE_H
