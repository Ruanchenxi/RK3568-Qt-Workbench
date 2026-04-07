/**
 * @file workbenchpage.h
 * @brief 工作台页面头文件
 *
 * 工作台是系统的核心操作页面，提供：
 * - 停电操作入口
 * - 送电操作入口
 * - 审核管理入口
 * - 人员管理入口
 * - 信息查询入口
 */

#ifndef WORKBENCHPAGE_H
#define WORKBENCHPAGE_H

#include <functional>
#include <QElapsedTimer>
#include <QWidget>
#include <QWebEngineView>
#include <QShowEvent>
#include "features/workbench/application/WorkbenchController.h"

class KeyboardContainer;

namespace Ui
{
    class WorkbenchPage;
}

/**
 * @class WorkbenchPage
 * @brief 工作台页面类 - 内嵌网页显示
 */
class WorkbenchPage : public QWidget
{
    Q_OBJECT

public:
    explicit WorkbenchPage(QWidget *parent = nullptr);
    ~WorkbenchPage();
    void requestKeyboardActivation(KeyboardContainer *container, std::function<void(bool)> callback);

public slots:
    void reloadWorkbench();

protected:
    void showEvent(QShowEvent *event) override; // 页面显示时按需加载（首次或令牌变化）

private:
    Ui::WorkbenchPage *ui;
    QWebEngineView *m_webView; // WebEngine 视图
    WorkbenchController *m_controller;
    bool m_webViewInitialized;
    bool m_renderProcessTerminated;
    bool m_reloadScheduled;
    bool m_recreatePageOnReload;
    bool m_pendingBusinessReload;
    QElapsedTimer m_reloadCooldownTimer;

    void ensureWebViewInitialized();
    void initWebView();   // 初始化 WebView
    void attachDebugPage(bool recreate); // 安装/重建 WebEngine Page
    void loadWorkbench(); // 加载工作台网页（带 token，通过中转页面）
    void scheduleWorkbenchReload(); // 渲染进程异常后的延迟恢复
    void insertTextFromKeyboard(const QString &text);
    void backspaceFromKeyboard();
    void commitFromKeyboard();
    void clearFromKeyboard();
};

#endif // WORKBENCHPAGE_H
