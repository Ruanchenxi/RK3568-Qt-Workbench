/**
 * @file workbenchpage.cpp
 * @brief 工作台页面实现文件 - 内嵌网页显示
 */

#include "workbenchpage.h"
#include "ui_workbenchpage.h"
#include "core/ConfigManager.h"
#include "core/authservice.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>
#include <QUrl>
#include <QUrlQuery>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 */
WorkbenchPage::WorkbenchPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::WorkbenchPage),
      m_webView(nullptr)
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

    // 获取 token 信息
    QString accessToken = AuthService::instance()->getAccessToken();
    QString refreshToken = AuthService::instance()->getRefreshToken();
    QString tenantId = ConfigManager::instance()->tenantCode();

    // 避免在日志中输出敏感 token 内容，仅记录是否存在与长度
    qDebug() << "[WorkbenchPage] accessToken状态:" << (accessToken.isEmpty() ? "空" : "存在")
             << "长度:" << accessToken.size();
    qDebug() << "[WorkbenchPage] refreshToken状态:" << (refreshToken.isEmpty() ? "空" : "存在")
             << "长度:" << refreshToken.size();
    qDebug() << "[WorkbenchPage] tenantId:" << (tenantId.isEmpty() ? "空" : tenantId);

    // 读取基础地址并做合法性校验
    const QString baseUrlText = ConfigManager::instance()->homeUrl().trimmed();
    if (baseUrlText.isEmpty())
    {
        qWarning() << "[WorkbenchPage] 未配置工作台网址";
        return;
    }

    const QUrl baseUrl = QUrl::fromUserInput(baseUrlText);
    if (!baseUrl.isValid() || baseUrl.scheme().isEmpty() || baseUrl.host().isEmpty())
    {
        qWarning() << "[WorkbenchPage] 工作台网址无效:" << baseUrlText;
        return;
    }

    QUrl targetUrl;
    if (!accessToken.isEmpty() && !refreshToken.isEmpty())
    {
        // 指向中转页面，token 通过查询参数传递，并使用 QUrlQuery 做 URL 编码
        targetUrl = baseUrl;
        targetUrl.setPath("/pad/pages/oauth/third-login");
        QUrlQuery query;
        query.addQueryItem("accessToken", accessToken);
        query.addQueryItem("refreshToken", refreshToken);
        query.addQueryItem("tenantId", tenantId);
        targetUrl.setQuery(query);

        qDebug() << "[WorkbenchPage] 使用中转页面 URL（已脱敏）:"
                 << QString("%1://%2:%3/pad/pages/oauth/third-login")
                        .arg(targetUrl.scheme())
                        .arg(targetUrl.host())
                        .arg(targetUrl.port(targetUrl.scheme() == "https" ? 443 : 80));
    }
    else
    {
        // 无 token 时直接加载首页，交给前端按未登录状态处理
        targetUrl = baseUrl;
        qWarning() << "[WorkbenchPage] Token 为空，加载工作台网页（未登录）:" << targetUrl.toString();
    }

    // 加载网页
    qDebug() << "[WorkbenchPage] 开始加载网页...";
    m_webView->load(targetUrl);
    qDebug() << "[WorkbenchPage] ========== 加载工作台结束 ==========";
}

/**
 * @brief 页面显示事件 - 每次显示时重新加载
 */
void WorkbenchPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    loadWorkbench(); // 每次显示页面时重新加载（获取最新的 token）
}
