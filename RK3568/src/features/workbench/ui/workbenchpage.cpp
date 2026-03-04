/**
 * @file workbenchpage.cpp
 * @brief 工作台页面实现文件 - 内嵌网页显示
 */

#include "workbenchpage.h"
#include "ui_workbenchpage.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QUrl>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 */
WorkbenchPage::WorkbenchPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::WorkbenchPage),
      m_webView(nullptr),
      m_controller(new WorkbenchController(nullptr, this))
{
    ui->setupUi(this);
    initWebView();
}

/**
 * @brief 析构函数
 */
WorkbenchPage::~WorkbenchPage()
{
    delete ui;
}

/**
 * @brief 初始化 WebEngine 视图
 */
void WorkbenchPage::initWebView()
{
    // 创建 QWebEngineView
    m_webView = new QWebEngineView(this);

    // 将 WebView 添加到容器中
    QVBoxLayout *layout = new QVBoxLayout(ui->webViewContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_webView);

    // ✅ 不再需要注入 token，中转页面会自动处理
    connect(m_webView, &QWebEngineView::loadFinished, this, [=](bool ok)
            {
        if (ok) {
            qDebug() << "[WorkbenchPage] 网页加载成功";
        } else {
            qWarning() << "[WorkbenchPage] 网页加载失败";
        } });
}

/**
 * @brief 加载工作台网页（带 token）
 */
void WorkbenchPage::loadWorkbench()
{
    qDebug() << "[WorkbenchPage] ========== 开始加载工作台 ==========";
    QUrl targetUrl;
    QString safeSummary;
    QString error;
    if (!m_controller->buildTargetUrl(&targetUrl, &safeSummary, &error))
    {
        qWarning() << "[WorkbenchPage]" << error;
        return;
    }
    qDebug() << "[WorkbenchPage] 使用中转页面 URL（已脱敏）:" << safeSummary;

    // 加载网页
    qDebug() << "[WorkbenchPage] 开始加载网页...";
    m_webView->load(targetUrl);
    qDebug() << "[WorkbenchPage] ========== 加载工作台结束 ==========";
}

/**
 * @brief 页面显示事件 - 首次显示或 token 变化时才重新加载
 */
void WorkbenchPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_controller->shouldLoadNow()) {
        loadWorkbench();
    }
}
