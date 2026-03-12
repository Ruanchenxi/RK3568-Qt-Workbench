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

#include <QWidget>
#include <QWebEngineView>
#include <QShowEvent>
#include "features/workbench/application/WorkbenchController.h"

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

protected:
    void showEvent(QShowEvent *event) override; // 页面显示时按需加载（首次或令牌变化）

private:
    Ui::WorkbenchPage *ui;
    QWebEngineView *m_webView; // WebEngine 视图
    WorkbenchController *m_controller;

    void initWebView();   // 初始化 WebView
    void loadWorkbench(); // 加载工作台网页（带 token，通过中转页面）
};

#endif // WORKBENCHPAGE_H
