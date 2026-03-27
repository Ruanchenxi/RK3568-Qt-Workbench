/**
 * @file logpage.h
 * @brief 服务日志页面（静态骨架在 .ui，动态逻辑在 .cpp）
 * @author RK3568 项目组
 * @date 2026-02-10
 *
 * 设计说明：
 * 1. .ui 负责页面骨架和样式，代码只做生命周期与日志缓冲。
 * 2. 页面会根据配置决定是否在构造阶段立即启动日志控制器。
 * 3. 页面在 showEvent 中触发 start / flush，保证显示时状态同步。
 */

#ifndef LOGPAGE_H
#define LOGPAGE_H

#include <QWidget>
#include <QString>
#include <QStringList>

class QShowEvent;
class LogController;

namespace Ui
{
    class LogPage;
}

/**
 * @class LogPage
 * @brief 服务日志页面（View）
 * @details 页面只承接 .ui 里的静态骨架，运行时负责按配置启动日志控制器并刷新隐藏期间缓冲的日志。
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
    bool shouldStartControllerOnConstruction() const;
    void ensureStarted();
    void flushBufferedLogs();
    void appendOrBufferLog(const QString &text);

    Ui::LogPage *ui;
    LogController *m_controller;
    bool m_started;
    QStringList m_pendingLogs;
};

#endif // LOGPAGE_H
